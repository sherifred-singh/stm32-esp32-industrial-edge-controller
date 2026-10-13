/**
 * @file    wifi_mqtt_gateway.h
 * @brief   Wi-Fi Station and MQTT Telemetry Publisher for ESP32-S3.
 * @author  Sherifred Singh
 */

#ifndef WIFI_MQTT_GATEWAY_H
#define WIFI_MQTT_GATEWAY_H

#include "esp_err.h"
#include "../../common/include/intermcu_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_BROKER_URI             "mqtt://broker.hivemq.com"
#define MQTT_TOPIC_TELEMETRY        "industrial/edge_hub/telemetry"
#define MQTT_TOPIC_COMMANDS         "industrial/edge_hub/commands"

/**
 * @brief  Initializes Wi-Fi station, starts MQTT client, and registers event loops.
 */
esp_err_t WifiMqtt_Init(void);

/**
 * @brief  Serializes telemetry into JSON and publishes to MQTT broker with QoS 1.
 * @param  telemetry: Pointer to telemetry payload.
 */
void WifiMqtt_PublishTelemetry(const intermcu_telemetry_payload_t *telemetry);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MQTT_GATEWAY_H */
