#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi.h"
#include "mqtt_cli.h"
#include "esp_random.h"
static const char *TAG = "MAIN_APP";


// Tarea de FreeRTOS para generar y publicar telemetría de prueba
void telemetry_task(void *pvParameters) {
    telemetry_payload_t mock_session = {
        .device_uuid = "c4a760a8-d5e3-4f91-9876-123456789abc",
        .session_id = 1000,
        .n_program = 3,
        .duration_s = 0,
        .fault_events = 0
    };

    while (1) {
        // Generación de valores sintéticos representativos
        mock_session.z_avg_ohm = 1000.0f + (esp_random() % 2000); // Impedancia entre 1k y 3k ohms
        mock_session.avg_level_A = (esp_random() % 200) / 10.0f;  // 0.0 - 20.0 mA
        mock_session.avg_level_B = (esp_random() % 200) / 10.0f;  
        mock_session.duration_s += 5; 
        
        char *payload = generate_telemetry_json(&mock_session);
        if (payload != NULL) {
            if (mqtt_cli_publish_telemetry(payload)) {
                ESP_LOGI(TAG, "Telemetría encolada -> Z: %.1f ohm, mA_A: %.1f", 
                         mock_session.z_avg_ohm, mock_session.avg_level_A);
            }
            free(payload); // Liberación del heap crítico
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGI(TAG, "=================================================");
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