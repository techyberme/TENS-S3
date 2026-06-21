#ifndef BOOST_CTRL_H
#define BOOST_CTRL_H

#include <stdint.h>
#include <stdbool.h>
// Configuración I2C
#define I2C_MASTER_SCL_IO   41    
#define I2C_MASTER_SDA_IO   40
 
#define I2C_MASTER_NUM       I2C_NUM_0  
#define I2C_MASTER_FREQ_HZ   100000 // 100kHz 
#define MCP4725_ADDR_A 0x60
#define MCP4725_ADDR_B 0x61  
#define BOOST_EN_GPIO 38 
#define RC_FILTER_GPIO  8
#define V_MARGIN_TARGET 2.0f
#define V_MARGIN_BAND 0.5f                  
#define LED_STRIP_GPIO_PIN  48
#define LED_STRIP_LED_COUNT 1
//10MHz for the RMT
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

typedef enum {
    ERR_NONE = 0,
    ERR_OVERCURRENT,    // > 80mA 
    ERR_OVERVOLTAGE,    // > V 40en colector (Hardware)
    ERR_OPEN_CIRCUIT,   // Electrodos sueltos
    ERR_EFFICIENCY_LOW,  // Saturación del sistema
    WARN_DONE,
    WARN_OPEN_CIRCUIT, 
    ERR_CHARGE, 
} system_state_t;

/**
 * @brief Converter, DAC & LED initialization
 */
void boost_init(void);

void set_DAC_value(uint16_t value, char channel); // 0 a 4095

void boost_enable(bool enable);
/**
 * @brief Emergency Stop
 */
void boost_stop(system_state_t error);

void rcfilter_init(void);
/**
 * @brief
 * RC filter DCDuty (38 to 0)
 */
void set_pwm_duty_cycle(uint32_t duty_cycle);

/**
 * @brief Converter's voltage control
 */
void update_voltage(void);

void update_led(system_state_t);
esp_err_t mcp4725_init_safe_start(void);
#endif