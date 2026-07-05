#ifndef ADC_MON_ONESHOT_H
#define ADC_MON_ONESHOT_H

#define ADC_ATTEN_CURRENT    ADC_ATTEN_DB_6 //Change to 6
#define ADC_ATTEN_VOL    ADC_ATTEN_DB_6

#define CHARGE_PIN 2
#define ADC_CURR_A    ADC_CHANNEL_1 //ADC_CHANNEL_1 //ADC2_CH1
#define ADC_CURR_B    ADC_CHANNEL_3 //ADC_CHANNEL_3     //ADC2_CH3
#define ADC_VOL_A    ADC_CHANNEL_2 //ADC_CHANNEL_4  //ADC2_CH2
#define ADC_VOL_B    ADC_CHANNEL_4  //ADC_CHANNEL_1 //ADC_CHANNEL_2 //ADC2_CH4
#define ADC_BAT       ADC_CHANNEL_5  //ADC_CHANNEL_5 //ADC_CH5
#include <stdint.h>

void adc_monitor_init(void);
void charging_monitor_init(void);
void adc_calibrate_init(void);
void adc_stop(void);
void process_current(uint32_t raw_val, char channel);

void process_voltage(uint32_t raw_val, char channel);
 
/**
 * @return Returns battery percentage
 */
float calc_percentage(int volt);

void charge_task(void *pvParameters);

#endif