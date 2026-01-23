#ifndef CURRENT_MONITOR_H
#define CURRENT_MONITOR_H

#include <stdint.h>


/**
 * @brief Inicialización del periférico ADC
 */
void current_monitor_init(void);
/**
 * @brief Calibración
 */
void current_monitor_calibrate_init(void);

#endif