#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <unistd.h>
#include <sdkconfig.h>
#include "hbridge_driver.h"
#include "flyback_control.h"
#include "current_monitor.h"
 #include "esp_log.h"


static const char *TAG = "TENS_MAIN";

void app_main(void) {
    // 1. Inicialización de periféricos
    ESP_LOGI(TAG, "Inicializando sistema...");
    // Inicio ADC
    current_monitor_init();
    // Inicio Flyback
    flyback_init(); 
    
    //Deadtime de 5us (50 ticks a 10MHz)
    hbridge_init(50);

    // 2. Protocolo de seguridad inicial
    // Hay que cambiar el value
    //set_DAC_value(0); 
    //vTaskDelay(pdMS_TO_TICKS(100)); // Esperar estabilización

    // 3. Activación del tratamiento (Ejemplo)
    // ESP_LOGI(TAG, "Iniciando estimulación...");
    // flyback_enable(true); // Encender el LT3757

    // // Bucle principal: Control de intensidad
    // uint16_t intensidad = 0;
    while (1) {
        // Ejemplo: Rampa ascendente de voltaje para probar el Flyback
        // if (intensidad < 2000) { // Subir hasta la mitad del rango del DAC
        //     intensidad += 10;
        //     flyback_set_raw_value(intensidad);
        // }

        // En un TENS real, aquí leerías botones o un encoder para ajustar la potencia
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}