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
    //module initialization
    mcp4725_init_safe_start();
    init_nvs();
    watchdog_init();
    boost_init(); 
    rcfilter_init();
    buttons_init(); // Inicializa los botones
    adc_monitor_init();
    display_init();
    buzzer_init();
    uint32_t last_log_time = 0;
    xTaskCreate( 
        os_control_task,    
        "ControlTask",          // Debug TAG
        4096,                   // stack size
        NULL,                   // pvParameters 
        10,                     // Priority
        NULL                   // Handle
    );

    xTaskCreate(buttons_task, "ButtonsTask", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Initizialization complete");
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



