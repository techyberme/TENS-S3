#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi.h"
#include "mqtt_cli.h"
#include "esp_random.h"
static const char *TAG = "MAIN_APP";
extern bool connected;


// Tarea de FreeRTOS para generar y publicar telemetría de prueba
void telemetry_task(void *pvParameters) {
    mqtt_msg_t mock_session = {
        .program_id= 2,
        .duration = 0,
        .fault_events = 0,
        .user_sensation_day = 3,
        .user_sensation_treatment = 1,
    };

    while (1) {
        mock_session.avg_intensity_ch_a= (esp_random() % 200) / 10.0f;  // 0.0 - 20.0 mA
        mock_session.avg_intensity_ch_b= (esp_random() % 200) / 10.0f;  
        mock_session.duration += 5; 
        
        
        char *payload = generate_telemetry_json(&mock_session);
        if (payload != NULL) {
            if (mqtt_cli_publish_telemetry(payload)) {
                ESP_LOGI(TAG, "Message set -> mA_A: %.1f", 
                          mock_session.avg_intensity_ch_a);
            }
            free(payload); // Liberación del heap crítico
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
void app_main(void) {
     SP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, " Iniciando Prueba de Conexión Wi-Fi - ESP32-S3  ");
    ESP_LOGI(TAG, "=================================================");

    // Inicializa el módulo de Wi-Fi de forma asíncrona (asigna tareas al Core 0)
    wifi_init();
    vTaskDelay(pdMS_TO_TICKS(5000)); 
    
    mqtt_cli_init();
    xTaskCreatePinnedToCore(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL, 0);

    // Bucle principal para mantener el firmware vivo mientras los eventos gestionan la red
    while (1) {
        ESP_LOGI(TAG, "Sistema operativo corriendo... Monitoreando hilos.");
        vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
}