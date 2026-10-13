/**
 * @file    main.h
 * @brief   System configuration and hardware pin mappings for STM32F446RE
 *          Industrial Edge Controller.
 * @author  Sherifred Singh
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* System Clock Frequency */
#define SYSTEM_CLOCK_HZ             180000000U  /* 180 MHz Core Clock */

/* Pin Allocations for Nucleo-F446RE */
/* USART2: Inter-MCU Communication to ESP32-S3 */
#define USART2_TX_PIN               GPIO_PIN_2   /* PA2 -> ESP32-S3 GPIO18 (RX) */
#define USART2_RX_PIN               GPIO_PIN_3   /* PA3 -> ESP32-S3 GPIO17 (TX) */
#define USART2_GPIO_PORT            GPIOA

/* ADC1 Inputs: Analog Sensor Signals */
#define SENSOR_ADC_CHANNEL_COUNT    3U
#define ADC_TEMP_CHANNEL_PIN        GPIO_PIN_0   /* PA0: Temp NTC */
#define ADC_VIB_CHANNEL_PIN         GPIO_PIN_1   /* PA1: Vibration Sensor */
#define ADC_VOLT_CHANNEL_PIN        GPIO_PIN_4   /* PA4: DC Bus Voltage */
#define ADC_GPIO_PORT               GPIOA

/* TIM1 PWM Output: Actuator Motor Speed */
#define PWM_OUTPUT_PIN              GPIO_PIN_8   /* PA8: TIM1_CH1 */
#define PWM_GPIO_PORT               GPIOA
#define PWM_FREQUENCY_HZ            20000U       /* 20 kHz Inaudible Industrial PWM */

/* Digital Actuator Outputs: Relays */
#define RELAY1_PIN                  GPIO_PIN_0   /* PB0: Solenoid Relay 1 */
#define RELAY2_PIN                  GPIO_PIN_1   /* PB1: Solenoid Relay 2 */
#define RELAY_GPIO_PORT             GPIOB

/* Diagnostic Status LEDs */
#define HEARTBEAT_LED_PIN           GPIO_PIN_5   /* PA5: Onboard Green LED (LD2) */
#define LED_GPIO_PORT               GPIOA

/* FreeRTOS Priority Definitions (0 = Lowest, 4 = Highest) */
#define PRIORITY_INTERMCU_COMM      1
#define PRIORITY_SENSOR_ACQ         2
#define PRIORITY_ACTUATOR_CTRL      3
#define PRIORITY_FAULT_SUPERVISOR   4

/* FreeRTOS Stack Allocations (Words: 1 word = 4 bytes) */
#define STACK_SIZE_INTERMCU_COMM    256
#define STACK_SIZE_SENSOR_ACQ       256
#define STACK_SIZE_ACTUATOR_CTRL    256
#define STACK_SIZE_FAULT_SUPERVISOR 256

/* Queue Sizes */
#define TELEMETRY_QUEUE_LENGTH      4
#define COMMAND_QUEUE_LENGTH        4

/* System Error Codes */
typedef enum {
    SYS_OK = 0,
    SYS_ERR_INIT,
    SYS_ERR_TIMEOUT,
    SYS_ERR_RESOURCE
} sys_error_t;

/* Global Function Prototypes */
void SystemClock_Config(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
