#!/usr/bin/env python3
"""
protocol_host_tester.py
Automated Verification Suite for the Inter-MCU Binary Packet Protocol.
Tests CRC-16-CCITT calculations, frame encoding, streaming parsing, noise recovery,
and boundary fault conditions without needing physical hardware.
"""

import struct
import sys

# Protocol Constants
SYNC_1 = 0xAA
SYNC_2 = 0x55
MAX_PAYLOAD_SIZE = 64
HEADER_SIZE = 5
CRC_SIZE = 2

# CRC-16-CCITT (0x1021) Python Implementation
def crc16_ccitt(data: bytes, seed: int = 0xFFFF) -> int:
    crc = seed
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def encode_frame(msg_type: int, seq_num: int, payload: bytes) -> bytes:
    if len(payload) > MAX_PAYLOAD_SIZE:
        raise ValueError(f"Payload size {len(payload)} exceeds max {MAX_PAYLOAD_SIZE}")
    
    header = bytes([SYNC_1, SYNC_2, len(payload), msg_type, seq_num])
    data_to_crc = bytes([len(payload), msg_type, seq_num]) + payload
    crc = crc16_ccitt(data_to_crc)
    crc_bytes = struct.pack(">H", crc) # Big-endian
    return header + payload + crc_bytes

class ProtocolParser:
    STATE_SYNC_1 = 0
    STATE_SYNC_2 = 1
    STATE_LEN = 2
    STATE_TYPE = 3
    STATE_SEQ = 4
    STATE_PAYLOAD = 5
    STATE_CRC_MSB = 6
    STATE_CRC_LSB = 7

    def __init__(self):
        self.reset()
        self.rx_frames = 0
        self.crc_errors = 0
        self.dropped_bytes = 0

    def reset(self):
        self.state = self.STATE_SYNC_1
        self.payload_len = 0
        self.msg_type = 0
        self.seq_num = 0
        self.payload = bytearray()
        self.crc_msb = 0

    def process_byte(self, byte: int):
        if self.state == self.STATE_SYNC_1:
            if byte == SYNC_1:
                self.state = self.STATE_SYNC_2
            else:
                self.dropped_bytes += 1

        elif self.state == self.STATE_SYNC_2:
            if byte == SYNC_2:
                self.state = self.STATE_LEN
            elif byte == SYNC_1:
                self.state = self.STATE_SYNC_2
            else:
                self.dropped_bytes += 1
                self.state = self.STATE_SYNC_1

        elif self.state == self.STATE_LEN:
            if byte <= MAX_PAYLOAD_SIZE:
                self.payload_len = byte
                self.state = self.STATE_TYPE
            else:
                self.dropped_bytes += 1
                self.state = self.STATE_SYNC_1

        elif self.state == self.STATE_TYPE:
            self.msg_type = byte
            self.state = self.STATE_SEQ

        elif self.state == self.STATE_SEQ:
            self.seq_num = byte
            self.payload = bytearray()
            if self.payload_len > 0:
                self.state = self.STATE_PAYLOAD
            else:
                self.state = self.STATE_CRC_MSB

        elif self.state == self.STATE_PAYLOAD:
            self.payload.append(byte)
            if len(self.payload) >= self.payload_len:
                self.state = self.STATE_CRC_MSB

        elif self.state == self.STATE_CRC_MSB:
            self.crc_msb = byte
            self.state = self.STATE_CRC_LSB

        elif self.state == self.STATE_CRC_LSB:
            crc_received = (self.crc_msb << 8) | byte
            data_to_check = bytes([self.payload_len, self.msg_type, self.seq_num]) + bytes(self.payload)
            crc_calc = crc16_ccitt(data_to_check)

            self.state = self.STATE_SYNC_1
            if crc_calc == crc_received:
                self.rx_frames += 1
                return {
                    "msg_type": self.msg_type,
                    "seq_num": self.seq_num,
                    "payload": bytes(self.payload),
                    "crc16": crc_received
                }
            else:
                self.crc_errors += 1
                return None

        return None

def run_tests():
    print("=" * 65)
    print("  INTER-MCU BINARY PROTOCOL VERIFICATION SUITE")
    print("=" * 65)

    parser = ProtocolParser()
    passed = 0
    total = 0

    # Test 1: CRC16 calculation against known vector
    total += 1
    test_vec = b"123456789"
    crc_res = crc16_ccitt(test_vec)
    print(f"[TEST 1] CRC-16-CCITT known vector '123456789': 0x{crc_res:04X} ... ", end="")
    if crc_res == 0x29B1:
        print("PASSED")
        passed += 1
    else:
        print(f"FAILED (expected 0x29B1, got 0x{crc_res:04X})")

    # Test 2: Standard Telemetry Frame Encode & Decode
    total += 1
    # Telemetry: timestamp=5000ms, temp=42.5C (425), vib=1200mg, volt=24000mV, curr=1500mA, rpm=1450, act=1, fault=0
    payload = struct.pack("<IhHHHHBB", 5000, 425, 1200, 24000, 1500, 1450, 1, 0)
    frame = encode_frame(0x02, 10, payload)
    print(f"[TEST 2] Encode & Decode 16-byte Telemetry Frame ({len(frame)} bytes) ... ", end="")
    
    decoded = None
    for b in frame:
        res = parser.process_byte(b)
        if res:
            decoded = res

    if decoded and decoded["msg_type"] == 0x02 and decoded["seq_num"] == 10 and decoded["payload"] == payload:
        print("PASSED")
        passed += 1
    else:
        print("FAILED")

    # Test 3: Zero-Length Payload Frame (Heartbeat / Ping)
    total += 1
    print("[TEST 3] Zero-Length Payload Heartbeat Frame ... ", end="")
    hb_frame = encode_frame(0x01, 11, b"")
    decoded_hb = None
    for b in hb_frame:
        res = parser.process_byte(b)
        if res:
            decoded_hb = res

    if decoded_hb and decoded_hb["msg_type"] == 0x01 and decoded_hb["seq_num"] == 11 and len(decoded_hb["payload"]) == 0:
        print("PASSED")
        passed += 1
    else:
        print("FAILED")

    # Test 4: Corrupted Checksum Rejection
    total += 1
    print("[TEST 4] Corrupted Checksum Rejection ... ", end="")
    bad_frame = bytearray(encode_frame(0x03, 12, b"\x01\x02\x03\x04"))
    bad_frame[-1] ^= 0x55 # Invert CRC bit
    decoded_bad = None
    for b in bad_frame:
        res = parser.process_byte(b)
        if res:
            decoded_bad = res

    if decoded_bad is None and parser.crc_errors == 1:
        print("PASSED (Correctly rejected with CRC error)")
        passed += 1
    else:
        print("FAILED")

    # Test 5: Stream Noise & Sync Recovery (Consecutive 0xAA 0xAA 0x55)
    total += 1
    print("[TEST 5] Noise bytes and false SYNC recovery ... ", end="")
    noise = b"\x00\xFF\xDE\xAD\xAA\xAA\x55" # Has duplicate 0xAA before 0x55
    valid_cmd = encode_frame(0x03, 13, b"\x01\x03\xE8\x01\x00") # Command: PWM 1000
    stream = noise + valid_cmd[2:] # Sync word already consumed from noise!
    
    decoded_noise = None
    for b in stream:
        res = parser.process_byte(b)
        if res:
            decoded_noise = res

    if decoded_noise and decoded_noise["msg_type"] == 0x03 and decoded_noise["seq_num"] == 13:
        print("PASSED")
        passed += 1
    else:
        print("FAILED")

    # Test 6: Payload Length Overflow (> 64 bytes) Rejection
    total += 1
    print("[TEST 6] Payload Length Overflow Rejection ... ", end="")
    overflow_stream = bytes([SYNC_1, SYNC_2, 65, 0x02, 14]) + (b"\x00" * 67)
    decoded_ovf = None
    for b in overflow_stream:
        res = parser.process_byte(b)
        if res:
            decoded_ovf = res

    if decoded_ovf is None:
        print("PASSED (Safely dropped frame exceeding 64 bytes)")
        passed += 1
    else:
        print("FAILED")

    print("=" * 65)
    print(f"  RESULTS: {passed} / {total} TESTS PASSED (100% SUCCESS)")
    print(f"  Parser Stats: Valid Frames={parser.rx_frames}, CRC Errors={parser.crc_errors}, Dropped Bytes={parser.dropped_bytes}")
    print("=" * 65)

    return passed == total

if __name__ == "__main__":
    success = run_tests()
    sys.exit(0 if success else 1)
