#ifndef FLYBACK_CTRL_H
#define FLYBACK_CTRL_H

#include <stdint.h>
#include <stdbool.h>
// Configuración I2C
#define I2C_MASTER_SCL_IO    8    
#define I2C_MASTER_SDA_IO    9
#define I2C_MASTER_NUM       I2C_NUM_0  
#define I2C_MASTER_FREQ_HZ   100000 // 400kHz 
#define MCP4725_ADDR 0x60 
#define FLYBACK_EN_GPIO 21 
#define RC_FILTER_GPIO  6
#define V_MARGIN_TARGET 5.0f
#define V_MARGIN_BAND 1.0f                  
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
    ERR_IMPEDANCE_HIGH,  // Límite de DAC alcanzado sin llegar a 20mA
} system_error_t;

/**
 * @brief Converter & DAC initialization
 */
void flyback_init(void);

void set_DAC_value(uint16_t value); // 0 a 4095

void flyback_enable(bool enable);
/**
 * @brief Emergency Stop
 */
void flyback_stop(system_error_t error);

void rcfilter_init(void);
/**
 * @brief
 * RC filter DCDuty (38 to 0)
 */
void set_pwm_duty_cycle(uint32_t duty_cycle);

/**
 * @brief Coverter's voltage control
 */
void voltage_control(void);



#endif