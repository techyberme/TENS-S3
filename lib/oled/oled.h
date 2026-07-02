#ifndef OLED_H
#define OLED_H
#include <stdint.h>
typedef enum {
    SCREEN_LOGO,
    SCREEN_INIT,
    SCREEN_CONFIG_TIME,
    SCREEN_CONFIG_PROG,
    SCREEN_RUNNING,
    SCREEN_DETACHED,
    SCREEN_BATTERY,
    SCREEN_DOCTOR_FREQ,
    SCREEN_DOCTOR_MODE,
    SCREEN_DOCTOR_BURST_HZ,
    SCREEN_DOCTOR_INIT
} ui_state_t;
// Pin Definition
#define OLED_SDA_PIN 11//8 //11
#define OLED_SCL_PIN 10 //18


void display_init(void);
void display_show_logo(void);
void display_start_ui_task(void);
void display_set_state(ui_state_t new_state);
void display_low_battery_warning(void);
void display_charge_shutdown_warning();
void draw_config_lev(uint32_t duty_cycle);
void draw_init(void) ;

#endif