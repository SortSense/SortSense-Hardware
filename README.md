# SortSense - ESP32-S3 Hardware Subsystem

[![PlatformIO CI](https://img.shields.io/badge/PlatformIO-Build%20Passing-00878a?logo=platformio&logoColor=white)](platformio.ini)
[![Unit Tests](https://img.shields.io/badge/Unity%20Tests-73%2F73%20Passing-brightgreen?logo=checkmarx&logoColor=white)](test/)
[![Target MCU](https://img.shields.io/badge/Hardware-ESP32--S3--WROOM--1--N16R8-red?logo=espressif&logoColor=white)](include/config.h)
[![Sensor Driver](https://img.shields.io/badge/Camera-OV5640%20(DVP%20YUV422%2FJPEG)-blue)](src/camera_handler.cpp)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

The **SortSense Hardware Subsystem** is an edge-computing smart waste classification and bin telemetry module powered by the **ESP32-S3 (16MB Flash, 8MB Octal PSRAM)** and an **OV5640 5MP camera**. It coordinates real-time camera streaming, optical waste drop detection, multi-compartment laser fill-level monitoring, and responsive visual lighting feedback.

---

## Key Capabilities

- **Dual-Core FreeRTOS Architecture**:
  - **Core 0**: Wi-Fi stack, non-blocking captive portal provisioning (`WiFiManager`), asynchronous Web Dashboard (`ESPAsyncWebServer`), and WebSocket networking.
  - **Core 1**: Real-time zero-copy camera DMA acquisition, interrupt-driven drop sensing, and multi-sensor I2C ranging.
- **Low-Latency Vision Pipeline**: Captures HVGA (480x320) frames from the OV5640 camera, buffered in 8MB Octal PSRAM, streaming binary JPEG frames over WebSocket to a local LAN AI classification server at 20-30 FPS.
- **Microsecond Drop Confirmation**: 4x LM393 infrared break-beam sensors on hardware `FALLING` interrupts with 500 ms software debounce, verifying physical drops within 5 ms via `POST /internal/verify-drop`.
- **Laser Time-of-Flight Telemetry**: 4x Pololu VL53L0X laser distance sensors on a shared I2C bus using sequential XSHUT address bootstrapping (`0x30`..`0x33`), periodically transmitting compartment capacities to the cloud website over HTTPS.
- **Onboard WS2812 Visual UX**: Drives the built-in NeoPixel on GPIO 48 without requiring external soldered LED strips. Visualizes bin classifications (Plastic: Blue, Paper: Green, Metal: Yellow, General: White, Idle: Cyan breathing, Full Alert: Pulsing Red).
- **Hermetic Unit Test Suite**: 73 host-native Unity unit test cases executable without physical hardware (`pio test -e native`).

---

## System Architecture

```mermaid
flowchart TD
    subgraph ESP32["ESP32-S3 Hardware Node (ST-001)"]
        Cam["OV5640 Camera (DVP)"] -->|PSRAM DMA Buffers| Stream["WebSocket Vision Streamer"]
        IR["4x LM393 IR Break-Beam Sensors"] -->|Hardware ISR (FALLING)| DropEngine["Drop Verification Engine"]
        ToF["4x VL53L0X ToF Sensors"] -->|Sequential I2C Bus| FillTelemetry["Fill Capacity Telemetry"]
        LED["Onboard WS2812 RGB LED (GPIO 48)"] <-->|RMT NRZ Feedback| UserUX["Visual Status UX"]
    end

    subgraph LAN["Local LAN AI Server (:8000)"]
        WSEndpoint["WS /ws/vision (YOLOv8 Inferences)"]
        DropEndpoint["POST /internal/verify-drop"]
    end

    subgraph Cloud["Cloud Platform (Vercel)"]
        CloudEndpoint["POST /api/telemetry/fill-levels (HTTPS)"]
        WebDashboard["Public Analytics Dashboard"]
    end

    Stream -->|Raw JPEG Frames (Every ~500ms)| WSEndpoint
    WSEndpoint -->|JSON Classification Result| LED
    DropEngine -->|Immediate Drop Event Trigger| DropEndpoint
    DropEndpoint -->|Forward Correct Sort Event| CloudEndpoint
    FillTelemetry -->|Periodic Fill Levels (Every ~15s)| CloudEndpoint
    CloudEndpoint --> WebDashboard
```

---

## Master Hardware Pinout Reference

All active GPIOs are mathematically isolated to eliminate internal silicon collisions:

| Peripheral | Signal | Pin | Type | Notes |
|---|---|:---:|:---:|---|
| **Onboard RGB LED** | Data (WS2812) | **GPIO 48** | Output | Built-in NeoPixel; no external soldering required |
| **I2C Bus (ToF)** | SDA | **GPIO 47** | I/O | Shared 400 kHz Fast-Mode I2C data |
| **I2C Bus (ToF)** | SCL | **GPIO 21** | Output | Remapped from 48 to prevent RGB LED collision |
| **ToF Sensor 1** | XSHUT (Plastic) | **GPIO 1** | Output | Active-high enable, address `0x30` |
| **ToF Sensor 2** | XSHUT (Paper) | **GPIO 2** | Output | Active-high enable, address `0x31` |
| **ToF Sensor 3** | XSHUT (Metal) | **GPIO 41** | Output | Active-high enable, address `0x32` |
| **ToF Sensor 4** | XSHUT (General) | **GPIO 42** | Output | Active-high enable, address `0x33` |
| **IR Break-Beam 1** | Digital Out (Plastic) | **GPIO 14** | Input | Falling-edge hardware interrupt (`INPUT_PULLUP`) |
| **IR Break-Beam 2** | Digital Out (Paper) | **GPIO 38** | Input | Falling-edge hardware interrupt (`INPUT_PULLUP`) |
| **IR Break-Beam 3** | Digital Out (Metal) | **GPIO 39** | Input | Falling-edge hardware interrupt (`INPUT_PULLUP`) |
| **IR Break-Beam 4** | Digital Out (General) | **GPIO 40** | Input | Falling-edge hardware interrupt (`INPUT_PULLUP`) |
| **Camera DVP** | XCLK | **GPIO 15** | Output | 20 MHz sensor master clock |
| **Camera DVP** | SIOD / SIOC | **GPIO 4, 5** | I/O | SCCB camera control bus |
| **Camera DVP** | D0 - D7 | **11, 9, 8, 10, 12, 18, 17, 16** | Input | 8-bit parallel DVP pixel data bus |
| **Camera DVP** | VSYNC, HREF, PCLK | **GPIO 6, 7, 13** | Input | Synchronization clocks |

> [!IMPORTANT]
> For complete step-by-step physical assembly, optical alignment, and trimpot/height calibration instructions, see **[SETUP.md](file:///home/Fabio/project/univ/SortSense/SortSense-Hardware/SETUP.md)**.

---

## Endpoints Integration Cheat Sheet

The ESP32-S3 communicates across 3 main endpoints:

### Camera Stream (Real-Time Vision)
- **Target**: Local LAN AI Server
- **Protocol & URL**: `WS ws://<LAN_HOST>:8000/ws/vision`
- **Rate**: Every ~500 ms (non-blocking timer while connected)
- **Sent**: Binary JPEG frame buffer bytes (`fb->buf`, `fb->len`)
- **Received**:
  ```json
  {
    "category": "plastic",
    "bin_index": 0,
    "confidence": 0.94
  }
  ```

### IR Drop Confirmation (Trash Dropped)
- **Target**: Local LAN AI Server
- **Method & URL**: `POST http://<LAN_HOST>:8000/internal/verify-drop`
- **Trigger**: Immediately upon LM393 beam break interrupt (`FALLING` edge)
- **Payload**:
  ```json
  {
    "station_id": "ST-001",
    "bin_index": 0
  }
  ```
  *(The local server forwards the verified event to the cloud website).*

### Compartment Fill Levels (Capacity Telemetry)
- **Target**: Cloud Website (Vercel)
- **Method & URL**: `POST https://<CLOUD_DOMAIN>/api/telemetry/fill-levels`
- **Protocol**: HTTPS via `WiFiClientSecure` with `client.setInsecure()` (or local HTTP fallback)
- **Rate**: Every 15 seconds during pitch demos
- **Payload**:
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

## Visual UX & LED Color Indication

The onboard RGB LED on GPIO 48 provides visual feedback:

| Classification | Bin Index | Target LED Color | Visual Behavior |
|---|:---:|---|---|
| **Plastic** | `0` | **Blue** (`#003CFF`) | Solid indicator |
| **Paper** | `1` | **Green** (`#00FF1E`) | Solid indicator |
| **Metal** | `2` | **Yellow / Amber** (`#FFB400`) | Solid indicator |
| **General** | `3` | **White** (`#DCDCDC`) | Solid indicator |
| **Idle / Standby** | `-1` | **Cyan** (`#001423`) | Gentle breathing animation |
| **Drop Confirmed** | Any | **Bright Flash** (`#FFFFFF`) | 600 ms confirmation burst |
| **Bin Full (>= 90%)** | Any | **Pulsing Red** (`#FF0000`) | Continuous alert pulse |

---

## Quickstart Guide

### 1. Requirements
- [PlatformIO Core](https://platformio.org/install/cli) or PlatformIO IDE extension for VS Code.
- Python 3.9+ with `aiohttp` for the local test server:
  ```bash
  pip install aiohttp
  ```

### 2. Run the Interactive Python Test Server
Before flashing the hardware, launch the local test server on your computer:
```bash
python3 tools/test_ws_server.py --host 0.0.0.0 --port 8000
```
This launches a WebSocket listener, an HTTP receiver for telemetry, and an **interactive terminal REPL**:
- Type `0`, `1`, `2`, `3` to instantly change the ESP32's onboard LED.
- Type `none` for idle Cyan breathing.
- Type `auto` to toggle 3-second automatic category cycling.
- Type `status` to inspect connected clients and frame stream rates.

### 3. Compile & Upload Firmware
Connect your ESP32-S3 via the UART USB-C port:
```bash
# Build firmware binary
pio run -e esp32-s3-devkitc-1

# Flash to ESP32-S3
pio run -e esp32-s3-devkitc-1 -t upload

# Open high-speed Serial Monitor (460800 baud)
pio device monitor -b 460800
```

---

## Automated Unit Test Suite

The repository contains **73 hermetic host-native unit tests** executed using PlatformIO's Unity test framework on your workstation without physical hardware:

```bash
pio test -e native
```

### Test Suite Coverage

| Test Suite | File | Tests | Validations |
|---|---|:---:|---|
| **`test_config`** | [test_config.cpp](file:///home/Fabio/project/univ/SortSense/SortSense-Hardware/test/test_config/test_config.cpp) | 15 | GPIO collision avoidance, silicon bounds, strapping pin safety, memory isolation |
| **`test_camera_handler`** | [test_camera_handler.cpp](file:///home/Fabio/project/univ/SortSense/SortSense-Hardware/test/test_camera_handler/test_camera_handler.cpp) | 12 | PSRAM verification, driver initialization state machine, frame acquisition/release |
| **`test_network_handler`** | [test_network_handler.cpp](file:///home/Fabio/project/univ/SortSense/SortSense-Hardware/test/test_network_handler/test_network_handler.cpp) | 11 | WebSocket frame dispatch, heartbeat logic, JSON parsing, callback routing |
| **`test_tof_handler`** | [test_tof_handler.cpp](file:///home/Fabio/project/univ/SortSense/SortSense-Hardware/test/test_tof_handler/test_tof_handler.cpp) | 11 | Sequential XSHUT boot, single-sensor graceful fallback, linear distance-to-fill math |
| **`test_async_response`** | [test_async_response.cpp](file:///home/Fabio/project/univ/SortSense/SortSense-Hardware/test/test_async_response/test_async_response.cpp) | 9 | Chunked HTTP pagination, EOF handling, RAII buffer and mutex release |
| **`test_ir_handler`** | [test_ir_handler.cpp](file:///home/Fabio/project/univ/SortSense/SortSense-Hardware/test/test_ir_handler/test_ir_handler.cpp) | 9 | Hardware ISR registration, 500ms software debounce, atomic event polling |
| **`test_led_handler`** | [test_led_handler.cpp](file:///home/Fabio/project/univ/SortSense/SortSense-Hardware/test/test_led_handler/test_led_handler.cpp) | 6 | RGB color category mapping, brightness scaling, drop flash, red alert override |
| **Total** | | **73** | **100% Passed (Zero hardware dependencies)** |

---

## Repository Layout

```
SortSense-Hardware/
├── include/
│   ├── async_camera_response.h  # RAII chunked HTTP streaming response
│   ├── camera_handler.h         # Modular OV5640 acquisition interfaces
│   ├── config.h                 # Master GPIO pinouts & system constants
│   ├── ir_handler.h             # LM393 break-beam interrupt driver
│   ├── led_handler.h            # Onboard WS2812 RGB LED manager
│   ├── network_handler.h        # WebSocket client & JSON parser
│   └── tof_handler.h            # 4x VL53L0X ToF laser distance subsystem
├── src/
│   ├── camera_handler.cpp       # Camera driver setup & PSRAM frame grabs
│   ├── ir_handler.cpp           # Falling-edge ISRs & drop verification client
│   ├── led_handler.cpp          # RMT NeoPixel control & breathing animation
│   ├── main.cpp                 # FreeRTOS dual-core tasks & setup/loop
│   ├── network_handler.cpp      # WebSocket streaming & classification parsing
│   └── tof_handler.cpp          # Sequential XSHUT init & telemetry dispatch
├── test/
│   ├── mocks/                   # Hermetic native mocks (Arduino, ESP-IDF, Wire, VL53L0X)
│   ├── test_async_response/     # Async streaming tests
│   ├── test_camera_handler/     # Camera driver tests
│   ├── test_config/             # Pin collision & safety tests
│   ├── test_ir_handler/         # IR interrupt & debounce tests
│   ├── test_led_handler/        # RGB LED UX tests
│   ├── test_network_handler/    # WebSocket & JSON parsing tests
│   └── test_tof_handler/        # ToF multi-sensor & calibration tests
├── tools/
│   └── test_ws_server.py        # Standalone Python AI Server & Interactive REPL
├── platformio.ini               # Dual-target PlatformIO configuration
├── SETUP.md                     # Step-by-step physical wiring, mounting & calibration
└── README.md                    # Primary project documentation
```

---

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
