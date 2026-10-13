/**
 * @file    fault_supervisor.h
 * @brief   16-Entry Circular Fault Supervisor Table with Mutex Protection.
 * @author  Sherifred Singh
 */

#ifndef FAULT_SUPERVISOR_H
#define FAULT_SUPERVISOR_H

#include "main.h"
#include "../../common/include/intermcu_protocol.h"
#include "FreeRTOS.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FAULT_TABLE_CAPACITY 16U

/* Global Mutex with Priority Inheritance protecting the Fault Table */
extern SemaphoreHandle_t xFaultTableMutex;

/**
 * @brief  Initializes the circular fault table and creates the priority inheritance mutex.
 */
void FaultSupervisor_Init(void);

/**
 * @brief  FreeRTOS task executing periodic supervisory health checks at 50 Hz.
 * @param  pvParameters: FreeRTOS task argument pointer.
 */
void vFaultSupervisorTask(void *pvParameters);

/**
 * @brief  Logs a new fault entry into the circular table with automatic oldest overwrite.
 *         Protected by xFaultTableMutex.
 * @param  code: Fault type identifier.
 * @param  severity: Info, Warning, or Critical Trip.
 * @param  param: Offending measurement value.
 */
void FaultSupervisor_ReportFault(intermcu_fault_code_t code, intermcu_fault_severity_t severity, uint16_t param);

/**
 * @brief  Returns active fault bitmask.
 */
uint8_t FaultSupervisor_GetActiveFlags(void);

/**
 * @brief  Safely extracts a snapshot of the fault table for telemetry reporting.
 * @param  out_entries: Buffer for up to FAULT_TABLE_CAPACITY entries.
 * @param  max_count: Maximum entries buffer can receive.
 * @param  out_actual_count: Number of entries copied.
 */
void FaultSupervisor_GetHistorySnapshot(intermcu_fault_entry_t *out_entries, uint8_t max_count, uint8_t *out_actual_count);

/**
 * @brief  Increments telemetry queue dropped counter.
 */
void FaultSupervisor_RecordDrop(void);

/**
 * @brief  Clears logged faults and resets active flags.
 */
void FaultSupervisor_ClearFaults(void);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_SUPERVISOR_H */
