#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <math.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "intensity_control.h"
#include "flyback_control.h"
#include "hbridge_driver.h"
#define COMP_MS 100 //compensación cada 100 ms.
static const char *TAG = "CONTROL_LOGIC";
static system_state_t current_state = STATE_INIT;
static uint16_t current_dac_val = DAC_MIN_VAL;
static uint16_t saved_dac_val = DAC_MIN_VAL; //me sirve para guardar el DAC_valor en standby
static uint16_t low_current_counter = 0;    
static uint16_t recovery_counter = 0;   
static uint32_t last_compen_time = 0;
extern volatile float current_ma_global;
float target_ma = 5.0f; //valor inicial
int level= 0;
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
    
    ESP_LOGI(TAG, "Iniciando búsqueda de límite: 5mA");

    for (;;) {          //equivalente a while(1)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);  //espera 20 ms desde que se inicia la tarea, me permite calcular el tiempo de sesion
        switch (current_state) {
            case STATE_BASE:
                //if (current_dac_val < DAC_TARGET_VAL) {
                if (current_ma_global < 5.0f) { //al no ser el espejo ideal, no hay una clara correlación DAC-Corriente
                    // Verificación de límite inferior de seguridad para el DAC
                    if (current_ma_global<40.0f) {
                        current_dac_val += DAC_STEP;
                        set_DAC_value(current_dac_val);
                    } else {
                        ESP_LOGW(TAG, "Alta corriente detectada");
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
                else if (current_ma_global < 3.0f) {  //Tengo que pensar en el límite
                    low_current_counter++;
                    if (low_current_counter > 5) { // 100ms de seguridad
                        set_DAC_value(DAC_MIN_VAL); //apago DAC
                        ESP_LOGW(TAG, "Electrodos desconectados");
                        saved_dac_val = current_dac_val;
                        current_state = STATE_STANDBY;
                    }
                } else {
                    low_current_counter = 0;
                }
                uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (now - last_compen_time >= COMP_MS && !bridge_silence) {
                    last_compen_time = now;
                    float error = target_ma - current_ma_global;
                    //si el error es muy grande, DAC_STEP más agresivo, si es pequeño, DAC_STEP normal.
                    if (fabs(error) > 2.0f) { 
                        if (error > 0) {
                            if (current_dac_val < DAC_MAX_VAL) current_dac_val+= DAC_STEP*10;
                        } else {
                            if (current_dac_val > DAC_MIN_VAL) current_dac_val-= DAC_STEP*10;
                        }
                        set_DAC_value(current_dac_val);
                    }
                    else if (fabs(error) > 0.5f) { 
                        if (error > 0) {
                            if (current_dac_val < DAC_MAX_VAL) current_dac_val+= DAC_STEP;
                        } else {
                            if (current_dac_val > DAC_MIN_VAL) current_dac_val-= DAC_STEP;
                        }
                        set_DAC_value(current_dac_val);
                    }
                }
                    //no espero a que llegue info
                    if (xQueueReceive(gpio_evt_queue, &io_num, 0)) {
                        if (io_num == CURRENT_UP_GPIO) {
                                target_ma += 5.0f; // El usuario sube 5 mA, son 8 niveles
                                if (target_ma > 40.0f) target_ma = 40.0f;     
                        }
                        else if (io_num == CURRENT_DOWN_GPIO) {
                            target_ma -= 5.0f;
                            if (target_ma < 0.0f) target_ma = 0.0f;
                        }
                    ESP_LOGI(TAG, "Nuevo Objetivo: %.1f mA (DAC actual: %d)", target_ma, current_dac_val);
                    level = (int)(target_ma / 5.0f);   //nivel escogido por el usuario, de 0 a 8.
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
                if (current_time - last_poll_time > 500) { // Probar cada 500ms
                last_poll_time = current_time;

                // 2. Breve pulso de sondeo a 5 mA (o el mínimo de tu hardware)
                set_DAC_value(DAC_TARGET_VAL);
                // Pequeño delay para estabilización de la malla analógica
                esp_rom_delay_us(500); 
                // 3. Evaluar si hay contacto
                if (current_ma_global >= 3.0f) {
                    recovery_counter++;
                } else {
                    recovery_counter = 0;
                    set_DAC_value(DAC_MIN_VAL); // Volver a seguridad inmediatamente
                }
            }
            if (recovery_counter >= 3) {
                ESP_LOGI(TAG, "Contacto detectado. Iniciando rampa de recuperación...");
                // IMPORTANTE: No vuelvo de golpe a la corriente anterior.
                current_dac_val= DAC_MIN_VAL;
                set_DAC_value(DAC_MIN_VAL);
                current_state = STATE_RECU; 
                recovery_counter = 0;
            }
                break;
            case STATE_RECU:
                if (current_dac_val<saved_dac_val){
                    current_dac_val += DAC_STEP;
                    set_DAC_value(current_dac_val);
                    if (current_ma_global< 3.0f) { 
                        // Si se pierde el contacto otra vez durante la rampa, abortar
                        ESP_LOGI(TAG, "Contacto perdido durante recuperación");
                        set_DAC_value(DAC_MIN_VAL);
                        current_state = STATE_STANDBY;
                    }
                else{
                    ESP_LOGI(TAG, "Recuperación finalizada");
                    current_state = STATE_FUNC;

                }
                    
                }

                break;
            case STATE_DONE:
                flyback_stop();
                ESP_LOGI(TAG, "Programa completado");
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
