#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <unistd.h>
#include <sdkconfig.h>
#include "hbridge_driver.h"
#include "flyback_control.h"
#include "current_monitor.h"
#include "intensity_control.h"
#include "esp_log.h"
#include "driver/i2c.h"

extern volatile float current_ma_global;
static const char *TAG = "TENS_MAIN";


void app_main(void) {
    // 1. Inicialización de periféricos
    vTaskDelay(pdMS_TO_TICKS(10000));
    flyback_init();
    buttons_init();
    current_monitor_init();
    uint32_t last_log_time = 0;
    //2. Aquí es donde REALMENTE creas la tarea de control
    xTaskCreate( 
        flyback_control_task,   // Función que definiste en current_control.c
        "ControlTask",          // Nombre para debug
        4096,                   // Tamaño del stack
        NULL,                   // pvParameters 
        10,                     // Prioridad
        NULL                   // Handle
    );
    ESP_LOGI(TAG, "I2C y GPIO inicializados.");


    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_log_time > 1000) {
            // Imprimimos la lectura del ADC que la otra tarea está actualizando
            ESP_LOGI(TAG, "Corriente Medida: %.1f mA", current_ma_global);
            last_log_time = now;
        }
         vTaskDelay(pdMS_TO_TICKS(20));
    }
}


