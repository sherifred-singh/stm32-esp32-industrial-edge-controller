/**
 * @file    uart_dma_comm.h
 * @brief   USART2 DMA Circular Reception & IDLE Line Interrupt Driver for STM32F446RE.
 * @author  Sherifred Singh
 */

#ifndef UART_DMA_COMM_H
#define UART_DMA_COMM_H

#include "main.h"
#include "../../common/include/intermcu_protocol.h"
#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UART_RX_DMA_BUFFER_SIZE 256U
#define UART_TX_DMA_BUFFER_SIZE 128U

extern TaskHandle_t xInterMcuTaskHandle;

/**
 * @brief  Initializes USART2 at 115200/921600 baud with DMA1 Streams and IDLE line interrupt.
 */
void UartDmaComm_Init(void);

/**
 * @brief  FreeRTOS task responsible for Inter-MCU packet parsing and transmission.
 * @param  pvParameters: FreeRTOS task argument pointer.
 */
void vInterMcuCommTask(void *pvParameters);

/**
 * @brief  ISR callback executed on USART2 IDLE line detection.
 */
void UartDmaComm_IdleLineISR(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_DMA_COMM_H */
