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

    
    bool doctor = false;    
    int duration = 0;     
    int program  = 0;

    ESP_LOGI(TAG, "Writing doctor...");
    err = nvs_set_u8(h, "doctor", doctor);
    if (err != ESP_OK)  ESP_LOGE(TAG, "Error writing values!");

    ESP_LOGI(TAG, "Writing duration...");
    err = nvs_set_i32(h, "duration", duration);
    if (err != ESP_OK)  ESP_LOGE(TAG, "Error writing values!");

    ESP_LOGI(TAG, "Writing program...");
    err = nvs_set_i32(h, "program", program);
    if (err != ESP_OK)  ESP_LOGE(TAG, "Error writing values!");

    ESP_LOGI(TAG, "\nCommitting updates in NVS...");

    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

}
void write_doctor(bool doctor, int program, int duration)
{
    esp_err_t err;
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &h));


    ESP_LOGI(TAG, "Writing doctor...");
    err = nvs_set_u8(h, "doctor", doctor ? 1 : 0);
    if (err != ESP_OK)  ESP_LOGE(TAG, "Error writing values!");

    ESP_LOGI(TAG, "Writing duration...");
    err = nvs_set_i32(h, "duration", duration);
    if (err != ESP_OK)  ESP_LOGE(TAG, "Error writing values!");

    ESP_LOGI(TAG, "Writing program...");
    err = nvs_set_i32(h, "program", program);
    if (err != ESP_OK)  ESP_LOGE(TAG, "Error writing values!");

    ESP_LOGI(TAG, "\nCommitting updates in NVS...");

    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);


}

doctor_data_t read_doctor(void){
    
    doctor_data_t out = {0};
    esp_err_t err;
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &h));

    uint8_t doctor_u8 = 0;
    err = nvs_get_u8(h, "doctor", &doctor_u8);
    out.doctor = (doctor_u8 != 0);
    if (err != ESP_OK)  ESP_LOGE(TAG, "Error reading values!");

    int read_duration = 0;
    err = nvs_get_i32(h, "duration", &read_duration);
    out.duration = read_duration;
    if (err != ESP_OK)  ESP_LOGE(TAG, "Error reading values!");

    int read_program = 0;
    err = nvs_get_i32(h, "program", &read_program);
    out.program = read_program;
    if (err != ESP_OK)  ESP_LOGE(TAG, "Error reading values!");

    nvs_close(h);
    
    return out;
}