# FreeRTOS Concurrency & Real-Time Scheduling Architecture

This document details the multi-tasking architecture, synchronization primitives, and ISR safety guarantees implemented in the **STM32F446RE Real-Time Core**.

---

## 1. Task Hierarchy & Priority Allocation

FreeRTOS uses a **Fixed-Priority Preemptive Scheduling** algorithm with time-slicing enabled. Priorities range from `0` (Idle) to `4` (Highest Criticality):

| Task Name | Priority | Period / Wakeup Condition | Stack (Words) | Responsibility |
| :--- | :---: | :---: | :---: | :--- |
| **`vFaultSupervisorTask`** | **4 (Highest)** | Periodic: 20 ms (50 Hz) | 256 (1024 B) | Software watchdog refresh, thermal/vibration trip checks, E-Stop activation. |
| **`vActuatorControlTask`** | **3** | Event-Driven (`xCommandQueue`) | 256 (1024 B) | TIM1 PWM duty cycle adjustments, relay solenoid state transitions. |
| **`vSensorAcqTask`** | **2** | Periodic: 100 ms (10 Hz) | 256 (1024 B) | ADC1 circular DMA oversampling, temperature/vibration unit conversion. |
| **`vInterMcuCommTask`** | **1 (Lowest)** | Hybrid (Queue + IDLE ISR Notification) | 256 (1024 B) | Inter-MCU packet encoding, streaming parser, UART DMA dispatch. |

### Why this priority ordering is mathematically optimal (Rate Monotonic & Criticality Alignment):
1. **Safety First:** The `FaultSupervisorTask` must preempt all communication and sensor math to execute emergency stops in $< 1\,\text{ms}$ upon an over-temperature or brownout trip.
2. **Deterministic Control:** The `ActuatorControlTask` needs immediate responsiveness to commands without waiting for network serialization.
3. **Sensor Jitter Prevention:** The `SensorAcqTask` uses `vTaskDelayUntil()` to eliminate timing drift across 10 Hz sampling intervals.
4. **Communication Backgrounding:** `InterMcuCommTask` runs at Priority 1. Network transmission is inherently asynchronous and must never stall physical sensor sampling or safety monitoring.

---

## 2. Priority Inversion Mitigation (`xFaultTableMutex`)

### The Problem:
Consider three tasks:
* **Task H (High):** `vFaultSupervisorTask` (Priority 4)
* **Task M (Medium):** `vSensorAcqTask` (Priority 2)
* **Task L (Low):** `vInterMcuCommTask` (Priority 1)

If Task L acquires a lock on the circular fault table to prepare a telemetry snapshot, and is preempted by Task M (which has no interest in the lock), Task H will block when it attempts to acquire the lock. Task M can now execute indefinitely, indirectly blocking the highest-priority safety task. This classic flaw is **Unbounded Priority Inversion** (famous for causing the 1997 Mars Pathfinder system reset).

### The Solution:
`xFaultTableMutex` is created using `xSemaphoreCreateMutex()`, which enables **FreeRTOS Priority Inheritance**:
* When Task H attempts to take `xFaultTableMutex` while Task L holds it, FreeRTOS **temporarily elevates Task L's priority to 4**.
* Task M cannot preempt Task L while Task L finishes accessing the fault table.
* Once Task L releases the mutex, its priority drops back to 1, and Task H immediately unblocks and executes.

---

## 3. High-Speed Event Signaling: Direct Task Notifications vs. Semaphores

For waking up the `InterMcuCommTask` from the USART IDLE Line Interrupt:
* **Traditional Approach:** Binary Semaphore (`xSemaphoreGiveFromISR`).
* **Our Approach:** Direct Task Notification (`vTaskNotifyGiveFromISR`).

### Advantages:
1. **45% Faster Execution:** Direct task notifications modify the target task's TCB directly, bypassing queue registry lookups.
2. **Zero RAM Allocation:** No `QueueHandle_t` structure or semaphore control block is allocated.
3. **Interrupt Latency Reduction:** Significantly shorter ISR execution time in `USART2_IRQHandler`.

---

## 4. Stack Overflow Trapping & Production Guardrails

1. **Stack Overflow Hook (`configCHECK_FOR_STACK_OVERFLOW = 2`):**
   * FreeRTOS fills the stack with the canary pattern `0xA5A5A5A5`.
   * On context switch, the kernel checks the final 16 bytes of the stack. If any byte is altered, `vApplicationStackOverflowHook` traps the CPU immediately.
2. **Memory Allocation Hook (`configUSE_MALLOC_FAILED_HOOK = 1`):**
   * Traps if `pvPortMalloc` fails due to heap exhaustion.
