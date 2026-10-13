/**
 * @file    intermcu_crc16.h
 * @brief   Fast lookup table based CRC-16-CCITT calculation for the
 *          Inter-MCU communication protocol.
 * @author  Sherifred Singh
 *
 * Polynomial: 0x1021 (X^16 + X^12 + X^5 + 1)
 * Initial Value: 0xFFFF
 */

#ifndef INTERMCU_CRC16_H
#define INTERMCU_CRC16_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INTERMCU_CRC16_INITIAL_SEED  0xFFFFU

/**
 * @brief  Computes CRC-16-CCITT over a buffer starting from seed 0xFFFF.
 * @param  data: Pointer to byte array.
 * @param  length: Number of bytes to compute.
 * @retval 16-bit CRC value.
 */
uint16_t intermcu_crc16_calculate(const uint8_t *data, size_t length);

/**
 * @brief  Updates an ongoing CRC-16-CCITT with new data.
 * @param  current_crc: Previous CRC value.
 * @param  data: Pointer to byte array.
 * @param  length: Number of bytes.
 * @retval Updated 16-bit CRC value.
 */
uint16_t intermcu_crc16_update(uint16_t current_crc, const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* INTERMCU_CRC16_H */
