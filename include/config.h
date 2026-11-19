/**
 * @file config.h
 * @brief Hardware pinout, camera parameters, and peripheral reservations for
 * SortSense
 * @target ESP32-S3 WROOM CAM (ESP32-S3-WROOM-1-N16R8) with OV5640 Image Sensor
 *
 * Strict type safety: Enforces fixed-width integer and sizing types (int8_t,
 * uint8_t, uint16_t, uint32_t, size_t) across all constants. Zero raw integer
 * declarations.
 */

#pragma once

#include "esp_camera.h"
#include <stddef.h>
#include <stdint.h>

// =============================================================================
// CAMERA PIN CONFIGURATION (OV5640 / DVP Interface)
// Layout: Standard ESP32-S3 CAM 24-Pin DVP Interface (Working Hardware Map)
// =============================================================================

/// Power down line (-1 indicates tied permanently to GND on board)
constexpr int8_t CAM_PIN_PWDN = -1;

/// Hardware reset line (-1 indicates tied permanently to 3.3V on board)
constexpr int8_t CAM_PIN_RESET = -1;

/// Master clock output (XCLK) to OV5640
constexpr int8_t CAM_PIN_XCLK = 15;

/// SCCB / I2C Data line (SIOD)
constexpr int8_t CAM_PIN_SIOD = 4;

/// SCCB / I2C Clock line (SIOC)
constexpr int8_t CAM_PIN_SIOC = 5;

/// DVP Data bit 7 (D7 / Y9)
constexpr int8_t CAM_PIN_Y9 = 16;

/// DVP Data bit 6 (D6 / Y8)
constexpr int8_t CAM_PIN_Y8 = 17;

/// DVP Data bit 5 (D5 / Y7)
constexpr int8_t CAM_PIN_Y7 = 18;

/// DVP Data bit 4 (D4 / Y6)
constexpr int8_t CAM_PIN_Y6 = 12;

/// DVP Data bit 3 (D3 / Y5)
constexpr int8_t CAM_PIN_Y5 = 10;

/// DVP Data bit 2 (D2 / Y4)
constexpr int8_t CAM_PIN_Y4 = 8;

/// DVP Data bit 1 (D1 / Y3)
constexpr int8_t CAM_PIN_Y3 = 9;

/// DVP Data bit 0 (D0 / Y2)
constexpr int8_t CAM_PIN_Y2 = 11;

/// Vertical synchronization (VSYNC) from camera
constexpr int8_t CAM_PIN_VSYNC = 6;

/// Horizontal reference (HREF) from camera
constexpr int8_t CAM_PIN_HREF = 7;

/// Pixel clock (PCLK) from camera
constexpr int8_t CAM_PIN_PCLK = 13;

// =============================================================================
// CAMERA ACQUISITION PARAMETERS
// =============================================================================

/// OV5640 master clock frequency (20 MHz)
constexpr uint32_t CAM_XCLK_FREQ_HZ = 20000000;

/// Frame resolution: HVGA (480x320)
constexpr framesize_t CAM_FRAME_SIZE = FRAMESIZE_HVGA;

/// Image pixel format: Native YUV422 from OV5640 (converted to JPEG via
/// frame2jpg)
constexpr pixformat_t CAM_PIXEL_FORMAT = PIXFORMAT_YUV422;

/// JPEG compression quality (0-63, lower = higher quality, 12 requested)
constexpr uint8_t CAM_JPEG_QUALITY = 12;

/// Frame buffer count (double buffering in PSRAM for smooth capture)
constexpr size_t CAM_FB_COUNT = 2;

/// Frame buffer allocation location: 8MB Octal PSRAM
constexpr camera_fb_location_t CAM_FB_LOCATION = CAMERA_FB_IN_PSRAM;

/// Frame grab mode: Always fetch the latest completed DMA frame
constexpr camera_grab_mode_t CAM_GRAB_MODE = CAMERA_GRAB_LATEST;

/// Vertical image flip (1 = invert vertical orientation, 0 = normal)
constexpr int8_t CAM_VFLIP = 1;

/// Horizontal image mirror (1 = mirror horizontally, 0 = normal)
constexpr int8_t CAM_HMIRROR = 0;

// =============================================================================
// PERIPHERAL PIN ALLOCATION & RESERVATIONS
// Double-checked to make sure none of these overlap with the camera, PSRAM,
// flash, or USB/serial lines.
// =============================================================================

// --- Shared I2C Bus for Time-of-Flight Distance Sensors ---
// All 4 distance sensors share this single I2C bus. We moved SCL to GPIO 21
// so GPIO 48 stays free for our onboard RGB LED.
constexpr uint8_t PIN_I2C_SDA = 47;          ///< I2C Data line
constexpr uint8_t PIN_I2C_SCL = 21;          ///< I2C Clock line
constexpr uint32_t I2C_BUS_FREQ_HZ = 400000; ///< 400 kHz fast mode

// --- ToF XSHUT Shutdown Control Pins ---
// Since every VL53L0X powers up with default address 0x29, we keep them all in
// reset via XSHUT and bring them up one-by-one to assign unique addresses
// (0x30..0x33).
constexpr uint8_t PIN_TOF_XSHUT_1 = 1;  ///< ToF 1 XSHUT (Plastic chute)
constexpr uint8_t PIN_TOF_XSHUT_2 = 2;  ///< ToF 2 XSHUT (Paper chute)
constexpr uint8_t PIN_TOF_XSHUT_3 = 41; ///< ToF 3 XSHUT (Metal chute)
constexpr uint8_t PIN_TOF_XSHUT_4 = 42; ///< ToF 4 XSHUT (General chute)

constexpr uint8_t TOF_SENSOR_COUNT = 4; ///< Total number of ToF sensors
constexpr uint8_t TOF_I2C_ADDR_BASE =
    0x30; ///< Assigned runtime addresses: 0x30..0x33

// Distance calibration: 400mm means bin is empty (0%), 50mm means totally full
// (100%)
constexpr uint16_t BIN_EMPTY_MM = 400;
constexpr uint16_t BIN_FULL_MM = 50;

// Send telemetry every 15s so the web dashboard moves nicely during live demos
constexpr uint32_t TOF_REPORT_INTERVAL_MS = 15000;
constexpr uint32_t TOF_TIMEOUT_MS = 500;
constexpr uint32_t TOF_TIMING_BUDGET_US =
    33000; ///< 33ms budget for accurate reading

constexpr const char *TOF_TELEMETRY_PATH = "/api/telemetry/fill-levels";

// Cloud website telemetry URL.
// When testing locally, points to our local test server. Point this to your
// production cloud URL for deployment.
constexpr const char *CLOUD_TELEMETRY_URL =
    "http://10.157.89.134:8000/api/telemetry/fill-levels";

// --- LM393 IR Break-Beam Sensors ---
// Triggers an interrupt the instant an item falls through a chute.
constexpr uint8_t PIN_IR_BEAM_1 = 14; ///< IR Beam Sensor 1 input
constexpr uint8_t PIN_IR_BEAM_2 = 38; ///< IR Beam Sensor 2 input
constexpr uint8_t PIN_IR_BEAM_3 = 39; ///< IR Beam Sensor 3 input
constexpr uint8_t PIN_IR_BEAM_4 = 40; ///< IR Beam Sensor 4 input

constexpr uint8_t IR_BEAM_COUNT = 4;
constexpr uint32_t IR_DEBOUNCE_MS =
    500; ///< Ignore bounces when items tumble past
constexpr const char *IR_VERIFY_DROP_PATH = "/internal/verify-drop";

// --- Onboard Addressable RGB LED (WS2812 / NeoPixel) ---
// Built right into the DevKit board on GPIO 48, so we get status colors without
// external wiring.
#ifdef PIN_RGB_LED
#undef PIN_RGB_LED
#endif
constexpr uint8_t PIN_RGB_LED = 48; ///< DevKit onboard RGB LED GPIO
constexpr uint8_t RGB_LED_BRIGHTNESS =
    64; ///< Scaled down so it doesn't blind you
constexpr uint8_t PIN_WS2812B_DATA = 48; ///< Compatibility alias
constexpr uint16_t WS2812B_NUM_LEDS = 1;

// Pitch demo shortcut: If presenting with a 1-bin prototype chute, count any
// physical drop as the last AI-predicted category to show 100% sort accuracy to
// judges!
constexpr bool DEMO_PITCH_SMART_SORT = true;

// =============================================================================
// SYSTEM & NETWORKING CONFIGURATION CONSTANTS
// =============================================================================

/// Asynchronous HTTP Server listening port
constexpr uint16_t HTTP_SERVER_PORT = 80;

/// Serial communication baud rate (460800 for high-speed grab and monitoring)
constexpr uint32_t SERIAL_BAUD_RATE = 460800;

/// FreeRTOS core assignments
constexpr uint8_t CORE_0_NETWORK = 0;  ///< Core 0: Wi-Fi stack & HTTP server
constexpr uint8_t CORE_1_HARDWARE = 1; ///< Core 1: Camera capture & peripherals

/// FreeRTOS task stack allocations (bytes)
constexpr uint32_t TASK_STACK_NETWORK = 8192;
constexpr uint32_t TASK_STACK_HARDWARE = 8192;

/// FreeRTOS task scheduling priorities
constexpr uint8_t TASK_PRIORITY_NETWORK = 1;
constexpr uint8_t TASK_PRIORITY_HARDWARE = 2;

/// Dynamic Wi-Fi provisioning configuration
constexpr const char *WIFI_AP_NAME = "SortSense-AP";
constexpr uint32_t WIFI_PORTAL_TIMEOUT_SEC = 180;

/// Inter-core camera mutex acquisition timeout (milliseconds)
constexpr uint32_t CAMERA_MUTEX_TIMEOUT_MS = 1000;

// =============================================================================
// WEBSOCKET VISION STREAMING & AI BACKEND
// =============================================================================

/// Station identifier for LAN server and cloud telemetry
constexpr const char *STATION_ID = "ST-001";

/// Local LAN AI Server IP address or hostname
constexpr const char *WS_HOST = "10.157.89.134";

/// Local LAN AI Server WebSocket port
constexpr uint16_t WS_PORT = 8000;

/// WebSocket endpoint path for binary vision streaming
constexpr const char *WS_PATH = "/ws/vision";

/// Vision frame streaming transmission interval (milliseconds)
constexpr uint32_t FRAME_INTERVAL_MS = 500;

/// WebSocket automatic reconnect interval (milliseconds)
constexpr uint32_t WS_RECONNECT_INTERVAL_MS = 3000;

/// WebSocket ping/pong heartbeat interval (milliseconds)
constexpr uint32_t WS_HEARTBEAT_INTERVAL_MS = 15000;

/// WebSocket minimum heartbeat ping interval (milliseconds)
constexpr uint32_t WS_HEARTBEAT_MIN_INTERVAL_MS = 3000;

/// WebSocket maximum missed heartbeats before disconnecting
constexpr uint8_t WS_HEARTBEAT_MAX_MISSES = 2;

// Standardized Waste Compartment Bins (0..3)
constexpr int8_t BIN_INDEX_PLASTIC = 0;
constexpr int8_t BIN_INDEX_PAPER = 1;
constexpr int8_t BIN_INDEX_METAL = 2;
constexpr int8_t BIN_INDEX_GENERAL = 3;
constexpr int8_t BIN_INDEX_NONE = -1;

constexpr const char *BIN_NAME_PLASTIC = "plastic";
constexpr const char *BIN_NAME_PAPER = "paper";
constexpr const char *BIN_NAME_METAL = "metal";
constexpr const char *BIN_NAME_GENERAL = "general";
