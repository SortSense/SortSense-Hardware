/**
 * @file led_handler.h
 * @brief Onboard / Addressable WS2812 RGB LED manager for SortSense visual UX.
 *
 * Drives the ESP32-S3 onboard RGB LED (GPIO 48) using high-speed RMT NRZ
 * pulses. Translates AI classification results into standardized bin indicator
 * colors:
 *   - Plastic (Bin 0): Blue
 *   - Paper   (Bin 1): Green
 *   - Metal   (Bin 2): Yellow
 *   - General (Bin 3): White
 *   - Idle    (-1)   : Soft Cyan Breathing
 *   - Alert / Full   : Pulsing Red
 */

#pragma once

#include "config.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the onboard RGB LED subsystem.
 * Sets default idle color state.
 * @return ESP_OK on success.
 */
esp_err_t led_init(void);

/**
 * @brief Set raw RGB color directly on the LED.
 * @param r Red component (0..255).
 * @param g Green component (0..255).
 * @param b Blue component (0..255).
 */
void led_set_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Set LED color corresponding to waste bin classification.
 * @param binIndex Waste compartment index (-1..3).
 */
void led_set_category(int8_t binIndex);

/**
 * @brief Flash the LED to confirm a physical trash drop verification.
 * Momentarily flashes bright white/green, then restores category color.
 */
void led_flash_drop_confirmed(void);

/**
 * @brief Set or clear high fill-level warning state (>= 90% full).
 * When active, displays pulsing RED.
 * @param alertActive true to show red warning, false to restore normal display.
 */
void led_set_full_warning(bool alertActive);

/**
 * @brief Turn the LED completely off.
 */
void led_turn_off(void);

/**
 * @brief Get currently active bin index displayed on the LED.
 * @return Bin index (-1..3).
 */
int8_t led_get_current_bin(void);

/**
 * @brief Non-blocking LED update loop. Call periodically in loop() for smooth
 * animations (idle breathing, flash recovery).
 */
void led_loop(void);

#ifdef __cplusplus
}
#endif
