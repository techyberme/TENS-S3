#include "oled.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <unistd.h>
#include <sdkconfig.h>
#include "hbridge_driver.h"
#include "flyback_control.h"
#include "adc_monitor.h"
#include "intensity_control.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "settings.h"

extern volatile float current_ma_global;
static const char *TAG = "TENS_MAIN";


void app_main(void) {
    // 1. Inicialización de periféricos
    // 1. Inicializar hardware de pantalla
    display_init();
    //Initialize NVS for settings storage
    init_nvs();

    // 2. Mostrar logo 3 segundos
    display_show_logo();
    vTaskDelay(pdMS_TO_TICKS(3000));
    // 3. Lanzar la interfaz y continuar con el resto del sistema
    display_start_ui_task();
    flyback_init();
    buttons_init();
    current_monitor_init();
    // Esperamos a que el filtro RC de FB se cargue (p. ej. desde un DAC o PWM)
    // Si el filtro es de 5ms, espera 10ms por seguridad (2 constantes de tiempo).
    vTaskDelay(pdMS_TO_TICKS(10)); 

    // Ahora es seguro encender
    ESP_LOGI("INIT", "Filtro FB estabilizado. Encendiendo Flyback.");
    gpio_set_level(FLYBACK_EN_GPIO, 0); // Suelta el pin VC
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




