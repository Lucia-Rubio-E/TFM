#include <string.h>
#include <stdio.h>
#include <time.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "esp_sntp.h"

#define N_MAX_ANCHORS 8
#define SESIONES_POR_RONDA 8

#define WIFI_SSID "Lucía"
#define WIFI_PASS "passwordlucia"
#define MQTT_URI         "mqtt://172.20.10.13:1884"
#define MQTT_TOPIC       "data"

static const char *TAG = "FTM_TAG";
static uint8_t mac_tag[6];

typedef struct {
    wifi_ap_record_t records[N_MAX_ANCHORS];
    uint8_t count;
    uint8_t current;
} anchor_info_t;

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_DISCONNECTED_BIT = BIT1;

static EventGroupHandle_t ftm_event_group;
static esp_mqtt_client_handle_t mqtt_client = NULL;

static uint32_t s_rtt_est = 0, s_dist_est = 0;
static anchor_info_t anchor_info = {0};

const int FTM_REPORT_BIT = BIT0;
const int FTM_FAILURE_BIT = BIT1;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT desconectado");
            break;
        default:
            break;
    }
}

static void initialise_mqtt(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_URI,
        .network.disable_auto_reconnect = true,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_DISCONNECTED:
                xEventGroupSetBits(wifi_event_group, WIFI_DISCONNECTED_BIT);
                break;
            case WIFI_EVENT_STA_CONNECTED:
                xEventGroupClearBits(wifi_event_group, WIFI_DISCONNECTED_BIT);
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void connect_to_mqtt_wifi(void) {
    ESP_LOGI(TAG, "Conectando a Wi-Fi SSID: %s", WIFI_SSID);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_connect());
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "Conectado a Wi-Fi para la conexión MQTT");
}

static void disconnect_from_mqtt_wifi(void) {

    ESP_LOGI(TAG, "Desconectando de Wi-Fi SSID: %s", WIFI_SSID);
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler));
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_DISCONNECTED_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(5000));

    if (bits & WIFI_DISCONNECTED_BIT) {
        ESP_LOGI(TAG, "Desconexión de Wi-Fi correcta");
    } else {
        ESP_LOGW(TAG, "Tiempo de espera agotado esperando la desconexión Wi-Fi");
    }

    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
}

static void ftm_report_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    wifi_event_ftm_report_t *event = (wifi_event_ftm_report_t *) event_data;

    if (event->status == FTM_STATUS_SUCCESS) {
        s_rtt_est = event->rtt_est;
        s_dist_est = event->dist_est;
        xEventGroupSetBits(ftm_event_group, FTM_REPORT_BIT);
    } else {
        ESP_LOGW(TAG, "FTM fallido (Estado - %d)", event->status);
        xEventGroupSetBits(ftm_event_group, FTM_FAILURE_BIT);
    }
}

static esp_err_t initialize_anchors(void) {
    wifi_scan_config_t scan_config = {0};
    uint16_t ap_count = 0;
    wifi_ap_record_t* ap_records = NULL;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    if (ap_count == 0) {
        ESP_LOGW(TAG, "No se han encontrado APs");
        return ESP_FAIL;
    }

    ap_records = malloc(ap_count * sizeof(wifi_ap_record_t));
    if (!ap_records) {
        ESP_LOGE(TAG, "Error al asignar memoria para los registros AP");
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

    anchor_info.count = 0;
    for (int i = 0; i < ap_count && anchor_info.count < N_MAX_ANCHORS; i++) {
        if (ap_records[i].ftm_responder) {
            memcpy(&anchor_info.records[anchor_info.count], &ap_records[i], sizeof(wifi_ap_record_t));
            ESP_LOGI(TAG, "Se ha encontrado un nodo FTM: " MACSTR " en canal %d", MAC2STR(ap_records[i].bssid), ap_records[i].primary);
            anchor_info.count++;
        }
    }
    if (anchor_info.count > 0) {
        ESP_LOGI(TAG, "Iniciando con %d nodos anchor FTM", anchor_info.count);
    }
    free(ap_records);
    return ESP_OK;
}

static void initialise_wifi(void) {

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ftm_event_group = xEventGroupCreate();
    wifi_event_group = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_FTM_REPORT, &ftm_report_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

}

static void ftm_session_task(void *param) {
    while (1) {
        if (anchor_info.count == 0) {
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        char mqtt_buffer[1024] = "[";
        int json_count = 0;

        for (int anchor_idx = 0; anchor_idx < anchor_info.count; anchor_idx++) {
            uint64_t sum_rtt = 0;
            uint64_t sum_dist = 0;
            int valid_measurements = 0;

            for (int session = 0; session < SESIONES_POR_RONDA; session++) {
                wifi_ftm_initiator_cfg_t ftmi_cfg = {
                    .frm_count = 16,
                    .burst_period = 2,
                    .channel = anchor_info.records[anchor_idx].primary,
                    .use_get_report_api = true,
                };

                memcpy(ftmi_cfg.resp_mac, anchor_info.records[anchor_idx].bssid, 6);

                ESP_LOGI(TAG, "Iniciando sesión FTM con " MACSTR " (Ancla %d/%d, Sesión %d/%d)",
                         MAC2STR(ftmi_cfg.resp_mac),
                         anchor_idx + 1, anchor_info.count,
                         session + 1, SESIONES_POR_RONDA);

                if (esp_wifi_ftm_initiate_session(&ftmi_cfg) == ESP_OK) {
                    EventBits_t bits = xEventGroupWaitBits(ftm_event_group,
                                                           FTM_REPORT_BIT | FTM_FAILURE_BIT,
                                                           pdTRUE, pdFALSE,
                                                           pdMS_TO_TICKS(10000));

                    if (bits & FTM_REPORT_BIT) {
                        ESP_LOGI(TAG, "FTM éxito: RTT estimado - %lu ns, Distancia estimada - %lu cm", s_rtt_est, s_dist_est);
                        sum_rtt += s_rtt_est;
                        sum_dist += s_dist_est;
                        valid_measurements++;
                    } else if (bits & FTM_FAILURE_BIT) {
                        ESP_LOGW(TAG, "Sesión FTM fallida. Reintentando en 2 segundos...");
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        continue;
                    } else {
                        ESP_LOGW(TAG, "Tiempo de espera agotado en la sesión FTM");
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        continue;
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            if (valid_measurements > 0) {
                uint32_t avg_rtt = sum_rtt / valid_measurements;
                uint32_t avg_distance = sum_dist / valid_measurements;

                ESP_LOGI(TAG, "Promedio para " MACSTR ": RTT - %lu ns, Distancia - %lu cm",
                         MAC2STR(anchor_info.records[anchor_idx].bssid),
                         avg_rtt, avg_distance);

                char mac_src_str[18];
                snprintf(mac_src_str, sizeof(mac_src_str),
                         "%02X:%02X:%02X:%02X:%02X:%02X",
                         mac_tag[0], mac_tag[1], mac_tag[2],
                         mac_tag[3], mac_tag[4], mac_tag[5]);

                char mac_dst_str[18];
                snprintf(mac_dst_str, sizeof(mac_dst_str),
                         "%02X:%02X:%02X:%02X:%02X:%02X",
                         anchor_info.records[anchor_idx].bssid[0],
                         anchor_info.records[anchor_idx].bssid[1],
                         anchor_info.records[anchor_idx].bssid[2],
                         anchor_info.records[anchor_idx].bssid[3],
                         anchor_info.records[anchor_idx].bssid[4],
                         anchor_info.records[anchor_idx].bssid[5]);

                char json_buffer[256];
                snprintf(json_buffer, sizeof(json_buffer),
                         "{"
                         "\"mac_src\":\"%s\","
                         "\"mac_dst\":\"%s\","
                         "\"distance_cm\":%.2f,"
                         "\"rtt_ns\":%.2f"
                         "}",
                         mac_src_str, mac_dst_str, (double)avg_distance, (double)avg_rtt);

                if (json_count > 0) {
                    strncat(mqtt_buffer, ",", sizeof(mqtt_buffer) - strlen(mqtt_buffer) - 1);
                }
                strncat(mqtt_buffer, json_buffer, sizeof(mqtt_buffer) - strlen(mqtt_buffer) - 1);
                json_count++;
            } else {
                ESP_LOGW(TAG, "No se obtuvieron mediciones válidas para " MACSTR,
                         MAC2STR(anchor_info.records[anchor_idx].bssid));
            }
        }

        if (json_count > 0) {
            strncat(mqtt_buffer, "]", sizeof(mqtt_buffer) - strlen(mqtt_buffer) - 1);

            ESP_LOGI(TAG, "Mensaje a enviar por MQTT: %s", mqtt_buffer);

            connect_to_mqtt_wifi();
            initialise_mqtt();

            vTaskDelay(pdMS_TO_TICKS(2000));

            int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, mqtt_buffer, 0, 1, 0);
            if (msg_id < 0) {
                ESP_LOGE(TAG, "Error al publicar mensaje MQTT");
            } else {
                ESP_LOGI(TAG, "Mensaje MQTT publicado, msg_id=%d", msg_id);
            }

            vTaskDelay(pdMS_TO_TICKS(2000));

            esp_mqtt_client_stop(mqtt_client);
            esp_mqtt_client_destroy(mqtt_client);
            mqtt_client = NULL;
            disconnect_from_mqtt_wifi();

            memset(mqtt_buffer, 0, sizeof(mqtt_buffer));
            mqtt_buffer[0] = '[';
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_read_mac(mac_tag, ESP_MAC_WIFI_STA));
    initialise_wifi();
    esp_log_level_set("wifi", ESP_LOG_INFO);


    if (initialize_anchors() != ESP_OK) {
        ESP_LOGE(TAG, "Error al buscar nodos anchor");
        return;
    }

    xTaskCreate(ftm_session_task, "FTM Session Task", 4096, NULL, configMAX_PRIORITIES - 1, NULL);
}

