#ifndef ADC_MONITOR_H
#define ADC_MONITOR_H

#define ADC_ATTEN_CURRENT    ADC_ATTEN_DB_0
#define ADC_ATTEN_VOL    ADC_ATTEN_DB_12
#define ADC_VOL     ADC_CHANNEL_4  //pin 15 en S3, utilizo el ADC2 porque este trabaja en oneshot y el ADC1 en continuo,

#include <stdint.h>


/**
 * @brief Current ADC
 */
void current_monitor_init(void);

/**
 * @brief Voltage ADC
 */
void voltage_monitor_init(void);

void current_monitor_calibrate_init(void);

void voltage_monitor_calibrate_init(void);

/**
 * @return Collector Voltae
 */
float get_voltage(void);

#endif