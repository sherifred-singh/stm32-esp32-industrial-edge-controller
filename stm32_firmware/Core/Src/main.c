/**
 * @file    main.c
 * @brief   Main Entry Point and Task Initialization for STM32F446RE Edge Controller.
 * @author  Sherifred Singh
 */

#include "main.h"
#include "sensor_acquisition.h"
#include "actuator_control.h"
#include "fault_supervisor.h"
#include "uart_dma_comm.h"
#include "FreeRTOS.h"
#include "task.h"

uint32_t SystemCoreClock = SYSTEM_CLOCK_HZ;

void SystemClock_Config(void)
{
    /* In actual hardware: Configures 8 MHz HSE, PLL_M=8, PLL_N=360, PLL_P=2 to achieve 180 MHz SYSCLK */
    /* Flash latency set to 5 wait states for 180 MHz at 3.3V */
}

void Error_Handler(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {
        /* Trap in endless loop for hardware debugger breakpoint */
    }
}

/* FreeRTOS Hook: Traps Stack Overflows */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    /* Record critical fault in memory before halt */
    taskDISABLE_INTERRUPTS();
    for (;;);
}

/* FreeRTOS Hook: Traps Memory Allocation Failures */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;);
}

int main(void)
{
    /* 1. Configure System Clocks */
    SystemClock_Config();

    /* 2. Initialize Subsystem Modules */
    FaultSupervisor_Init();
    ActuatorControl_Init();
    SensorAcq_Init();
    UartDmaComm_Init();

    /* 3. Create FreeRTOS Tasks with Explicit Prioritization */
    BaseType_t ret;

    ret = xTaskCreate(vFaultSupervisorTask,
                      "FaultSuper",
                      STACK_SIZE_FAULT_SUPERVISOR,
                      NULL,
                      PRIORITY_FAULT_SUPERVISOR,
                      NULL);
    configASSERT(ret == pdPASS);

    ret = xTaskCreate(vActuatorControlTask,
                      "ActuatorCtrl",
                      STACK_SIZE_ACTUATOR_CTRL,
                      NULL,
                      PRIORITY_ACTUATOR_CTRL,
                      NULL);
    configASSERT(ret == pdPASS);

    ret = xTaskCreate(vSensorAcqTask,
                      "SensorAcq",
                      STACK_SIZE_SENSOR_ACQ,
                      NULL,
                      PRIORITY_SENSOR_ACQ,
                      NULL);
    configASSERT(ret == pdPASS);

    ret = xTaskCreate(vInterMcuCommTask,
                      "InterMcuComm",
                      STACK_SIZE_INTERMCU_COMM,
                      NULL,
                      PRIORITY_INTERMCU_COMM,
                      &xInterMcuTaskHandle);
    configASSERT(ret == pdPASS);

    /* 4. Start the FreeRTOS Real-Time Scheduler */
    vTaskStartScheduler();

    /* Should never reach here unless heap is exhausted */
    for (;;);
    return 0;
}
