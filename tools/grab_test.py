#!/usr/bin/env python3
"""
Simple script to trigger and save a test photo from ESP32-S3 over Serial.
"""
import sys
import time
import base64
import os
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
BAUD = 460800
OUT_PATH = sys.argv[2] if len(sys.argv) > 2 else "/home/Fabio/Downloads/camera_test.jpg"
ARTIFACT_PATH = "/home/Fabio/.gemini/antigravity/brain/710c7aba-7b48-4e9c-8d10-40f3ec6f0ddc/basic_test.jpg"

print(f"Connecting to {PORT} at {BAUD} baud...")
try:
    ser = serial.Serial(PORT, BAUD, timeout=1.0)
except Exception as e:
    print(f"Error opening port {PORT}: {e}")
    sys.exit(1)

time.sleep(1.0)
ser.reset_input_buffer()

print("Sending 'c' to trigger capture...")
ser.write(b"c\n")
ser.flush()

b64_lines = []
capturing = False
start_time = time.time()

while time.time() - start_time < 10.0:
    line = ser.readline().decode("utf-8", errors="ignore").strip()
    if not line:
        continue
    if "---BEGIN_PHOTO---" in line:
        capturing = True
        b64_lines = []
        print("Receiving photo stream...")
        continue
    elif "---END_PHOTO---" in line:
        print(f"Photo received! Total base64 lines: {len(b64_lines)}")
        break
    elif capturing:
        b64_lines.append(line)
    else:
        print(f"[ESP32] {line}")

ser.close()

if not b64_lines:
    print("Failed to receive photo data within timeout.")
    sys.exit(1)

full_b64 = "".join(b64_lines)
try:
    img_bytes = base64.b64decode(full_b64)
    with open(OUT_PATH, "wb") as f:
        f.write(img_bytes)
    with open(ARTIFACT_PATH, "wb") as f:
        f.write(img_bytes)
    print(f"SUCCESS! Saved {len(img_bytes)} bytes to:")
    print(f"  1. {OUT_PATH}")
    print(f"  2. {ARTIFACT_PATH}")
except Exception as e:
    print(f"Base64 decode error: {e}")
    sys.exit(1)

