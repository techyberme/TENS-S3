#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <unistd.h>
#include <sdkconfig.h>
#include <math.h>
#include "hbridge_driver.h"
#include "flyback_control.h"
#include "adc_monitor.h"
#include "intensity_control.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#define CURRENT_UP_GPIO  10
#define CURRENT_DOWN_GPIO 11
#define COMP_MS 100 //compensación cada 100 ms.
static uint32_t last_compen_time = 0;
extern volatile float current_ma_global;
static const char *TAG = "TENS_MAIN";
static QueueHandle_t gpio_evt_queue = NULL;
static uint32_t last_intr_time = 0;
static uint16_t dac_val = 0;
static float current_wanted = 50.0f;
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t current_time = xTaskGetTickCountFromISR();
    uint32_t gpio_num = (uint32_t) arg;
    //200 ms desde la última que se ha pulsado el botón, para evitar oscilaciones
    if ((current_time - last_intr_time) > pdMS_TO_TICKS(200)) {
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
        last_intr_time = current_time;
    }
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
void app_main(void) {
    // 1. Inicialización de periféricos
    buttons_init();
    flyback_init();
    hbridge_init(50);
    current_monitor_init();
    uint32_t last_log_time = 0;
    
    ESP_LOGI(TAG, "I2C y GPIO inicializados.");


    while (1) {
        uint32_t io_num;
        
        if (xQueueReceive(gpio_evt_queue, &io_num, 0)) {
            if (io_num == CURRENT_UP_GPIO) {
                    current_wanted += 10.0f;

                }
            else if (io_num == CURRENT_DOWN_GPIO) {
                current_wanted -= 10.0f;
                if (current_wanted < 0.0f) {
                    current_wanted = 0.0f;
                }
            }
        }
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_compen_time >= COMP_MS ) {
                    last_compen_time = now;
                    float error = current_wanted - current_ma_global;
                    //si el error es muy grande, DAC_STEP más agresivo, si es pequeño, DAC_STEP normal.
                    if (fabs(error) > 2.0f) { 
                        if (error > 0) {
                            if (dac_val < 4085) {
                                dac_val+= 50;
                            }
                            else{
                                dac_val=4095;
                            }
                        } else {
                            if (dac_val > 50) 
                            {dac_val-= 50;
                            }
                            else{
                                dac_val=0;
                            }
                        }
                        set_DAC_value(dac_val);
                    }
                    else if (fabs(error) > 0.5f) { 
                        if (error > 0) {
                            if (dac_val < 4085) {
                                dac_val+= 10;
                            }
                            else{
                                dac_val=4095;
                            }
                        } else {
                            if (dac_val > 100) 
                            {dac_val-= 10;
                            }
                            else{
                                dac_val=0;
                            }
                        }
                        set_DAC_value(dac_val);
                    }
                }
        
        // 3. Logueo controlado (Cada 1000ms)
        
        if (now - last_log_time > 1000) {
            // Imprimimos la lectura del ADC que la otra tarea está actualizando
            ESP_LOGI(TAG, "DAC: %d | Real (ADC): %.1f mA, Objetivo: %.1f mA", 
                    dac_val, current_ma_global, current_wanted);
            last_log_time = now;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
                }
}



