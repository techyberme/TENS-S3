#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "esp_attr.h"
#include "flyback_control.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "adc_monitor.h"
#include "hbridge_driver.h"
#include "led_strip.h"

static const char *RCTAG = "RC Filter"; 
static mcpwm_cmpr_handle_t eff_comparator = NULL;
static led_strip_handle_t led_strip;
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
void flyback_init(void) {
    esp_err_t err;
    // 1. Inicializar bus I2C
    ESP_LOGE("INIT", "inicializando");
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,    
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, //Internal resistences
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
        .pin_bit_mask = (1ULL << FLYBACK_EN_GPIO),  
        .mode = GPIO_MODE_OUTPUT,  
        .pull_up_en = 1 // Por seguridad, el pin a 3.3V. Apaga el flyback
    };
    gpio_config(&io_conf);
    gpio_set_level(FLYBACK_EN_GPIO, 1); // Empezamos apagados

    led_strip = configure_led();
    if (led_strip){
        ESP_LOGI("INIT", "LED configurado correctamente");
        led_strip_clear(led_strip); // Aseguramos que el LED empieza apagado
    }
    else{
        ESP_LOGE("INIT", "Error al configurar el LED");
    }
}

void set_DAC_value(uint16_t level, char channel) {
    uint16_t value = level * 62; //0-20 level to 0 - 40 mA.;
    if (value > 1250) value = 1250;
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
void flyback_enable(bool enable) {
    gpio_set_level(FLYBACK_EN_GPIO, !enable); // Encendido del Flyback
    //TODO controlled converter turn on
}

void flyback_stop(system_error_t error) {
    gpio_set_level(FLYBACK_EN_GPIO, 1); // Flyback off
    set_DAC_value(0, 'A'); // DACS off
    set_DAC_value(0, 'B'); 
    hbridge_stop('A'); // Parada de emergencia del puente H, lo hago después del flyback para evitar picos de corriente al cortar el puente H antes que el flyback
    hbridge_stop('B');
    switch (error) {
        case ERR_IMPEDANCE_HIGH:
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
    if (duty_cycle > 38){   //límite a 1,24
        duty_cycle= 38;
    }
    esp_err_t ret=mcpwm_comparator_set_compare_value(eff_comparator, duty_cycle);
    if (ret != ESP_OK) {
        ESP_LOGE("PWM", "Error al ajustar el Duty Cycle: %s", ret);
    }
  }  


void update_voltage(void){
    static uint32_t current_duty= 38;
    float volt_A = get_voltage('A');
    float volt_B = get_voltage('B');
    //Take voltage with less margin
    float volt = (volt_A < volt_B) ? volt_A : volt_B;
    //
    if (volt> (V_MARGIN_TARGET + V_MARGIN_BAND)){
        if (current_duty < 38) current_duty +=1;   //1 volt change
    }
    else if (volt > (V_MARGIN_TARGET - V_MARGIN_BAND)){
        if (current_duty>1) current_duty -=1; 
    }
    set_pwm_duty_cycle(current_duty);
  }


