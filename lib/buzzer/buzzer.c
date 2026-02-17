#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "buzzer.h"
void buzzer_init(void) {
    // 1. Configuración del Timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = BUZZER_MODE,
        .timer_num        = BUZZER_TIMER,
        .duty_resolution  = LEDC_TIMER_13_BIT, // Resolución de 13 bits (0-8191)
        .freq_hz          = 2000,              // Frecuencia inicial 2kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // 2. Configuración del Canal
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = BUZZER_MODE,
        .channel        = BUZZER_CHANNEL,
        .timer_sel      = BUZZER_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = BUZZER_GPIO,
        .duty           = 0,                   // Empezamos apagado
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void beep(uint32_t duration_ms) {
    // Frecuencia estándar de feedback (2.7 kHz es muy audible)
    ledc_set_freq(BUZZER_MODE, BUZZER_TIMER, 2700);
    
    // Activar sonido (Duty 50%)
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 4096);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);

    // Esperar la duración del pitido
    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    // Apagar el sonido (Duty 0%)
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 0);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
}

void buzzer_alarm(){
    for (int i = 0; i < 3; i++) {
        // Tono un poco más agudo para la alarma (3.5 kHz)
        ledc_set_freq(BUZZER_MODE, BUZZER_TIMER, 3500);
        ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 4096);
        ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
        
        vTaskDelay(pdMS_TO_TICKS(150)); // Sonido
        
        ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 0);
        ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Silencio corto
    }

}