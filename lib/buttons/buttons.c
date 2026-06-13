#include "buttons.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "buzzer.h"
#include "tensOS.h" 
#include "settings.h"

static const char *TAG = "BUTTONS";

// Global Variables
extern TensChannel_t ch_A;
extern TensChannel_t ch_B;
extern volatile int program;
extern volatile uint32_t duration_session;
extern volatile SystemState_t current_state; 


static button_state_t lock_state = UNLOCKED_STATE_A;
static uint32_t hold_counter = 0;
static uint32_t inactivity_counter = 0;

button_state_t get_button_state(void) {
    return lock_state;
}

void buttons_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << UP_GPIO) | (1ULL << DOWN_GPIO) | (1ULL << OK_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void buttons_task(void *pvParameters) {
    bool last_up_state = false;
    bool last_down_state = false;
    bool last_ok_state = false;
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // Muestreo a 50Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        bool ok_pressed = (gpio_get_level(OK_GPIO) == 0);
        bool up_pressed = (gpio_get_level(UP_GPIO) == 0);
        bool down_pressed = (gpio_get_level(DOWN_GPIO) == 0);

        // Detección de flanco de subida (solo dispara 1 vez por pulsación)
        bool up_trigger = (up_pressed && !last_up_state);
        bool down_trigger = (down_pressed && !last_down_state);
        bool ok_trigger = (ok_pressed && !last_ok_state);

        last_up_state = up_pressed;
        last_down_state = down_pressed;
        last_ok_state = ok_pressed;

        // --- STATE_MACHINE ---
        switch (current_state)
        {
        case STATE_TIME:
            if (up_trigger) {
                    duration_session += 1;
                    if (duration_session > 40) duration_session = 40;     
                    beep(50);
                } else if (down_trigger) {
                    duration_session -= 1;
                    if (duration_session < 5) duration_session = 5;    
                    beep(50);
                } else if (ok_trigger) {
                    current_state = STATE_PROGRAM;
                    beep(50);
                }
                break;
        case STATE_PROGRAM:
            if (up_trigger) {
                    program += 1;
                    if (program > 5) program = 5;     
                    beep(50);
                } else if (down_trigger) {
                    program -= 1;
                    if (program < 1) program = 1;    
                    beep(50);
                } else if (ok_trigger) {
                    current_state = STATE_FUNC;
                    beep(50);
                }
                break;
        case STATE_FUNC:
        switch (lock_state) {
                    case LOCKED_STATE:
                        if (ok_pressed) {
                            lock_state = UNLOCKING_STATE;
                            hold_counter = 0;
                        }
                        break;
                        if (down_pressed) {
                            lock_state = DOCTOR_STATE;
                            hold_counter = 0;
                        }
                        break;

                    case DOCTOR_STATE:
                        if (down_pressed) {
                            hold_counter++;
                            if (hold_counter >= 250) { // 5 seconds hold
                                beep(400);
                                //check if doctor mode is already set.
                                doctor_data_t doctor_data = read_doctor();
                                if (!doctor_data.doctor) {
                                    doctor_data_t doctor_set = {
                                    .doctor = true,
                                    .program = program,
                                    .duration = duration_session
                                    };
                                    write_doctor(doctor_set.doctor, doctor_set.program, doctor_set.duration);
                                    ESP_LOGI(TAG, "Doctor mode set");
                                    lock_state = LOCKED_STATE;
                                }
                                //if doctor is set, go back to regular mode. Go back to time configuration.
                                else{
                                    write_doctor(false, 0, 0); //reset doctor mode
                                    ESP_LOGI(TAG, "Doctor mode reset");
                                    lock_state = STATE_TIME;
                                }
                                
                            }
                        }
                        else lock_state = LOCKED_STATE;  
                            break;
                    case UNLOCKING_STATE:
                        if (ok_pressed) {
                            hold_counter++;
                            if (hold_counter >= UNLOCK_HOLD_TICKS) {
                                lock_state = UNLOCKED_STATE_A;
                                inactivity_counter = 0;
                                beep(200);
                                ESP_LOGI(TAG, "Level A unlocked");
                            }
                        }
                        else lock_state = LOCKED_STATE;  
                            break;
                    case UNLOCKED_STATE_A:
                        inactivity_counter++;
                        if (up_trigger) {
                            ch_A.level++;
                            if (ch_A.level> 20) ch_A.level = 20; 
                            inactivity_counter = 0;    
                            beep(50);
                        } else if (down_trigger) {
                            if (ch_A.level > 0) {
                                ch_A.level--;
                            }
                            inactivity_counter = 0;   
                            beep(50);
                        } else if (ok_trigger) {
                            lock_state = UNLOCKED_STATE_B;
                            inactivity_counter = 0;
                            beep(100);
                            ESP_LOGI(TAG, "Level B unlocked");
                        }
                        if (inactivity_counter > LOCK_TIMEOUT_TICKS) {
                            lock_state = LOCKED_STATE;
                            inactivity_counter = 0;
                            beep(200);
                            ESP_LOGI(TAG, "Buttons locked due to inactivity");
                        }
                        break;
                    case UNLOCKED_STATE_B:
                        inactivity_counter++;
                        if (up_trigger) {
                            ch_B.level++;
                            if (ch_B.level > 20) ch_B.level = 20;   
                            inactivity_counter = 0;  
                            beep(50);
                        } else if (down_trigger) {
                            if (ch_B.level > 0) {
                                ch_B.level--;
                            }
                            inactivity_counter = 0;   
                            beep(50);
                        } else if (ok_trigger) {
                            lock_state = UNLOCKED_STATE_A;
                            inactivity_counter = 0;
                            beep(100);
                            ESP_LOGI(TAG, "Level A unlocked");
                        }
                        if (inactivity_counter > LOCK_TIMEOUT_TICKS) {
                            lock_state = LOCKED_STATE;
                            inactivity_counter = 0;
                            beep(200);
                            ESP_LOGI(TAG, "Buttons locked due to inactivity");
                        }
                        break;
                    default:
                        break;

                    }
                    break;
        default:
            break;
        }
        
    }
}