/**
 * @file camera_handler.h
 * @brief Public interface for OV5640 camera initialization and frame acquisition.
 * @target ESP32-S3 WROOM CAM (ESP32-S3-WROOM-1-N16R8) with OV5640 Image Sensor
 * 
 * Strict type safety: Enforces fixed-width integer and sizing types (uint8_t, uint16_t,
 * uint32_t, int8_t, size_t) across all function signatures and declarations.
 * Zero raw integer declarations.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the OV5640 camera driver using hardware parameters from config.h.
 * 
 * Validates external PSRAM presence, populates camera_config_t structure,
 * configures DVP GPIO lines and XCLK generation, and verifies image sensor communication.
 * 
 * @return esp_err_t ESP_OK on success, ESP_ERR_NOT_FOUND if PSRAM is missing,
 *                   or specific ESP-IDF camera driver error code on failure.
 */
esp_err_t camera_init(void);

/**
 * @brief Deinitializes the camera driver and releases allocated hardware resources.
 * 
 * @return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_STATE if not initialized.
 */
esp_err_t camera_deinit(void);

/**
 * @brief Acquires the latest captured frame buffer from the camera DMA queue.
 * 
 * This call must be paired with camera_release_frame() to return the buffer to the driver.
 * When called across multiple tasks/cores, access should be protected via mutex.
 * 
 * @return camera_fb_t* Pointer to the frame buffer, or nullptr on failure or if uninitialized.
 */
camera_fb_t* camera_acquire_frame(void);

/**
 * @brief Returns an acquired frame buffer back to the driver reuse pool.
 * 
 * Must be called for every buffer returned by camera_acquire_frame(). Safe to pass nullptr.
 * 
 * @param fb Pointer to camera_fb_t previously obtained from camera_acquire_frame().
 */
void camera_release_frame(camera_fb_t* fb);

/**
 * @brief Queries whether the camera driver is currently initialized and operational.
 * 
 * @return bool true if initialized, false otherwise.
 */
bool is_camera_initialized(void);

#ifdef __cplusplus
}
#endif
