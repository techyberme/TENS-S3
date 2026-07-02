#ifndef MQTT_CLI_H
#define MQTT_CLI_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    char device_uuid[37];
    uint32_t session_id;
    uint8_t n_program;
    uint32_t duration_s;
    float avg_level_A;
    float avg_level_B;
    float z_avg_ohm;
    uint8_t fault_events;
} telemetry_payload_t;

/**
 * @brief Serializes the telemetry payload into a JSON string.
 * @param data Pointer to the populated telemetry structure.
 * @return char* Dynamically allocated JSON string. MUST be freed by the caller.
 */
char* generate_telemetry_json(const telemetry_payload_t *data);

/**
 * @brief Inicializa el cliente MQTT, configura los certificados TLS y arranca la tarea de red.
 */
void mqtt_cli_init(void);

/**
 * @brief Publica un payload JSON en el broker.
 * @param json_payload Cadena de caracteres con el JSON ya formateado.
 * @return true si el mensaje se ha encolado correctamente, false en caso contrario.
 */
bool mqtt_cli_publish_telemetry(const char *json_payload);

#endif