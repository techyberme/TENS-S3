#ifndef INTENSITY_CONTROL_H
#define INTENSITY_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

#define WDI_GPIO 15
#define SESSION_DURATION         5    
#define SESSION_TICKS         (pdMS_TO_TICKS(SESSION_DURATION *60*1000))   
#define RECOVER_LEVEL 3  
typedef enum {
    CHAN_RUNNING,               
    CHAN_STBY,
    CHAN_RECOVER,  
} ChannelStatus_t;
//Channel's structure
typedef struct {
    char id;
    uint8_t level;
    uint8_t applied_level;
    uint8_t saved_level;
    float current;
    float voltage;
    bool silence;
    bool was_silenced;
    uint8_t low_current_cnt;
    uint8_t recovery_counter;
    uint32_t last_poll_time;
    bool pulse_active;
    ChannelStatus_t status;
} TensChannel_t;

 
typedef enum {
    STATE_ZERO,
    STATE_TIME,
    STATE_PROGRAM,
    STATE_INIT,          
    STATE_FUNC,             // Main State
    STATE_DONE,
    STATE_LOW_BATTERY,
    STATE_ERROR          
} SystemState_t;

/**
 * @brief Main system's task
 */
void os_control_task(void *pvParameters);

/**
 * @return Returns the system's state
 */
SystemState_t get_system_state(void);

void watchdog_init(void);
#endif  