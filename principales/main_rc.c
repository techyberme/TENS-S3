#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <unistd.h>
#include <sdkconfig.h>
#include <math.h>
#include "flyback_control.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#define CURRENT_UP_GPIO  10
#define CURRENT_DOWN_GPIO 11
#define COMP_MS 100 //compensación cada 100 ms.
extern volatile float current_A;
static const char *TAG = "TENS_MAIN";
static QueueHandle_t gpio_evt_queue = NULL;
static uint32_t last_intr_time_up = 0;
static uint32_t last_intr_time_down = 0;
static uint16_t rc_val = 0;
float real_dac = 0.;
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
    rcfilter_init();
    uint32_t last_log_time = 0;
    ESP_LOGI(TAG, "I2C y GPIO inicializados.");


    while (1) {
        uint32_t io_num;
        
        if (xQueueReceive(gpio_evt_queue, &io_num, 0)) {
            if (io_num == CURRENT_UP_GPIO) {
                    if (rc_val < 37) {
                                rc_val+= 1;
                            }
                            else{
                                rc_val=38;
                            }
                        
                        set_pwm_duty_cycle(rc_val);

                }
            else if (io_num == CURRENT_DOWN_GPIO) {
                            if (rc_val > 1) {
                                rc_val-= 1;
                            }
                            else{
                                rc_val=0;
                            }
                            set_pwm_duty_cycle(rc_val);
            }
        }
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
      
        
        // 3. Logueo controlado (Cada 1000ms)
        
        if (now - last_log_time > 1000) {
            // Imprimimos la lectura del ADC que la otra tarea está actualizando
            real_dac = 3.3 * rc_val/100;
            ESP_LOGI(TAG, "Duty cycle: %d | Real: %.2f V", 
                      rc_val, real_dac);
            last_log_time = now;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
                }
}



