#ifndef OLED_H
#define OLED_H
typedef enum {
    SCREEN_LOGO,
    SCREEN_CONFIG_TIME,
    SCREEN_CONFIG_PROG,
    SCREEN_RUNNING,
    SCREEN_DETACHED,
    SCREEN_BATTERY
} ui_state_t;

// Pin Definition
#define OLED_SDA_PIN 17 //19
#define OLED_SCL_PIN 18


void display_init(void);
void display_show_logo(void);
void display_start_ui_task(void);
void display_set_state(ui_state_t new_state);
void display_low_battery_warning(void);
void display_charge_shutdown_warning();

#endif