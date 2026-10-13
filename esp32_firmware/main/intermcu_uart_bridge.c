/**
 * @file    intermcu_uart_bridge.c
 * @brief   Inter-MCU UART Driver and Telemetry Dispatcher for ESP32-S3.
 * @author  Sherifred Singh
 */

#include "intermcu_uart_bridge.h"
#include "ble_gatt_server.h"
#include "wifi_mqtt_gateway.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "INTERMCU_BRIDGE";

static intermcu_parser_t parser;
static uint8_t tx_seq_counter = 0;

esp_err_t InterMcuBridge_Init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = INTERMCU_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(INTERMCU_UART_PORT, INTERMCU_UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (err != ESP_OK) return err;

    err = uart_param_config(INTERMCU_UART_PORT, &uart_config);
    if (err != ESP_OK) return err;

    err = uart_set_pin(INTERMCU_UART_PORT, INTERMCU_UART_TX_PIN, INTERMCU_UART_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    intermcu_parser_init(&parser);
    ESP_LOGI(TAG, "Inter-MCU UART1 initialized on TX=%d, RX=%d at %d baud",
             INTERMCU_UART_TX_PIN, INTERMCU_UART_RX_PIN, INTERMCU_UART_BAUD_RATE);
    return ESP_OK;
}

esp_err_t InterMcuBridge_SendCommand(const intermcu_actuator_cmd_t *cmd)
{
    if (!cmd) return ESP_ERR_INVALID_ARG;

    uint8_t tx_buf[INTERMCU_MAX_FRAME_SIZE];
    size_t frame_len = intermcu_encode_frame(INTERMCU_MSG_ACTUATOR_COMMAND,
                                             tx_seq_counter++,
                                             cmd,
                                             sizeof(intermcu_actuator_cmd_t),
                                             tx_buf,
                                             sizeof(tx_buf));
    if (frame_len == 0) return ESP_FAIL;

    int bytes_sent = uart_write_bytes(INTERMCU_UART_PORT, tx_buf, frame_len);
    if (bytes_sent != (int)frame_len) {
        ESP_LOGE(TAG, "Failed to transmit complete frame (sent %d of %u bytes)", bytes_sent, (unsigned)frame_len);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Dispatched Actuator Command to STM32 (Cmd=0x%02X, PWM=%u, Relays=0x%02X)",
             cmd->command_id, cmd->pwm_duty_permille, cmd->relay_mask);
    return ESP_OK;
}

void vInterMcuBridgeTask(void *pvParameters)
{
    (void)pvParameters;
    uint8_t rx_buffer[128];
    intermcu_frame_t frame;

    ESP_LOGI(TAG, "Inter-MCU Bridge Task started on Core 1");

    for (;;) {
        /* Read available bytes from UART ring buffer */
        int bytes_read = uart_read_bytes(INTERMCU_UART_PORT, rx_buffer, sizeof(rx_buffer), pdMS_TO_TICKS(20));

        if (bytes_read > 0) {
            for (int i = 0; i < bytes_read; i++) {
                if (intermcu_parser_process_byte(&parser, rx_buffer[i], &frame)) {
                    
                    /* Valid frame received from STM32 */
                    switch (frame.msg_type) {
                    case INTERMCU_MSG_TELEMETRY_REPORT:
                        if (frame.payload_len >= sizeof(intermcu_telemetry_payload_t)) {
                            intermcu_telemetry_payload_t telemetry;
                            memcpy(&telemetry, frame.payload, sizeof(telemetry));

                            /* 1. Push to BLE 5.0 GATT Notify Characteristic */
                            BleGattServer_NotifyTelemetry(&telemetry);

                            /* 2. Dispatch to Wi-Fi MQTT Publisher */
                            WifiMqtt_PublishTelemetry(&telemetry);
                        }
                        break;

                    case INTERMCU_MSG_ACK_NACK:
                        ESP_LOGD(TAG, "Received ACK from STM32");
                        break;

                    case INTERMCU_MSG_FAULT_SNAPSHOT_RSP:
                        ESP_LOGW(TAG, "Received Fault Snapshot from STM32 (%u entries)",
                                 (unsigned)(frame.payload_len / sizeof(intermcu_fault_entry_t)));
                        break;

                    default:
                        ESP_LOGW(TAG, "Unknown message type: 0x%02X", frame.msg_type);
                        break;
                    }
                }
            }
        }
    }
}
