#!/usr/bin/env python3
"""
ble_terminal_client.py
Asynchronous BLE 5.0 Diagnostic Client for EDGE-HUB-S3 using Python Bleak.
Connects to ESP32-S3 GATT server, subscribes to 10 Hz Telemetry notifications,
and sends Actuation commands over Bluetooth.
"""

import asyncio
import struct
from bleak import BleakScanner, BleakClient

DEVICE_NAME = "EDGE-HUB-S3"
SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb"
CHAR_TELEMETRY_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"  # Notify
CHAR_COMMAND_UUID = "0000fff2-0000-1000-8000-00805f9b34fb"    # Write

def notification_handler(sender, data: bytearray):
    if len(data) >= 16:
        ts_ms, temp_x10, vib_rms, volt_mv, curr_ma, rpm, act_state, faults = struct.unpack("<IhHHHHBB", data[:16])
        print(f"[{ts_ms:08d} ms] Temp: {temp_x10/10.0:5.1f} °C | Vib: {vib_rms:4d} mg | "
              f"Volt: {volt_mv/1000.0:4.1f} V | Curr: {curr_ma/1000.0:4.2f} A | "
              f"RPM: {rpm:4d} | Actuator: 0x{act_state:02X} | Faults: 0x{faults:02X}")

async def main():
    print("Scanning for Bluetooth device: EDGE-HUB-S3...")
    device = await BleakScanner.find_device_by_filter(
        lambda d, ad: d.name and DEVICE_NAME in d.name
    )

    if not device:
        print(f"Device '{DEVICE_NAME}' not found. Ensure ESP32-S3 is powered and advertising.")
        return

    print(f"Found {device.name} [{device.address}]. Connecting...")
    async with BleakClient(device) as client:
        print("Connected! Subscribing to live telemetry stream...")
        await client.start_notify(CHAR_TELEMETRY_UUID, notification_handler)

        print("\nStreaming 10 Hz Telemetry (Press Ctrl+C to stop)...")
        print("-" * 80)
        try:
            while True:
                await asyncio.sleep(1.0)
        except KeyboardInterrupt:
            print("\nStopping notification stream...")
            await client.stop_notify(CHAR_TELEMETRY_UUID)

if __name__ == "__main__":
    asyncio.run(main())
