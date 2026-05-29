#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <unistd.h>
#include <sdkconfig.h>
#include "hbridge_driver.h"
#include "boost_control.h"
#include "current_monitor.h"
#include "intensity_control.h"
#include "esp_log.h"
#include "driver/i2c.h"


static const char *TAG = "TENS_MAIN";

// void app_main(void) {
//     // 1. Inicialización de periféricos
//     ESP_LOGI(TAG, "Inicializando sistema...");
//     // Inicio ADC
//     //current_monitor_init();
//     // Inicio Flyback
//     //boost_init(); 
    
//     //Deadtime de 5us (50 ticks a 10MHz)
//     hbridge_init(200);
//     //Control del boost
//     // 2. Aquí es donde REALMENTE creas la tarea de control
//     xTaskCreate( 
//         boost_control_task,   // Función que definiste en current_control.c
//         "ControlTask",          // Nombre para debug
//         4096,                   // Tamaño del stack
//         NULL,                   // pvParameters (el que preguntaste antes)
//         10,                     // Prioridad
//         NULL                   // Handle
//     );
//     // 2. Protocolo de seguridad inicial
//     // Hay que cambiar el value
//     //set_DAC_value(0); 
//     //vTaskDelay(pdMS_TO_TICKS(100)); // Esperar estabilización

//     // 3. Activación del tratamiento (Ejemplo)
//     // ESP_LOGI(TAG, "Iniciando estimulación...");
//     // boost_enable(true); // Encender el LT3757

//     // // Bucle principal: Control de intensidad
//     // uint16_t intensidad = 0;
//     while (1) {
//         // Ejemplo: Rampa ascendente de voltaje para probar el Flyback
//         // if (intensidad < 2000) { // Subir hasta la mitad del rango del DAC
//         //     intensidad += 10;
//         //     boost_set_raw_value(intensidad);
//         // }

//         // En un TENS real, aquí leerías botones o un encoder para ajustar la potencia
//         vTaskDelay(pdMS_TO_TICKS(50)); 
//     }
// }
void app_main(void) {
    // 1. Inicialización de periféricos
    vTaskDelay(pdMS_TO_TICKS(10000));
    boost_init();
    ESP_LOGI(TAG, "I2C y GPIO inicializados.");

    uint16_t dac_val = 0;

    while (1) {
        // Generar una rampa de 0 a 40mA
        for (dac_val = 0; dac_val <= 4095; dac_val += 100) {
            
            set_DAC_value(dac_val);

            ESP_LOGI(TAG, "DAC Val: %d", dac_val);
            
            // Pausa de 200ms para poder medir con calma
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        // Pequeño silencio de seguridad al final de la rampa
        ESP_LOGW(TAG, "Rampa finalizada. Silencio de 2 segundos.");
        set_DAC_value(0); 
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}


