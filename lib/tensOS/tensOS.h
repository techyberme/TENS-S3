#ifndef INTENSITY_CONTROL_H
#define INTENSITY_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

#define WDI_GPIO 15
#define SESSION_DURATION         5    
#define SESSION_TICKS         (pdMS_TO_TICKS(SESSION_DURATION *60*1000))   
#define RECOVER_LEVEL 3  
typedef enum {
    STATE_ZERO,
    STATE_TIME,
    STATE_PROGRAM,
    STATE_INIT,          
    STATE_FUNC,             // Main State
    STATE_STANDBY,          // Disconnected Electrodes
    STATE_RECU,
    STATE_DONE,
    STATE_LOW_BATTERY,
    STATE_ERROR          
} system_state_t;

/**
 * @brief Main system's task
 */
void os_control_task(void *pvParameters);

/**
 * @return Returns the system's state
 */
system_state_t get_system_state(void);

void watchdog_init(void);
#endif  