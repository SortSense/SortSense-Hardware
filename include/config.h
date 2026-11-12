/**
 * @file config.h
 * @brief Hardware pinout, camera parameters, and peripheral reservations for SortSense
 * @target ESP32-S3 WROOM CAM (ESP32-S3-WROOM-1-N16R8) with OV5640 Image Sensor
 * 
 * Strict type safety: Enforces fixed-width integer and sizing types (int8_t, uint8_t,
 * uint16_t, uint32_t, size_t) across all constants. Zero raw integer declarations.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_camera.h"

// =============================================================================
// CAMERA PIN CONFIGURATION (OV5640 / DVP Interface)
// Layout: Standard ESP32-S3 CAM (XCLK=10, SIOD=40, SIOC=39)
// =============================================================================

/// Power down line (-1 indicates tied permanently to GND on board)
constexpr int8_t CAM_PIN_PWDN        = -1;

/// Hardware reset line (-1 indicates tied permanently to 3.3V on board)
constexpr int8_t CAM_PIN_RESET       = -1;

/// Master clock output (XCLK) to OV5640
constexpr int8_t CAM_PIN_XCLK        = 10;

/// SCCB / I2C Data line (SIOD)
constexpr int8_t CAM_PIN_SIOD        = 40;

/// SCCB / I2C Clock line (SIOC)
constexpr int8_t CAM_PIN_SIOC        = 39;

/// DVP Data bit 7 (D7 / Y9)
constexpr int8_t CAM_PIN_Y9          = 48;

/// DVP Data bit 6 (D6 / Y8)
constexpr int8_t CAM_PIN_Y8          = 11;

/// DVP Data bit 5 (D5 / Y7)
constexpr int8_t CAM_PIN_Y7          = 12;

/// DVP Data bit 4 (D4 / Y6)
constexpr int8_t CAM_PIN_Y6          = 14;

/// DVP Data bit 3 (D3 / Y5)
constexpr int8_t CAM_PIN_Y5          = 16;

/// DVP Data bit 2 (D2 / Y4)
constexpr int8_t CAM_PIN_Y4          = 18;

/// DVP Data bit 1 (D1 / Y3)
constexpr int8_t CAM_PIN_Y3          = 17;

/// DVP Data bit 0 (D0 / Y2)
constexpr int8_t CAM_PIN_Y2          = 15;

/// Vertical synchronization (VSYNC) from camera
constexpr int8_t CAM_PIN_VSYNC       = 38;

/// Horizontal reference (HREF) from camera
constexpr int8_t CAM_PIN_HREF        = 47;

/// Pixel clock (PCLK) from camera
constexpr int8_t CAM_PIN_PCLK        = 13;

// =============================================================================
// CAMERA ACQUISITION PARAMETERS
// =============================================================================

/// OV5640 master clock frequency (20 MHz)
constexpr uint32_t CAM_XCLK_FREQ_HZ           = 20000000;

/// Frame resolution: VGA (640x480)
constexpr framesize_t CAM_FRAME_SIZE          = FRAMESIZE_VGA;

/// Image pixel format: JPEG compressed stream
constexpr pixformat_t CAM_PIXEL_FORMAT        = PIXFORMAT_JPEG;

/// JPEG compression quality (0-63, lower = higher quality, 12 requested)
constexpr uint8_t CAM_JPEG_QUALITY            = 12;

/// Frame buffer count (double buffering in PSRAM for smooth capture)
constexpr size_t CAM_FB_COUNT                 = 2;

/// Frame buffer allocation location: 8MB Octal PSRAM
constexpr camera_fb_location_t CAM_FB_LOCATION = CAMERA_FB_IN_PSRAM;

/// Frame grab mode: Always fetch the latest completed DMA frame
constexpr camera_grab_mode_t CAM_GRAB_MODE     = CAMERA_GRAB_LATEST;

// =============================================================================
// PERIPHERAL PIN ALLOCATION & RESERVATIONS (SORTSENSE SMART SYSTEM)
// Mathematical verification: Zero overlap with Camera, PSRAM, Flash, or USB/UART
// =============================================================================

// --- 1. Shared I2C Bus for 4x VL53L0X Time-of-Flight Distance Sensors ---
// Physical header: Header J1 (Pins J1-12 and J1-15 - ESP32-S3 default)
constexpr uint8_t PIN_I2C_SDA        = 8;      ///< I2C Data line
constexpr uint8_t PIN_I2C_SCL        = 9;      ///< I2C Clock line
constexpr uint32_t I2C_BUS_FREQ_HZ   = 400000; ///< 400 kHz Fast-Mode bus clock

// --- 2. 4x VL53L0X Digital Output XSHUT Shutdown Pins ---
// Physical header: Header J3 (Pins J3-4, J3-5, J3-7, J3-6)
constexpr uint8_t PIN_TOF_XSHUT_1    = 1;      ///< ToF Sensor 1 XSHUT (Chute 1)
constexpr uint8_t PIN_TOF_XSHUT_2    = 2;      ///< ToF Sensor 2 XSHUT (Chute 2)
constexpr uint8_t PIN_TOF_XSHUT_3    = 41;     ///< ToF Sensor 3 XSHUT (Chute 3)
constexpr uint8_t PIN_TOF_XSHUT_4    = 42;     ///< ToF Sensor 4 XSHUT (Chute 4)

constexpr uint8_t TOF_SENSOR_COUNT   = 4;      ///< Total number of ToF sensors
constexpr uint8_t TOF_I2C_ADDR_BASE  = 0x30;   ///< Base address (assigned 0x30..0x33)

// --- 3. 4x LM393 IR Break-Beam Sensor Inputs (Interrupt Capable) ---
// Physical header: Header J1 (Pins J1-4, J1-5, J1-6, J1-7)
constexpr uint8_t PIN_IR_BEAM_1      = 4;      ///< IR Beam Sensor 1 input
constexpr uint8_t PIN_IR_BEAM_2      = 5;      ///< IR Beam Sensor 2 input
constexpr uint8_t PIN_IR_BEAM_3      = 6;      ///< IR Beam Sensor 3 input
constexpr uint8_t PIN_IR_BEAM_4      = 7;      ///< IR Beam Sensor 4 input

constexpr uint8_t IR_BEAM_COUNT      = 4;      ///< Total number of IR sensors

// --- 4. 12x WS2812B Addressable RGB LED Status Indicator Strip ---
// Physical header: Header J3 (Pin J3-18 driven via RMT / FastLED)
constexpr uint8_t PIN_WS2812B_DATA   = 21;     ///< WS2812B single-wire NRZ data pin
constexpr uint16_t WS2812B_NUM_LEDS  = 12;     ///< Number of cascaded LEDs

// =============================================================================
// SYSTEM & NETWORKING CONFIGURATION CONSTANTS
// =============================================================================

/// Asynchronous HTTP Server listening port
constexpr uint16_t HTTP_SERVER_PORT            = 80;

/// Serial communication baud rate
constexpr uint32_t SERIAL_BAUD_RATE            = 115200;

/// FreeRTOS core assignments
constexpr uint8_t CORE_0_NETWORK               = 0;  ///< Core 0: Wi-Fi stack & HTTP server
constexpr uint8_t CORE_1_HARDWARE              = 1;  ///< Core 1: Camera capture & peripherals

/// FreeRTOS task stack allocations (bytes)
constexpr uint32_t TASK_STACK_NETWORK          = 8192;
constexpr uint32_t TASK_STACK_HARDWARE         = 8192;

/// FreeRTOS task scheduling priorities
constexpr uint8_t TASK_PRIORITY_NETWORK        = 1;
constexpr uint8_t TASK_PRIORITY_HARDWARE       = 2;

/// Dynamic Wi-Fi provisioning configuration
constexpr const char* WIFI_AP_NAME             = "SortSense-AP";
constexpr uint32_t WIFI_PORTAL_TIMEOUT_SEC     = 180;

/// Inter-core camera mutex acquisition timeout (milliseconds)
constexpr uint32_t CAMERA_MUTEX_TIMEOUT_MS     = 1000;
