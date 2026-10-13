/**
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS Kernel Configuration for STM32F446RE (ARM Cortex-M4 @ 180MHz).
 * @author  Sherifred Singh
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* Ensure stdint.h types are used */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
    extern uint32_t SystemCoreClock;
#endif

#define configUSE_PREEMPTION                     1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1
#define configUSE_TICKLESS_IDLE                  0
#define configCPU_CLOCK_HZ                       (SystemCoreClock)
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     (5)
#define configMINIMAL_STACK_SIZE                 ((uint16_t)128)
#define configMAX_TASK_NAME_LEN                  (16)
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1

/* Concurrency & Synchronization Primitives */
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configQUEUE_REGISTRY_SIZE                8
#define configUSE_QUEUE_SETS                     0
#define configUSE_TIME_SLICING                   1
#define configUSE_NEWLIB_REENTRANT               0

/* Memory Allocation */
#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configTOTAL_HEAP_SIZE                    ((size_t)(40 * 1024))
#define configAPPLICATION_ALLOCATED_HEAP         0

/* Hook & Debugging Functions */
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCHECK_FOR_STACK_OVERFLOW           2  /* Method 2: Pattern checking (0xA5) */
#define configUSE_MALLOC_FAILED_HOOK             1
#define configUSE_DAEMON_TASK_STARTUP_HOOK       0
#define configGENERATE_RUN_TIME_STATS            0

/* Software Timer Configuration */
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                (3)
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             (configMINIMAL_STACK_SIZE * 2)

/* Cortex-M4 Specific Interrupt Priority Settings */
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS                      __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS                      4  /* 15 Priority Levels on STM32F4 */
#endif

/* Lowest interrupt priority (15) */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15

/* Maximum interrupt priority from which FreeRTOS API calls can be made */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configKERNEL_INTERRUPT_PRIORITY          (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Critical Assert Macro for Production Safety */
#define configASSERT(x) if ((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }

/* Definitions that map FreeRTOS port handlers */
#define vPortSVCHandler                          SVC_Handler
#define xPortPendSVHandler                       PendSV_Handler
#define xPortSysTickHandler                      SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
