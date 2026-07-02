#include <stdlib.h>
#include "cJSON.h"
#include "mqtt_cli.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_CLI";
static esp_mqtt_client_handle_t mqtt_client = NULL;

static const char isrg_root_x1_pem[] = 
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n";
char* generate_telemetry_json(const telemetry_payload_t *data) {
    if (data == NULL) {
        return NULL;
    }

    // 1. Initialize the root JSON object
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    // 2. Add the UUID directly to the root
    cJSON_AddStringToObject(root, "device_uuid", data->device_uuid);

    // 3. Initialize the nested "session_metrics" object
    cJSON *metrics = cJSON_CreateObject();
    if (metrics == NULL) {
        cJSON_Delete(root); // Cleanup root if nested allocation fails
        return NULL;
    }

    // 4. Populate the nested metrics
    cJSON_AddNumberToObject(metrics, "session_id", data->session_id);
    cJSON_AddNumberToObject(metrics, "program", data->n_program);
    cJSON_AddNumberToObject(metrics, "duration_s", data->duration_s);
    
    // Using cJSON_AddNumberToObject implicitly handles floats
    cJSON_AddNumberToObject(metrics, "avg_intensity_A", data->avg_level_A);
    cJSON_AddNumberToObject(metrics, "avg_intensity_B", data->avg_level_B);
    cJSON_AddNumberToObject(metrics, "z_avg_ohm", data->z_avg_ohm);
    cJSON_AddNumberToObject(metrics, "fault_events", data->fault_events);

    // 5. Attach the metrics object to the root object
    cJSON_AddItemToObject(root, "session_metrics", metrics);

    //Use cJSON_PrintUnformatted(root) in production to eliminate whitespace and save MQTT payload size.
    char *json_string = cJSON_PrintUnformatted(root);

    // 7. Free the internal cJSON structures from the FreeRTOS heap
    cJSON_Delete(root);

    return json_string;
}


/**
 * @brief Manejador de eventos asíncronos del cliente MQTT.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Túnel mTLS establecido. Conectado a HiveMQ Cloud.");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado del broker MQTT.");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Mensaje publicado exitosamente. msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Error en la capa MQTT/TLS.");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Error de transporte reportado desde tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "Error de transporte reportado desde mbedtls: 0x%x", event->error_handle->esp_tls_stack_err);
            }
            break;
        default:
            break;
    }
}

void mqtt_cli_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address = {
            .uri = "mqtts://24cb7e655c374c60951752e03a5602d7.s1.eu.hivemq.cloud",
            .port = 8883,
        },
        .broker.verification = {
            .certificate = isrg_root_x1_pem,
        },
        .credentials = {
            .username = "hf-tens",
            .authentication.password = "password"
        },
        .network = {
            .reconnect_timeout_ms = 10000,
            .timeout_ms = 10000,
        }
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Fallo al inicializar el cliente MQTT.");
        return;
    }
    
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

bool mqtt_cli_publish_telemetry(const char *json_payload) {
    if (mqtt_client == NULL || json_payload == NULL) {
        ESP_LOGE(TAG, "Cliente no inicializado o payload nulo.");
        return false;
    }

    // Publicación con QoS 1 (Al menos una vez) para asegurar la llegada del dato médico.
    // El último '0' indica que el mensaje no es 'retained'.
    int msg_id = esp_mqtt_client_publish(mqtt_client, "hospital/neurology/tens/v1/telemetry", json_payload, 0, 1, 0);
    
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Fallo al encolar el mensaje MQTT.");
        return false;
    }
    
    ESP_LOGD(TAG, "Mensaje encolado con ID: %d", msg_id);
    return true;
}