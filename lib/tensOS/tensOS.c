#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "tensOS.h"
#include "flyback_control.h"
#include "hbridge_driver.h"
#include "oled.h"
#include "buzzer.h"
#include "settings.h"
#define COMP_MS 100 //compensación cada 100 ms.
static const char *TAG = "CONTROL_LOGIC";
volatile system_state_t current_state = STATE_TIME;
system_state_t last_state = STATE_ZERO;
float max_ma = 50.0f; //valor inicial
volatile int level_A= 0;
volatile int level_B= 0;
volatile int program =1;
static int saved_level_A = 0; //standby auxiliary value
static int applied_level_A = 0; //standby auxiliary value
static int saved_level_B = 1;
static int applied_level_B = 1; //standby auxiliary value
static int recovery_level = RECOVER_LEVEL; 
//Enable flyback
static bool EN_PWR = false;

static uint16_t low_current_counter = 0;    
static uint16_t recovery_counter = 0;   
static bool was_silenced = false; //dac silences
uint32_t io_num;
extern volatile float current_ma_global;
extern volatile bool A_bridge_silence;
volatile uint32_t time_session= 0;
volatile uint32_t duration_session= SESSION_DURATION;

system_state_t get_system_state(void) {
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
    flyback_enable(false); 
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
        //Load screen only when state changes.
        if (current_state != last_state) {
            ESP_LOGI(TAG, "Transición de estado: %d -> %d", last_state, current_state);
            
            // Acciones que se ejecutan UNA SOLA VEZ al entrar a un nuevo estado
            if (current_state == STATE_TIME) {
                display_set_state(SCREEN_CONFIG_TIME);
                time_session = 0;
            }
            if (current_state == STATE_PROGRAM) {
                display_set_state(SCREEN_CONFIG_PROG);
                program = 1; 
            }  
            if (current_state == STATE_FUNC) {
                display_set_state(SCREEN_RUNNING);
                program = 1; 
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

            // Dentro de flyback_control_task...
            case STATE_FUNC:
                // Acumulación y Comprobación del tiempo
                //Stay until the user set a level different from zero.
                while (level_A == 0 && level_B == 0) {
                    vTaskDelay(xFrequency); //Espero un ciclo antes de volver a comprobar
                }
                if (!EN_PWR){
                    flyback_enable(true);
                    EN_PWR = true;
                }
                            // Ahora es seguro encender
                ESP_LOGI("INIT", "Filtro FB estabilizado. Encendiendo Flyback.");
            
            
                time_session += xFrequency;  //solo acumlo en estate_func
                if (time_session>= pdMS_TO_TICKS(duration_session * 60000)) {
                        ESP_LOGI(TAG, "Sesión terminada. Finalizando...");
                        current_state = STATE_DONE; 
                        break;
                    }
                //Seguridad, electros desconectados
                    // If we are on a silence, the counter is reset. The value of the dac is reset to avoid spikes
                if (A_bridge_silence) {
                    low_current_counter = 0; 
                    set_DAC_value(0, 'A'); 
                    was_silenced = true;

                }
                else{
                    //update DAC if needed
                    if (applied_level_A != level_A) {
                    set_DAC_value(level_A, 'A');
                    applied_level_A = level_A;
                    }
                    if (applied_level_B != level_B) {
                        set_DAC_value(level_B, 'B');
                        applied_level_B = level_B;
                    }
                    if (was_silenced){
                    set_DAC_value(level_A, 'A'); //back to previous value.
                    was_silenced= false;
                    }
                    // if it's not a dead time, and current is low, we start counting 
                    if (current_ma_global < 0.0f) {  //TODO: Cambiarlo a 3.0
                        low_current_counter++;
                        if (low_current_counter > 5) { // 100ms de seguridad
                            set_DAC_value(0, 'A'); //DAC down for safety
                            flyback_enable(false);
                            EN_PWR = false;
                            ESP_LOGW(TAG, "Electrodos desconectados");
                            saved_level_A= level_A;
                            current_state = STATE_STANDBY;
                        }
                    } else {
                        low_current_counter = 0;
                    }
            }
                break;
            case STATE_STANDBY:
            /*
            Al reducir el ciclo de trabajo del
            espejo de corriente a un pulso de apenas 5 ms cada medio segundo, 
            el sistema permanece en un estado de "baja potencia" prácticamente inerte para el usuario, 
            pero suficiente para que el ADC valide la continuidad del circuito. Esto evita dejar una tensión de 80 V 
            expuesta de forma continua en los electrodos (lo cual podría causar una micro-estimulación desagradable o 
            degradación galvánica) y garantiza que la electrónica solo entregue potencia real cuando el modelo de impedancia de 
            la piel de Vargas Luna detecte una carga cerrada y estable.
            */
                static uint32_t last_poll_time = 0;
                uint32_t current_time = esp_log_timestamp();
                //Auxiliary varuable to avoid calling the DAC too soon.
                static bool pulse_active = false;
                if (!pulse_active) {
                    if (current_time - last_poll_time > 500) {
                        set_DAC_value(recovery_level, 'A'); // Initiate a 6 mA pulse to check for contact
                        pulse_active = true;
                        last_poll_time = current_time;
                    }
                } else {
                    // 20 ms after the pulse starts, check the current
                    if (current_ma_global >= 3.0f) {
                        recovery_counter++;
                    } else {
                        recovery_counter = 0;
                        set_DAC_value(0, 'A'); // Apagar pulso
                        pulse_active = false;
                    }
                
                    if (recovery_counter >= 3) {
                        ESP_LOGI(TAG, "Contacto detectado. Iniciando rampa de recuperación...");
                        // IMPORTANTE: No vuelvo de golpe a la corriente anterior.
                        level_A= recovery_level;  //vuelvo al valor mínimo
                        set_DAC_value(level_A, 'A');
                        current_state = STATE_RECU; 
                        recovery_counter = 0;
                        pulse_active = false;
                    }
                    }
        
                break;
            case STATE_RECU:
                if (level_A < saved_level_A){
                    level_A += 1; //recovery ramp
                    set_DAC_value(level_A, 'A');
                    if (current_ma_global< 3.0f) { 
                        // If contact is lost, go back to standby.
                        ESP_LOGI(TAG, "Contacto perdido durante recuperación");
                        set_DAC_value(0, 'A');
                        current_state = STATE_STANDBY;
                    }
                }
                else{
                    ESP_LOGI(TAG, "Recuperación finalizada, el nivel es", level_A);
                    current_state = STATE_FUNC;

                }

                break;
            case STATE_DONE:
                buzzer_alarm();
                flyback_stop(WARN_DONE);
                ESP_LOGI(TAG, "Programa completado");
                vTaskSuspend(NULL); // Bloquea la tarea por seguridad
                break;
            case STATE_ERROR:
                flyback_stop(ERR_IMPEDANCE_HIGH);
                vTaskSuspend(NULL); // Bloquea la tarea por seguridad
                break;

            default:
                break;
        
    
        }
    }
}
