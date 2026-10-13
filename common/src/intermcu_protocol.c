/**
 * @file    intermcu_protocol.c
 * @brief   Streaming frame parser and packet serializer for the Inter-MCU protocol.
 * @author  Sherifred Singh
 */

#include "intermcu_protocol.h"
#include "intermcu_crc16.h"
#include <string.h>

void intermcu_parser_init(intermcu_parser_t *parser)
{
    if (!parser) return;
    memset(parser, 0, sizeof(intermcu_parser_t));
    parser->state = PARSER_STATE_WAIT_SYNC_1;
}

size_t intermcu_encode_frame(uint8_t msg_type,
                             uint8_t seq_num,
                             const void *payload,
                             uint8_t payload_len,
                             uint8_t *out_buffer,
                             size_t max_buffer_len)
{
    if (!out_buffer) return 0;
    if (payload_len > INTERMCU_MAX_PAYLOAD_SIZE) return 0;

    size_t total_size = INTERMCU_HEADER_SIZE + payload_len + INTERMCU_CRC_SIZE;
    if (max_buffer_len < total_size) return 0;

    /* Write Sync Header */
    out_buffer[0] = INTERMCU_SYNC_BYTE_1;
    out_buffer[1] = INTERMCU_SYNC_BYTE_2;
    out_buffer[2] = payload_len;
    out_buffer[3] = msg_type;
    out_buffer[4] = seq_num;

    /* Write Payload */
    if (payload_len > 0 && payload != NULL) {
        memcpy(&out_buffer[5], payload, payload_len);
    }

    /* Compute CRC-16 over LEN, MSG_TYPE, SEQ, and PAYLOAD (Offset 2 to 4+payload_len) */
    size_t crc_payload_len = 3 + payload_len;
    uint16_t calculated_crc = intermcu_crc16_calculate(&out_buffer[2], crc_payload_len);

    /* Append CRC-16 Big-Endian (MSB first) */
    out_buffer[5 + payload_len] = (uint8_t)((calculated_crc >> 8) & 0xFF);
    out_buffer[6 + payload_len] = (uint8_t)(calculated_crc & 0xFF);

    return total_size;
}

bool intermcu_parser_process_byte(intermcu_parser_t *parser, uint8_t byte, intermcu_frame_t *out_frame)
{
    if (!parser) return false;

    switch (parser->state) {
    case PARSER_STATE_WAIT_SYNC_1:
        if (byte == INTERMCU_SYNC_BYTE_1) {
            parser->state = PARSER_STATE_WAIT_SYNC_2;
        } else {
            parser->dropped_byte_count++;
        }
        break;

    case PARSER_STATE_WAIT_SYNC_2:
        if (byte == INTERMCU_SYNC_BYTE_2) {
            parser->state = PARSER_STATE_GET_LEN;
        } else if (byte == INTERMCU_SYNC_BYTE_1) {
            /* Handle consecutive 0xAA 0xAA 0x55 safely */
            parser->state = PARSER_STATE_WAIT_SYNC_2;
        } else {
            parser->state = PARSER_STATE_WAIT_SYNC_1;
            parser->dropped_byte_count++;
        }
        break;

    case PARSER_STATE_GET_LEN:
        if (byte <= INTERMCU_MAX_PAYLOAD_SIZE) {
            parser->payload_len = byte;
            parser->current_frame.payload_len = byte;
            parser->payload_index = 0;
            parser->state = PARSER_STATE_GET_TYPE;
        } else {
            /* Length exceeds max allowed payload, framing error */
            parser->state = PARSER_STATE_WAIT_SYNC_1;
            parser->dropped_byte_count++;
        }
        break;

    case PARSER_STATE_GET_TYPE:
        parser->current_frame.msg_type = byte;
        parser->state = PARSER_STATE_GET_SEQ;
        break;

    case PARSER_STATE_GET_SEQ:
        parser->current_frame.seq_num = byte;
        if (parser->payload_len > 0) {
            parser->state = PARSER_STATE_GET_PAYLOAD;
        } else {
            parser->state = PARSER_STATE_GET_CRC_MSB;
        }
        break;

    case PARSER_STATE_GET_PAYLOAD:
        parser->current_frame.payload[parser->payload_index++] = byte;
        if (parser->payload_index >= parser->payload_len) {
            parser->state = PARSER_STATE_GET_CRC_MSB;
        }
        break;

    case PARSER_STATE_GET_CRC_MSB:
        parser->rx_crc = ((uint16_t)byte) << 8;
        parser->state = PARSER_STATE_GET_CRC_LSB;
        break;

    case PARSER_STATE_GET_CRC_LSB:
        parser->rx_crc |= (uint16_t)byte;
        parser->current_frame.crc16 = parser->rx_crc;

        /* Verify CRC-16 over LEN, TYPE, SEQ, and PAYLOAD */
        uint8_t check_buf[INTERMCU_MAX_PAYLOAD_SIZE + 3];
        check_buf[0] = parser->current_frame.payload_len;
        check_buf[1] = parser->current_frame.msg_type;
        check_buf[2] = parser->current_frame.seq_num;
        if (parser->payload_len > 0) {
            memcpy(&check_buf[3], parser->current_frame.payload, parser->payload_len);
        }

        uint16_t computed_crc = intermcu_crc16_calculate(check_buf, 3 + parser->payload_len);

        if (computed_crc == parser->rx_crc) {
            parser->rx_frame_count++;
            if (out_frame != NULL) {
                *out_frame = parser->current_frame;
            }
            parser->state = PARSER_STATE_WAIT_SYNC_1;
            return true;
        } else {
            parser->crc_error_count++;
            parser->state = PARSER_STATE_WAIT_SYNC_1;
            return false;
        }

    default:
        parser->state = PARSER_STATE_WAIT_SYNC_1;
        break;
    }

    return false;
}
