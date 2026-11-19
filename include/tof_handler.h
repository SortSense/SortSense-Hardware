/**
 * @file tof_handler.h
 * @brief 4x VL53L0X Time-of-Flight fill-level sensor subsystem for SortSense.
 *
 * Manages 4 VL53L0X laser distance sensors on a shared I2C bus using sequential
 * XSHUT hardware address reassignment (0x30..0x33).
 * Converts distance (mm) to fill percentage (0..100%) and dispatches telemetry
 * payloads to the server via non-blocking HTTP POST.
 *
 * Supports graceful degradation: If only 1 sensor is connected for testing,
 * the subsystem initializes only that sensor without hanging or failing.
 */

#pragma once

#include "config.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Distance sentinel indicating sensor timeout or out-of-range measurement
constexpr uint16_t TOF_DISTANCE_INVALID = 8190;

/**
 * @brief Initialize the I2C bus and all connected VL53L0X ToF sensors.
 * Pulls all XSHUT pins LOW, then brings each up sequentially to assign
 * unique I2C addresses (0x30..0x33).
 * Supports graceful single/partial sensor testing: inactive sensors are marked
 * offline without blocking or failing the system.
 *
 * @return ESP_OK if at least one sensor was successfully initialized.
 *         ESP_ERR_NOT_FOUND if no sensors responded (warning logged).
 */
esp_err_t tof_init(void);

/**
 * @brief Deinitialize ToF sensors by asserting XSHUT LOW on all channels.
 * @return ESP_OK on success.
 */
esp_err_t tof_deinit(void);

/**
 * @brief Check if a specific ToF sensor is currently active and initialized.
 * @param binIndex Sensor index (0..3 matching plastic, paper, metal, general).
 * @return true if sensor is active and available for ranging.
 */
bool tof_is_sensor_active(uint8_t binIndex);

/**
 * @brief Read raw distance in millimeters from a specific bin compartment.
 * @param binIndex Sensor index (0..3).
 * @return Distance in mm, or TOF_DISTANCE_INVALID on error/timeout/inactive.
 */
uint16_t tof_read_distance_mm(uint8_t binIndex);

/**
 * @brief Calculate compartment fill percentage (0..100%) from distance in mm.
 * Distance >= BIN_EMPTY_MM yields 0% (empty).
 * Distance <= BIN_FULL_MM yields 100% (full).
 * Interpolated linearly between empty and full.
 *
 * @param distanceMm Measured distance in millimeters.
 * @return Clamped fill level 0..100%.
 */
uint8_t tof_calculate_fill_percentage(uint16_t distanceMm);

/**
 * @brief Read all 4 compartment fill percentages into an array.
 * @param[out] outFillLevels Array of size TOF_SENSOR_COUNT (4) to store
 * percentages.
 * @return true if at least one sensor successfully returned a reading.
 */
bool tof_read_all_fill_levels(uint8_t outFillLevels[TOF_SENSOR_COUNT]);

/**
 * @brief Dispatch fill levels telemetry JSON via HTTP POST to the server.
 * Contract:
 * {
 *   "station_id": "ST-001",
 *   "fill_levels": {
 *     "plastic": 45,
 *     "paper": 80,
 *     "metal": 20,
 *     "general": 95
 *   }
 * }
 *
 * @param fillLevels Array of 4 fill level percentages.
 * @return true if HTTP POST returned HTTP 200/2xx, false otherwise.
 */
bool tof_send_telemetry(const uint8_t fillLevels[TOF_SENSOR_COUNT]);

#ifdef __cplusplus
}
#endif

