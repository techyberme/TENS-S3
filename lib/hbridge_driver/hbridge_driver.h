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
} tens_mode_t;

typedef struct {
    uint32_t frequency_hz;      
    uint32_t deadtime_ticks;   
    tens_mode_t mode;           
    uint32_t burst_hz;   
} tens_program_t;
extern const tens_program_t PROGRAM_DATABASE[];

void hbridge_init(const tens_program_t *prog);

/*
    Erase all handlers and stop the timer.
*/
void hbridge_deinit(void);
void hbridge_stop(char channel);

void hbridge_start(char channel);

void hbridge_set_mode(tens_mode_t mode);



#endif