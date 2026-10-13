#!/usr/bin/env python3
"""
mqtt_live_subscriber.py
Real-Time Industrial IoT Telemetry Subscriber for EDGE-HUB-S3.
Subscribes to broker.hivemq.com (or local broker) and displays formatted
JSON edge metrics in real-time.
"""

import json
import sys
try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("paho-mqtt not installed. Run: pip install paho-mqtt")
    sys.exit(0)

BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC = "industrial/edge_hub/telemetry"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to MQTT Broker [{BROKER}:{PORT}] successfully!")
        client.subscribe(TOPIC)
        print(f"Subscribed to topic: {TOPIC}\n" + "-" * 75)
    else:
        print(f"Failed to connect, return code {rc}")

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        print(f"[MQTT] Dev: {data.get('dev')} | Time: {data.get('ts')} ms | "
              f"Temp: {data.get('temp'):.1f} °C | Vib: {data.get('vib_rms')} mg | "
              f"Volt: {data.get('volt'):.2f} V | Curr: {data.get('curr_a'):.2f} A | "
              f"RPM: {data.get('rpm')} | Fault: {data.get('fault')}")
    except Exception as e:
        print(f"Raw Message: {msg.payload.decode()}")

def main():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    print(f"Connecting to {BROKER}...")
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nExiting subscriber...")
        client.disconnect()

if __name__ == "__main__":
    main()
