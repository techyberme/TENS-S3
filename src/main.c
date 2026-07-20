#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "oled.h"
#include "wifi.h"
#include "mqtt_cli.h"
#include "esp_random.h"
static const char *TAG = "MAIN_APP";
extern bool connected;
typedef enum {
    DAY_STATE,
    SESS_STATE,  // Esperando los 5 segundos de pulsación
    STATE_END,
    STATE_BASE
} main_state_t;
main_state_t main_state = DAY_STATE;
int day = 0;
int sess = 0;
int last_day = 0;
int last_sess = 0;

#define UP_GPIO   4           
#define DOWN_GPIO 5         
#define OK_GPIO   6




void rc_buttons_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << UP_GPIO) | (1ULL << DOWN_GPIO) | (1ULL << OK_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void app_main(void) {
    // 1. Inicialización de periféricos físicos
    rc_buttons_init();
    display_init();
    wifi_init();
    vTaskDelay(pdMS_TO_TICKS(5000)); 
    display_show_logo();
    mqtt_cli_init();
    bool last_up_state = false;
    bool last_down_state = false;
    bool last_ok_state = false;
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // Muestreo determinista a 50Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();
    draw_sens_day(0);
    while (1) {
        // Bloqueo preciso del RTOS
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Lectura de pines (Lógica negativa por Pull-Up pasivo)
        bool ok_pressed   = (gpio_get_level(OK_GPIO) == 0);
        bool up_pressed   = (gpio_get_level(UP_GPIO) == 0);
        bool down_pressed = (gpio_get_level(DOWN_GPIO) == 0);

        // Detección de flancos de subida (Rising Edge)
        bool up_trigger   = (up_pressed && !last_up_state);
        bool down_trigger = (down_pressed && !last_down_state);
        bool ok_trigger   = (ok_pressed && !last_ok_state);

        // Actualización del histórico para el siguiente ciclo
        last_up_state   = up_pressed;
        last_down_state = down_pressed;
        last_ok_state   = ok_pressed;
        switch (main_state){
            // Gestión del ciclo de trabajo mediante los triggers filtrados
            case DAY_STATE:
                if (up_trigger) {
                    day += 1;
                    if (day > 5) {
                        day = 5;     
                    }
                } 
                else if (down_trigger) {
                    if (day > 1) { 
                        day -= 1;
                    } else {
                        day = 1;
                    }
                }
                draw_sens_day(day);
                last_day = day;
                // El trigger del botón OK queda disponible aquí para transiciones en tu tensOS
                if (ok_trigger) {
                    main_state = SESS_STATE;
                }
                break;
            case SESS_STATE:
                if (up_trigger) {
                    sess += 1;
                    if (sess > 5) {
                        sess = 5;     
                    }
                } 
                else if (down_trigger) {
                    if (sess > 1) { 
                        sess -= 1;
                    } else {
                        sess = 1;
                    }
                }
               
                draw_sens_treat(sess);
                
                last_sess = sess;
                // El trigger del botón OK queda disponible aquí para transiciones en tu tensOS
                if (ok_trigger) {
                    main_state = STATE_END;
                }
                break;
            case STATE_END:

                mqtt_msg_t session_payload = {
                    .program_id= (esp_random() % 2) + 1,
                    .duration = esp_random() % 28,
                    .avg_intensity_ch_a = (esp_random() % 200) / 10.0f,
                    .avg_intensity_ch_b = (esp_random() % 200) / 10.0f,
                    .fault_events = 0,
                    .user_sensation_day = day,
                    .user_sensation_treatment = sess,
                };
                char *payload = generate_telemetry_json(&session_payload);
                if (payload != NULL) {
                    if (mqtt_cli_publish_telemetry(payload)) {
                        ESP_LOGI(TAG, "Message sent!");
                    }
                    free(payload); 
                }
                display_tens_shutdown();
                main_state = STATE_BASE;
                break;
            case STATE_BASE:
                if (ok_trigger) {
                    main_state = DAY_STATE;
                    day = 0;
                    sess = 0;
                }
        }
    }
}