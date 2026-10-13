/**
 * @file    actuator_control.h
 * @brief   PWM & Relay Actuator Control Task Interface for STM32F446RE.
 * @author  Sherifred Singh
 */

#ifndef ACTUATOR_CONTROL_H
#define ACTUATOR_CONTROL_H

#include "main.h"
#include "../../common/include/intermcu_protocol.h"
#include "FreeRTOS.h"
#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

extern QueueHandle_t xCommandQueue;

/**
 * @brief  Initializes TIM1 PWM peripheral, Relay GPIO pins, and command queue.
 */
void ActuatorControl_Init(void);

/**
 * @brief  FreeRTOS task executing actuator state machine and command processing.
 * @param  pvParameters: FreeRTOS task argument pointer.
 */
void vActuatorControlTask(void *pvParameters);

/**
 * @brief  Immediate hardware Emergency Stop (< 1 ms latency).
 *         Disables PWM and opens safety relays.
 */
void Actuator_EmergencyStop(void);

/**
 * @brief  Retrieves active actuator state byte (Bit 0: Relay 1, Bit 1: Relay 2, Bit 2: PWM active).
 */
uint8_t Actuator_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* ACTUATOR_CONTROL_H */
