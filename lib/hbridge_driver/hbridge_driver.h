#ifndef HBRIDGE_DRIVER_H
#define HBRIDGE_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

// Configuración de pines (Cámbialos según tu PCB)
#define HBRIDGE_GPIO_A  4
#define HBRIDGE_GPIO_B  5


// Inicializa el hardware (Deadtime en ticks de 0.1us)
void hbridge_init(uint32_t deadtime_ticks);

// Apaga el puente inmediatamente
void hbridge_stop(void);

#endif