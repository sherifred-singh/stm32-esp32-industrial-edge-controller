/**
 * @file    intermcu_protocol.h
 * @brief   Inter-MCU Binary Packet Protocol Specification and Data Types
 *          shared between STM32F446RE (Real-Time Core) and ESP32-S3 (Wireless Gateway).
 * @author  Sherifred Singh
 *
 * Frame Format:
 * [0..1] SYNC WORD     : 0xAA 0x55
 * [2]    PAYLOAD LENGTH: 0 .. 64 bytes
 * [3]    MSG TYPE      : intermcu_msg_type_t
 * [4]    SEQUENCE NUM  : 0 .. 255 (rolling frame counter)
 * [5..N] PAYLOAD       : Raw structured payload (N = PAYLOAD LENGTH)
 * [N+1]  CRC16 MSB     : CRC-16-CCITT high byte
 * [N+2]  CRC16 LSB     : CRC-16-CCITT low byte
 */

#ifndef INTERMCU_PROTOCOL_H
#define INTERMCU_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Protocol Framing Constants */
#define INTERMCU_SYNC_BYTE_1          0xAAU
#define INTERMCU_SYNC_BYTE_2          0x55U
#define INTERMCU_MAX_PAYLOAD_SIZE     64U
#define INTERMCU_HEADER_SIZE          5U   /* SYNC1 + SYNC2 + LEN + TYPE + SEQ */
#define INTERMCU_CRC_SIZE             2U   /* 16-bit CRC */
#define INTERMCU_MAX_FRAME_SIZE       (INTERMCU_HEADER_SIZE + INTERMCU_MAX_PAYLOAD_SIZE + INTERMCU_CRC_SIZE)

/* Message Types */
typedef enum {
    INTERMCU_MSG_HEARTBEAT            = 0x01,
    INTERMCU_MSG_TELEMETRY_REPORT     = 0x02,
    INTERMCU_MSG_ACTUATOR_COMMAND     = 0x03,
    INTERMCU_MSG_FAULT_SNAPSHOT_REQ   = 0x04,
    INTERMCU_MSG_FAULT_SNAPSHOT_RSP   = 0x05,
    INTERMCU_MSG_ACK_NACK             = 0x06
} intermcu_msg_type_t;

/* Status / ACK Codes */
typedef enum {
    INTERMCU_STATUS_OK                = 0x00,
    INTERMCU_STATUS_ERR_CRC           = 0x01,
    INTERMCU_STATUS_ERR_INVALID_CMD   = 0x02,
    INTERMCU_STATUS_ERR_PARAM_RANGE   = 0x03,
    INTERMCU_STATUS_ERR_BUSY          = 0x04,
    INTERMCU_STATUS_ERR_OVERFLOW      = 0x05
} intermcu_status_code_t;

/* Fault Codes recorded in circular supervisor table */
typedef enum {
    FAULT_NONE                        = 0x00,
    FAULT_OVER_TEMPERATURE            = 0x01,
    FAULT_HIGH_VIBRATION              = 0x02,
    FAULT_OVER_CURRENT                = 0x03,
    FAULT_UNDER_VOLTAGE               = 0x04,
    FAULT_COMM_TIMEOUT                = 0x05,
    FAULT_WATCHDOG_EARLY_WARNING      = 0x06
} intermcu_fault_code_t;

/* Fault Severities */
typedef enum {
    FAULT_SEV_INFO                    = 0x01,
    FAULT_SEV_WARNING                 = 0x02,
    FAULT_SEV_CRITICAL_TRIP           = 0x03
} intermcu_fault_severity_t;

#pragma pack(push, 1)

/**
 * @brief Telemetry payload streamed from STM32 to ESP32 at 10 Hz.
 */
typedef struct {
    uint32_t timestamp_ms;         /* System tick in milliseconds */
    int16_t  temperature_c_x10;    /* Process Temperature in 0.1°C (-400 to 1250) */
    uint16_t vibration_rms_mg;     /* Acceleration RMS in milli-g (0 to 10000 mg) */
    uint16_t voltage_mv;           /* DC Bus Voltage in mV (e.g., 24000 = 24.0V) */
    uint16_t current_ma;           /* Load Current in mA (e.g., 1500 = 1.5A) */
    uint16_t shaft_rpm;            /* Process Motor / Shaft Speed */
    uint8_t  actuator_state;       /* Bit 0: Relay1, Bit 1: Relay2, Bit 2: PWM active */
    uint8_t  active_fault_flags;   /* Bitmask of currently active faults */
} intermcu_telemetry_payload_t;

/**
 * @brief Actuator command sent from ESP32 (BLE/MQTT) to STM32.
 */
typedef struct {
    uint8_t  command_id;           /* 0x01: Set PWM, 0x02: Set Relays, 0x03: E-Stop, 0x04: Reset Faults */
    uint16_t pwm_duty_permille;    /* 0 to 1000 (0.0% to 100.0%) */
    uint8_t  relay_mask;           /* Bit 0: Relay1 Enable, Bit 1: Relay2 Enable */
    uint8_t  reserved;             /* Alignment pad */
} intermcu_actuator_cmd_t;

/**
 * @brief Single entry in the 16-entry circular fault supervisor table.
 */
typedef struct {
    uint32_t timestamp_ms;         /* Occurrence timestamp */
    uint8_t  fault_code;           /* intermcu_fault_code_t */
    uint8_t  severity;             /* intermcu_fault_severity_t */
    uint16_t fault_param;          /* Offending measured value */
} intermcu_fault_entry_t;

/**
 * @brief Bidirectional link heartbeat keepalive payload.
 */
typedef struct {
    uint32_t uptime_seconds;       /* Node uptime */
    uint8_t  system_state;         /* 0: Normal, 1: Warning, 2: Fault Tripped */
    uint8_t  fault_count;          /* Total faults currently logged in memory */
} intermcu_heartbeat_payload_t;

/**
 * @brief Command acknowledgement payload.
 */
typedef struct {
    uint8_t  acked_seq;            /* Sequence number being acknowledged */
    uint8_t  status_code;          /* intermcu_status_code_t */
} intermcu_ack_payload_t;

/**
 * @brief Decoded packet structure.
 */
typedef struct {
    uint8_t  msg_type;
    uint8_t  seq_num;
    uint8_t  payload_len;
    uint8_t  payload[INTERMCU_MAX_PAYLOAD_SIZE];
    uint16_t crc16;
} intermcu_frame_t;

#pragma pack(pop)

/* Parser State Machine States */
typedef enum {
    PARSER_STATE_WAIT_SYNC_1,
    PARSER_STATE_WAIT_SYNC_2,
    PARSER_STATE_GET_LEN,
    PARSER_STATE_GET_TYPE,
    PARSER_STATE_GET_SEQ,
    PARSER_STATE_GET_PAYLOAD,
    PARSER_STATE_GET_CRC_MSB,
    PARSER_STATE_GET_CRC_LSB
} intermcu_parser_state_t;

/**
 * @brief Streaming frame parser instance.
 */
typedef struct {
    intermcu_parser_state_t state;
    uint8_t                 payload_len;
    uint8_t                 payload_index;
    uint8_t                 rx_buffer[INTERMCU_MAX_FRAME_SIZE];
    uint16_t                rx_crc;
    uint32_t                rx_frame_count;
    uint32_t                crc_error_count;
    uint32_t                dropped_byte_count;
    intermcu_frame_t        current_frame;
} intermcu_parser_t;

/* Public API Functions */

/**
 * @brief  Initializes or resets a protocol parser instance.
 * @param  parser: Pointer to parser structure.
 */
void intermcu_parser_init(intermcu_parser_t *parser);

/**
 * @brief  Feeds a single byte from UART DMA into the streaming parser.
 * @param  parser: Pointer to parser instance.
 * @param  byte: Received byte from UART RX buffer.
 * @param  out_frame: Destination pointer where complete frame will be copied on success.
 * @retval true if a complete, CRC-verified frame was decoded; false otherwise.
 */
bool intermcu_parser_process_byte(intermcu_parser_t *parser, uint8_t byte, intermcu_frame_t *out_frame);

/**
 * @brief  Serializes and encodes a frame into a transmission buffer with CRC-16.
 * @param  msg_type: Message type identifier.
 * @param  seq_num: Rolling sequence counter.
 * @param  payload: Pointer to data payload.
 * @param  payload_len: Size of payload in bytes (<= 64).
 * @param  out_buffer: Destination array for the framed packet.
 * @param  max_buffer_len: Maximum capacity of destination buffer.
 * @retval Total bytes written to out_buffer, or 0 on error.
 */
size_t intermcu_encode_frame(uint8_t msg_type,
                             uint8_t seq_num,
                             const void *payload,
                             uint8_t payload_len,
                             uint8_t *out_buffer,
                             size_t max_buffer_len);

#ifdef __cplusplus
}
#endif

#endif /* INTERMCU_PROTOCOL_H */
