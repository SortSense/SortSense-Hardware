/**
 * @file ir_handler.h
 * @brief Interrupt-driven LM393 IR break-beam sensor manager for SortSense
 * trash drop detection.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize IR break-beam pins with internal pullup resistors and
 * attach FALLING edge hardware interrupts.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t ir_init(void);

/**
 * @brief Deinitialize IR break-beam pins and detach all interrupts.
 * @return ESP_OK on success.
 */
esp_err_t ir_deinit(void);

/**
 * @brief Non-blocking poll for trash drop events.
 * If a drop was captured by an interrupt, resets the flag and returns true.
 * @param[out] outBinIndex Pointer to store the detected bin index (0..3).
 * @return true if a drop event was pending, false otherwise.
 */
bool ir_poll_drop_event(int *outBinIndex);

/**
 * @brief Check raw digital state of a specific IR beam sensor.
 * @param binIndex Bin index to inspect (0..3).
 * @return true if beam is currently broken (pin LOW), false if beam is clear
 * (pin HIGH).
 */
bool ir_is_beam_broken(uint8_t binIndex);

/**
 * @brief Transmit HTTP POST /internal/verify-drop telemetry payload to LAN AI
 * server. Contract: {"station_id": "ST-001", "bin_index": <0-3>}
 * @param binIndex Dropped waste bin compartment index (0..3).
 * @return true if HTTP server returned 200 OK, false otherwise.
 */
bool ir_send_verify_drop(int binIndex);

#ifdef __cplusplus
}
#endif

