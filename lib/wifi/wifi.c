#include "wifi.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"


#define WIFI_SSID      "WifiJavi"
#define WIFI_PASS      "hola1233"
#define MAX_RETRYS     5

static const char *TAG = "WIFI_CLI";
static int retry_num = 0;

/**
 * @brief Manejador de eventos para transiciones de estado de Wi-Fi e IP.
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Iniciando conexión Wi-Fi...");
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < MAX_RETRYS) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGW(TAG, "Conexión perdida. Reintentando... (%d/%d)", retry_num, MAX_RETRYS);
        } else {
            ESP_LOGE(TAG, "Fallo al conectar al AP tras múltiples intentos.");
        }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Conectado con éxito. IP asignada: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0; // Reiniciar contador de reintentos al conectar
    }
}

void wifi_init(void) {
    // 1. Inicializar el NVS (Requisito estricto del hardware Wi-Fi PHY)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Inicializar la pila TCP/IP base (LwIP)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. Configurar el hardware Wi-Fi con los parámetros por defecto de Espressif
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4. Registrar los manejadores de eventos (Wi-Fi e IP)
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    // 5. Configurar credenciales y arrancar el driver en modo Estación
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // Forzar estándar de seguridad mínimo
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}