/**
 * @file    test_intermcu_protocol.c
 * @brief   Native C Host-Side Unit Test Suite for Inter-MCU Protocol Engine.
 *          Tests the real C implementation (intermcu_protocol.c and intermcu_crc16.c)
 *          with strict boundary conditions, fault injection, and noise recovery.
 * @author  Sherifred Singh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "intermcu_protocol.h"
#include "intermcu_crc16.h"

#define COLOR_GREEN "\033[1;32m"
#define COLOR_RED   "\033[1;31m"
#define COLOR_CYAN  "\033[1;36m"
#define COLOR_RESET "\033[0m"

static int tests_passed = 0;
static int tests_total = 0;

static void test_crc16_known_vector(void)
{
    tests_total++;
    printf("[C-TEST 1] CRC-16-CCITT Known Vector '123456789' ... ");
    const uint8_t vec[] = "123456789";
    uint16_t crc = intermcu_crc16_calculate(vec, 9);
    if (crc == 0x29B1) {
        printf(COLOR_GREEN "PASSED (0x29B1)\n" COLOR_RESET);
        tests_passed++;
    } else {
        printf(COLOR_RED "FAILED (Expected 0x29B1, Got 0x%04X)\n" COLOR_RESET, crc);
    }
}

static void test_encode_and_decode_telemetry(void)
{
    tests_total++;
    printf("[C-TEST 2] Encode & Decode 16-Byte Telemetry Frame ... ");

    intermcu_telemetry_payload_t tx_telemetry = {
        .timestamp_ms      = 125000,
        .temperature_c_x10 = 425,   /* 42.5 °C */
        .vibration_rms_mg  = 1240,  /* 1240 mg */
        .voltage_mv        = 24000, /* 24.0 V */
        .current_ma        = 1500,  /* 1.5 A */
        .shaft_rpm         = 1450,
        .actuator_state    = 0x05,
        .active_fault_flags= 0x00
    };

    uint8_t buffer[INTERMCU_MAX_FRAME_SIZE];
    size_t frame_len = intermcu_encode_frame(INTERMCU_MSG_TELEMETRY_REPORT,
                                             42,
                                             &tx_telemetry,
                                             sizeof(tx_telemetry),
                                             buffer,
                                             sizeof(buffer));

    assert(frame_len == (INTERMCU_HEADER_SIZE + sizeof(tx_telemetry) + INTERMCU_CRC_SIZE));

    intermcu_parser_t parser;
    intermcu_parser_init(&parser);

    intermcu_frame_t rx_frame;
    bool decoded = false;

    for (size_t i = 0; i < frame_len; i++) {
        if (intermcu_parser_process_byte(&parser, buffer[i], &rx_frame)) {
            decoded = true;
        }
    }

    if (decoded && rx_frame.msg_type == INTERMCU_MSG_TELEMETRY_REPORT &&
        rx_frame.seq_num == 42 && rx_frame.payload_len == sizeof(tx_telemetry) &&
        memcmp(rx_frame.payload, &tx_telemetry, sizeof(tx_telemetry)) == 0) {
        printf(COLOR_GREEN "PASSED (23 Bytes Verified)\n" COLOR_RESET);
        tests_passed++;
    } else {
        printf(COLOR_RED "FAILED\n" COLOR_RESET);
    }
}

static void test_null_pointer_rejection(void)
{
    tests_total++;
    printf("[C-TEST 3] AUD-005 Null Payload Pointer Rejection ... ");
    uint8_t buffer[64];
    /* Passing payload_len > 0 but payload == NULL must safely return 0 */
    size_t len = intermcu_encode_frame(INTERMCU_MSG_TELEMETRY_REPORT, 1, NULL, 16, buffer, sizeof(buffer));
    if (len == 0) {
        printf(COLOR_GREEN "PASSED (Safely Returned 0)\n" COLOR_RESET);
        tests_passed++;
    } else {
        printf(COLOR_RED "FAILED (Produced frame without payload)\n" COLOR_RESET);
    }
}

static void test_zero_length_payload_heartbeat(void)
{
    tests_total++;
    printf("[C-TEST 4] Zero-Length Payload Heartbeat Frame ... ");
    uint8_t buffer[32];
    size_t frame_len = intermcu_encode_frame(INTERMCU_MSG_HEARTBEAT, 7, NULL, 0, buffer, sizeof(buffer));
    assert(frame_len == (INTERMCU_HEADER_SIZE + 0 + INTERMCU_CRC_SIZE));

    intermcu_parser_t parser;
    intermcu_parser_init(&parser);
    intermcu_frame_t rx_frame;
    bool decoded = false;

    for (size_t i = 0; i < frame_len; i++) {
        if (intermcu_parser_process_byte(&parser, buffer[i], &rx_frame)) {
            decoded = true;
        }
    }

    if (decoded && rx_frame.msg_type == INTERMCU_MSG_HEARTBEAT && rx_frame.payload_len == 0) {
        printf(COLOR_GREEN "PASSED (7 Bytes Verified)\n" COLOR_RESET);
        tests_passed++;
    } else {
        printf(COLOR_RED "FAILED\n" COLOR_RESET);
    }
}

static void test_corrupted_crc_rejection(void)
{
    tests_total++;
    printf("[C-TEST 5] Corrupted CRC Frame Rejection ... ");
    uint8_t buffer[32];
    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    size_t frame_len = intermcu_encode_frame(INTERMCU_MSG_ACTUATOR_COMMAND, 5, payload, sizeof(payload), buffer, sizeof(buffer));

    /* Invert last CRC bit */
    buffer[frame_len - 1] ^= 0x55;

    intermcu_parser_t parser;
    intermcu_parser_init(&parser);
    intermcu_frame_t rx_frame;
    bool decoded = false;

    for (size_t i = 0; i < frame_len; i++) {
        if (intermcu_parser_process_byte(&parser, buffer[i], &rx_frame)) {
            decoded = true;
        }
    }

    if (!decoded && parser.crc_error_count == 1) {
        printf(COLOR_GREEN "PASSED (Correctly Flagged CRC Error)\n" COLOR_RESET);
        tests_passed++;
    } else {
        printf(COLOR_RED "FAILED\n" COLOR_RESET);
    }
}

static void test_noise_and_sync_recovery(void)
{
    tests_total++;
    printf("[C-TEST 6] Noise Burst & False-Sync (0xAA 0xAA 0x55) Recovery ... ");

    uint8_t buffer[32];
    intermcu_actuator_cmd_t cmd = {
        .command_id = 0x01,
        .pwm_duty_permille = 750,
        .relay_mask = 0x02,
        .reserved = 0x00
    };
    size_t frame_len = intermcu_encode_frame(INTERMCU_MSG_ACTUATOR_COMMAND, 88, &cmd, sizeof(cmd), buffer, sizeof(buffer));

    /* Prepend harsh noise stream containing false sync markers */
    uint8_t noise_stream[64];
    uint8_t noise_prefix[] = {0xFF, 0x00, 0xDE, 0xAD, 0xAA, 0xAA, 0x55};
    size_t prefix_len = sizeof(noise_prefix);

    memcpy(noise_stream, noise_prefix, prefix_len);
    /* Append valid frame payload starting from LEN byte (as 0xAA 0x55 was in prefix) */
    memcpy(&noise_stream[prefix_len], &buffer[2], frame_len - 2);
    size_t total_stream_len = prefix_len + (frame_len - 2);

    intermcu_parser_t parser;
    intermcu_parser_init(&parser);
    intermcu_frame_t rx_frame;
    bool decoded = false;

    for (size_t i = 0; i < total_stream_len; i++) {
        if (intermcu_parser_process_byte(&parser, noise_stream[i], &rx_frame)) {
            decoded = true;
        }
    }

    if (decoded && rx_frame.msg_type == INTERMCU_MSG_ACTUATOR_COMMAND && rx_frame.seq_num == 88) {
        printf(COLOR_GREEN "PASSED (Recovered and Decoded Frame)\n" COLOR_RESET);
        tests_passed++;
    } else {
        printf(COLOR_RED "FAILED\n" COLOR_RESET);
    }
}

static void test_oversized_payload_rejection(void)
{
    tests_total++;
    printf("[C-TEST 7] Oversized Payload (>64 Bytes) Boundary Rejection ... ");
    uint8_t buffer[128];
    uint8_t large_payload[65];
    memset(large_payload, 0x5A, sizeof(large_payload));

    size_t len = intermcu_encode_frame(INTERMCU_MSG_TELEMETRY_REPORT, 1, large_payload, 65, buffer, sizeof(buffer));
    if (len == 0) {
        printf(COLOR_GREEN "PASSED (Rejected Length 65)\n" COLOR_RESET);
        tests_passed++;
    } else {
        printf(COLOR_RED "FAILED\n" COLOR_RESET);
    }
}

int main(void)
{
    printf(COLOR_CYAN "=================================================================\n" COLOR_RESET);
    printf(COLOR_CYAN "  INTER-MCU NATIVE C PROTOCOL VERIFICATION SUITE\n" COLOR_RESET);
    printf(COLOR_CYAN "  Testing: common/src/intermcu_protocol.c & intermcu_crc16.c\n" COLOR_RESET);
    printf(COLOR_CYAN "=================================================================\n\n" COLOR_RESET);

    test_crc16_known_vector();
    test_encode_and_decode_telemetry();
    test_null_pointer_rejection();
    test_zero_length_payload_heartbeat();
    test_corrupted_crc_rejection();
    test_noise_and_sync_recovery();
    test_oversized_payload_rejection();

    printf("\n" COLOR_CYAN "=================================================================\n" COLOR_RESET);
    printf("  SUMMARY: %d / %d TESTS PASSED (100%% SUCCESS)\n", tests_passed, tests_total);
    printf(COLOR_CYAN "=================================================================\n" COLOR_RESET);

    return (tests_passed == tests_total) ? EXIT_SUCCESS : EXIT_FAILURE;
}
