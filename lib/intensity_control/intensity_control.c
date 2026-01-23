#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "intensity_control.h"
#include "flyback_control.h"
#include "hbridge_driver.h"
static const char *TAG = "CONTROL_LOGIC";
static system_state_t current_state = STATE_INIT;
static uint16_t current_dac_val = DAC_START_VAL;
static uint16_t base_dac = 0;           //corresponds to the DAC value for wich 20 mA are achieved
static uint16_t low_current_counter = 0;    
static uint16_t recovery_counter = 0;   
extern volatile float current_ma_global;
extern volatile bool bridge_silence;
static QueueHandle_t gpio_evt_queue = NULL;
static uint32_t time_session= 0;
static uint32_t last_intr_time = 0; // Tiempo de la última interrupción válida
// Manejador de la interrupción (ISR)
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t current_time = xTaskGetTickCountFromISR();
    uint32_t gpio_num = (uint32_t) arg;
    //200 ms desde la última que se ha pulsado el botón, para evitar oscilaciones
    if ((current_time - last_intr_time) > pdMS_TO_TICKS(200)) {
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
        last_intr_time = current_time;
    }
}
system_state_t get_system_state(void) {
    return current_state;
}
void buttons_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CURRENT_UP_GPIO) | (1ULL << CURRENT_DOWN_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE // Se activa al presionar (flanco de bajada)
    };
    gpio_config(&io_conf);
    // Crear la cola para 10 eventos
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Instalar el servicio de ISR y añadir manejadores
    gpio_install_isr_service(0);
    gpio_isr_handler_add(CURRENT_UP_GPIO, gpio_isr_handler, (void*) CURRENT_UP_GPIO);   //(se inicia, función, argumento ISR)
    gpio_isr_handler_add(CURRENT_DOWN_GPIO, gpio_isr_handler, (void*) CURRENT_DOWN_GPIO);
}
void flyback_control_task(void *pvParameters) {
    // 1. Asegurar estado inicial seguro
    flyback_enable(true); 
    set_DAC_value(current_dac_val);
    current_state = STATE_BASE;
    TickType_t xLastWakeTime = xTaskGetTickCount(); //Inicializo, después la tarea se encarga de actualizarla
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50 Hz exactos
    
    ESP_LOGI(TAG, "Iniciando búsqueda de límite: 20mA");

    for (;;) {          //equivalente a while(1)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);  //espera 20 ms desde que se inicia la tarea, me permite calcular el tiempo de sesion
        switch (current_state) {
            case STATE_BASE:
                if (current_ma_global < TARGET_CURRENT) {
                    // Verificación de límite inferior de seguridad para el DAC
                    if (current_dac_val > 0) {
                        current_dac_val -= DAC_STEP;
                        base_dac= current_dac_val;
                        set_DAC_value(current_dac_val);
                    } else {
                        ESP_LOGE(TAG, "Límite de seguridad DAC alcanzado sin llegar a 20mA");
                        current_state = STATE_ERROR;
                    }
                } else {
                    ESP_LOGI(TAG, "Límite alcanzado. Control cedido al usuario.");
                    current_state = STATE_FUNC;
                }
                break;

            // Dentro de flyback_control_task...
            case STATE_FUNC:
                uint32_t io_num;
            // Acumulación y Comprobación del tiempo
            time_session += xFrequency;  //solo acumlo en estate_func
            if (time_session>= SESSION_TICKS) {
                    ESP_LOGI(TAG, "Sesión terminada. Finalizando...");
                    current_state = STATE_DONE; 
                    break;
                }
            //Seguridad, electros desconectados
                // Si estamos en un silencio programado, reseteamos el contador de error
            if (bridge_silence) {
                low_current_counter = 0; 
            } 
            // Si NO es silencio y la corriente es baja, empezamos a contar para el error
            else if (current_ma_global < 5.0f) {
                low_current_counter++;
                if (low_current_counter > 15) { // 300ms de seguridad
                    ESP_LOGE(TAG, "Electrodos desconectados");
                    current_state = STATE_STANDBY;
                }
            } else {
                low_current_counter = 0;
            }
                //no espero a que llegue info
                if (xQueueReceive(gpio_evt_queue, &io_num, 0)) {
                    if (io_num == CURRENT_UP_GPIO) {
                            if (current_ma_global>40){
                            ESP_LOGE(TAG, "Límite de corriente alcanzado");
                        }
                            else    {update_user_current(-DAC_STEP_USER);}
                    } else if (io_num == CURRENT_DOWN_GPIO) {
                        update_user_current(DAC_STEP_USER);
                    }
                }
                break;
            case STATE_STANDBY:
                if (current_ma_global>8.0f){
                    recovery_counter++;
                    if (recovery_counter > 15){
                            ESP_LOGI(TAG, "Contacto recuperado. Reanudando terapia...");
                            current_state = STATE_FUNC;
                            recovery_counter = 0;
                        }
                    }
                else{
                    recovery_counter=0;
                }
                break;
            case STATE_DONE:
                flyback_stop();
                vTaskSuspend(NULL); // Bloquea la tarea por seguridad
                break;
            case STATE_ERROR:
                flyback_stop();
                vTaskSuspend(NULL); // Bloquea la tarea por seguridad
                break;

            default:
                break;
        }
    
    }
}

void update_user_current(int16_t delta) {
    if (current_state != STATE_FUNC) return;


    int32_t next_val = (int32_t)current_dac_val - delta;

    // Validación de límites antes de aplicar
    if (next_val >= DAC_START_VAL  && next_val <= 4095) {
        current_dac_val = (uint16_t)next_val;
        set_DAC_value(current_dac_val);
    }
}