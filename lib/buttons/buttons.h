#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LOCKED_STATE,
    UNLOCKING_STATE,
    DOCTOR_STATE,
    UNLOCKED_STATE_A,
    UNLOCKED_STATE_B,
    DOCTOR_HOLD_STATE,       // Esperando los 5 segundos de pulsación
} button_state_t;

#define UP_GPIO  6
#define DOWN_GPIO 7
#define OK_GPIO 9
#define POLL_RATE_MS       20
#define UNLOCK_HOLD_TICKS  (2000 / POLL_RATE_MS) // 100 ticks = 2 seconds
#define LOCK_TIMEOUT_TICKS (5000 / POLL_RATE_MS)

 


void buttons_init(void);


void buttons_task(void *pvParameters);
void write_new_config();
button_state_t get_button_state(void);

#endif // BUTTON_UI_H