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

void Actuator_EmergencyStop(void)
{
    taskENTER_CRITICAL();
    is_estop_active = true;
    current_pwm_duty = 0;
    current_relays = 0;
    /* Hardware register write: Set PWM compare register CCR1 = 0 and clear relay GPIOs */
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
                }
                break;

            case 0x02: /* Set Relays */
                current_relays = (cmd.relay_mask & 0x03);
                break;

            case 0x03: /* Soft Emergency Stop Trigger */
                Actuator_EmergencyStop();
                FaultSupervisor_ReportFault(FAULT_NONE, FAULT_SEV_CRITICAL_TRIP, 0xEE);
                break;

            case 0x04: /* Reset Faults / Clear E-Stop */
                taskENTER_CRITICAL();
                is_estop_active = false;
                taskEXIT_CRITICAL();
                FaultSupervisor_ClearFaults();
                break;

            default:
                break;
            }
        }
    }
}
