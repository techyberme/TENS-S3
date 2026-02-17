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

// En la mayoría de ESP32-S3 DevKits, el LED RGB está en el GPIO 48
#define LED_STRIP_GPIO_PIN  48
// Solo hay 1 LED en la placa
#define LED_STRIP_LED_COUNT 1
// Resolución de 10MHz para el RMT
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

static const char *TAG = "RGB_DEBUG";

led_strip_handle_t configure_led(void)
{
    // 1. Configuración general del LED
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,
        .max_leds = LED_STRIP_LED_COUNT,
        .led_model = LED_MODEL_WS2812, // Modelo estándar en S3
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }
    };

    // 2. Configuración del backend RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .mem_block_symbols = 0, // Auto
        .flags = {
            .with_dma = false, // No necesario para 1 solo LED
        }
    };

    led_strip_handle_t led_handle;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_handle));
    
    return led_handle;
}

void app_main(void)
{
    led_strip_handle_t led_strip = configure_led();
    uint8_t color_state = 0; // 0: Rojo, 1: Verde, 2: Azul, 3: Blanco

    ESP_LOGI(TAG, "Iniciando ciclo de colores en el LED");

    while (1) {
        switch (color_state) {
            case 0: // Rojo
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 50, 0, 0));
                ESP_LOGI(TAG, "Color: ROJO");
                break;
            case 1: // Verde
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 0, 50, 0));
                ESP_LOGI(TAG, "Color: VERDE");
                break;
            case 2: // Azul
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 0, 0, 50));
                ESP_LOGI(TAG, "Color: AZUL");
                break;
            case 3: // Blanco (Mezcla de los tres)
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 30, 30, 30));
                ESP_LOGI(TAG, "Color: BLANCO");
                break;
        }

        // Aplicar el color seleccionado
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));

        // Incrementar estado y resetear al llegar a 4
        color_state = (color_state + 1) % 4;

        // Esperar un segundo antes del siguiente cambio
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Opcional: Si quieres que se apague entre colores, añade un clear aquí
        // ESP_ERROR_CHECK(led_strip_clear(led_strip));
        // vTaskDelay(pdMS_TO_TICKS(200));
    }
}