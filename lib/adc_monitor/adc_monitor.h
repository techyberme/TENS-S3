#ifndef ADC_MONITOR_H
#define ADC_MONITOR_H

#define ADC_ATTEN_CURRENT    ADC_ATTEN_DB_6 //Change to 6
#define ADC_ATTEN_VOL    ADC_ATTEN_DB_6

#define ADC_VOL_A     ADC_CHANNEL_4  //pin 15 en S3, utilizo el ADC2 porque este trabaja en oneshot y el ADC1 en continuo,
#define ADC_VOL_B     ADC_CHANNEL_2 //pin 13
#define ADC_BAT        ADC_CHANNEL_5 
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
 * @return Drain voltage
 */
float get_voltage(char channel);

/**
 * @return Returns battery percentage
 */
uint8_t get_battery(void);

#endif