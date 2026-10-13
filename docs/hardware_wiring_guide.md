# Hardware Wiring & Inter-Board Connection Guide

This guide details the physical wiring and pin connections between the **STM32F446RE (Nucleo-64)** and the **ESP32-S3-DevKitC-1 v1.1**.

---

## 1. Inter-Board Pin Connections

Only **3 jumper wires** are required for full-duplex communication and ground reference:

| Signal Role | STM32F446RE Pin (Nucleo-64) | ESP32-S3-DevKitC-1 Pin | Notes |
| :--- | :---: | :---: | :--- |
| **STM32 TX $\to$ ESP32 RX** | **`PA2`** (Morpho CN10-Pin 35 / Arduino D1) | **`GPIO 18`** | 3.3V Logic Level Direct Connection |
| **STM32 RX $\leftarrow$ ESP32 TX** | **`PA3`** (Morpho CN10-Pin 37 / Arduino D0) | **`GPIO 17`** | 3.3V Logic Level Direct Connection |
| **Common Ground (GND)** | **`GND`** (CN11-Pin 9 or CN10-Pin 20) | **`GND`** | **MANDATORY:** Shared reference potential |

> [!IMPORTANT]
> **Logic Level Compatibility:** Both the STM32F446RE and ESP32-S3 operate at **3.3V VDD logic**. No level shifters or voltage dividers are needed between the UART lines.

---

## 2. Sensor & Actuator Test Points (STM32F446RE)

| Peripheral | Nucleo Pin | Physical Purpose / Test Harness Connection |
| :--- | :---: | :--- |
| **ADC1 Channel 0** | **`PA0`** (A0) | Analog Input 1: NTC Temperature Sensor or 10kΩ Potentiometer (0 to 3.3V). |
| **ADC1 Channel 1** | **`PA1`** (A1) | Analog Input 2: Vibration Accelerometer AC Signal or Function Generator. |
| **ADC1 Channel 4** | **`PA4`** (A2) | Analog Input 3: DC Bus Voltage divider monitor (scaled 0 to 3.3V). |
| **TIM1 PWM Output** | **`PA8`** (D7) | 20 kHz Motor Speed PWM. Connect to Oscilloscope / Logic Analyzer. |
| **Relay 1 Control** | **`PB0`** (A3) | Digital Output: Solenoid Relay 1 Driver. |
| **Relay 2 Control** | **`PB1`** (A6) | Digital Output: Solenoid Relay 2 Driver. |
| **Heartbeat LED** | **`PA5`** (D13) | On-board Green LED (`LD2`): Toggles every 500 ms when scheduler is active. |

---

## 3. Power Supply Recommendations

1. **Development / Lab Setup:**
   * Power the STM32 Nucleo via its onboard ST-LINK Mini-USB connector (powers ST-LINK + MCU).
   * Power the ESP32-S3 via its USB-to-UART Type-C connector.
   * Connect the **GND** pin of both boards together with a sturdy jumper wire.
2. **Standalone Bench Setup:**
   * Supply 5.0V regulated power to the 5V pin on both boards from a bench supply, sharing the common ground.
