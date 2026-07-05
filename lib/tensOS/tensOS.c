#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "tensOS.h"
#include "adc_mon_oneshot.h"
#include "boost_control.h"
#include "hbridge_driver.h"
#include "oled.h"
#include "buzzer.h"
#include "settings.h"
#include "wifi.h"
#include "mqtt_cli.h"
#define COMP_MS 100 //compensación cada 100 ms.
static const char *TAG = "CONTROL_LOGIC";
volatile SystemState_t current_state = STATE_INIT;
extern float batt_percentage;
extern bool s_wifi_ready;
extern bool s_mqtt_ready;
SystemState_t last_state = STATE_ZERO;
static uint32_t doctor_timer = 0;
static uint32_t disconnected_timer = 0;
static uint32_t acc_timer = 0;
static uint32_t session_seconds_sampled = 0;
static uint32_t level_A_accumulator = 0;
static uint32_t level_B_accumulator = 0;
static uint8_t fault_events = 0;
volatile int program = 1;
static int recovery_level = RECOVER_LEVEL; 
//Enable boost
static bool EN_PWR = false;
static bool mqtt_initiated = false;
uint32_t io_num;
volatile uint32_t time_session= 0;
volatile uint32_t duration_session = SESSION_DURATION;
volatile TensChannel_t ch_A = { .id = 'A', .level = 0, .current = 0.0f };
volatile TensChannel_t ch_B = { .id = 'B', .level = 0, .current = 0.0f };
extern volatile uint8_t day_score;
extern volatile uint8_t sess_score;
SystemState_t get_system_state(void) {
    return current_state;
}
void watchdog_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << WDI_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(WDI_GPIO, 0); 
}

void os_control_task(void *pvParameters) {
    current_state = STATE_INIT;
    TickType_t xLastWakeTime = xTaskGetTickCount(); //Inicializo, después la tarea se encarga de actualizarla
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50 Hz exactos
    //Watchdog variables
    uint8_t wdi_cycle_counter = 0;
    uint8_t wdi_current_state = 0;

    while(1) {         
        vTaskDelayUntil(&xLastWakeTime, xFrequency);  //Wait exactly 20 ms seconds, to calculate time
        //Watchdog
        wdi_cycle_counter++;
        if (wdi_cycle_counter >= 25) { //Every 500 ms
            wdi_current_state = !wdi_current_state; // Invert state
            gpio_set_level(WDI_GPIO, wdi_current_state);
            wdi_cycle_counter = 0;
        }
        //Actions carried on just once the state changes
        if (current_state != last_state) {
            ESP_LOGI(TAG, "Transición de estado: %d -> %d", last_state, current_state);
            if (current_state == STATE_INIT) {
                //clean stuff
                volatile TensChannel_t* channels[2] = {&ch_A, &ch_B};
                //clean everything
                for (int i = 0; i < 2; i++) {
                    channels[i]->level = 0;
                    channels[i]->applied_level = 0;
                    channels[i]->current = 0.0f;
                    channels[i]->low_current_cnt = 0;
                    channels[i]->status = CHAN_RUNNING; 
                }
                boost_enable(false); 
                set_DAC_value(0, 'A');
                // set_DAC_value(0, 'B');
                display_set_state(SCREEN_INIT);
            }  
            if (current_state == STATE_TIME) {
                doctor_data_t doctor_data = read_doctor();
                // --- DEBUG NVS DUMP ---
                ESP_LOGI(TAG, "--- LEYENDO NVS (DOC_KEY) ---");
                ESP_LOGI(TAG, "Estado Doctor:  %d", doctor_data.doctor);
                ESP_LOGI(TAG, "Duracion (min): %d", doctor_data.duration);
                ESP_LOGI(TAG, "Frecuencia:     %lu Hz", (unsigned long)doctor_data.frequency);
                ESP_LOGI(TAG, "Burst Freq:     %lu Hz", (unsigned long)doctor_data.burst_hz);
                ESP_LOGI(TAG, "Deadtime:       %lu ticks", (unsigned long)doctor_data.deadtime);
                ESP_LOGI(TAG, "Modo de Onda:   %d", doctor_data.mode);
                ESP_LOGI(TAG, "-----------------------------");
                //print stats
                export_stats_to_serial(); 
                if (doctor_data.doctor) {
                    ESP_LOGI(TAG, "Doctor mode is on");
                    duration_session = doctor_data.duration;
                    program = 4;
                    tens_program_t program_doctor;
                    program_doctor.burst_hz= doctor_data.burst_hz;
                    program_doctor.frequency_hz = doctor_data.frequency;
                    program_doctor.deadtime_ticks = doctor_data.deadtime;
                    program_doctor.mode = doctor_data.mode;
                    hbridge_init(&program_doctor);
                    current_state = STATE_FUNC;
                    beep(50);
                }
                ESP_LOGI(TAG, "displaying first screen");
                display_set_state(SCREEN_CONFIG_TIME);
                time_session = 0;
                //if (batt_percentage < 20) current_state = STATE_LOW_BATTERY;
            }
            if (current_state == STATE_PROGRAM) {
                display_set_state(SCREEN_CONFIG_PROG);
                ESP_LOGI("LED", "TURNING LED ON");
                update_led(ERR_NONE);
                program = 1; 
            }  
            if (current_state == STATE_FUNC) {
                display_set_state(SCREEN_RUNNING);
            }  
            if (current_state == STATE_LOW_BATTERY) {
                display_set_state(SCREEN_BATTERY);
            }  
            if (current_state == DOCTOR_CFG_FREQ) {
                display_set_state(SCREEN_DOCTOR_FREQ);
             } 
            if (current_state == DOCTOR_CFG_MODE) {
                display_set_state(SCREEN_DOCTOR_MODE);
             } 
             if (current_state == DOCTOR_CFG_BURST_HZ) {
                display_set_state(SCREEN_DOCTOR_BURST_HZ);
             } 
             
             if (current_state == STATE_DOCTOR_INIT) {
                display_set_state(SCREEN_DOCTOR_INIT);
                doctor_timer = 0;
             } 
             if (current_state == SURV_DAY) {
                buzzer_alarm();
                EN_PWR = false;
                boost_stop(WARN_DONE);
                display_set_state(SCREEN_SURV_DAY);
                //free ADC2
                adc_stop();
             } 
             if (current_state == SURV_SESS) {
                display_set_state(SCREEN_SURV_SESS);
             } 
            last_state = current_state;  
        }
        switch (current_state) {
            case STATE_INIT:
                break;
            case STATE_TIME:
                break;
            case STATE_PROGRAM:
                break;

            case STATE_FUNC:

                // Use of pointers to easily iterate
                volatile TensChannel_t* channels[2] = {&ch_A, &ch_B};
                if (ch_A.level == 0 && ch_B.level == 0) {
                    break;
                }
               
                if (!EN_PWR){
                    ESP_LOGI("INIT", "Turning on boost");
                    boost_enable(true);
                    EN_PWR = true;
                }
                for (int i = 0; i < 2; i++) {
                    volatile TensChannel_t* ch = channels[i];
                    //Take the other channel w/ XOR
                    volatile TensChannel_t* other_ch = channels[i ^ 1];
                    if (ch->status == CHAN_RUNNING){
                        if (ch->level == 0) {
                            hbridge_stop(ch->id);
                        } else if (ch->applied_level == 0 && ch->level > 0) {
                            ESP_LOGI("INIT", "Starting %c", ch->id);
                            hbridge_start(ch->id); 
                        }
                        if (ch->silence) {
                            ch->low_current_cnt = 0;
                            set_DAC_value(0, ch->id);
                            ch->was_silenced = true;
                            continue; //skip update logic
                        }
                        if (ch->applied_level != ch->level) {
                            if (abs(ch->level - other_ch->level) > 10 && (ch->level != 0 && other_ch->level != 0)) {
                                // if the difference is too big, keep the difference
                                other_ch->level += ch->level - ch->applied_level;
                                set_DAC_value(other_ch->level, other_ch->id);
                                set_DAC_value(ch->level, ch->id);
                                other_ch->applied_level = other_ch->level;
                                ch->applied_level = ch->level;
                            } else {
                                set_DAC_value(ch->level, ch->id);
                                ch->applied_level = ch->level;
                            }
                        }

                        if (ch->was_silenced) {
                            set_DAC_value(ch->level, ch->id);
                            ch->was_silenced = false;
                        }

                        // if (ch->current < 1.0f) {
                        //     ch->low_current_cnt++;
                        //     if (ch->low_current_cnt > 5) {
                        //         set_DAC_value(0, ch->id);
                        //         //hbridge_stop(ch->id);                                
                        //         ESP_LOGW(TAG, "Electrodos desconectados en canal %c", ch->id);
                        //         fault_events += 1;
                        //         ch->saved_level = ch->level;
                        //         ch->status = CHAN_STBY;
                        //     }
                        // } else {
                        //     ch->low_current_cnt = 0;
                        // }

                    }
                
                //if in standby
                    else if (ch->status == CHAN_STBY){
                    uint32_t current_time = esp_log_timestamp();
                    //Auxiliary variable to avoid calling the DAC too soon.
                    if (!ch->pulse_active) {
                        if (current_time - ch->last_poll_time > 500) {
                            set_DAC_value(recovery_level, ch->id); // Initiate a 6 mA pulse to check for contact
                            ch->pulse_active = true;
                            ch->last_poll_time = current_time;
                            disconnected_timer++;
                        }
                    } else {
                        // 20 ms after the pulse starts, check the current
                        if (ch->current > 1.0f) {
                            ch->recovery_counter++;
                            disconnected_timer = 0;
                        } else {
                            ch->recovery_counter = 0;
                            set_DAC_value(0, ch->id); //stop pulse
                            ch->pulse_active = false;
                        }
                    
                        if (ch->recovery_counter >= 3) {
                            ESP_LOGI(TAG, "Contacto detectado. Iniciando rampa de recuperación...");
                            ch->level= recovery_level;  //recovery level
                            set_DAC_value(ch->level,ch->id);
                            ch->status = CHAN_RECOVER; 
                            ch->recovery_counter = 0;
                            ch->pulse_active = false;
                        }
                    }
                    if (disconnected_timer >= 30){ //after 30 seconds
                        boost_stop(ERR_OPEN_CIRCUIT);
                        EN_PWR = false;
                        current_state = SURV_DAY;
                    }
                }
                    else if (ch->status == CHAN_RECOVER){
                        if (ch->level < ch->saved_level){
                            ch->level += 1; //recovery ramp
                            set_DAC_value(ch->level, ch->id);
                            if (ch->current < 1.0f) { 
                                // If contact is lost, go back to standby.
                                ESP_LOGI(TAG, "Contacto perdido durante recuperación");
                                set_DAC_value(0, ch->id);
                                ch->status = CHAN_STBY;
                            }
                }
                else{
                    ESP_LOGI(TAG, "Recuperación finalizada, el nivel es %d", ch->level);
                    ch->status = CHAN_RUNNING;

                }
                    }
                }
                if (ch_A.status == CHAN_RUNNING || ch_B.status == CHAN_RUNNING){
                time_session += xFrequency;  //just count if one of the channels is running
                }
                if (time_session>= pdMS_TO_TICKS(duration_session * 60000)) {
                        ESP_LOGI(TAG, "Sesión terminada. Finalizando...");
                        current_state = SURV_DAY; 
                        break;
                    }
                break;
            
            case STATE_DONE:
                uint8_t avg_A = 0;
                uint8_t avg_B = 0;
                if (!s_wifi_ready){
                    break;
                }
                if (s_wifi_ready){
                    if (!mqtt_initiated){
                        mqtt_cli_init();
                        mqtt_initiated = true;
                    }
                }
                if (!s_mqtt_ready) {
                    ESP_LOGW(TAG, "Waiting for MQTT broker");
                    break; 
                }
                if (session_seconds_sampled > 0) {
                    avg_A = (uint8_t)(level_A_accumulator / session_seconds_sampled);
                    avg_B = (uint8_t)(level_B_accumulator / session_seconds_sampled);
                }

                // save to flash
                write_stats(program, session_seconds_sampled/60 , avg_A, avg_B);
                
                mqtt_msg_t session_payload = {
                    .program_id= program,
                    .duration = session_seconds_sampled/60,
                    .avg_intensity_ch_a = avg_A,
                    .avg_intensity_ch_b = avg_B,
                    .fault_events = fault_events,
                    .user_sensation_day = day_score,
                    .user_sensation_treatment = sess_score,
                };
                char *payload = generate_telemetry_json(&session_payload);
                if (payload != NULL) {
                    if (mqtt_cli_publish_telemetry(payload)) {
                        ESP_LOGI(TAG, "Message sent!");
                    }
                    free(payload); 
                }
                // reset accumulators
                level_A_accumulator = 0;
                level_B_accumulator = 0;
                session_seconds_sampled = 0;
                ESP_LOGI(TAG, "Session Ended!");
                current_state = STATE_INIT; 
                break;
            case STATE_LOW_BATTERY:
                vTaskSuspend(NULL); // Block task 
                break;
            case STATE_ERROR:
                boost_stop(ERROR);
                EN_PWR = false;
                vTaskSuspend(NULL); 
                break;
            case STATE_DOCTOR_INIT:
                doctor_timer += 20; 
                
                //show Doctor's intro screen for 2 seconds
                if (doctor_timer >= 2000) {
                    current_state = DOCTOR_CFG_FREQ; 
                }
                break;
            case SURV_DAY:
            if (s_wifi_ready){
                if (!mqtt_initiated){
                    mqtt_cli_init();
                    mqtt_initiated = true;
                }
                
            }
                break;
            case SURV_SESS:
            if (s_wifi_ready){
                if (!mqtt_initiated){
                    mqtt_cli_init();
                    mqtt_initiated = true;
                }
                
            }
                break;
            default:
                break;

        }
        if (current_state == STATE_FUNC) {
            acc_timer += 20; 
            if (acc_timer >= 1000) {
                //take the info 
                level_A_accumulator += ch_A.level;
                level_B_accumulator += ch_B.level;
                session_seconds_sampled++;
                acc_timer= 0; // Reiniciamos el cronómetro
            }
        }
    }
}
