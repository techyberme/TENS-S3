#ifndef HBRIDGE_DRIVER_H
#define HBRIDGE_DRIVER_H

#include <stdint.h>
#include "esp_err.h"


#define HBRIDGE_GPIO_A  4
#define HBRIDGE_GPIO_B  5

typedef enum {
    TENS_MODE_CONTINUO, // 4 kHz constante
    TENS_MODE_BURST,      // 4 kHz modulated at 100 Hz
    TENS_MODE_OFF         
} tens_mode_t;


void hbridge_init(uint32_t deadtime_ticks);

void hbridge_stop(void);


#endif