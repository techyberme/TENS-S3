#ifndef INTENSITY_CONTROL_H
#define INTENSITY_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/queue.h"

#define UP_GPIO  10
#define DOWN_GPIO 11
#define OK_GPIO 12
#define DAC_MIN_VAL    0  
#define DAC_TARGET_VAL  2095  // 0.05 mA para empezar
#define DAC_MAX_VAL    4096  // tomo 1.4V como el salto de los bjts.
#define DAC_STEP         20    // 0,1 mA/salto
#define SESSION_DURATION         5    
#define SESSION_TICKS         (pdMS_TO_TICKS(SESSION_DURATION *60*1000))    
//button logic
#define POLL_RATE_MS       20
#define UNLOCK_HOLD_TICKS  (2000 / POLL_RATE_MS) // 100 ticks = 2 seconds
#define LOCK_TIMEOUT_TICKS (5000 / POLL_RATE_MS)
/* --- Definiciones de Estados del Sistema --- */
typedef enum {
    STATE_TIME,
    STATE_PROGRAM,
    STATE_INIT,          
    STATE_BASE,             // 5 mA lookup phase
    STATE_FUNC,             // Main State
    STATE_STANDBY,          // Disconnected Electrodes
    STATE_RECU,
    STATE_DONE,
    STATE_ERROR          
} system_state_t;

/* Buttons*/
typedef enum {
    LOCKED_STATE, 
    UNLOCKING_STATE,         
    UNLOCKED_STATE_A,         
    UNLOCKED_STATE_B,                     
} button_state_t;

/**
 * @brief Main system's task
 */
void flyback_control_task(void *pvParameters);

/**
 * @return Returns the system's state
 */
system_state_t get_system_state(void);

/**
 * @return Returns the buttons's state
 */
button_state_t get_button_state(void);


void buttons_init(void);
#endif  