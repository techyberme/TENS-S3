#include <stdlib.h>
#include "cJSON.h"
#include "mqtt_cli.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_mac.h"
static const char *TAG = "MQTT_CLI";
static esp_mqtt_client_handle_t mqtt_client = NULL;

volatile bool s_mqtt_ready = false;
char uuid[20];

static const char isrg_root_x1_pem[] = 
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n";
char* generate_telemetry_json(const mqtt_msg_t *data) {
    // Initialize the root JSON object
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    //Populate root
    cJSON_AddNumberToObject(root, "program_id", data->program_id);
    cJSON_AddNumberToObject(root, "duration", data->duration);
    cJSON_AddNumberToObject(root, "avg_intensity_ch_a", data->avg_intensity_ch_a);
    cJSON_AddNumberToObject(root, "avg_intensity_ch_b", data->avg_intensity_ch_b);
    cJSON_AddNumberToObject(root, "fault_events", data->fault_events);
    cJSON_AddNumberToObject(root, "user_sensation_day", data->user_sensation_day);
    cJSON_AddNumberToObject(root, "user_sensation_treatment", data->user_sensation_treatment);

    //Use cJSON_PrintUnformatted(root) eliminate whitespace
    char *json_string = cJSON_PrintUnformatted(root);

    // Free memory
    cJSON_Delete(root);

    return json_string;
}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to HiveMQ");
            s_mqtt_ready = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from HiveMQ");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Message successfully published. msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Error in the MQTT/TLS layer.");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Transport error reported from tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "Transport error reported from mbedtls: 0x%x", event->error_handle->esp_tls_stack_err);
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
    //===============
    //Take the uuid
    //===============
    uint8_t mac[6];
    // Read the base MAC address burned into the eFuse
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    // Example output: TENS-348518A6C304
    snprintf(uuid, 18, "TENS-%02X%02X%02X%02X%02X%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
bool mqtt_cli_publish_telemetry(const char *json_payload) {
    if (mqtt_client == NULL || json_payload == NULL) {
        ESP_LOGE(TAG, "Null payload or client");
        return false;
    }
    char topic[40]; 

    snprintf(topic, sizeof(topic), "tens/%s/sessions", uuid);
    // Publicación con QoS 1 (Al menos una vez) para asegurar la llegada del dato médico.
    // El último '0' indica que el mensaje no es 'retained'.
    esp_mqtt_client_publish(mqtt_client, topic, json_payload, 0, 1, 0);
    return true;
}