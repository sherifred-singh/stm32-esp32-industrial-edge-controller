/**
 * @file    ble_gatt_server.h
 * @brief   BLE 5.0 GATT Server Interface for Industrial Edge Field Diagnostics.
 * @author  Sherifred Singh
 */

#ifndef BLE_GATT_SERVER_H
#define BLE_GATT_SERVER_H

#include "esp_err.h"
#include "../../common/include/intermcu_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Custom 16-bit UUIDs for GATT Profile */
#define BLE_SERVICE_UUID_EDGE        0xFFF0
#define BLE_CHAR_UUID_TELEMETRY      0xFFF1  /* Notify */
#define BLE_CHAR_UUID_COMMAND        0xFFF2  /* Write */

/**
 * @brief  Initializes the BLE 5.0 stack, registers GATT attributes, and begins advertising.
 */
esp_err_t BleGattServer_Init(void);

/**
 * @brief  Pushes updated telemetry payload to connected BLE central devices via GATT Notify.
 * @param  telemetry: Pointer to telemetry payload.
 */
void BleGattServer_NotifyTelemetry(const intermcu_telemetry_payload_t *telemetry);

#ifdef __cplusplus
}
#endif

#endif /* BLE_GATT_SERVER_H */
