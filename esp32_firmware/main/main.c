/**
 * @file    main.c
 * @brief   Application Entry Point for ESP32-S3 Wireless Gateway & Cloud Node.
 * @author  Sherifred Singh
 */

#include "nvs_flash.h"
#include "esp_log.h"
#include "intermcu_uart_bridge.h"
#include "ble_gatt_server.h"
#include "wifi_mqtt_gateway.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN_APP";

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "  ESP32-S3 Industrial Edge Gateway Firmware v1.0.0");
    ESP_LOGI(TAG, "==================================================");

    /* 1. Initialize Non-Volatile Storage (NVS) for Wi-Fi & BLE calibration */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. Initialize Inter-MCU UART Driver */
    ESP_ERROR_CHECK(InterMcuBridge_Init());

    /* 3. Initialize BLE 5.0 GATT Server for Field Diagnostics */
    ESP_ERROR_CHECK(BleGattServer_Init());

    /* 4. Initialize Wi-Fi & MQTT Cloud Publisher */
    ESP_ERROR_CHECK(WifiMqtt_Init());

    /* 5. Spawn Inter-MCU Bridge Task Pinned to Core 1 */
    xTaskCreatePinnedToCore(vInterMcuBridgeTask,
                            "InterMcuBridge",
                            4096,
                            NULL,
                            5,
                            NULL,
                            1); /* Core 1 */

    ESP_LOGI(TAG, "System initialization complete. Core 0: Network | Core 1: BLE + IPC");
}
