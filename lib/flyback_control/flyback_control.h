#ifndef FLYBACK_CTRL_H
#define FLYBACK_CTRL_H

#include <stdint.h>
#include <stdbool.h>

#define MCP4725_ADDR 0x60 // Identificador bus I2C por defecto. En el módulo, ADDR está soldado a tierra -> 0x60
#define FLYBACK_EN_GPIO 21 //ENabler
/**
 * @brief Inicialización del convertidor y del DAC
 */
void flyback_init(void);
/**
 * @brief Establece el valor de la DAC
 */
void set_DAC_value(uint16_t value); // 0 a 4095
/**
 * @brief Enabler del convertidor
 */
void flyback_enable(bool enable);
/**
 * @brief Parado de emergencia
 */
void flyback_stop(void);

#endif