/**
 * @file led_handler.cpp
 * @brief Onboard / Addressable WS2812 RGB LED manager for SortSense visual UX.
 */

#include "led_handler.h"
#include "config.h"

#include <Arduino.h>

// =============================================================================
// STATIC STATE & TIMING
// =============================================================================
static int8_t s_currentBinIndex = BIN_INDEX_NONE;
static bool s_fullWarning = false;
static bool s_isFlashing = false;
static uint32_t s_flashEndTime = 0;

static uint8_t s_activeR = 0;
static uint8_t s_activeG = 0;
static uint8_t s_activeB = 0;

// =============================================================================
// INTERNAL HELPERS
// =============================================================================
static inline uint8_t scale_val(uint8_t val) {
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(val) * RGB_LED_BRIGHTNESS) / 255);
}

static void apply_hardware_rgb(uint8_t r, uint8_t g, uint8_t b) {
  s_activeR = r;
  s_activeG = g;
  s_activeB = b;
  neopixelWrite(PIN_RGB_LED, scale_val(r), scale_val(g), scale_val(b));
}

// =============================================================================
// PUBLIC INTERFACES
// =============================================================================
esp_err_t led_init(void) {
  s_currentBinIndex = BIN_INDEX_NONE;
  s_fullWarning = false;
  s_isFlashing = false;
  s_flashEndTime = 0;

  // Set default idle state (Dim Cyan)
  apply_hardware_rgb(0, 20, 35);

  Serial.printf(
      "[LED] Initialized onboard RGB LED on GPIO %u (Brightness %u/255)\n",
      PIN_RGB_LED, RGB_LED_BRIGHTNESS);
  return ESP_OK;
}

void led_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
  s_isFlashing = false;
  apply_hardware_rgb(r, g, b);
}

void led_set_category(int8_t binIndex) {
  s_currentBinIndex = binIndex;

  if (s_fullWarning) {
    apply_hardware_rgb(255, 0, 0); // Full alert takes precedence
    return;
  }

  if (s_isFlashing) {
    return; // Drop flash in progress; will restore when flash completes
  }

  switch (binIndex) {
  case BIN_INDEX_PLASTIC: // 0 -> Plastic (Blue)
    apply_hardware_rgb(0, 60, 255);
    break;
  case BIN_INDEX_PAPER: // 1 -> Paper (Green)
    apply_hardware_rgb(0, 255, 30);
    break;
  case BIN_INDEX_METAL: // 2 -> Metal (Yellow / Amber)
    apply_hardware_rgb(255, 180, 0);
    break;
  case BIN_INDEX_GENERAL: // 3 -> General (White)
    apply_hardware_rgb(220, 220, 220);
    break;
  default: // -1 -> None / Idle (Soft Cyan)
    apply_hardware_rgb(0, 15, 25);
    break;
  }
}

void led_flash_drop_confirmed(void) {
  s_isFlashing = true;
  s_flashEndTime =
      static_cast<uint32_t>(millis()) + 600; // 600ms bright confirmation flash
  neopixelWrite(PIN_RGB_LED, scale_val(255), scale_val(255), scale_val(255));
}

void led_set_full_warning(bool alertActive) {
  s_fullWarning = alertActive;
  if (s_fullWarning) {
    apply_hardware_rgb(255, 0, 0);
  } else {
    led_set_category(s_currentBinIndex);
  }
}

void led_turn_off(void) {
  s_isFlashing = false;
  apply_hardware_rgb(0, 0, 0);
}

int8_t led_get_current_bin(void) { return s_currentBinIndex; }

void led_loop(void) {
  const uint32_t now = static_cast<uint32_t>(millis());

  // Check if the drop confirmation flash has finished
  if (s_isFlashing) {
    if (now >= s_flashEndTime) {
      s_isFlashing = false;
      led_set_category(s_currentBinIndex);
    }
    return;
  }

  // Red pulsing animation when a bin is getting full
  if (s_fullWarning) {
    static uint32_t lastAlertStep = 0;
    if (now - lastAlertStep >= 40) {
      lastAlertStep = now;
      uint32_t cycleStep = (now / 20) % 50;
      uint8_t redIntensity = (cycleStep < 25) ? (100 + cycleStep * 6)
                                              : (250 - (cycleStep - 25) * 6);
      neopixelWrite(PIN_RGB_LED, scale_val(redIntensity), 0, 0);
    }
    return;
  }

  // Gentle breathing glow when waiting for trash
  if (s_currentBinIndex == BIN_INDEX_NONE) {
    static uint32_t lastAnimTime = 0;
    if (now - lastAnimTime >= 35) {
      lastAnimTime = now;
      uint32_t step = (now / 30) % 60;
      uint8_t br = (step < 30) ? (5 + step) : (65 - step);
      neopixelWrite(PIN_RGB_LED, 0, scale_val(br / 2), scale_val(br));
    }
  }
}
