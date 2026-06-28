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
 


void app_main(void) {
    //module initialization
    init_nvs();
    display_init();
    display_show_logo();
    //boost_init();
    buzzer_init();
    buttons_init();
    vTaskDelay(pdMS_TO_TICKS(3000));
    // 3. Lanzar la interfaz y continuar con el resto del sistema
    display_start_ui_task();
    //charge_task();
    xTaskCreate( 
        os_control_task,    
        "ControlTask",          // Debug TAG
        4096,                   // stack size
        NULL,                   // pvParameters 
        10,                     // Priority
        NULL                   // Handle
    );

    xTaskCreate(buttons_task, "ButtonsTask", 4096, NULL, 4, NULL);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


