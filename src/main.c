#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <unistd.h>
#include <sdkconfig.h>
#include <math.h>
#include "flyback_control.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "hbridge_driver.h"
#include "adc_monitor.h"
#define CURRENT_UP_GPIO  10
#define CURRENT_DOWN_GPIO 11
#define COMP_MS 100 //compensación cada 100 ms.
extern volatile float current_A;
static const char *TAG = "TENS_MAIN";
static QueueHandle_t gpio_evt_queue = NULL;
static uint32_t last_intr_time_up = 0;
static uint32_t last_intr_time_down = 0;
extern volatile float current_A;
static float volt_A;
int level = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t current_time = xTaskGetTickCountFromISR();
    uint32_t gpio_num = (uint32_t) arg;

    if (gpio_num == CURRENT_UP_GPIO) {
        if ((current_time - last_intr_time_up) > pdMS_TO_TICKS(200)) {
            xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
            last_intr_time_up = current_time;
        }
    } else if (gpio_num == CURRENT_DOWN_GPIO) {
        if ((current_time - last_intr_time_down) > pdMS_TO_TICKS(200)) {
            xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
            last_intr_time_down = current_time;
        }
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
    //rcfilter_init();
    hbridge_init(200);
    hbridge_start('A');
    flyback_init();
    voltage_monitor_init();
    current_monitor_init();
    uint32_t last_log_time = 0;
    ESP_LOGI(TAG, "I2C y GPIO inicializados.");


    while (1) {
        uint32_t io_num;
        
        if (xQueueReceive(gpio_evt_queue, &io_num, 0)) {
            if (io_num == CURRENT_UP_GPIO) {
                    if (level < 20) {
                                level+= 1;
                            }
                            else{
                                level=20;
                            }
                        
                        set_DAC_value(level, 'A');

                }
            else if (io_num == CURRENT_DOWN_GPIO) {
                            if (level > 0) {
                                level-= 1;
                            }
                            else{
                                level=0;
                            }
                            set_DAC_value(level,'A');
            }
        }
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
      
        
        // 3. Logueo controlado (Cada 1000ms)
        
        if (now - last_log_time > 1000) {
            volt_A = get_voltage('A');
            ESP_LOGI(TAG, "Level: %d, voltage: %f.  | corriente: %f.", level,volt_A, current_A);
            last_log_time = now;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
                }
}



