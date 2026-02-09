#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "esp_attr.h"
#include "flyback_control.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/mcpwm_prelude.h"
static const char *RCTAG = "RC Filter"; 
static mcpwm_cmpr_handle_t eff_comparator = NULL;
void flyback_init(void) {
    esp_err_t err;
    // 1. Inicializar bus I2C
    ESP_LOGE("INIT", "inicializando");
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,    //La esp32-s3 es el máster
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, //Activación de resistencias internas para estabilidad
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE("INIT", "Error en i2c_param_config: %s", esp_err_to_name(err));
    }
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);  //0,0,0 es el tamaño buffers rx,tx y flags.
                                                             //Datos cortos, no hace falta buffer adicional
if (err != ESP_OK) {
        ESP_LOGE("INIT", "Error en i2c_driver_install: %s", esp_err_to_name(err));
    }
    // Configuración Pin Enable
    gpio_config_t io_conf = { 
        .pin_bit_mask = (1ULL << FLYBACK_EN_GPIO),  //Config al pin 21 
        .mode = GPIO_MODE_OUTPUT,  //output
        .pull_up_en = 1 // Por seguridad, el pin a 3.3V. Apaga el flyback
    };
    gpio_config(&io_conf);
    gpio_set_level(FLYBACK_EN_GPIO, 1); // Empezamos apagados
}

void set_DAC_value(uint16_t value) {
    if (value > 4095) value = 4095;
    // Protocolo MCP4725: [C2,C1,C0,X,X,PD1,PD0,X] + [D11...D4] + [D3...D0,X,X,X,X]
    // Para escritura rápida:
    uint8_t data[2]; //I2C envía paquetes de 
    //Primer paquete, cojo los 4 MSB y escojo modo escritura rápida
    data[0] = (value >> 8) & 0x0F; 
    //Segundo paquete, 8LSB
    data[1] = value & 0xFF;        // 8 bits LSB
    
    esp_err_t err =  i2c_master_write_to_device(I2C_MASTER_NUM, MCP4725_ADDR, data, 2, pdMS_TO_TICKS(10)); //Time out de 10 ms, por si el bus está bloqueado


    // Verificamos el resultado
    if (err != ESP_OK) {
        ESP_LOGE("DAC_I2C", "Error de escritura: %s (Direccion: 0x%02X)", esp_err_to_name(err), MCP4725_ADDR);
    }
}
void flyback_enable(bool enable) {
    gpio_set_level(FLYBACK_EN_GPIO, !enable); // Encendido del Flyback
}
void flyback_stop() {
    gpio_set_level(FLYBACK_EN_GPIO, 1); // Encendido del Flyback
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
        .group_id = 0,
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


  voltage_control(void){
    //Aquí se implementaría el control de voltaje, leyendo el valor del ADC y ajustando el duty cycle en consecuencia.
    //Por ejemplo, podríamos usar un PID para mantener el voltaje deseado.
  }