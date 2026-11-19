# SortSense-Hardware — Repo Context & AI Instructions

> **Repo:** `SortSense-Hardware` (Owner: Fabio)
> **Usage:** Place this file in the repo root as `CONTEXT.md` (or `.cursorrules`). It defines this repo's exact endpoints and payloads so it connects seamlessly with https://github.com/SortSense/SortSense-Hardware and https://github.com/SortSense/SortSense-AI_backend.

---

## 1. Role in the SortSense System
This repository contains the **Edge Firmware** for the SortSense smart recycling station. It does **NOT** run AI locally and does **NOT** talk to the database directly. Its job is to:
1. Stream compressed JPEG frames from the **OV5640 camera** over a local **WebSocket** to the LAN AI/LLM Backend.
2. Receive classification results back over the WebSocket and light up the corresponding **WS2812B LED** zone.
3. Detect trash drops via **4x LM393 IR break-beam interrupts** and report the dropped bin index to the LAN server.
4. Periodically poll **4x VL53L0X ToF laser sensors** to calculate compartment fill percentages and `POST` them to the Cloud API.

---

## 2. Strict Hardware Specifications
* **MCU:** ESP32-S3-WROOM-1 **N16R8** (16MB Flash, 8MB PSRAM, Dual USB-C: UART/CH343 + OTG).
* **Framework:** PlatformIO + Arduino Framework (C++).
* **Mandatory Build Flags (`platformio.ini`):**
  * `-DBOARD_HAS_PSRAM` (Camera frame buffer MUST use `CAMERA_FB_IN_PSRAM`)
  * `-DARDUINO_USB_MODE=1`
  * `-DARDUINO_USB_CDC_ON_BOOT=0` (or `1` if using OTG USB port; 0 routes Serial to UART CH343)
* **Peripherals:**
  * **ToF Sensors:** 4x VL53L0X (Purple boards with `XSHUT`). Share 1 I2C bus (`SDA`/`SCL`) + 4 dedicated `XSHUT` GPIOs to assign unique I2C addresses on boot.
  * **IR Sensors:** 4x LM393 180° modules. Output goes **LOW** on detection → attach hardware interrupts on **`FALLING`** edge.
  * **LEDs:** WS2812B daisy-chained addressable LEDs on 1 GPIO pin (controlled via `FastLED`).

---

## 3. Standardized Bin & Color Mapping
Always use these exact indices and strings:
* `0` → `"plastic"` (Blue)
* `1` → `"paper"` (Green)
* `2` → `"metal"` (Yellow)
* `3` → `"general"` (White)

---

## 4. External API Contracts (Strict Handshake)

### A. To Local LAN AI/LLM Server (`http://<LAN_IP>:8000`)

1. **`WS /ws/vision`**
   * **Send (Binary):** Raw JPEG bytes from `camera_fb_t`.
   * **Receive (Text JSON):**
   ```json
   {
     "category": "plastic",
     "bin_index": 0,
     "confidence": 0.91
   }
   ```
   *(If `bin_index == -1`, stay in idle animation).*

2. **`POST /internal/verify-drop`** (Triggered when an IR interrupt fires)
   ```json
   {
     "station_id": "ST-001",
     "bin_index": 0
   }
   ```

### B. To Cloud Website API (`https://<VERCEL_DOMAIN>/api/...`)

1. **`POST /api/telemetry/fill-levels`** (Sent every 1–5 mins or after drops)
   ```json
   {
     "station_id": "ST-001",
     "fill_levels": {
       "plastic": 45,
       "paper": 80,
       "metal": 20,
       "general": 95
     }
   }
   ```

---

## 5. Coding Rules for AI Assistants
* **Modular Architecture:** Separate logic cleanly into `include/` and `src/` (`config.h`, `camera_handler`, `tof_handler`, `ir_handler`, `led_handler`, `network_handler`, `main.cpp`).
* **Non-Blocking Loop:** NEVER use `delay()` inside `loop()`—always use `millis()` timers so WebSockets and ISRs don't drop or trigger watchdog resets.
* **Safe ISRs:** Keep IR interrupt functions marked `IRAM_ATTR`, only setting a `volatile` flag/timestamp and handling HTTP calls inside `loop()`.

