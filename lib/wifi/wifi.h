#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

/**
 * @brief Inicializa la partición NVS, la pila TCP/IP y conecta al AP configurado.
 * Es una función no bloqueante; el estado de conexión se gestiona por eventos.
 */
void wifi_init(void);

#endif 