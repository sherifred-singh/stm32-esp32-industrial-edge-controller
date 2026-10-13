# Inter-MCU Binary Packet Protocol Specification

## 1. Protocol Architecture & Overview

The **Inter-MCU Binary Packet Protocol** defines a full-duplex, low-latency, point-to-point data link layer between the **STM32F446RE** (Real-Time Control Core) and the **ESP32-S3** (Wireless Connectivity & Edge Gateway).

### Design Goals:
1. **Zero-Copy DMA Streaming:** Framing enables direct circular DMA ring buffer reception without per-byte interrupt overhead.
2. **Determinism & Low Overhead:** Fixed 5-byte header, 2-byte CRC-16, and up to 64 bytes payload ($< 10\%$ protocol overhead).
3. **Data Integrity Guarantee:** CRC-16-CCITT with polynomial `0x1021` detects all single-bit, double-bit, and burst transmission errors.
4. **Noise & Framing Resilience:** Predefined 2-byte sync preamble (`0xAA 0x55`) and payload length boundary guards allow immediate resynchronization upon line noise or partial packets.

---

## 2. Frame Structure

```
+---------------+---------------+---------------+---------------+---------------+-------------------+---------------+---------------+
|  SYNC_BYTE_1  |  SYNC_BYTE_2  |  PAYLOAD_LEN  |   MSG_TYPE    |    SEQ_NUM    |      PAYLOAD      |   CRC16_MSB   |   CRC16_LSB   |
|     (0xAA)    |     (0x55)    |  (0..64 Bytes)| (Enum: 1 Byte)| (0..255 Cnt)  | (Variable Length) |   (High Byte) |   (Low Byte)  |
+---------------+---------------+---------------+---------------+---------------+-------------------+---------------+---------------+
|<----------------------------- HEADER (5 Bytes) ------------------------------>|<-- PAYLOAD (N) -->|<------ CRC16 (2 Bytes) ------>|
```

### Field Definitions:

| Offset | Field Name | Data Type | Description |
| :---: | :--- | :---: | :--- |
| `0` | `SYNC_BYTE_1` | `uint8_t` | Fixed synchronization byte: `0xAA`. |
| `1` | `SYNC_BYTE_2` | `uint8_t` | Fixed synchronization byte: `0x55`. |
| `2` | `PAYLOAD_LEN` | `uint8_t` | Number of data payload bytes ($0 \le N \le 64$). Frames with $N > 64$ are rejected. |
| `3` | `MSG_TYPE` | `uint8_t` | Message type identifier (see Message Dictionary below). |
| `4` | `SEQ_NUM` | `uint8_t` | Rolling sequence counter ($0\dots 255$) used to detect lost frames. |
| `5..(4+N)` | `PAYLOAD` | `uint8_t[N]` | Raw structured payload data. |
| `(5+N)` | `CRC16_MSB` | `uint8_t` | CRC-16-CCITT high byte calculated over fields `2..(4+N)`. |
| `(6+N)` | `CRC16_LSB` | `uint8_t` | CRC-16-CCITT low byte. |

---

## 3. Message Type Dictionary

| MSG_TYPE | Symbolic Constant | Direction | Payload Size | Purpose |
| :---: | :--- | :---: | :---: | :--- |
| `0x01` | `INTERMCU_MSG_HEARTBEAT` | Bidirectional | 6 Bytes | Keepalive ping tracking uptime, system state, and active fault count. |
| `0x02` | `INTERMCU_MSG_TELEMETRY_REPORT` | STM32 $\to$ ESP32 | 16 Bytes | Periodic 10 Hz telemetry stream (Temp, Vib, Volt, Current, RPM, Actuator State). |
| `0x03` | `INTERMCU_MSG_ACTUATOR_COMMAND` | ESP32 $\to$ STM32 | 5 Bytes | Actuator commands (PWM Duty Cycle, Relay output toggling, E-Stop). |
| `0x04` | `INTERMCU_MSG_FAULT_SNAPSHOT_REQ` | ESP32 $\to$ STM32 | 0 Bytes | Request historical snapshot of the 16-entry circular fault table. |
| `0x05` | `INTERMCU_MSG_FAULT_SNAPSHOT_RSP` | STM32 $\to$ ESP32 | $N \times 8$ Bytes | Response containing up to 16 historical fault log entries. |
| `0x06` | `INTERMCU_MSG_ACK_NACK` | Bidirectional | 2 Bytes | Command acknowledgement and error status report. |

---

## 4. Payload Data Structures

### A. Telemetry Report Payload (`0x02`)
```c
struct {
    uint32_t timestamp_ms;         /* System tick in milliseconds */
    int16_t  temperature_c_x10;    /* Process Temperature in 0.1°C (-400 to 1250) */
    uint16_t vibration_rms_mg;     /* Acceleration RMS in milli-g (0 to 10000 mg) */
    uint16_t voltage_mv;           /* DC Bus Voltage in mV (e.g., 24000 = 24.0V) */
    uint16_t current_ma;           /* Load Current in mA (e.g., 1500 = 1.5A) */
    uint16_t shaft_rpm;            /* Motor / Shaft Speed (RPM) */
    uint8_t  actuator_state;       /* Bit 0: Relay1, Bit 1: Relay2, Bit 2: PWM active */
    uint8_t  active_fault_flags;   /* Bitmask of active faults */
};
```

### B. Actuator Command Payload (`0x03`)
```c
struct {
    uint8_t  command_id;           /* 0x01: Set PWM, 0x02: Set Relays, 0x03: E-Stop, 0x04: Reset Faults */
    uint16_t pwm_duty_permille;    /* 0 to 1000 (0.0% to 100.0%) */
    uint8_t  relay_mask;           /* Bit 0: Relay 1 Enable, Bit 1: Relay 2 Enable */
    uint8_t  reserved;             /* Pad byte */
};
```

---

## 5. CRC-16 Verification

The polynomial used is **CRC-16-CCITT**:
$$P(x) = x^{16} + x^{12} + x^5 + 1 \quad (\text{0x1021})$$
* **Initial Seed:** `0xFFFF`
* **Coverage:** Calculated over `PAYLOAD_LEN`, `MSG_TYPE`, `SEQ_NUM`, and `PAYLOAD[0..N-1]`.
* **Byte Order:** Transmitted Big-Endian (MSB first, followed by LSB).
