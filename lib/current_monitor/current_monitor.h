#ifndef CURRENT_MONITOR_H
#define CURRENT_MONITOR_H

#define ADC_ATTEN    ADC_ATTEN_DB_12
#define ADC_VOL     ADC_CHANNEL_4  //pin 15 en S3, utilizo el ADC2 porque este trabaja en oneshot y el ADC1 en continuo,

#include <stdint.h>


/**
 * @brief Inicialización del periférico ADC
 */
void current_monitor_init(void);

/**
 * @brief Inicialización del periférico ADC para lectura de voltaje
 */
void voltage_monitor_init(void);
/**
 * @brief Calibración
 */
void current_monitor_calibrate_init(void);

void voltage_monitor_calibrate_init(void);

/**
 * @brief Lectura del voltaje en el Espejo de corriente.
 * @return Voltaje en mV
 */
float get_voltage(void);

#endif