/**
 * @file    intermcu_uart_bridge.h
 * @brief   Inter-MCU UART Driver and Bridge for ESP32-S3.
 * @author  Sherifred Singh
 */

#ifndef INTERMCU_UART_BRIDGE_H
#define INTERMCU_UART_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "../../common/include/intermcu_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INTERMCU_UART_PORT          UART_NUM_1
#define INTERMCU_UART_TX_PIN        17          /* Connects to STM32 PA3 (RX) */
#define INTERMCU_UART_RX_PIN        18          /* Connects to STM32 PA2 (TX) */
#define INTERMCU_UART_BAUD_RATE     115200
#define INTERMCU_UART_BUF_SIZE      512

/**
 * @brief  Initializes UART1 on ESP32-S3 with FreeRTOS ring buffer.
 */
esp_err_t InterMcuBridge_Init(void);

/**
 * @brief  Task consuming serial stream from STM32 and dispatching to BLE & MQTT.
 * @param  pvParameters: FreeRTOS task argument.
 */
void vInterMcuBridgeTask(void *pvParameters);

/**
 * @brief  Transmits an actuator command packet down to the STM32 over UART.
 * @param  cmd: Pointer to actuator command structure.
 * @retval ESP_OK on success.
 */
esp_err_t InterMcuBridge_SendCommand(const intermcu_actuator_cmd_t *cmd);

#ifdef __cplusplus
}
#endif

#endif /* INTERMCU_UART_BRIDGE_H */
