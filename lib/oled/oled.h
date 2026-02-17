#ifndef OLED_H
#define OLED_H
typedef enum {
    SCREEN_LOGO,
    SCREEN_CONFIG_TIME,
    SCREEN_CONFIG_PROG,
    SCREEN_RUNNING
} ui_state_t;

// Pin Definition
#define OLED_SDA_PIN 16
#define OLED_SCL_PIN 17


void display_init(void);
void display_show_logo(void);
void display_start_ui_task(void);
void display_set_state(ui_state_t new_state);

#endif