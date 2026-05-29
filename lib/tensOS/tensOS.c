#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "tensOS.h"
#include "adc_monitor.h"
#include "boost_control.h"
#include "hbridge_driver.h"
#include "oled.h"
#include "buzzer.h"
#include "settings.h"
#define COMP_MS 100 //compensación cada 100 ms.
static const char *TAG = "CONTROL_LOGIC";
volatile SystemState_t current_state = STATE_TIME;
extern float batt_percentage;
SystemState_t last_state = STATE_ZERO;
float max_ma = 50.0f; //valor inicial
volatile int program =1;
static int recovery_level = RECOVER_LEVEL; 
//Enable boost
static bool EN_PWR = false;
uint32_t io_num;
volatile uint32_t time_session= 0;
volatile uint32_t duration_session= SESSION_DURATION;
volatile TensChannel_t ch_A = { .id = 'A', .level = 0, .current = 0.0f };
volatile TensChannel_t ch_B = { .id = 'B', .level = 0, .current = 0.0f };
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
    // 1. Asegurar estado inicial seguro
    boost_enable(false); 
    set_DAC_value(0, 'A');
    set_DAC_value(0, 'B');
    current_state = STATE_TIME;
    TickType_t xLastWakeTime = xTaskGetTickCount(); //Inicializo, después la tarea se encarga de actualizarla
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50 Hz exactos
    //Watchdog variables
    uint8_t wdi_cycle_counter = 0;
    uint8_t wdi_current_state = 0;

    while(1) {         
        vTaskDelayUntil(&xLastWakeTime, xFrequency);  //espera 20 ms desde que se inicia la tarea, me permite calcular el tiempo de sesion
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
            
            if (current_state == STATE_TIME) {
                display_set_state(SCREEN_CONFIG_TIME);
                time_session = 0;
                if (batt_percentage < 20) current_state = STATE_LOW_BATTERY;
            }
            if (current_state == STATE_PROGRAM) {
                display_set_state(SCREEN_CONFIG_PROG);
                program = 1; 
            }  
            if (current_state == STATE_FUNC) {
                display_set_state(SCREEN_RUNNING);
            }  
            if (current_state == STATE_LOW_BATTERY) {
                display_set_state(SCREEN_BATTERY);
            }  
            last_state = current_state; // Actualizar para no repetir
        }
        switch (current_state) {
            case STATE_TIME:
                doctor_data_t doctor_data = read_doctor();
                if (doctor_data.doctor) {
                    duration_session = doctor_data.duration;
                    program = doctor_data.program;
                    current_state = STATE_FUNC;
                    beep(50);
                }

                break;
            case STATE_PROGRAM:
                break;

            // Dentro de boost_control_task...
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

                        if (ch->current < 1.0f) {
                            ch->low_current_cnt++;
                            if (ch->low_current_cnt > 5) {
                                set_DAC_value(0, ch->id);
                                hbridge_stop(ch->id);                                
                                ESP_LOGW(TAG, "Electrodos desconectados en canal %c", ch->id);
                                
                                ch->saved_level = ch->level;
                                ch->status = CHAN_STBY;
                            }
                        } else {
                            ch->low_current_cnt = 0;
                        }

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
                        }
                    } else {
                        // 20 ms after the pulse starts, check the current
                        if (ch->current > 1.0f) {
                            ch->recovery_counter++;
                        } else {
                            ch->recovery_counter = 0;
                            set_DAC_value(0, ch->id); // Apagar pulso
                            ch->pulse_active = false;
                        }
                    
                        if (ch->recovery_counter >= 3) {
                            ESP_LOGI(TAG, "Contacto detectado. Iniciando rampa de recuperación...");
                            // IMPORTANTE: No vuelvo de golpe a la corriente anterior.
                            ch->level= recovery_level;  //vuelvo al valor mínimo
                            set_DAC_value(ch->level,ch->id);
                            ch->status = CHAN_RECOVER; 
                            ch->recovery_counter = 0;
                            ch->pulse_active = false;
                        }
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
                        current_state = STATE_DONE; 
                        break;
                    }
                break;
            
            case STATE_DONE:
                buzzer_alarm();
                boost_stop(WARN_DONE);
                ESP_LOGI(TAG, "Programa completado");
                vTaskSuspend(NULL); // Bloquea la tarea por seguridad
                break;
            case STATE_LOW_BATTERY:
                vTaskSuspend(NULL); // Bloquea la tarea por seguridad
                break;
            case STATE_ERROR:
                boost_stop(ERR_IMPEDANCE_HIGH);
                vTaskSuspend(NULL); // Bloquea la tarea por seguridad
                break;

            default:
                break;
        
    
        }
    }
}
