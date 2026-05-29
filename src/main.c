#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "adc_monitor.h"
#include "boost_control.h" // Incluye donde esté tu función del DAC
#include "oled.h"
#include "tensOS.h"
#include "buttons.h"
#include "hbridge_driver.h"
#include "buzzer.h"
#include "settings.h"
static const char *TAG = "MAIN";


void app_main(void) {
    boost_init(); // Inicializa tu sistema de control del boost si es necesario
    buttons_init(); // Inicializa los botones
    adc_monitor_init();
    // Esperamos a que el filtro RC de FB se cargue (p. ej. desde un DAC o PWM)
    // Si el filtro es de 5ms, espera 10ms por seguridad (2 constantes de tiempo).
    vTaskDelay(pdMS_TO_TICKS(10)); 
    display_init();
    buzzer_init();
    init_nvs();
    uint32_t last_log_time = 0;
    xTaskCreate( 
        os_control_task,   // Función que definiste en current_control.c
        "ControlTask",          // Nombre para debug
        4096,                   // Tamaño del stack
        NULL,                   // pvParameters 
        10,                     // Prioridad
        NULL                   // Handle
    );

    xTaskCreate( 
        buttons_task,   
        "ButtonsTask",          // Nombre para debug
        4096,                   // Tamaño del stack
        NULL,                   // pvParameters 
        4,                     // Prioridad
        NULL                   // Handle
    );
    ESP_LOGI(TAG, "I2C y GPIO inicializados.");
    display_start_ui_task();

    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_log_time > 1000) {
            // Imprimimos la lectura del ADC que la otra tarea está actualizando
             last_log_time = now;
        }
         vTaskDelay(pdMS_TO_TICKS(20));
    }
}

