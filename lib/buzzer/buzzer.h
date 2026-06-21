#ifndef BUFFER_H
#define BUFFER_H
#include <stdio.h>
#include "driver/ledc.h"

#define BUZZER_GPIO       42 //18           // Pin donde conectas el buzzer
#define BUZZER_CHANNEL    LEDC_CHANNEL_0
#define BUZZER_TIMER      LEDC_TIMER_0
#define BUZZER_MODE       LEDC_LOW_SPEED_MODE

void buzzer_init(void);
/**
 * @brief Button beep
 */
void beep(uint32_t duration_ms);
void buzzer_alarm(void);

#endif