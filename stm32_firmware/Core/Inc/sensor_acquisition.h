/**
 * @file    sensor_acquisition.h
 * @brief   Multi-channel ADC + DMA Sensor Acquisition Module for STM32F446RE.
 * @author  Sherifred Singh
 */

#ifndef SENSOR_ACQUISITION_H
#define SENSOR_ACQUISITION_H

#include "main.h"
#include "../../common/include/intermcu_protocol.h"
#include "FreeRTOS.h"
#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Global Queue Handle for outgoing telemetry frames */
extern QueueHandle_t xTelemetryQueue;

/**
 * @brief  Initializes ADC1, GPIO analog pins, DMA2 Stream 0, and FreeRTOS queues.
 */
void SensorAcq_Init(void);

/**
 * @brief  FreeRTOS task executing deterministic sensor acquisition at 10 Hz.
 * @param  pvParameters: FreeRTOS task argument pointer.
 */
void vSensorAcqTask(void *pvParameters);

/**
 * @brief  Retrieves latest snapshot of raw filtered sensor readings.
 * @param  out_data: Destination pointer for telemetry payload.
 */
void SensorAcq_GetLatestData(intermcu_telemetry_payload_t *out_data);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_ACQUISITION_H */
