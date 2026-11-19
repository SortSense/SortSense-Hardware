/**
 * @file network_handler.h
 * @brief Thread-safe network manager encapsulating Wi-Fi connectivity and
 * WebSocketsClient lifecycle for SortSense vision streaming and AI
 * classification.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Classification hook invoked when AI server returns waste category.
 * @param binIndex Target bin index (0: plastic, 1: paper, 2: metal, 3: general,
 * -1: none)
 * @param category Classification name string (e.g. "plastic")
 * @param confidence Model confidence score (0.0 to 1.0)
 */
typedef void (*ClassificationCallback)(int binIndex, const char *category,
                                       float confidence);

/**
 * @brief Initialize WebSocket client connection parameters and heartbeat.
 * @param host AI server hostname or IP address.
 * @param port AI server WebSocket port.
 * @param path WebSocket endpoint path (e.g. "/ws/vision").
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on invalid parameters.
 */
esp_err_t network_init(const char *host, uint16_t port, const char *path);

/**
 * @brief Service incoming WebSocket packets, heartbeats, and reconnect states.
 * Must be called periodically from network task or main loop.
 */
void network_loop(void);

/**
 * @brief Check if WebSocket client is actively connected to the AI server.
 * @return true if connected, false otherwise.
 */
bool network_is_connected(void);

/**
 * @brief Transmit binary JPEG frame buffer over WebSocket to AI server.
 * Thread-safe: can be called from hardware task while network_loop() runs on
 * Core 0.
 * @param data Pointer to raw JPEG buffer.
 * @param len Byte size of JPEG buffer.
 * @return true if frame was successfully queued for transmission, false
 * otherwise.
 */
bool network_send_frame(const uint8_t *data, size_t len);

/**
 * @brief Register callback function to be invoked on incoming classification
 * results.
 * @param cb Function pointer conforming to ClassificationCallback.
 */
void network_register_classification_callback(ClassificationCallback cb);

/**
 * @brief Parse incoming JSON classification text payload.
 * Exposed for unit testing and internal event handling.
 * Contract: {"category": "plastic", "bin_index": 0, "confidence": 0.91}
 * @param payload Raw payload buffer.
 * @param length Byte size of payload.
 * @param binIndex Output parameter for bin index.
 * @param category Output buffer for category string.
 * @param maxCategoryLen Size of category output buffer.
 * @param confidence Output parameter for confidence.
 * @return true if JSON was valid and parsed successfully, false otherwise.
 */
bool network_parse_classification_json(const uint8_t *payload, size_t length,
                                       int *binIndex, char *category,
                                       size_t maxCategoryLen,
                                       float *confidence);

#ifdef __cplusplus
}
#endif

