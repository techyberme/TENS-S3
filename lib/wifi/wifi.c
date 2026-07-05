#include "wifi.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include "lwip/sockets.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
// Variables globales estáticas para almacenar las credenciales en RAM
static char nvs_ssid[32] = {0};
static char nvs_pass[64] = {0};
static const char* TAG = "WIFI";
bool connected = false;
#define MAX_RETRYS     5
static int retry_num = 0;
volatile bool s_wifi_ready = false;

//RAW html for the captive page.
static const char* html_page = 
    "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>TENS Config</title><style>"
    "body{font-family:Arial;display:flex;justify-content:center;align-items:center;height:100vh;background:#f3f4f6;margin:0;}"
    ".card{background:#fff;padding:2rem;border-radius:8px;box-shadow:0 4px 6px rgba(0,0,0,0.1);width:100%;max-width:320px;}"
    "input{width:100%;padding:10px;margin:10px 0;border:1px solid #ccc;border-radius:4px;box-sizing:border-box;}"
    "button{width:100%;padding:10px;background:#2563eb;color:#fff;border:none;border-radius:4px;cursor:pointer;}"
    "</style></head><body>"
    "<div class=\"card\"><h2>TENS Setup</h2>"
    "<form action=\"/connect\" method=\"POST\">"
    "<input type=\"text\" name=\"ssid\" placeholder=\"Nombre del Wi-Fi\" required>"
    "<input type=\"password\" name=\"pass\" placeholder=\"Contraseña\" required>"
    "<button type=\"submit\">Conectar Dispositivo</button>"
    "</form></div></body></html>";

// Get handler, sends the UI
static esp_err_t get_handler(httpd_req_t *req) {
    //get request
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Post handler function, saves the wifi credentials in flash
static esp_err_t post_handler(httpd_req_t *req) {
    char buf[128];
    //return value
    int ret;
    int remaining = req->content_len;

    // Makesure payload isn't too big
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Payload too big");
        return ESP_FAIL;
    }

    if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) httpd_resp_send_408(req);
        return ESP_FAIL;
    }
    // set end of buffer
    buf[ret] = '\0';

    char ssid[32] = {0};
    char pass[64] = {0};
    
    if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) == ESP_OK &&
        httpd_query_key_value(buf, "pass", pass, sizeof(pass)) == ESP_OK) {
        
        ESP_LOGI("HTTP_SRV", "Received credentials. SSID: %s", ssid);

        //Save credentials in the NVS
        nvs_handle_t nvs_handle;
        if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
            nvs_set_str(nvs_handle, "ssid", ssid);
            nvs_set_str(nvs_handle, "pass", pass);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }

        httpd_resp_send(req, "Guardado. El estimulador se reiniciara ahora.", HTTPD_RESP_USE_STRLEN);
        
        // Short delay to make sure the response reaches the phone before restarting.
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Formato invalido");
    }
    return ESP_OK;
}

// HTTP server, 
static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Any unknown petition (android or ios) will be redirected
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&server, &config) == ESP_OK) {
        //any * get petition will use get_handler
        httpd_uri_t uri_get = { .uri = "/*", .method = HTTP_GET, .handler = get_handler};
         //connect post will use post_handler
        httpd_uri_t uri_post = { .uri = "/connect", .method = HTTP_POST, .handler = post_handler};
         // Registrar POST before get so that /* 
         //does not take over /connect
        httpd_register_uri_handler(server, &uri_post);
        httpd_register_uri_handler(server, &uri_get);
    }
    return server;
}
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Iniciando conexión Wi-Fi...");
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < MAX_RETRYS) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGW(TAG, "Connection Lost. Retrying... (%d/%d)", retry_num, MAX_RETRYS);
        } else {
            ESP_LOGE(TAG, "Wrong credentials or unknown network, restartint...");
            // NVS purge
            nvs_handle_t nvs_handle;
            if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
                nvs_erase_key(nvs_handle, "ssid");
                nvs_erase_key(nvs_handle, "pass");
                nvs_commit(nvs_handle);
                nvs_close(nvs_handle);
            }
            esp_restart(); // Restart esp32
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Successfully connected. Assigned IP: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;  
        s_wifi_ready = true;
    }
}
//take any DNS request and redirect to my ip
static void captive_dns_task(void *pvParameters) {
    //UDP configuration
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(53);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE("DNS_SRV", "Error creating socket");
        vTaskDelete(NULL);
    }

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE("DNS_SRV", "Error linking prot 53");
        close(sock);
        vTaskDelete(NULL);
    }

    uint8_t rx_buffer[128];
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&client_addr, &client_len);
        
        if (len > 0) {
            // Assume default IP for the AP: 192.168.4.1 (0xC0A80401)
            rx_buffer[2] |= 0x80; // Set Response flag
            rx_buffer[3] |= 0x80; // Set Recursion Available flag
            rx_buffer[7] = 1;     // Answer count = 1
            
            // Inyectar puntero a la pregunta original, tipo A, clase IN, TTL, longitud (4 bytes), y la IP local
            uint8_t dns_answer[] = {
                0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x04, 
                192, 168, 4, 1 // Default ip for esp32 as AP
            };
            
            if (len + sizeof(dns_answer) <= sizeof(rx_buffer)) {
                memcpy(rx_buffer + len, dns_answer, sizeof(dns_answer));
                sendto(sock, rx_buffer, len + sizeof(dns_answer), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Ceder tiempo de CPU al Scheduler
    }
}
void wifi_init(void) {
    // NVS partition
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Take credentials from NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    bool credentials_found = false;

    if (err == ESP_OK) {
        size_t ssid_len = sizeof(nvs_ssid);
        size_t pass_len = sizeof(nvs_pass);
        
        esp_err_t err_ssid = nvs_get_str(nvs_handle, "ssid", nvs_ssid, &ssid_len);
        esp_err_t err_pass = nvs_get_str(nvs_handle, "pass", nvs_pass, &pass_len);
        
        if (err_ssid == ESP_OK && err_pass == ESP_OK) {
            credentials_found = true;
            ESP_LOGI(TAG, "Credenciales encontradas en NVS. SSID: %s", nvs_ssid);
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGW(TAG, "Partición NVS no formateada o vacía.");
    }

    // 3. Inicialize tcp/ip
    ESP_ERROR_CHECK(esp_netif_init());
    //Event loop library, recommended for wifi
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 4. Lógica de transición de estado
    if (credentials_found) {
        // station mode
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        //wifi status changes
        esp_event_handler_instance_t instance_any_id;
        //wifi ip
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

        wifi_config_t wifi_config = {0};
        strncpy((char *)wifi_config.sta.ssid, nvs_ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char *)wifi_config.sta.password, nvs_pass, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        connected = true;
        
    } else {
        // AP mode (access point)
        ESP_LOGW(TAG, "No credentials, initicialising AP");
        esp_netif_create_default_wifi_ap();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        wifi_config_t wifi_config = {
            .ap = {
                .ssid = "TENS_WiFi",
                .ssid_len = strlen("TENS_WiFi"),
                .channel = 1,
                .password = "", // no password, easy access up
                .max_connection = 4,
                .authmode = WIFI_AUTH_OPEN
            },
        };

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        
        ESP_ERROR_CHECK(esp_wifi_start());
        
        // Launch web server
        start_webserver();
        
        // Captive dns task
        xTaskCreate(captive_dns_task, "captive_dns_task", 2048, NULL, 5, NULL);
    }
}
