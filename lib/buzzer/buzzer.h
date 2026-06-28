#ifndef BUFFER_H
#define BUFFER_H
#include <stdio.h>
#include "driver/ledc.h"

#define BUZZER_GPIO       7        
#define BUZZER_CHANNEL    LEDC_CHANNEL_0
#define BUZZER_TIMER      LEDC_TIMER_0
#define BUZZER_MODE       LEDC_LOW_SPEED_MODE
//todo ver esto
void buzzer_init(void);
/**
 * @brief Button beep
 */
void beep(uint32_t duration_ms);
void buzzer_alarm(void);

#endif