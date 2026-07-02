/*
 * Adaptación para ESP32-S3 - LED RGB Interno
 * Versión de librería: led_strip v3.0.x
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"
#include "oled.h"

static const char *TAG = "RGB_DEBUG";


void app_main(void)
{
    display_init();
    uint8_t color_state = 0; // 0: Rojo, 1: Verde, 2: Azul, 3: Blanco

    ESP_LOGI(TAG, "Iniciando ciclo de colores en el LED");

    while (1) {
        switch (color_state) {
            case 0: // Rojo
                display_charge_shutdown_warning() ;
                ESP_LOGI(TAG, "Color: ROJO");
                break;
            case 1: // Verde
                display_low_battery_warning();
                ESP_LOGI(TAG, "Color: VERDE");
                break;
            case 2: // Azul
                draw_init();
                ESP_LOGI(TAG, "Color: AZUL");
                break;
        }



        // Incrementar estado y resetear al llegar a 4
        color_state = (color_state + 1) % 3;

        // Esperar un segundo antes del siguiente cambio
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Opcional: Si quieres que se apague entre colores, añade un clear aquí
        // ESP_ERROR_CHECK(led_strip_clear(led_strip));
        // vTaskDelay(pdMS_TO_TICKS(200));
    }
}