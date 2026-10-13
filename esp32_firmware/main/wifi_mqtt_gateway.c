/**
 * @file    wifi_mqtt_gateway.c
 * @brief   Wi-Fi Station and MQTT Telemetry Publisher Implementation.
 * @author  Sherifred Singh
 */

#include "wifi_mqtt_gateway.h"
#include "intermcu_uart_bridge.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "WIFI_MQTT_GATEWAY";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool is_mqtt_connected = false;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        is_mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT Connected to Broker: %s", MQTT_BROKER_URI);
        esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_COMMANDS, 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        is_mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT Disconnected from Broker");
        break;

    case MQTT_EVENT_DATA: {
        ESP_LOGI(TAG, "MQTT Remote Command Received on topic: %.*s (Length: %d bytes)",
                 event->topic_len, event->topic, event->data_len);
        
        /* Safe length-bounded buffer copy preventing buffer over-read (AUD-002) */
        char cmd_buf[128];
        if (event->data_len > 0 && event->data_len < (int)sizeof(cmd_buf)) {
            memcpy(cmd_buf, event->data, event->data_len);
            cmd_buf[event->data_len] = '\0';

            /* Command Authorization & Authenticity Validation (AUD-003) */
            /* In production industrial telematics: Require shared authentication token */
            const char *AUTH_TOKEN = "TOKEN:SEC_EDGE_8820";
            if (strstr(cmd_buf, AUTH_TOKEN) == NULL) {
                ESP_LOGE(TAG, "Security Alert: Unauthorized MQTT command attempt rejected (Missing valid token)");
                break;
            }

            intermcu_actuator_cmd_t cmd;
            memset(&cmd, 0, sizeof(cmd));

            if (strstr(cmd_buf, "CMD:ESTOP")) {
                cmd.command_id = 0x03; /* Hardware E-Stop */
                ESP_LOGW(TAG, "Authorized Remote Emergency Stop triggered via MQTT");
                InterMcuBridge_SendCommand(&cmd);
            } else if (strstr(cmd_buf, "CMD:RESET_FAULTS")) {
                cmd.command_id = 0x04; /* Fault reset */
                ESP_LOGI(TAG, "Authorized Fault Reset triggered via MQTT");
                InterMcuBridge_SendCommand(&cmd);
            } else if (strstr(cmd_buf, "CMD:SET_PWM")) {
                char *pwm_val = strstr(cmd_buf, "VAL:");
                if (pwm_val) {
                    int duty = atoi(pwm_val + 4);
                    if (duty >= 0 && duty <= 1000) {
                        cmd.command_id = 0x01;
                        cmd.pwm_duty_permille = (uint16_t)duty;
                        InterMcuBridge_SendCommand(&cmd);
                    }
                }
            }
        }
        break;
    }

    default:
        break;
    }
}

esp_err_t WifiMqtt_Init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) return ESP_FAIL;

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    ESP_LOGI(TAG, "MQTT Client initialized and connecting...");
    return ESP_OK;
}

void WifiMqtt_PublishTelemetry(const intermcu_telemetry_payload_t *telemetry)
{
    if (!is_mqtt_connected || !mqtt_client || !telemetry) return;

    char json_payload[256];
    int len = snprintf(json_payload, sizeof(json_payload),
        "{\"dev\":\"edge_hub_s3\",\"ts\":%u,\"temp\":%.1f,\"vib_rms\":%u,"
        "\"volt\":%.2f,\"curr_a\":%.2f,\"rpm\":%u,\"fault\":%u}",
        (unsigned)telemetry->timestamp_ms,
        (float)telemetry->temperature_c_x10 / 10.0f,
        (unsigned)telemetry->vibration_rms_mg,
        (float)telemetry->voltage_mv / 1000.0f,
        (float)telemetry->current_ma / 1000.0f,
        (unsigned)telemetry->shaft_rpm,
        (unsigned)telemetry->active_fault_flags);

    if (len > 0 && len < (int)sizeof(json_payload)) {
        esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_TELEMETRY, json_payload, len, 1, 0);
    }
}
