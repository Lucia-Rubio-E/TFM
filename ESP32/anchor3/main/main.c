#include <stdio.h>
#include <string.h>
#include <time.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_sntp.h"
#include "mqtt_client.h"
#include "esp_wifi_types.h"
#define TAG "gtec-ftm-anchor3"


#define WIFI_SSID "Lucía"
#define WIFI_PASS "passwordlucia"
#define MQTT_URI         "mqtt://172.20.10.13:1884"
#define MQTT_TOPIC       "data"
#define MQTT_INTERVAL_MS 60000  // intervalo de envío (60 segundos)

// configuración FTM
#define CURRENT_BW       WIFI_BW_HT20  // ancho de banda (20MHz)
#define CURRENT_CHANNEL  6             // canal wifi
#define WIFI_RETRY_MAX   -1            // nº max de intentos de reconexión wifi ( -1 => infinitos)

// configuración nodo ancla 1
#define ANCHOR_ID        "3"
#define POSITION_X       0.0f
#define POSITION_Y       10.0f
#define POSITION_Z       0.0f

// bits para los eventos WiFi y FTM
typedef enum {
    WIFI_AP_START_BIT = BIT0,
    WIFI_STA_CONNECTED_BIT = BIT1,
    FTM_RESPONDER_ENABLED_BIT = BIT2
} wifi_event_bits_t;

// estructura global anchor_context_t
typedef struct {
    EventGroupHandle_t event_group;       // eventos WiFI y FTM
    esp_mqtt_client_handle_t mqtt_client; // conexión MQTT
    char mac_str[18];                     // dirección MAC de este nodo
    TaskHandle_t mqtt_task_handle;
    time_t last_mqtt_time;
    uint8_t wifi_retry_count;
} anchor_context_t;

static anchor_context_t g_ctx = {0}; // contexto del nodo ancla

// funciones
static void mqtt_task(void *pvParameters);
static void send_position_update(void);
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void initialise_wifi(void);
static void initialise_mqtt(void);

// tarea MQTT
static void mqtt_task(void *pvParameters) {
    TickType_t last_wake_time = xTaskGetTickCount(); // tiempo inicio tarea MQTT

    while (1) {
        EventBits_t bits = xEventGroupGetBits(g_ctx.event_group);  // lee los bits de los eventos actuales
        if (bits & WIFI_STA_CONNECTED_BIT) {                        // si está conectado al AP
            send_position_update();                                 // envía la posición
            g_ctx.last_mqtt_time = time(NULL);                      // tiempo publicación MQTT
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(MQTT_INTERVAL_MS));  // espera al prox envío de MQTT
    }
}

// envía la posición por MQTT
static void send_position_update(void) {
    if (!g_ctx.mqtt_client) {
        ESP_LOGW(TAG, "MQTT client not initialized"); // si el cliente MQTT no está iniciado
        return;
    }

    char json_buffer[256]; // buffer para el mensaje JSON
    int written = snprintf(json_buffer, sizeof(json_buffer),
                 "["
                 "{"
                 "\"mac_anchor\":\"%s\","
                 "\"positionx\":%.1f,"
                 "\"positiony\":%.1f"
                 "}"
                 "]",
                 g_ctx.mac_str, POSITION_X, POSITION_Y);

    if (written >= sizeof(json_buffer)) {
        ESP_LOGE(TAG, "JSON buffer overflow");  // si se sobrepasa el buffer del JSON
        return;
    }

    int msg_id = esp_mqtt_client_publish(g_ctx.mqtt_client, MQTT_TOPIC, json_buffer, 0, 1, 0); // este cliente publica el JSON por MQTT
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish MQTT message"); // fallo al publicar MQTT
    } else {
        ESP_LOGI(TAG, "Published MQTT message, msg_id=%d", msg_id); // se ha publicado el mensaje MQTT
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) { // handler wifi
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START: // AP ha iniciado
                ESP_LOGI(TAG, "SoftAP started");
                xEventGroupSetBits(g_ctx.event_group, WIFI_AP_START_BIT);
                break;

            case WIFI_EVENT_AP_STACONNECTED: { // STA se conecta al AP
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG, "Station connected to AP: " MACSTR ", AID=%d",
                         MAC2STR(event->mac), event->aid);
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: { // STA se desconecta del AP
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                ESP_LOGI(TAG, "Station disconnected from AP: " MACSTR ", AID=%d",
                         MAC2STR(event->mac), event->aid);
                break;
            }

            case WIFI_EVENT_STA_START: // STA ha iniciado
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_CONNECTED: // se ha conectado al AP por MQTT
                ESP_LOGI(TAG, "Connected to AP for MQTT");
                g_ctx.wifi_retry_count = 0;
                xEventGroupSetBits(g_ctx.event_group, WIFI_STA_CONNECTED_BIT);
                break;

            case WIFI_EVENT_STA_DISCONNECTED: // se ha desconectado del AP
                ESP_LOGI(TAG, "Disconnected from AP, attempting reconnection...");
                esp_wifi_connect();
                xEventGroupClearBits(g_ctx.event_group, WIFI_STA_CONNECTED_BIT);
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip)); // log de la IP
        xEventGroupSetBits(g_ctx.event_group, WIFI_STA_CONNECTED_BIT);
    }
}

static void initialise_mqtt(void) { // inicio MQTT
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_URI,
        .credentials.client_id = "esp32_anchor_1",
        .session.keepalive = 60,
    };

    g_ctx.mqtt_client = esp_mqtt_client_init(&mqtt_cfg); // configuración del cliente MQTT
    if (g_ctx.mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(g_ctx.mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(g_ctx.mqtt_client));
    ESP_LOGI(TAG, "MQTT client started");
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) { // handler MQTT
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected"); // conexión MQTT establecida
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");  // desconexión MQTT
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT message received: %.*s", event->data_len, event->data); // mensaje MQTT recibido
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error occurred"); // error en la conexión MQTT
            esp_mqtt_client_stop(g_ctx.mqtt_client);
            esp_mqtt_client_start(g_ctx.mqtt_client);
            break;

        default:
            ESP_LOGD(TAG, "Unhandled MQTT event: %ld", event_id); // evento MQTT no manejado
            break;
    }
}

static void initialise_wifi(void) {  // inicio del WiFi en modo AP y STA
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    g_ctx.event_group = xEventGroupCreate();
    if (g_ctx.event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    // MAC y SSID
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
    snprintf(g_ctx.mac_str, sizeof(g_ctx.mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char ssid[32];
    snprintf(ssid, sizeof(ssid), "ftm_%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // configuración del AP con FTM Responder habilitado
    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = strlen(ssid),
            .channel = CURRENT_CHANNEL,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .ftm_responder = true,  // Habilitar FTM Responder
            .pairwise_cipher = WIFI_CIPHER_TYPE_CCMP,
        },
    };
    strlcpy((char *)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid));

    // configuración del STA para MQTT
    wifi_config_t sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    strlcpy((char *)sta_config.sta.ssid, WIFI_SSID, sizeof(sta_config.sta.ssid));
    strlcpy((char *)sta_config.sta.password, WIFI_PASS, sizeof(sta_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, CURRENT_BW));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialized in APSTA mode");
    ESP_LOGI(TAG, "AP SSID: %s (FTM Responder)", ssid);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();  // se inicia la non-volatile storage
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    initialise_wifi(); // inicio WiFi

    ESP_LOGI(TAG, "Waiting for WiFi connections...");
    EventBits_t bits = xEventGroupWaitBits(g_ctx.event_group, WIFI_AP_START_BIT | WIFI_STA_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(30000)); // timeout de 30 seg

    if ((bits & (WIFI_AP_START_BIT | WIFI_STA_CONNECTED_BIT)) != (WIFI_AP_START_BIT | WIFI_STA_CONNECTED_BIT)) {
        ESP_LOGW(TAG, "WiFi initialization timeout, continuing anyway...");
    }

    initialise_mqtt();  // inicio de MQTT

    // tarea MQTT
    BaseType_t task_created;
    task_created = xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 5, &g_ctx.mqtt_task_handle);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MQTT task");
        return;
    }

    ESP_LOGI(TAG, "All tasks created successfully");
    ESP_LOGI(TAG, "FTM Responder is up and running");
}

