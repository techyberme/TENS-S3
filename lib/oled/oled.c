#include "oled.h"
#include <u8g2.h>
#include "u8g2_esp32_hal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tensOS.h"
#include "buttons.h"

static volatile ui_state_t current_state = SCREEN_LOGO;
static void draw_doctor_init(void); 
static void draw_doctor_freq(void);
static void draw_doctor_mode(void);
static void draw_doctor_burst(void);
//static void draw_init(void);
static ui_state_t past_state = SCREEN_LOGO;
static const char* TAG = "OLED";
static u8g2_t u8g2;
extern uint32_t time_session;
// extern volatile uint8_t day_score;
// extern volatile uint8_t sess_score;
volatile extern uint32_t duration_session;
volatile extern int program;
extern TensChannel_t ch_A;
extern TensChannel_t ch_B;
volatile extern uint32_t doc_setup_freq;
volatile extern uint8_t  doc_setup_mode;
volatile extern uint32_t doc_setup_burst;
const unsigned char logo[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x7f, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x80, 0x07, 0xe0, 0x01, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x01, 0x80, 0x07, 0x00, 0x00, 
	0x00, 0x00, 0x70, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x1c, 0x00, 0x00, 
	0x00, 0x00, 0x1c, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x70, 0x00, 0x00, 
	0x00, 0x00, 0x06, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x01, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x03, 0x80, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x00, 
	0x00, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x00, 0x00, 0x80, 0x00, 0x44, 0x22, 0x00, 0x01, 0x00, 
	0x00, 0xc0, 0x00, 0x44, 0x22, 0x00, 0x03, 0x00, 0x00, 0xc0, 0x00, 0x46, 0x62, 0x00, 0x03, 0x00, 
	0x00, 0xc0, 0x00, 0x4e, 0x72, 0x00, 0x03, 0x00, 0x00, 0xc0, 0x00, 0x6a, 0x56, 0x00, 0x03, 0x00, 
	0x00, 0xc0, 0xf0, 0x29, 0x94, 0x0f, 0x03, 0x00, 0x00, 0xc0, 0x00, 0x38, 0x1c, 0x00, 0x03, 0x00, 
	0x00, 0xc0, 0x00, 0x30, 0x0c, 0x00, 0x03, 0x00, 0x00, 0xc0, 0x00, 0x30, 0x0c, 0x00, 0x03, 0x00, 
	0x00, 0x80, 0x00, 0x10, 0x08, 0x00, 0x01, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80, 0x01, 0x00, 
	0x00, 0x80, 0x01, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x03, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x60, 0x00, 0x00, 
	0x00, 0x00, 0x0e, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x38, 0x00, 0x00, 
	0x00, 0x00, 0x38, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x0e, 0x00, 0x00, 
	0x00, 0x00, 0xe0, 0x01, 0x80, 0x07, 0x00, 0x00, 0x00, 0x00, 0x80, 0x07, 0xe0, 0x01, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xfe, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
void display_set_state(ui_state_t new_state) {
    current_state = new_state;
}
static void dumpScreenToSerial(u8g2_t *u8g2) {

    uint8_t* buffer = u8g2_GetBufferPtr(u8g2);
    size_t buffer_size = 8 * u8g2_GetBufferTileHeight(u8g2) * u8g2_GetBufferTileWidth(u8g2);

    ESP_LOGI("UI_DUMP", "---START_FRAME---");
    // This will automatically format and print the entire buffer in 16-byte chunks
    ESP_LOG_BUFFER_HEX("UI_DUMP", buffer, buffer_size);
    ESP_LOGI("UI_DUMP", "---END_FRAME---");

}
void display_init(void) {
  u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
  u8g2_esp32_hal.bus.i2c.sda = OLED_SDA_PIN;
  u8g2_esp32_hal.bus.i2c.scl = OLED_SCL_PIN;
  u8g2_esp32_hal_init(u8g2_esp32_hal);

    u8g2_Setup_sh1106_i2c_128x64_noname_f(&u8g2, U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb);  // These are I2C callback function for mapping 


  u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);  // The address is left-shifted 1 bit
  ESP_LOGI(TAG, "u8g2_InitDisplay");
  u8g2_InitDisplay(&u8g2);  // send init sequence to the display, display is in sleep mode after this,

  ESP_LOGI(TAG, "u8g2_SetPowerSave");
  u8g2_SetPowerSave(&u8g2, 0);  // wake up display
  ESP_LOGI(TAG, "u8g2_ClearBuffer");
    }
void display_show_logo(void) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawXBM(&u8g2, 35, 0, 64, 64, logo);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&u8g2, 15, 62, "STARTING TENS...");
    u8g2_SendBuffer(&u8g2);
}




static void draw_time_screen() {
    char buf[16];
    
    u8g2_ClearBuffer(&u8g2);
    
    // Title
    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "CONFIGURACION");
    u8g2_DrawHLine(&u8g2, 0, 12, 128);

    // Central Tag
    u8g2_SetFont(&u8g2, u8g2_font_helvB10_tr); 
    const char* tag_text = "Tiempo";
    int tag_width = u8g2_GetStrWidth(&u8g2, tag_text);
    u8g2_DrawStr(&u8g2, (128 - tag_width) / 2, 30, tag_text);
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    //Blinking
    //if ((now / 500) % 2 == 0){
        // Valor de los minutos (Grande)
        int mins = duration_session;
        int secs = 0;
        u8g2_SetFont(&u8g2, u8g2_font_helvB18_tr);
        snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
        int text_width = u8g2_GetStrWidth(&u8g2, buf);
        int x_centered = (128 - text_width) / 2;
        u8g2_DrawStr(&u8g2, x_centered, 58, buf);
        //}

    // Guía para el usuario en la parte inferior
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(&u8g2, 0, 64, "[-]                   [+]");
    
    u8g2_SendBuffer(&u8g2);
}
//static to make them private
static void draw_config_prog() {
    char buf[16];
    
    u8g2_ClearBuffer(&u8g2);
    
    // Title
    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "CONFIGURACION");
    u8g2_DrawHLine(&u8g2, 0, 12, 128);

    // Central Tag
    u8g2_SetFont(&u8g2, u8g2_font_helvB10_tr); 
    const char* tag_text = "Programa";
    int tag_width = u8g2_GetStrWidth(&u8g2, tag_text);
    u8g2_DrawStr(&u8g2, (128 - tag_width) / 2, 30, tag_text);
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    //Blinking
    if ((now / 500) % 2 == 0){
         u8g2_SetFont(&u8g2, u8g2_font_helvB24_tr);
        snprintf(buf, sizeof(buf), "%02d", program);
        
        int text_width = u8g2_GetStrWidth(&u8g2, buf);
        int x_centered = (128 - text_width) / 2;
        
        u8g2_DrawStr(&u8g2, x_centered, 58, buf);
       }

    // Guía para el usuario en la parte inferior
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(&u8g2, 0, 64, "[-]                   [+]");
    
    u8g2_SendBuffer(&u8g2);
}

static void draw_doctor_freq(){
    char buf[16];
    
    u8g2_ClearBuffer(&u8g2);
    
    // Title
    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "MODO DOCTOR");
    u8g2_DrawHLine(&u8g2, 0, 12, 128);

    u8g2_SetFont(&u8g2, u8g2_font_helvB10_tr); 
    const char* tag_text = "Frecuencia";
    int tag_width = u8g2_GetStrWidth(&u8g2, tag_text);
    u8g2_DrawStr(&u8g2, (128 - tag_width) / 2, 30, tag_text);
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    //Blinking
    //if ((now / 250) % 2 == 0){
        u8g2_SetFont(&u8g2, u8g2_font_helvB14_tr);
        sprintf(buf, "%lu Hz", doc_setup_freq);
        u8g2_DrawStr(&u8g2, 30, 58, buf);  
      //  }

    // Guía para el usuario en la parte inferior
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(&u8g2, 0, 64, "[-]                   [+]");
    
    u8g2_SendBuffer(&u8g2);
}
static void draw_doctor_mode(){
        char buf[16];
    
    u8g2_ClearBuffer(&u8g2);
    
    // Title
    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "MODO DOCTOR");
    u8g2_DrawHLine(&u8g2, 0, 12, 128);

    // Central Tag
    u8g2_SetFont(&u8g2, u8g2_font_helvB10_tr); 
    const char* tag_text = "Modo";
    int tag_width = u8g2_GetStrWidth(&u8g2, tag_text);
    u8g2_DrawStr(&u8g2, (128 - tag_width) / 2, 30, tag_text);
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    //Blinking
   if ((now / 250) % 2 == 0){
         u8g2_SetFont(&u8g2, u8g2_font_helvB18_tr);
        if (doc_setup_mode == 1) sprintf(buf, "%s", "BURST");
        else sprintf(buf, "%s", "NORMAL");
        int text_width = u8g2_GetStrWidth(&u8g2, buf);
        int x_centered = (128 - text_width) / 2;
        
        u8g2_DrawStr(&u8g2, x_centered, 58, buf); 
       }
        

    // Guía para el usuario en la parte inferior
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(&u8g2, 0, 64, "[-]                   [+]");
    
    u8g2_SendBuffer(&u8g2);
}
static void draw_doctor_burst(){
        char buf[16];
    
    u8g2_ClearBuffer(&u8g2);
    
    // Title
    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "MODO DOCTOR");
    u8g2_DrawHLine(&u8g2, 0, 12, 128);

    u8g2_SetFont(&u8g2, u8g2_font_helvB10_tr); 
    const char* tag_text = "Frec. BURST";
    int tag_width = u8g2_GetStrWidth(&u8g2, tag_text);
    u8g2_DrawStr(&u8g2, (128 - tag_width) / 2, 30, tag_text);
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    //Blinking
    //if ((now / 250) % 2 == 0){
        // Valor de los minutos (Grande)
        u8g2_SetFont(&u8g2, u8g2_font_helvB18_tr);
        sprintf(buf, "%lu Hz", doc_setup_burst);
        u8g2_DrawStr(&u8g2, 30, 58, buf);  
     //   }

    // Guía para el usuario en la parte inferior
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(&u8g2, 0, 64, "[-]                   [+]");
    
    u8g2_SendBuffer(&u8g2);
    
}

static void draw_main_ui()
{
    u8g2_ClearBuffer(&u8g2);
    char buf[16];
                    // Categories
    u8g2_SetFont(&u8g2, u8g2_font_helvB10_tr); 
    u8g2_DrawStr(&u8g2, 10, 20, "Nivel A");
    u8g2_DrawStr(&u8g2, 80, 20, "Nivel B");

    // Values
    u8g2_SetFont(&u8g2, u8g2_font_fub20_tn); 
    button_state_t button_state = get_button_state();

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    //Blinking
    //if (button_state == UNLOCKED_STATE_A){
        if ((now / 250) % 2 == 0){
         // Level A
        sprintf(buf, "%d",ch_A.level);
        u8g2_DrawStr(&u8g2, 15, 42, buf);  
        }
   // }
    //else{
         // Level A
    sprintf(buf, "%d",ch_A.level);
    u8g2_DrawStr(&u8g2, 15, 42, buf);
    //}
    if (button_state == UNLOCKED_STATE_B){
        if ((now / 250) % 2 == 0){
         // Level B
        sprintf(buf, "%d",ch_B.level);
        u8g2_DrawStr(&u8g2, 85, 42, buf); 
        }
    }
    else{
         // Level B
        sprintf(buf, "%d",ch_B.level);
        u8g2_DrawStr(&u8g2, 85, 42, buf);
    }

                  
    u8g2_DrawHLine(&u8g2, 0, 42, 128); 

    // Time left
    //duration_session(mins) * 60 *1000
    int ms_left=  duration_session * 60 *1000 - pdTICKS_TO_MS(time_session);   
    //in case there's a mismatch, avoid showing negative time.

    if (ms_left < 0) ms_left = 0;         
                    
    int mins = ms_left/1000/ 60;
    int secs = ms_left/1000 % 60;
    u8g2_SetFont(&u8g2, u8g2_font_helvB12_tr);
    sprintf(buf, "%02d:%02d", mins, secs);
    u8g2_DrawStr(&u8g2, 40, 58, buf);    

     u8g2_SendBuffer(&u8g2);
}
static void display_task(void *pvParameters) {
    while (1) {
            switch(current_state){
                case SCREEN_LOGO:
                    break;
                case SCREEN_INIT:
                    draw_init();
                    break;
                case SCREEN_CONFIG_PROG:
                    draw_config_prog();
                    break;
                case SCREEN_CONFIG_TIME:
                    draw_time_screen();
                    break;
                case SCREEN_RUNNING:
                    draw_main_ui();
                    break;
                case SCREEN_DETACHED:
                    //TODO
                    break;  
                case SCREEN_BATTERY:
                    break; 
                case SCREEN_DOCTOR_MODE:
                    draw_doctor_mode();
                    break;
                case SCREEN_DOCTOR_FREQ:
                    draw_doctor_freq();
                    break;
                case SCREEN_DOCTOR_BURST_HZ:
                    draw_doctor_burst();
                    break;
                case SCREEN_DOCTOR_INIT:
                    draw_doctor_init();
                    break;
                case SCREEN_SURV_DAY:
                    //draw_sens_day();
                    break;
                case SCREEN_SURV_SESS:
                    //draw_sens_treat();
                    break;
            }
        
        if (current_state != past_state) {
            
            // The buffer now contains the first frame of the new UI state
            dumpScreenToSerial(&u8g2);
            
            // Update past_state so this block doesn't trigger again until the next change
            past_state = current_state; 
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // wahit 100 ms
    }

}


void display_start_ui_task(void) {
    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);
}

void display_low_battery_warning() {
    u8g2_ClearBuffer(&u8g2);

    // open_iconic_all_4x
    u8g2_SetFont(&u8g2, u8g2_font_open_iconic_embedded_4x_t);
    u8g2_DrawGlyph(&u8g2, 48, 35, 64); 
 
    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr); // Fuente compacta para el título
    const char* str1 = "BATERIA";
    int width1 = u8g2_GetStrWidth(&u8g2, str1);
    u8g2_DrawStr(&u8g2, (128 - width1) / 2, 50, str1);

    u8g2_SetFont(&u8g2, u8g2_font_9x15_tf); 
    const char* str2 = "BAJA";
    int width2 = u8g2_GetStrWidth(&u8g2, str2);
    u8g2_DrawStr(&u8g2, (128 - width2) / 2, 64, str2);

    u8g2_SendBuffer(&u8g2);
    dumpScreenToSerial(&u8g2);
}

void display_charge_shutdown_warning() {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_open_iconic_embedded_4x_t);
    u8g2_DrawGlyph(&u8g2, 48, 35, 71); 

    // 2. Configurar texto informativo superior
    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr); 
    const char* str1 = "CARGA DETECTADA"; // ~90 píxeles de ancho (entra en los 128)
    int width1 = u8g2_GetStrWidth(&u8g2, str1);
    u8g2_DrawStr(&u8g2, (128 - width1) / 2, 50, str1);

    // 3. Configurar texto de acción en negrita/grande
    u8g2_SetFont(&u8g2, u8g2_font_9x15_tf); 
    const char* str2 = "APAGANDO..."; // ~99 píxeles de ancho (entra en los 128)
    int width2 = u8g2_GetStrWidth(&u8g2, str2);
    u8g2_DrawStr(&u8g2, (128 - width2) / 2, 64, str2);

    u8g2_SendBuffer(&u8g2);
    dumpScreenToSerial(&u8g2);
}

static void draw_doctor_init() {
    u8g2_ClearBuffer(&u8g2);

    // open_iconic_all_4x
    u8g2_SetFont(&u8g2, u8g2_font_open_iconic_embedded_4x_t);
    u8g2_DrawGlyph(&u8g2, 48, 35, 72); 



    u8g2_SetFont(&u8g2, u8g2_font_9x15_tf); 
    const char* str2 = "MODO DOCTOR!"; //
    int width2 = u8g2_GetStrWidth(&u8g2, str2);
    u8g2_DrawStr(&u8g2, (128 - width2) / 2, 64, str2);

    u8g2_SendBuffer(&u8g2);
}

void draw_init(void) {
    char buf[] = "Pulse OK para comenzar";
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawXBM(&u8g2, 32, 0, 64, 64, logo);
    u8g2_SetFont(&u8g2, u8g2_font_5x8_tf);
    int text_width = u8g2_GetStrWidth(&u8g2, buf);
    int x_centered = (128 - text_width) / 2;
    
    u8g2_DrawStr(&u8g2, x_centered, 60, buf);
    
    u8g2_SendBuffer(&u8g2);
    dumpScreenToSerial(&u8g2);
}
void draw_config_lev(uint32_t duty_cycle){
    char buf[16];
    
    u8g2_ClearBuffer(&u8g2);
    
    // Title
    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "CONFIGURACION");
    u8g2_DrawHLine(&u8g2, 0, 12, 128);

    // Central Tag
    u8g2_SetFont(&u8g2, u8g2_font_helvB10_tr); 
    const char* tag_text = "Programa";
    int tag_width = u8g2_GetStrWidth(&u8g2, tag_text);
    u8g2_DrawStr(&u8g2, (128 - tag_width) / 2, 30, tag_text);

    u8g2_SetFont(&u8g2, u8g2_font_helvB24_tr);
    snprintf(buf, sizeof(buf), "%lu", duty_cycle);
    
    int text_width = u8g2_GetStrWidth(&u8g2, buf);
    int x_centered = (128 - text_width) / 2;
    
    u8g2_DrawStr(&u8g2, x_centered, 58, buf);

    u8g2_SendBuffer(&u8g2);
    dumpScreenToSerial(&u8g2);
    
}

void draw_sens_day(int day_score) {
    char buf[16];
    
    u8g2_ClearBuffer(&u8g2);
    
    
    u8g2_SetFont(&u8g2, u8g2_font_helvB10_tr); 
    const char* tag_text = "Sensaciones";
    int tag_width = u8g2_GetStrWidth(&u8g2, tag_text);
    u8g2_DrawStr(&u8g2, (128 - tag_width) / 2, 16, tag_text); 
    
    tag_text = "Hoy"; 
    tag_width = u8g2_GetStrWidth(&u8g2, tag_text);
    u8g2_DrawStr(&u8g2, (128 - tag_width) / 2, 32, tag_text);
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    //Blinking
    //if ((now / 500) % 2 == 0){
        u8g2_SetFont(&u8g2, u8g2_font_helvB24_tr);
        snprintf(buf, sizeof(buf), "%u", day_score);
        
        int text_width = u8g2_GetStrWidth(&u8g2, buf);
        int x_centered = (128 - text_width) / 2;
        
        u8g2_DrawStr(&u8g2, x_centered, 58, buf);
    //}

    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(&u8g2, 0, 64, "[-]                   [+]");
    
    u8g2_SendBuffer(&u8g2);
}

void draw_sens_treat(int sess_score) {
    char buf[16];
    
    u8g2_ClearBuffer(&u8g2);
    
    
    u8g2_SetFont(&u8g2, u8g2_font_helvB10_tr); 
    const char* tag_text = "Sensaciones";
    int tag_width = u8g2_GetStrWidth(&u8g2, tag_text);
    u8g2_DrawStr(&u8g2, (128 - tag_width) / 2, 16, tag_text); 
    
    tag_text = "Tratamiento"; 
    tag_width = u8g2_GetStrWidth(&u8g2, tag_text);
    u8g2_DrawStr(&u8g2, (128 - tag_width) / 2, 32, tag_text);
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    //Blinking
    //if ((now / 500) % 2 == 0){
        u8g2_SetFont(&u8g2, u8g2_font_helvB24_tr);
        snprintf(buf, sizeof(buf), "%u", sess_score);
        
        int text_width = u8g2_GetStrWidth(&u8g2, buf);
        int x_centered = (128 - text_width) / 2;
        
        u8g2_DrawStr(&u8g2, x_centered, 58, buf);
    //}

    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(&u8g2, 0, 64, "[-]                   [+]");
    
    u8g2_SendBuffer(&u8g2);
}
void display_tens_shutdown(void) {
    u8g2_ClearBuffer(&u8g2);
 
    u8g2_SetFont(&u8g2, u8g2_font_open_iconic_embedded_4x_t);
    u8g2_DrawGlyph(&u8g2, 48, 32, 70); 


    u8g2_SetFont(&u8g2, u8g2_font_9x15_tf); 
    const char* str1 = "TENS APAGADO"; 
    int width1 = u8g2_GetStrWidth(&u8g2, str1);
    u8g2_DrawStr(&u8g2, (128 - width1) / 2, 48, str1);

    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr); 
    const char* str2 = "Pulse OK para encender"; 
    int width2 = u8g2_GetStrWidth(&u8g2, str2);
    u8g2_DrawStr(&u8g2, (128 - width2) / 2, 62, str2);

    u8g2_SendBuffer(&u8g2);
}