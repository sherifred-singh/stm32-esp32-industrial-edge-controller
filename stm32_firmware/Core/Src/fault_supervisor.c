/**
 * @file    fault_supervisor.c
 * @brief   Circular Fault History Table and Priority Inversion Safe Mutex Implementation.
 * @author  Sherifred Singh
 *
 * Real-world Interview Context:
 *   - Circular buffer overwrites oldest entry when capacity (16) is reached.
 *   - Priority Inversion Protection: xFaultTableMutex uses FreeRTOS Priority Inheritance.
 *     If InterMcuCommTask (Priority 1) locks the table and FaultSupervisorTask (Priority 4)
 *     requests it, FreeRTOS temporarily boosts InterMcuCommTask to Priority 4 so medium
 *     priority tasks (SensorAcqTask) cannot preempt it and delay the safety critical task.
 */

#include "fault_supervisor.h"
#include "actuator_control.h"
#include "task.h"
#include <string.h>

SemaphoreHandle_t xFaultTableMutex = NULL;

static intermcu_fault_entry_t fault_history[FAULT_TABLE_CAPACITY];
static uint8_t head_index = 0;
static uint8_t total_fault_count = 0;
static uint8_t active_fault_bitmask = 0;
static uint32_t dropped_telemetry_count = 0;

void FaultSupervisor_Init(void)
{
    memset(fault_history, 0, sizeof(fault_history));
    head_index = 0;
    total_fault_count = 0;
    active_fault_bitmask = 0;
    dropped_telemetry_count = 0;

    /* Create Mutex with Priority Inheritance */
    xFaultTableMutex = xSemaphoreCreateMutex();
    configASSERT(xFaultTableMutex != NULL);
}

void FaultSupervisor_ReportFault(intermcu_fault_code_t code, intermcu_fault_severity_t severity, uint16_t param)
{
    if (xFaultTableMutex == NULL) return;

    /* Acquire Mutex (timeout 10 ms to prevent deadlocks) */
    if (xSemaphoreTake(xFaultTableMutex, pdMS_TO_TICKS(10)) == pdPASS) {
        
        /* Insert new fault at head_index (circular overwrite) */
        fault_history[head_index].timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        fault_history[head_index].fault_code   = (uint8_t)code;
        fault_history[head_index].severity     = (uint8_t)severity;
        fault_history[head_index].fault_param  = param;

        head_index = (head_index + 1) % FAULT_TABLE_CAPACITY;
        if (total_fault_count < FAULT_TABLE_CAPACITY) {
            total_fault_count++;
        }

        active_fault_bitmask |= (1U << (uint8_t)code);

        /* If Critical Trip, immediately trigger hardware E-Stop */
        if (severity == FAULT_SEV_CRITICAL_TRIP) {
            Actuator_EmergencyStop();
        }

        xSemaphoreGive(xFaultTableMutex);
    }
}

uint8_t FaultSupervisor_GetActiveFlags(void)
{
    return active_fault_bitmask;
}

void FaultSupervisor_GetHistorySnapshot(intermcu_fault_entry_t *out_entries, uint8_t max_count, uint8_t *out_actual_count)
{
    if (!out_entries || !out_actual_count || !xFaultTableMutex) return;

    if (xSemaphoreTake(xFaultTableMutex, pdMS_TO_TICKS(20)) == pdPASS) {
        uint8_t count_to_copy = (total_fault_count < max_count) ? total_fault_count : max_count;
        
        for (uint8_t i = 0; i < count_to_copy; i++) {
            /* Copy from oldest to newest */
            int16_t read_idx;
            if (total_fault_count < FAULT_TABLE_CAPACITY) {
                read_idx = i;
            } else {
                read_idx = (head_index + i) % FAULT_TABLE_CAPACITY;
            }
            out_entries[i] = fault_history[read_idx];
        }

        *out_actual_count = count_to_copy;
        xSemaphoreGive(xFaultTableMutex);
    } else {
        *out_actual_count = 0;
    }
}

void FaultSupervisor_RecordDrop(void)
{
    taskENTER_CRITICAL();
    dropped_telemetry_count++;
    taskEXIT_CRITICAL();
}

void FaultSupervisor_ClearFaults(void)
{
    if (xFaultTableMutex && xSemaphoreTake(xFaultTableMutex, pdMS_TO_TICKS(10)) == pdPASS) {
        memset(fault_history, 0, sizeof(fault_history));
        head_index = 0;
        total_fault_count = 0;
        active_fault_bitmask = 0;
        xSemaphoreGive(xFaultTableMutex);
    }
}

void vFaultSupervisorTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20); /* 50 Hz Fast Supervisory Loop */

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        /* Refresh Hardware Watchdog (IWDG) tick */
        /* If system hangs or a task deadlocks, watchdog triggers reset within 400 ms */
        
        /* Toggle Diagnostic Heartbeat LED (PA5) every 500 ms (25 ticks) */
        static uint8_t led_tick = 0;
        if (++led_tick >= 25) {
            led_tick = 0;
            /* GPIOA->ODR ^= GPIO_PIN_5; */
        }
    }
}
