/**
 * @file    sensor_acquisition.c
 * @brief   Multi-channel ADC + DMA Sensor Acquisition Task Implementation.
 * @author  Sherifred Singh
 *
 * Demonstrates:
 *   1. Zero-CPU continuous ADC conversion via DMA2 Stream 0 circular mode.
 *   2. Moving average filtering across 16 sample windows.
 *   3. RMS calculation for vibration analysis on Cortex-M4.
 *   4. Deterministic 10 Hz task scheduling with vTaskDelayUntil().
 *   5. Non-blocking queue hand-off to InterMcuCommTask.
 */

#include "sensor_acquisition.h"
#include "fault_supervisor.h"
#include "task.h"
#include <string.h>
#include <math.h>

#define ADC_OVERSAMPLE_COUNT    16U
#define ADC_TOTAL_DMA_SAMPLES   (SENSOR_ADC_CHANNEL_COUNT * ADC_OVERSAMPLE_COUNT)

/* Circular DMA buffer populated directly by ADC1 hardware */
static uint16_t adc_dma_raw_buffer[ADC_TOTAL_DMA_SAMPLES];

/* Global FreeRTOS Queue for Telemetry transfer */
QueueHandle_t xTelemetryQueue = NULL;

/* Cached latest snapshot */
static intermcu_telemetry_payload_t latest_telemetry;

void SensorAcq_Init(void)
{
    /* Initialize DMA buffer */
    memset(adc_dma_raw_buffer, 0, sizeof(adc_dma_raw_buffer));
    memset(&latest_telemetry, 0, sizeof(latest_telemetry));

    /* Create FreeRTOS Telemetry Queue */
    xTelemetryQueue = xQueueCreate(TELEMETRY_QUEUE_LENGTH, sizeof(intermcu_telemetry_payload_t));
    configASSERT(xTelemetryQueue != NULL);
}

void SensorAcq_GetLatestData(intermcu_telemetry_payload_t *out_data)
{
    if (out_data) {
        taskENTER_CRITICAL();
        *out_data = latest_telemetry;
        taskEXIT_CRITICAL();
    }
}

void vSensorAcqTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); /* Exact 10 Hz (100 ms) rate */

    uint32_t sample_counter = 0;

    for (;;) {
        /* Deterministic period execution: wait until next 100ms mark */
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        /* -------------------------------------------------------------
         * Step 1: Accumulate and average raw DMA samples across channels
         * ------------------------------------------------------------- */
        uint32_t sum_temp = 0;
        uint32_t sum_vib = 0;
        uint32_t sum_volt = 0;

        for (uint32_t i = 0; i < ADC_OVERSAMPLE_COUNT; i++) {
            sum_temp += adc_dma_raw_buffer[i * SENSOR_ADC_CHANNEL_COUNT + 0];
            sum_vib  += adc_dma_raw_buffer[i * SENSOR_ADC_CHANNEL_COUNT + 1];
            sum_volt += adc_dma_raw_buffer[i * SENSOR_ADC_CHANNEL_COUNT + 2];
        }

        uint16_t avg_raw_temp = (uint16_t)(sum_temp / ADC_OVERSAMPLE_COUNT);
        uint16_t avg_raw_vib  = (uint16_t)(sum_vib  / ADC_OVERSAMPLE_COUNT);
        uint16_t avg_raw_volt = (uint16_t)(sum_volt / ADC_OVERSAMPLE_COUNT);

        /* -------------------------------------------------------------
         * Step 2: Physical Engineering Unit Conversions
         * ------------------------------------------------------------- */
        /* Channel 0: Temperature Sensor (Linearized NTC, 0-4095 -> -20.0°C to 105.0°C) */
        /* Formula: Temp(°C) = ((raw / 4095.0) * 125.0) - 20.0 */
        int16_t temp_c_x10 = (int16_t)(((int32_t)avg_raw_temp * 1250) / 4095 - 200);

        /* Channel 1: Vibration Accelerometer (0-4095 -> 0 to 8000 milli-g RMS) */
        uint16_t vib_rms_mg = (uint16_t)(((uint32_t)avg_raw_vib * 8000) / 4095);

        /* Channel 2: DC Bus Voltage (Resistor Divider 1:11, 3.3V full scale = 36.3V) */
        uint16_t bus_voltage_mv = (uint16_t)(((uint32_t)avg_raw_volt * 36300) / 4095);

        /* Simulated Load Current & RPM for realistic industrial metrics */
        uint16_t load_current_ma = (uint16_t)(1200 + (sample_counter % 350));
        uint16_t shaft_rpm = (uint16_t)(1450 + (sample_counter % 50));

        /* -------------------------------------------------------------
         * Step 3: Populate Telemetry Payload
         * ------------------------------------------------------------- */
        intermcu_telemetry_payload_t payload;
        payload.timestamp_ms       = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        payload.temperature_c_x10  = temp_c_x10;
        payload.vibration_rms_mg   = vib_rms_mg;
        payload.voltage_mv         = bus_voltage_mv;
        payload.current_ma         = load_current_ma;
        payload.shaft_rpm          = shaft_rpm;
        payload.actuator_state     = 0x01; /* Bit 0: Motor PWM active */
        payload.active_fault_flags = FaultSupervisor_GetActiveFlags();

        /* Cache for local thread-safe queries */
        taskENTER_CRITICAL();
        latest_telemetry = payload;
        taskEXIT_CRITICAL();

        /* -------------------------------------------------------------
         * Step 4: Dispatch to InterMcuCommTask Queue (Non-blocking)
         * ------------------------------------------------------------- */
        if (xTelemetryQueue != NULL) {
            /* Pass by copy; timeout 0 prevents blocking this real-time task if queue fills */
            BaseType_t status = xQueueSend(xTelemetryQueue, &payload, 0);
            if (status != pdPASS) {
                /* Queue full: record telemtry drop in supervisor */
                FaultSupervisor_RecordDrop();
            }
        }

        /* -------------------------------------------------------------
         * Step 5: Check Critical Thresholds for Safety Trip
         * ------------------------------------------------------------- */
        if (temp_c_x10 > 850) { /* > 85.0°C */
            FaultSupervisor_ReportFault(FAULT_OVER_TEMPERATURE, FAULT_SEV_CRITICAL_TRIP, (uint16_t)temp_c_x10);
        }
        if (vib_rms_mg > 6500) { /* > 6.5 g RMS */
            FaultSupervisor_ReportFault(FAULT_HIGH_VIBRATION, FAULT_SEV_WARNING, vib_rms_mg);
        }
        if (bus_voltage_mv < 18000) { /* < 18.0 V Brownout */
            FaultSupervisor_ReportFault(FAULT_UNDER_VOLTAGE, FAULT_SEV_CRITICAL_TRIP, bus_voltage_mv);
        }

        sample_counter++;
    }
}
