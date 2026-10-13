/**
 * @file    ble_gatt_server.c
 * @brief   BLE 5.0 GATT Server Implementation for Field Commissioning & Telemetry.
 * @author  Sherifred Singh
 */

#include "ble_gatt_server.h"
#include "intermcu_uart_bridge.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include <string.h>

static const char *TAG = "BLE_GATT_SERVER";

#define DEVICE_NAME "EDGE-HUB-S3"

static uint16_t telemetry_char_handle = 0;
static uint16_t conn_id = 0;
static bool is_connected = false;
static bool is_notify_subscribed = false;
static esp_gatt_if_t gatts_if_saved = 0;

/* GATT Profile Application ID */
#define PROFILE_APP_ID 0

/* GAP Advertising Data */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&adv_params);
        break;
    default:
        break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        gatts_if_saved = gatts_if;
        esp_ble_gap_set_device_name(DEVICE_NAME);
        esp_ble_gap_config_adv_data(&adv_data);

        /* Create Primary Service: 0xFFF0 */
        esp_gatt_srvc_id_t service_id;
        service_id.is_primary = true;
        service_id.id.inst_id = 0x00;
        service_id.id.uuid.len = ESP_UUID_LEN_16;
        service_id.id.uuid.uuid.uuid16 = BLE_SERVICE_UUID_EDGE;
        esp_ble_gatts_create_service(gatts_if, &service_id, 8);
        break;

    case ESP_GATTS_CREATE_EVT: {
        uint16_t service_handle = param->create.service_handle;
        esp_ble_gatts_start_service(service_handle);

        /* Add Telemetry Characteristic (Notify) */
        esp_bt_uuid_t char_uuid;
        char_uuid.len = ESP_UUID_LEN_16;
        char_uuid.uuid.uuid16 = BLE_CHAR_UUID_TELEMETRY;
        esp_ble_gatts_add_char(service_handle, &char_uuid,
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_READ,
                               NULL, NULL);

        /* Add Command Characteristic (Write) */
        char_uuid.uuid.uuid16 = BLE_CHAR_UUID_COMMAND;
        esp_ble_gatts_add_char(service_handle, &char_uuid,
                               ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL, NULL);
        break;
    }

    case ESP_GATTS_ADD_CHAR_EVT:
        if (param->add_char.char_uuid.uuid.uuid16 == BLE_CHAR_UUID_TELEMETRY) {
            telemetry_char_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Registered Telemetry Characteristic (Handle=%d)", telemetry_char_handle);
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        conn_id = param->connect.conn_id;
        is_connected = true;
        is_notify_subscribed = false;
        ESP_LOGI(TAG, "BLE Central Connected (conn_id=%d)", conn_id);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        is_connected = false;
        is_notify_subscribed = false;
        ESP_LOGI(TAG, "BLE Central Disconnected. Restarting advertising...");
        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_GATTS_WRITE_EVT: {
        esp_gatt_status_t status = ESP_GATT_OK;

        /* Check if write is targeting Client Characteristic Configuration Descriptor (CCCD) */
        if (param->write.handle == (telemetry_char_handle + 1) && param->write.len == 2) {
            uint16_t descr_value = (uint16_t)(param->write.value[1] << 8 | param->write.value[0]);
            if (descr_value == 0x0001) {
                is_notify_subscribed = true;
                ESP_LOGI(TAG, "BLE Central subscribed to 10 Hz Telemetry Notifications (CCCD=0x0001)");
            } else if (descr_value == 0x0000) {
                is_notify_subscribed = false;
                ESP_LOGI(TAG, "BLE Central unsubscribed from Telemetry Notifications (CCCD=0x0000)");
            }
        } 
        /* Command Characteristic Write */
        else if (param->write.len == sizeof(intermcu_actuator_cmd_t)) {
            intermcu_actuator_cmd_t cmd;
            memcpy(&cmd, param->write.value, sizeof(cmd));

            /* Semantic range validation (AUD-006) */
            if (cmd.command_id < 0x01 || cmd.command_id > 0x04 ||
                cmd.pwm_duty_permille > 1000 || cmd.relay_mask > 0x03) {
                ESP_LOGE(TAG, "Invalid BLE actuator command parameters rejected (Cmd=%d, PWM=%d, Relays=%d)",
                         cmd.command_id, cmd.pwm_duty_permille, cmd.relay_mask);
                status = ESP_GATT_INVALID_ATTR_LEN;
            } else {
                esp_err_t err = InterMcuBridge_SendCommand(&cmd);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to route BLE command down to STM32 (err=%d)", err);
                    status = ESP_GATT_INTERNAL_ERROR;
                } else {
                    ESP_LOGI(TAG, "BLE Actuator Command verified and forwarded to STM32 (Cmd=0x%02X)", cmd.command_id);
                }
            }
        } else {
            status = ESP_GATT_INVALID_ATTR_LEN;
        }

        /* Send write response if requested by client */
        if (param->write.need_rsp) {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
        }
        break;
    }

    default:
        break;
    }
}

esp_err_t BleGattServer_Init(void)
{
    esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) return ret;

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) return ret;

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) return ret;

    ret = esp_bluedroid_init();
    if (ret != ESP_OK) return ret;

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) return ret;

    esp_ble_gatts_register_callback(gatts_profile_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(PROFILE_APP_ID);

    ESP_LOGI(TAG, "BLE 5.0 GATT Server initialized successfully");
    return ESP_OK;
}

void BleGattServer_NotifyTelemetry(const intermcu_telemetry_payload_t *telemetry)
{
    if (is_connected && is_notify_subscribed && telemetry_char_handle != 0) {
        esp_err_t err = esp_ble_gatts_send_indicate(gatts_if_saved, conn_id, telemetry_char_handle,
                                                    sizeof(intermcu_telemetry_payload_t),
                                                    (uint8_t *)telemetry, false);
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "BLE notification indicate skipped or congested (err=%d)", err);
        }
    }
}
