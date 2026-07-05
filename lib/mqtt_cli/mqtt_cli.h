#ifndef MQTT_CLI_H
#define MQTT_CLI_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    uint8_t program_id;
    uint32_t duration;
    float avg_intensity_ch_a;
    float avg_intensity_ch_b;
    uint8_t fault_events;
    uint8_t user_sensation_day;
    uint8_t user_sensation_treatment;
} mqtt_msg_t;

/**
 * @brief Serializes the telemetry payload into a JSON string.
 * @param data Pointer to the populated telemetry structure.
 * @return char* Dynamically allocated JSON string. MUST be freed by the caller.
 */
char* generate_telemetry_json(const mqtt_msg_t *data);

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