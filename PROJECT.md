# Project: SortSense ESP32-S3 Modular Firmware & Unit Test Suite

## Architecture
The SortSense firmware is partitioned into a modular, decoupled embedded architecture for the ESP32-S3 WROOM CAM (N16R8) with OV5640 camera:
- **Core 0 (Network & Web Server)**: Runs LwIP TCP/IP stack, non-blocking `WiFiManager` auto-connect / portal process loop, and asynchronous HTTP server on port 80 using `ESPAsyncWebServer` + `AsyncTCP`.
- **Core 1 (Hardware Acquisition & Peripherals)**: Runs the camera acquisition pipeline (`camera_handler`), frame buffer extraction, and peripheral handling (VL53L0X ToF sensors on shared I2C bus, LM393 IR break-beam sensor interrupt handlers, and WS2812B LED status lighting).
- **Inter-Core Synchronization & RAII Streaming**: Mutex synchronization (`SemaphoreHandle_t cameraMutex`) guarantees thread safety between HTTP requests and the hardware camera driver. An RAII streaming class `AsyncCameraResponse` (`include/async_camera_response.h`) handles zero-copy chunked buffer streaming and automatic release of frame buffers and mutexes.
- **Memory Subsystem**: Utilizes the 8MB Octal SPI PSRAM for double frame-buffering (`CAMERA_FB_IN_PSRAM`, `fb_count = 2`, `CAMERA_GRAB_LATEST`), while strictly avoiding internal Octal PSRAM bus pins (GPIO 33-37) and SPI Flash pins (GPIO 26-32).
- **Type Safety**: Enforces strict fixed-width types (`uint8_t`, `int8_t`, `uint16_t`, `uint32_t`, `size_t`) across all headers and sources, eliminating raw `int`.
- **Unit Testing Architecture**: Dual-target PlatformIO configuration with `[env:esp32-s3-devkitc-1]` for physical firmware builds and `[env:native]` for fast, deterministic, host-based unit testing via Unity. Isolated mock headers in `test/mocks/` simulate ESP-IDF, FreeRTOS, and Arduino frameworks without contaminating target builds.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | Dependencies & PlatformIO Config | Define `platformio.ini` dependencies (WiFiManager, ESPAsyncWebServer, FastLED, VL53L0X, WebSockets) while preserving build flags (-DBOARD_HAS_PSRAM, -DARDUINO_USB_MODE=1, -DARDUINO_USB_CDC_ON_BOOT=1) | M1 | Survey 1, ORIGINAL_REQUEST §R1 |
| 2 | Safe Hardware Pinout & Peripheral Reservation | Define non-overlapping GPIOs in `include/config.h` for camera, I2C bus (SDA: 8, SCL: 9), 4x ToF XSHUT (1, 2, 41, 42), 4x IR break-beam (4, 5, 6, 7 with interrupt support), and WS2812B (21) | M1 | Survey 2, ORIGINAL_REQUEST §R2 |
| 3 | Fixed-Width Type Enforcement | Enforce `uint8_t`, `int8_t`, `uint16_t`, `uint32_t`, `size_t` across all declarations, constants, and signatures | M1 | Survey 3, ORIGINAL_REQUEST §R1 |
| 4 | OV5640 Camera Driver Initialization | Initialize OV5640 using `esp_camera.h` with `CAMERA_FB_IN_PSRAM`, `FRAMESIZE_VGA`, `jpeg_quality = 12`, `CAMERA_GRAB_LATEST`, `fb_count = 2` | M1 | Survey 3, ORIGINAL_REQUEST §R2 |
| 5 | Frame Acquisition & Release | Provide clean C++ API `camera_acquire_frame()` returning `camera_fb_t*` and `camera_release_frame()` calling `esp_camera_fb_return()` | M1 | Survey 3, ORIGINAL_REQUEST §R2 |
| 6 | WiFiManager Dynamic Provisioning | Integrate `tzapu/WiFiManager` in non-blocking mode (`setConfigPortalBlocking(false)`) without static credentials, reporting assigned local IP to `Serial` | M1 | Survey 3, ORIGINAL_REQUEST §R3 |
| 7 | Dual-Core FreeRTOS Task Pinning | Pin Network task to Core 0 and Hardware/Camera task to Core 1 using `xTaskCreatePinnedToCore`, yielding CPU via `vTaskDelay(1)` | M1 | Survey 3, ORIGINAL_REQUEST §R4 |
| 8 | Asynchronous HTTP Server on Port 80 | Run async web server listening on port 80 without blocking `loop()` or starving FreeRTOS tasks | M1 | Survey 3, ORIGINAL_REQUEST §R4 |
| 9 | GET `/capture` Endpoint | Return `image/jpeg` snapshot asynchronously using `esp_camera_fb_get()` and release buffer via `esp_camera_fb_return()` | M1 | Survey 3, ORIGINAL_REQUEST §R4 |
| 10 | End-to-End Build & Safety Verification | Verify clean compilation targeting `env:esp32-s3-devkitc-1`, zero pin collisions, type safety verification, and adversarial integrity audit | M2 | Survey 1-3, Acceptance Criteria |
| 11 | Host-Native Test Environment Config | Add `[env:native]` configuration in `platformio.ini` with `platform = native`, `test_framework = unity`, `test_build_src = yes`, source filtering, and include flags | M3 | Survey 1-3, ORIGINAL_REQUEST §R1 |
| 12 | AsyncCameraResponse Decoupled Header | Extract `AsyncCameraResponse` into `include/async_camera_response.h` with include guards and clean interfaces for modular unit testing | M3 | Survey 1-3, ORIGINAL_REQUEST §R1 |
| 13 | Hermetic Hardware Mock Layer | Implement isolated mock headers in `test/mocks/` (`esp_camera.h`, `esp_err.h`, `esp_log.h`, `esp_heap_caps.h`, `Arduino.h`, `WiFiManager.h`, `ESPAsyncWebServer.h`, `freertos/FreeRTOS.h`, `freertos/semphr.h`) | M3 | Survey 1-3, ORIGINAL_REQUEST §R1 |
| 14 | Hardware Config & Pinout Safety Unit Tests | Implement unit test suite in `test/test_config/test_config.cpp` covering pin collisions, silicon ranges (0..48), forbidden buses (PSRAM 33..37, Flash 26..32), camera parameters, and type safety | M3 | Survey 1-3, Acceptance Criteria |
| 15 | Camera Handler Lifecycle Unit Tests | Implement unit test suite in `test/test_camera_handler/test_camera_handler.cpp` covering init invariants, PSRAM detection guards, config mapping, init idempotency, driver failure paths, frame acquisition/release, and deinit | M3 | Survey 1-3, Acceptance Criteria |
| 16 | Async Streaming Response Unit Tests | Implement unit test suite in `test/test_async_response/test_async_response.cpp` covering single-chunk, multi-chunk pagination, EOF, null buffer handling, and RAII mutex/frame release | M3 | Survey 1-3, Acceptance Criteria |
| 17 | Test Execution & Production Build Verification | Verify clean compilation and execution of all unit tests (`pio test -e native` exit 0, 0 failures) and clean compilation of firmware (`pio run -e esp32-s3-devkitc-1` exit 0) | M3 | Survey 1-3, Acceptance Criteria |

## Milestones
| # | Name | Scope | Dependencies | Status | Outputs / Verdict |
|---|------|-------|-------------|--------|-------------------|
| M1 | Modular Firmware Implementation | Implement `platformio.ini`, `include/config.h`, `include/camera_handler.h`, `src/camera_handler.cpp`, and `src/main.cpp` fulfilling all R1, R2, R3, R4 functional and type safety requirements | none | DONE | Gate PASS: `pio run` exit 0, 0 pin collisions, 0 raw `int`, CLEAN audit |
| M2 | Final Verification, Adversarial Hardening & Acceptance | Pass 100% of E2E verification criteria: compilation check, pin collision script verification, static type check, and adversarial coverage hardening | M1 | DONE | All 4 acceptance criteria categories verified |
| M3 | Production Unit Test Suite | Implement native test runner `[env:native]` in `platformio.ini`, hermetic mock headers in `test/mocks/`, decoupled `include/async_camera_response.h`, and comprehensive unit tests for `config.h`, `camera_handler`, and `async_camera_response` | M1, M2 | DONE | Gate PASS: 35/35 unit tests pass, firmware build pass, 6/6 mutations killed, 20/20 stress runs pass, CLEAN forensic audit |

## Interface Contracts

### `include/config.h`
- Camera pins: `CAM_PIN_PWDN = -1`, `CAM_PIN_RESET = -1`, `CAM_PIN_XCLK = 15`, `CAM_PIN_SIOD = 4`, `CAM_PIN_SIOC = 5` (`int8_t`)
- Camera DVP bits: `CAM_PIN_Y9 = 16`, `CAM_PIN_Y8 = 17`, `CAM_PIN_Y7 = 18`, `CAM_PIN_Y6 = 12`, `CAM_PIN_Y5 = 10`, `CAM_PIN_Y4 = 8`, `CAM_PIN_Y3 = 9`, `CAM_PIN_Y2 = 11` (`int8_t`)
- Camera syncs: `CAM_PIN_VSYNC = 6`, `CAM_PIN_HREF = 7`, `CAM_PIN_PCLK = 13` (`int8_t`)
- Camera settings: `CAM_XCLK_FREQ_HZ = 20000000` (`uint32_t`), `CAM_FRAME_SIZE = FRAMESIZE_HVGA`, `CAM_PIXEL_FORMAT = PIXFORMAT_YUV422`, `CAM_JPEG_QUALITY = 12` (`uint8_t`), `CAM_FB_COUNT = 2` (`uint8_t`), `CAM_FB_LOCATION = CAMERA_FB_IN_PSRAM`, `CAM_GRAB_MODE = CAMERA_GRAB_LATEST`, `CAM_VFLIP = 1` (`int8_t`), `CAM_HMIRROR = 0` (`int8_t`)
- Reserved I2C bus: `PIN_I2C_SDA = 47`, `PIN_I2C_SCL = 48`, `I2C_BUS_FREQ_HZ = 400000` (`uint8_t`, `uint32_t`)
- Reserved ToF XSHUT: `PIN_TOF_XSHUT_1 = 1`, `PIN_TOF_XSHUT_2 = 2`, `PIN_TOF_XSHUT_3 = 41`, `PIN_TOF_XSHUT_4 = 42` (`uint8_t`)
- Reserved IR break-beam: `PIN_IR_BEAM_1 = 14`, `PIN_IR_BEAM_2 = 38`, `PIN_IR_BEAM_3 = 39`, `PIN_IR_BEAM_4 = 40` (`uint8_t`, falling-edge interrupt capable)
- Reserved WS2812B: `PIN_WS2812B_DATA = 21`, `WS2812B_NUM_LEDS = 12` (`uint8_t`)

### `include/camera_handler.h`
- `esp_err_t camera_init(void)`: Initializes OV5640 with configured parameters in PSRAM; returns `ESP_OK` on success.
- `camera_fb_t* camera_acquire_frame(void)`: Captures snapshot via `esp_camera_fb_get()`; returns pointer to `camera_fb_t` or `nullptr` on failure.
- `void camera_release_frame(camera_fb_t* fb)`: Returns frame buffer to driver pool via `esp_camera_fb_return(fb)`.
- `bool is_camera_initialized(void)`: Returns `true` if camera driver is active.

### `include/async_camera_response.h`
- Class `AsyncCameraResponse : public AsyncAbstractResponse`:
  - Constructor: `AsyncCameraResponse(camera_fb_t* fb, SemaphoreHandle_t mutex)`
  - Destructor: `~AsyncCameraResponse()` releases frame buffer via `camera_release_frame(fb)` and gives mutex via `xSemaphoreGive(mutex)`
  - `size_t _fillBuffer(uint8_t* data, size_t maxLen)`: Copies next slice of JPEG buffer into `data`, updates `_index`, and returns bytes written
  - `bool _sourceValid() const`: Returns `_fb != nullptr`

### `src/main.cpp`
- Includes `include/async_camera_response.h` instead of inline definition.
- Network Task (Core 0): WiFiManager autoConnect ("SortSense-AP") non-blocking, serial IP reporting, AsyncWebServer port 80 lifecycle.
- Hardware Task (Core 1): Camera and sensor management, TWDT yield via `vTaskDelay(1)`.
- Thread safety: `SemaphoreHandle_t cameraMutex` protects camera frame acquisition and release across cores.

## Code Layout
- `platformio.ini`: Dual-target environment (`esp32-s3-devkitc-1` and `native`).
- `include/config.h`: Comprehensive hardware pinouts, camera parameters, peripheral pin reservations, and fixed-width types.
- `include/camera_handler.h`: Camera module public interface declarations with fixed-width types.
- `include/async_camera_response.h`: Decoupled HTTP chunked streaming response class with RAII resource management.
- `src/camera_handler.cpp`: Implementation of camera initialization, frame acquisition, and cleanup logic.
- `src/main.cpp`: FreeRTOS task declarations, core pinning, WiFiManager non-blocking setup, async HTTP server, and route handlers.
- `test/mocks/`: Isolated mock headers for native testing (`esp_camera.h`, `esp_err.h`, `esp_heap_caps.h`, `Arduino.h`, `FreeRTOS.h`, `semphr.h`, `ESPAsyncWebServer.h`).
- `test/test_config/test_config.cpp`: Unit tests for hardware configuration and pin safety invariants.
- `test/test_camera_handler/test_camera_handler.cpp`: Unit tests for camera handler state machine and driver calls.
- `test/test_async_response/test_async_response.cpp`: Unit tests for async streaming buffer pagination and RAII safety.
