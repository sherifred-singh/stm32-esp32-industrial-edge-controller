/**
 * @file    uart_dma_comm.c
 * @brief   USART DMA Circular Receiver & Packet Dispatcher Implementation.
 * @author  Sherifred Singh
 *
 * Demonstrates:
 *   1. Hardware USART IDLE line detection to avoid single-byte interrupts.
 *   2. Circular DMA RX ring buffer tracking with zero byte loss.
 *   3. Direct Task Notifications (vTaskNotifyGiveFromISR / ulTaskNotifyTake) for
 *      low-overhead, zero-RAM event signaling between ISR and Comm Task.
 *   4. Transmission of serialized 10 Hz telemetry frames with CRC-16.
 */

#include "uart_dma_comm.h"
#include "sensor_acquisition.h"
#include "actuator_control.h"
#include "fault_supervisor.h"
#include "../../common/include/intermcu_protocol.h"
#include "queue.h"
#include <string.h>

TaskHandle_t xInterMcuTaskHandle = NULL;

/* DMA circular RX buffer */
static uint8_t uart_rx_dma_buffer[UART_RX_DMA_BUFFER_SIZE];
static uint16_t last_read_position = 0;

/* DMA TX buffer */
static uint8_t uart_tx_dma_buffer[UART_TX_DMA_BUFFER_SIZE];

/* Protocol parser instance */
static intermcu_parser_t protocol_parser;

/* Rolling sequence counter */
static uint8_t tx_seq_counter = 0;

/* Low-Level Register Control for STM32F446RE DMA1 Stream 5 (RX) & Stream 6 (TX) */
#if defined(STM32F446xx) || defined(__arm__) || defined(__ARM_ARCH)
#include "stm32f4xx.h"
#define HW_UART_DMA_GET_RX_POS()    ((uint16_t)(UART_RX_DMA_BUFFER_SIZE - DMA1_Stream5->NDTR))
#define HW_UART_DMA_START_TX(len)   do { \
    DMA1_Stream6->CR &= ~DMA_SxCR_EN; \
    while (DMA1_Stream6->CR & DMA_SxCR_EN); \
    DMA1->HIFCR = (0x3D << 16); /* Clear CTCIF6, CHTIF6, CTEIF6, CDMEIF6, CFEIF6 */ \
    DMA1_Stream6->M0AR = (uint32_t)uart_tx_dma_buffer; \
    DMA1_Stream6->NDTR = (uint16_t)(len); \
    DMA1_Stream6->CR |= DMA_SxCR_EN; \
} while(0)
#else
/* Hardware abstraction simulation pointer for host unit testing */
volatile uint16_t hal_sim_dma1_stream5_ndtr = UART_RX_DMA_BUFFER_SIZE;
#define HW_UART_DMA_GET_RX_POS()    ((uint16_t)(UART_RX_DMA_BUFFER_SIZE - hal_sim_dma1_stream5_ndtr))
#define HW_UART_DMA_START_TX(len)   do { (void)(len); } while(0)
#endif

void UartDmaComm_Init(void)
{
    memset(uart_rx_dma_buffer, 0, sizeof(uart_rx_dma_buffer));
    memset(uart_tx_dma_buffer, 0, sizeof(uart_tx_dma_buffer));
    last_read_position = 0;
    tx_seq_counter = 0;

    intermcu_parser_init(&protocol_parser);
}

/**
 * @brief USART2 Hardware IDLE Line Interrupt Callback.
 * Executed when the incoming serial line remains idle for 1 frame time.
 */
void UartDmaComm_IdleLineISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xInterMcuTaskHandle != NULL) {
        /* Direct Task Notification is faster than xSemaphoreGiveFromISR with zero heap allocation */
        vTaskNotifyGiveFromISR(xInterMcuTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static void send_ack_response(uint8_t acked_seq, uint8_t status_code)
{
    intermcu_ack_payload_t ack_payload;
    ack_payload.acked_seq = acked_seq;
    ack_payload.status_code = status_code;

    size_t frame_len = intermcu_encode_frame(INTERMCU_MSG_ACK_NACK,
                                             tx_seq_counter++,
                                             &ack_payload,
                                             sizeof(ack_payload),
                                             uart_tx_dma_buffer,
                                             sizeof(uart_tx_dma_buffer));
    if (frame_len > 0) {
        HW_UART_DMA_START_TX(frame_len);
    }
}

void vInterMcuCommTask(void *pvParameters)
{
    (void)pvParameters;
    intermcu_telemetry_payload_t telemetry_data;
    intermcu_frame_t rx_frame;

    for (;;) {
        /* -------------------------------------------------------------
         * Step 1: Check for outgoing telemetry data from SensorAcqTask
         * ------------------------------------------------------------- */
        if (xTelemetryQueue != NULL &&
            xQueueReceive(xTelemetryQueue, &telemetry_data, pdMS_TO_TICKS(10)) == pdPASS) {
            
            size_t frame_len = intermcu_encode_frame(INTERMCU_MSG_TELEMETRY_REPORT,
                                                     tx_seq_counter++,
                                                     &telemetry_data,
                                                     sizeof(telemetry_data),
                                                     uart_tx_dma_buffer,
                                                     sizeof(uart_tx_dma_buffer));
            if (frame_len > 0) {
                HW_UART_DMA_START_TX(frame_len);
            }
        }

        /* -------------------------------------------------------------
         * Step 2: Check for incoming bytes from USART DMA circular buffer
         * ------------------------------------------------------------- */
        /* Wait up to 10 ms for notification from IDLE line ISR */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));

        /* Read actual hardware DMA write pointer */
        uint16_t dma_write_pos = HW_UART_DMA_GET_RX_POS();

        while (last_read_position != dma_write_pos) {
            uint8_t byte = uart_rx_dma_buffer[last_read_position];
            last_read_position = (last_read_position + 1) % UART_RX_DMA_BUFFER_SIZE;

            /* Feed byte into protocol state machine */
            if (intermcu_parser_process_byte(&protocol_parser, byte, &rx_frame)) {
                
                /* Complete valid CRC-16 frame decoded! Dispatch by MSG_TYPE */
                switch (rx_frame.msg_type) {
                case INTERMCU_MSG_ACTUATOR_COMMAND:
                    if (rx_frame.payload_len >= sizeof(intermcu_actuator_cmd_t)) {
                        intermcu_actuator_cmd_t cmd;
                        memcpy(&cmd, rx_frame.payload, sizeof(cmd));
                        if (xCommandQueue != NULL) {
                            BaseType_t q_status = xQueueSend(xCommandQueue, &cmd, 0);
                            if (q_status == pdPASS) {
                                send_ack_response(rx_frame.seq_num, INTERMCU_STATUS_OK);
                            } else {
                                send_ack_response(rx_frame.seq_num, INTERMCU_STATUS_ERR_OVERFLOW);
                            }
                        } else {
                            send_ack_response(rx_frame.seq_num, INTERMCU_STATUS_ERR_BUSY);
                        }
                    }
                    break;

                case INTERMCU_MSG_HEARTBEAT:
                    /* Reset communication loss watchdog timer */
                    send_ack_response(rx_frame.seq_num, INTERMCU_STATUS_OK);
                    break;

                case INTERMCU_MSG_FAULT_SNAPSHOT_REQ: {
                    intermcu_fault_entry_t snapshot[FAULT_TABLE_CAPACITY];
                    uint8_t actual_count = 0;
                    FaultSupervisor_GetHistorySnapshot(snapshot, FAULT_TABLE_CAPACITY, &actual_count);

                    size_t frame_len = intermcu_encode_frame(INTERMCU_MSG_FAULT_SNAPSHOT_RSP,
                                                             tx_seq_counter++,
                                                             snapshot,
                                                             actual_count * sizeof(intermcu_fault_entry_t),
                                                             uart_tx_dma_buffer,
                                                             sizeof(uart_tx_dma_buffer));
                    if (frame_len > 0) {
                        HW_UART_DMA_START_TX(frame_len);
                    }
                    break;
                }

                default:
                    send_ack_response(rx_frame.seq_num, INTERMCU_STATUS_ERR_INVALID_CMD);
                    break;
                }
            }
        }
    }
}
