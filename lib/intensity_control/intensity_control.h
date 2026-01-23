#ifndef INTENSITY_CONTROL_H
#define INTENSITY_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/queue.h"

#define CURRENT_UP_GPIO  10
#define CURRENT_DOWN_GPIO 11
#define DAC_STEP_USER   50  // Cuánto cambia el DAC por cada pulsación
#define DAC_START_VAL    1376  // 1.244V inicial
#define TARGET_CURRENT   20    // Corriente mínima objetivo
#define DAC_STEP         20    // 0,1 mA/salto
#define SESSION_DURATION         20    //duración del programa en minutos
#define SESSION_TICKS         (pdMS_TO_TICKS(SESSION_DURATION *60*1000))    //duración del programa en ticks
/* --- Definiciones de Estados del Sistema --- */
typedef enum {
    STATE_INIT,          // Inicialización de hardware
    STATE_BASE,  // Bajada automática del DAC hasta alcanzar 20mA
    STATE_FUNC,  // El usuario tiene el control mediante botones
    STATE_STANDBY,          // Parada por desconexión de electrodos
    STATE_DONE,
    STATE_ERROR          // Parada de emergencia por fallo de lectura o hardware
} system_state_t;

/* --- Interfaz Pública --- */

/**
 * @brief Tarea principal de control de la FSM.
 * Se encarga de la búsqueda de los 20mA y la gestión posterior.
 */
void flyback_control_task(void *pvParameters);

/**
 * @brief Retorna el estado actual del sistema.
 * Útil para que otras tareas (UI/Buttons) sepan si pueden actuar.
 */
system_state_t get_system_state(void);

/**
 * @brief Permite actualizar la consigna del DAC externamente.
 * Solo debería ser efectiva si el estado es STATE_USER_CONTROL.
 */
void update_user_current(int16_t delta);


/**
 * @brief Inicialización de botones de control
 * Solo debería ser efectiva si el estado es STATE_USER_CONTROL.
 */
void buttons_init(void);
#endif // CONTROL_TASK_H