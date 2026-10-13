/**
 * @file    actuator_control.c
 * @brief   PWM & Relay Actuator Control Implementation with Safety Guardrails.
 * @author  Sherifred Singh
 */

#include "actuator_control.h"
#include "fault_supervisor.h"
#include "task.h"
#include <string.h>

QueueHandle_t xCommandQueue = NULL;

static volatile uint16_t current_pwm_duty = 0;  /* 0 to 1000 */
static volatile uint8_t  current_relays   = 0;  /* Bit 0: Relay 1, Bit 1: Relay 2 */
static volatile bool     is_estop_active  = false;

void ActuatorControl_Init(void)
{
    current_pwm_duty = 0;
    current_relays = 0;
    is_estop_active = false;

    /* Create Command Queue */
    xCommandQueue = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(intermcu_actuator_cmd_t));
    configASSERT(xCommandQueue != NULL);
}

/* Low-Level Hardware Control for TIM1 PWM & GPIOB Relays */
#if defined(STM32F446xx) || defined(__arm__) || defined(__ARM_ARCH)
#include "stm32f4xx.h"
#define HW_ACTUATOR_ESTOP() do { \
    TIM1->CCR1 = 0; \
    TIM1->BDTR &= ~TIM_BDTR_MOE; /* Kill PWM via Main Output Enable gate */ \
    GPIOB->BSRR = ((uint32_t)GPIO_PIN_0 | (uint32_t)GPIO_PIN_1) << 16; /* De-energize relays */ \
} while(0)
#define HW_ACTUATOR_SET_PWM(duty) do { \
    TIM1->CCR1 = (uint32_t)((TIM1->ARR * (duty)) / 1000); \
    TIM1->BDTR |= TIM_BDTR_MOE; \
} while(0)
#define HW_ACTUATOR_SET_RELAYS(mask) do { \
    if ((mask) & 0x01) GPIOB->BSRR = GPIO_PIN_0; else GPIOB->BSRR = (uint32_t)GPIO_PIN_0 << 16; \
    if ((mask) & 0x02) GPIOB->BSRR = GPIO_PIN_1; else GPIOB->BSRR = (uint32_t)GPIO_PIN_1 << 16; \
} while(0)
#else
#define HW_ACTUATOR_ESTOP()          do {} while(0)
#define HW_ACTUATOR_SET_PWM(duty)    do { (void)(duty); } while(0)
#define HW_ACTUATOR_SET_RELAYS(mask) do { (void)(mask); } while(0)
#endif

void Actuator_EmergencyStop(void)
{
    taskENTER_CRITICAL();
    is_estop_active = true;
    current_pwm_duty = 0;
    current_relays = 0;
    /* Physical peripheral-safe state execution */
    HW_ACTUATOR_ESTOP();
    taskEXIT_CRITICAL();
}

uint8_t Actuator_GetState(void)
{
    uint8_t state = 0;
    if (is_estop_active) return 0x80; /* Bit 7: E-Stop active */
    if (current_relays & 0x01) state |= 0x01;
    if (current_relays & 0x02) state |= 0x02;
    if (current_pwm_duty > 0)  state |= 0x04;
    return state;
}

void vActuatorControlTask(void *pvParameters)
{
    (void)pvParameters;
    intermcu_actuator_cmd_t cmd;

    for (;;) {
        /* Block until a command arrives from InterMcuCommTask */
        if (xQueueReceive(xCommandQueue, &cmd, portMAX_DELAY) == pdPASS) {
            
            /* If E-Stop is active, reject commands unless it is a fault reset */
            if (is_estop_active && cmd.command_id != 0x04) {
                continue;
            }

            switch (cmd.command_id) {
            case 0x01: /* Set PWM Speed */
                if (cmd.pwm_duty_permille <= 1000) {
                    current_pwm_duty = cmd.pwm_duty_permille;
                    HW_ACTUATOR_SET_PWM(current_pwm_duty);
                }
                break;

            case 0x02: /* Set Relays */
                current_relays = (cmd.relay_mask & 0x03);
                HW_ACTUATOR_SET_RELAYS(current_relays);
                break;

            case 0x03: /* Soft Emergency Stop Trigger */
                Actuator_EmergencyStop();
                FaultSupervisor_ReportFault(FAULT_NONE, FAULT_SEV_CRITICAL_TRIP, 0xEE);
                break;

            case 0x04: /* Reset Faults / Clear E-Stop */
                taskENTER_CRITICAL();
                is_estop_active = false;
                HW_ACTUATOR_SET_PWM(current_pwm_duty);
                HW_ACTUATOR_SET_RELAYS(current_relays);
                taskEXIT_CRITICAL();
                FaultSupervisor_ClearFaults();
                break;

            default:
                break;
            }
        }
    }
}
