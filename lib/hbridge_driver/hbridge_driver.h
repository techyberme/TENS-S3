#ifndef HBRIDGE_DRIVER_H
#define HBRIDGE_DRIVER_H

#include <stdint.h>
#include "esp_err.h"


#define HBRIDGE_GPIO_A1  4
#define HBRIDGE_GPIO_B1  5
#define HBRIDGE_GPIO_A2  14
#define HBRIDGE_GPIO_B2  15
#define GPIO_OR  7

typedef enum {
    TENS_MODE_CONTINUO, // 4 kHz c 
    TENS_MODE_BURST,      // 4 kHz modulated at 100 Hz
    TENS_MODE_OFF         
} tens_mode_t;


void hbridge_init(uint32_t deadtime_ticks);

void hbridge_stop(char channel);

void hbridge_start(char channel);

void hbridge_set_mode(tens_mode_t mode);



#endif