#ifndef FLYBACK_CTRL_H
#define FLYBACK_CTRL_H

#include <stdint.h>
#include <stdbool.h>
// Configuración I2C
#define I2C_MASTER_SCL_IO    8    // Ajusta según tus pines
#define I2C_MASTER_SDA_IO    9
#define I2C_MASTER_NUM       I2C_NUM_0  //EScojo el primer puerto I2C
#define I2C_MASTER_FREQ_HZ   100000 // 400kHz para rapidez
#define MCP4725_ADDR 0x60 // Identificador bus I2C por defecto. En el módulo, ADDR está soldado a tierra -> 0x60
#define FLYBACK_EN_GPIO 21 //ENabler
#define RC_FILTER_GPIO  6
#define V_MARGIN_TARGET 5.0f
#define V_MARGIN_BAND 1.0f //Evito oscilaciones.
// En la mayoría de ESP32-S3 DevKits, el LED RGB está en el GPIO 48
#define LED_STRIP_GPIO_PIN  48
// Solo hay 1 LED en la placa
#define LED_STRIP_LED_COUNT 1
// Resolución de 10MHz para el RMT
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
 * @brief Inicialización del convertidor y del DAC
 */
void flyback_init(void);

void set_DAC_value(uint16_t value); // 0 a 4095
/**
 * @brief Enabler del convertidor
 */
void flyback_enable(bool enable);
/**
 * @brief Parado de emergencia
 */
void flyback_stop(system_error_t error);

void rcfilter_init(void);
/**
 * @brief Duty Cycle del filtro RC, en porcentaje de 1,24 (38 ticks) a 0 (0 ticks)
 */
void set_pwm_duty_cycle(uint32_t duty_cycle);

/**
 * @brief Control del voltaje del convertidor.
 */
void voltage_control(void);



#endif