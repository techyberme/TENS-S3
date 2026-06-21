#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "esp_attr.h"
#include "boost_control.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "adc_monitor.h"
#include "hbridge_driver.h"
#include "tensOS.h"
#include "led_strip.h"
extern TensChannel_t ch_A;
extern TensChannel_t ch_B;
static const char *RCTAG = "RC Filter"; 
static uint32_t current_duty= 38;
static uint32_t saved_duty= 38;
static mcpwm_cmpr_handle_t eff_comparator = NULL;
static led_strip_handle_t led_strip;
static void boost_start_up(void *pvParameters);  
led_strip_handle_t configure_led(void)
{
    // 1. Configuración general del LED
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,
        .max_leds = LED_STRIP_LED_COUNT,
        .led_model = LED_MODEL_WS2812, // Modelo estándar en S3
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }
    };

    // 2. Configuración del backend RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .mem_block_symbols = 0, // Auto
        .flags = {
            .with_dma = false, // No necesario para 1 solo LED
        }
    };

    led_strip_handle_t led_handle;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_handle));
    
    return led_handle;
}
void boost_init(void) {
    esp_err_t err;
    // 1. Inicializar bus I2C
    ESP_LOGE("INIT", "inicializando");
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,    
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, //Internal resistances
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE("INIT", "Error en i2c_param_config: %s", esp_err_to_name(err));
    }
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);  //short messages
                                                                   //no need for rx,tx and flags buffers.
if (err != ESP_OK) {
        ESP_LOGE("INIT", "Error en i2c_driver_install: %s", esp_err_to_name(err));
    }
    // Enabler pin config.
    gpio_config_t io_conf = { 
        .pin_bit_mask = (1ULL << BOOST_EN_GPIO),  
        .mode = GPIO_MODE_OUTPUT,  
        .pull_up_en = 1 // pin to 3,3 for safety measures
    };
    gpio_config(&io_conf);
    gpio_set_level(BOOST_EN_GPIO, 1); // Start off

    led_strip = configure_led();
    if (led_strip){
        ESP_LOGI("INIT", "LED configurado correctamente");
        led_strip_clear(led_strip); // led starts off
    }
    else{
        ESP_LOGE("INIT", "Error al configurar el LED");
    }
}

void set_DAC_value(uint16_t level, char channel) {
    uint16_t value = level * 60; //0-20 level to 0 - 40 mA., used to be 60;
    if (value > 1250) value = 1250;  //1250
    uint8_t data[2]; 
    //First package, 4 MSB of value and Fast Mode
    data[0] = (value >> 8) & 0x0F; 

    //Second package, 8 LSB of value 
    data[1] = value & 0xFF;   
    //10 ms timeout, in case the bus is blocked
    if (channel == 'A') {
        esp_err_t  err = i2c_master_write_to_device(I2C_MASTER_NUM, MCP4725_ADDR_A, data, 2, pdMS_TO_TICKS(10));
        if (err != ESP_OK) {
        ESP_LOGE("DAC_CONTROL", "Fallo I2C escribiendo al canal %c. Código: %s", channel, esp_err_to_name(err));
    } 
        
    }
    else if (channel == 'B') {
        esp_err_t   err = i2c_master_write_to_device(I2C_MASTER_NUM, MCP4725_ADDR_B, data, 2, pdMS_TO_TICKS(10)); 
        if (err != ESP_OK) {
        ESP_LOGE("DAC_CONTROL", "Fallo I2C escribiendo al canal %c. Código: %s", channel, esp_err_to_name(err));
    }
    }
   

    
}
void boost_enable(bool enable) {
    gpio_set_level(BOOST_EN_GPIO, !enable); //negative logic
    if (enable){
        set_pwm_duty_cycle(38);  
        xTaskCreate(boost_start_up, "boost_ramp", 2048, NULL, 5, NULL);
    }
    else {
        // Seguridad crítica: Al apagar el EN, machacamos el PWM a 0
        set_pwm_duty_cycle(0);
    }
}

void boost_stop(system_state_t error) {
    gpio_set_level(BOOST_EN_GPIO, 1); // Flyback off
    set_DAC_value(0, 'A'); // DACS off
    set_DAC_value(0, 'B'); 
    hbridge_stop('A'); // Parada de emergencia del puente H, lo hago después del boost para evitar picos de corriente al cortar el puente H antes que el boost
    hbridge_stop('B');
    update_led(error);

} 

void update_led(system_state_t state){
    switch (state) {
        case ERROR:
            led_strip_set_pixel(led_strip, 0, 255, 0, 0); // RED
            ESP_LOGE("SAFETY", "STOP: ERROR GENERAL");
            break;
        case WARN_OPEN_CIRCUIT:
            led_strip_set_pixel(led_strip, 0, 212, 99, 28); // Naranja 
            ESP_LOGE("SAFETY", "STOP: Impedancia elevada");
            break;
        case ERR_OVERVOLTAGE:
            led_strip_set_pixel(led_strip, 0, 255, 0, 255); // Magenta (Peligro Voltaje)
            ESP_LOGE("SAFETY", "STOP: Voltaje de colector al límite");
            break;
        case ERR_OPEN_CIRCUIT:
            led_strip_set_pixel(led_strip, 0, 212, 212, 28); // Amarill(Circuito abierto)
            ESP_LOGW("SAFETY", "STOP: Electrodos Desconectados");
            break;
        case WARN_DONE:
            led_strip_set_pixel(led_strip, 0, 124, 252, 0); // Amarill(Circuito abierto)
            ESP_LOGW("SAFETY", "STOP: Electrodos Desconectados");
            break;
        default:
            led_strip_clear(led_strip);
            break;
    }
    led_strip_refresh(led_strip);
}

void rcfilter_init(void){
    // TIMER Definition
    ESP_LOGI(RCTAG, "Timer for RC filter");
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t eff_timer_config = {
        .group_id = 1,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT, // PLL Clock 160 MHz
        .resolution_hz = 10000000, //10 MHz Prescaler, 0.1us/ticks
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP, //just upwards
        .period_ticks = 100, // 100 ticks for 1 cycle, 10 MHz/ 100= 100 KHz Timer
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&eff_timer_config, &timer));

    // ----- OPERATOR, Definition of channel ----- 
    ESP_LOGI(RCTAG, "Create operators");
    mcpwm_oper_handle_t operators = NULL;
    mcpwm_operator_config_t eff_operator_config = {
        .group_id = 1,
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&eff_operator_config, &operators));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operators, timer));


    // -----Comparator-----//
    ESP_LOGI(RCTAG, "Create comparators");
    mcpwm_comparator_config_t eff_compare_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(operators, &eff_compare_config, &eff_comparator));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(eff_comparator, 50));  //50% Duty ratio

    // -----Generator-----//
    ESP_LOGI(RCTAG, "Create generators");
    mcpwm_gen_handle_t eff_generator = NULL;
    mcpwm_generator_config_t gen_config = {};
    const int gen_gpios = RC_FILTER_GPIO; 
    gen_config.gen_gpio_num = gen_gpios;
    ESP_ERROR_CHECK(mcpwm_new_generator(operators, &gen_config, &eff_generator));
    // ====== Generator Action  ====== //
    ESP_LOGI(RCTAG, "Set generator action on timer and compare event");
    // PWM Start with HIGH State when Timer is 0 and LOW State when Comparators value is equal Timer, For MCPWM_TIMER_COUNT_MODE_UP
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(eff_generator,MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(eff_generator,MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, eff_comparator, MCPWM_GEN_ACTION_LOW)));
    ESP_LOGI(RCTAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

}


void set_pwm_duty_cycle(uint32_t duty_cycle){
    // if (duty_cycle > 38){   //límite a 1,24
    //     duty_cycle= 38;
    // }
    // esp_err_t ret=mcpwm_comparator_set_compare_value(eff_comparator, duty_cycle);
    // if (ret != ESP_OK) {
    //     ESP_LOGE("PWM", "Error al ajustar el Duty Cycle: %s", ret);
    // }
  }  


void update_voltage(void){
    float  margin = (ch_A.voltage < ch_B.voltage) ? ch_A.voltage : ch_B.voltage;
    //
    if (margin > (V_MARGIN_TARGET + V_MARGIN_BAND)){
        if (current_duty < 38) current_duty +=1;   //1 volt change
    }
    else if (margin > (V_MARGIN_TARGET - V_MARGIN_BAND)){
        if (current_duty>1) current_duty -=1; 
    }
    set_pwm_duty_cycle(current_duty);
  }

  // temp task to handle boost start up
static void boost_start_up(void *pvParameters) {
    float current_duty = 38.0f;
    float target_duty = (float)saved_duty;
    
    // ramp, 10 steps, 5 ms each
    const int steps = 10; 
    float step_size = (current_duty - target_duty) / steps;

    for (int i = 0; i < steps; i++) {
        current_duty -= step_size;
        set_pwm_duty_cycle((uint32_t)current_duty);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    //in case there's a rounding error
    set_pwm_duty_cycle(saved_duty);
    
    // Deletion of task
    vTaskDelete(NULL); 
}
esp_err_t mcp4725_init_safe_start(void) {
    // Arrays para iterar sobre ambos DACs sin duplicar código
    const uint8_t addresses[2] = {MCP4725_ADDR_A, MCP4725_ADDR_B};
    const char channels[2] = {'A', 'B'};
    ESP_LOGI("SAFETY", "Iniciando secuencia de verificación Zero-Start para DACs...");

    for (int i = 0; i < 2; i++) {
        uint8_t data_rx[5] = {0};
        
        // Read current value
        esp_err_t err = i2c_master_read_from_device(I2C_MASTER_NUM, addresses[i], data_rx, 5, pdMS_TO_TICKS(10));
        if (err != ESP_OK) {
            ESP_LOGE("SAFETY", "Fallo I2C en DAC %c durante inicio: %s", channels[i], esp_err_to_name(err));
            return err; // Aborto inmediato, hardware no responde
        }

        uint16_t current_eeprom = ((data_rx[3] & 0x0F) << 8) | data_rx[4];

        // Correction
        if (current_eeprom != 0) {
            ESP_LOGW("SAFETY", "DAC %c tiene EEPROM = %d. Forzando hardware a 0V...", channels[i], current_eeprom);
            
            uint8_t data_tx[3] = {0x60, 0x00, 0x00}; // Comando Write DAC + EEPROM a 0
            err = i2c_master_write_to_device(I2C_MASTER_NUM, addresses[i], data_tx, 3, pdMS_TO_TICKS(50));
            
            if (err == ESP_OK) {
                ESP_LOGI("SAFETY", "EEPROM DAC %c reescrita. Bloqueando 50ms para guardado físico.", channels[i]);
                vTaskDelay(pdMS_TO_TICKS(50)); 
            } else {
                ESP_LOGE("SAFETY", "Error crítico al reescribir EEPROM del DAC %c", channels[i]);
                return err; // Aborto, la memoria está corrupta o el bus falló al escribir
            }
        } else {
            ESP_LOGI("SAFETY", "DAC %c verificado: Estado seguro (0V).", channels[i]);
        }
    }
    
    return ESP_OK; // Ambos DACs respondieron y están a 0V garantizado
}