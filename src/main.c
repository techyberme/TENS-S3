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
   display_init();
    uint32_t last_log_time = 0;
    
 

    uint32_t last_screen_time = 0;
    int screen_state = 0;

    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Evaluamos si han pasado 3000 ms (3 segundos)
        if (now - last_screen_time >= 3000) {
            last_screen_time = now;

            // Conmutador de pantallas según el estado actual
            switch (screen_state) {
                case 0:
                    display_show_logo();
                    break;
                case 1:
                    display_low_battery_warning();
                    break;
                case 2:
                    display_charge_shutdown_warning();
                    break;
                default:
                    break;
            }

            // Incrementamos el estado para la siguiente vuelta (0 -> 1 -> 2 -> 0...)
            screen_state = (screen_state + 1) % 3;
        }

        // Tu retraso base para que la tarea no sature la CPU
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


