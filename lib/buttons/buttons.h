#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LOCKED_STATE,
    UNLOCKING_STATE,
    DOCTOR_STATE,
    UNLOCKED_STATE_A,
    UNLOCKED_STATE_B
} button_lock_state_t;

#define UP_GPIO  10
#define DOWN_GPIO 11
#define OK_GPIO 12
#define POLL_RATE_MS       20
#define UNLOCK_HOLD_TICKS  (2000 / POLL_RATE_MS) // 100 ticks = 2 seconds
#define LOCK_TIMEOUT_TICKS (5000 / POLL_RATE_MS)

 


void buttons_init(void);


void buttons_task(void *pvParameters);

button_lock_state_t get_button_state(void);

#endif // BUTTON_UI_H