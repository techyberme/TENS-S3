#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "tensOS.h"
#include "settings.h"
//key to find the doctor data in NVS
#define DOC_KEY "doctor_data"
static const char *TAG = "nvs_app";

void init_nvs(void){
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI("NVS", "Driver NVS inicializado correctamente.");

}
void write_doctor(const doctor_data_t *doctor_program)
{
    
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &h));

    ESP_LOGI(TAG, "Writing data...");
    esp_err_t err = nvs_set_blob(h, DOC_KEY, doctor_program, sizeof(doctor_data_t));
    if (err != ESP_OK)  ESP_LOGE(TAG, "Error writing values!");

    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);


}

doctor_data_t read_doctor(void){
    doctor_data_t out = {0};
    nvs_handle_t h;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error abriendo NVS para lectura.");
        return out;
    }
    size_t required_size = sizeof(doctor_data_t);
    err = nvs_get_blob(h, DOC_KEY, &out, &required_size);
    if (err != ESP_OK)  ESP_LOGE(TAG, "Error reading values!");
    nvs_close(h);
    return out;
}


void write_stats(uint8_t n_program, uint8_t duration, uint8_t avg_level_A, uint8_t avg_level_B)
{
    nvs_handle_t h;
    uint32_t session_index = 0;
    
    //namespace stats
    ESP_ERROR_CHECK(nvs_open("stats", NVS_READWRITE, &h));

    // read total sessions
    nvs_get_u32(h, "total_s", &session_index);

    //Blob with stats structure
    stats_t current_session = {
        .n_program = n_program,
        .duration = duration,
        .avg_level_A = avg_level_A,
        .avg_level_B = avg_level_B
    };

    // key : s_x, eg. s_1
    char key_name[16];
    snprintf(key_name, sizeof(key_name), "s_%lu", (unsigned long)session_index);

    ESP_LOGI(TAG, "Writing session %s to NVS...", key_name);
    
    // save entry
    esp_err_t err = nvs_set_blob(h, key_name, &current_session, sizeof(stats_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing session blob!");
    } else {
        // increment index
        session_index++;
        nvs_set_u32(h, "total_s", session_index);
    }

    ESP_LOGI(TAG, "Committing updates in NVS...");
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
}


void export_stats_to_serial(void) {
    nvs_handle_t h;
    uint32_t total_s = 0;
    stats_t session;
    
    if (nvs_open("stats", NVS_READONLY, &h) == ESP_OK) {
        //take total session
        nvs_get_u32(h, "total_s", &total_s);
        // Print in CSV Structure
        printf("Numero_Programa,Duracion,Media_Nivel_A,Media_Nivel_B\n");
        for (uint32_t i = 0; i < total_s; i++) {
            char key[16];
            snprintf(key, sizeof(key), "s_%lu", (unsigned long)i);
            size_t size = sizeof(stats_t);
            if (nvs_get_blob(h, key, &session, &size) == ESP_OK) {
                printf("%d,%d,%d,%d\n", session.n_program, session.duration, 
                                       session.avg_level_A, session.avg_level_B);
            }
        }
        nvs_close(h);
    }
}
