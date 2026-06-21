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
    //Open NVS handle
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &h)); 
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

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