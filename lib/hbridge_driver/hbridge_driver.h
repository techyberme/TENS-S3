#ifndef HBRIDGE_DRIVER_H
#define HBRIDGE_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

// Configuración de pines para el puente en H
#define HBRIDGE_GPIO_A  4
#define HBRIDGE_GPIO_B  5

typedef enum {
    TENS_MODE_CONTINUO, // 4 kHz constante
    TENS_MODE_BURST,      // 4 kHz modulado a 100 Hz
    TENS_MODE_OFF         // Salida desactivada
} tens_mode_t;


// Inicializa el hardware (Deadtime en ticks de 0.1us)
/**
 * @brief Inicialización del Puente en H
 */
void hbridge_init(uint32_t deadtime_ticks);

/**
 * @brief Apagado del Puente en H
 */
void hbridge_stop(void);


#endif