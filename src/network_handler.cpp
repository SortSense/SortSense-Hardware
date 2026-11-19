/**
 * @file network_handler.cpp
 * @brief Thread-safe network manager encapsulating Wi-Fi connectivity and
 * WebSocketsClient lifecycle for SortSense vision streaming and AI
 * classification.
 */

#include "network_handler.h"
#include "config.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#ifdef UNIT_TEST
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#else
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

// =============================================================================
// STATIC STATE & INSTANCES
// =============================================================================
static WebSocketsClient s_webSocket;
static SemaphoreHandle_t s_wsMutex = nullptr;
static ClassificationCallback s_classificationCallback = nullptr;
static volatile bool s_wsConnected = false;
static char s_wsHost[64] = {0};
static uint16_t s_wsPort = 0;
static char s_wsPath[64] = {0};

// =============================================================================
// WEBSOCKET CLIENT EVENT DISPATCHER
// =============================================================================
static void onWebSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
  case WStype_DISCONNECTED:
    s_wsConnected = false;
    Serial.println("[WS] Disconnected from AI vision server.");
    break;

  case WStype_CONNECTED:
    s_wsConnected = true;
    Serial.printf("[WS] Connected to AI vision server at %s:%u%s\n", s_wsHost,
                  s_wsPort, s_wsPath);
    break;

  case WStype_TEXT: {
    int binIndex = -1;
    char category[32] = {0};
    float confidence = 0.0f;

    if (network_parse_classification_json(payload, length, &binIndex, category,
                                          sizeof(category), &confidence)) {
      Serial.printf("[AI RESULT] Category: %s | Bin: %d | Conf: %.2f\n",
                    category, binIndex, confidence);

      if (s_classificationCallback != nullptr) {
        s_classificationCallback(binIndex, category, confidence);
      }
    } else {
      Serial.printf("[WS] Received unparseable JSON (len=%u)\n",
                    static_cast<uint32_t>(length));
    }
    break;
  }

  case WStype_BIN:
    Serial.printf("[WS] Received unexpected binary payload (%u bytes)\n",
                  static_cast<uint32_t>(length));
    break;

  case WStype_ERROR:
    Serial.println("[WS] WebSocket connection error encountered.");
    break;

  default:
    break;
  }
}

// =============================================================================
// PUBLIC INTERFACE IMPLEMENTATIONS
// =============================================================================
esp_err_t network_init(const char *host, uint16_t port, const char *path) {
  if (host == nullptr || path == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }

  if (s_wsMutex == nullptr) {
    s_wsMutex = xSemaphoreCreateMutex();
    if (s_wsMutex == nullptr) {
      return ESP_ERR_NO_MEM;
    }
  }

  snprintf(s_wsHost, sizeof(s_wsHost), "%s", host);
  s_wsPort = port;
  snprintf(s_wsPath, sizeof(s_wsPath), "%s", path);

  s_webSocket.begin(s_wsHost, s_wsPort, s_wsPath);
  s_webSocket.onEvent(onWebSocketEvent);
  s_webSocket.setReconnectInterval(WS_RECONNECT_INTERVAL_MS);
  s_webSocket.enableHeartbeat(WS_HEARTBEAT_INTERVAL_MS,
                              WS_HEARTBEAT_MIN_INTERVAL_MS,
                              WS_HEARTBEAT_MAX_MISSES);

  Serial.printf("[WS] Initialized WebSocket client targeting %s:%u%s\n",
                s_wsHost, s_wsPort, s_wsPath);
  return ESP_OK;
}

void network_loop(void) {
  if (s_wsMutex != nullptr) {
    if (xSemaphoreTake(s_wsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      s_webSocket.loop();
      xSemaphoreGive(s_wsMutex);
    }
  } else {
    s_webSocket.loop();
  }
}

bool network_is_connected(void) { return s_wsConnected; }

bool network_send_frame(const uint8_t *data, size_t len) {
  if (!s_wsConnected || data == nullptr || len == 0) {
    return false;
  }

  if (s_wsMutex != nullptr) {
    if (xSemaphoreTake(s_wsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      bool ok = s_webSocket.sendBIN(data, len);
      xSemaphoreGive(s_wsMutex);
      return ok;
    }
    return false;
  }

  return s_webSocket.sendBIN(data, len);
}

void network_register_classification_callback(ClassificationCallback cb) {
  s_classificationCallback = cb;
}

bool network_parse_classification_json(const uint8_t *payload, size_t length,
                                       int *binIndex, char *category,
                                       size_t maxCategoryLen,
                                       float *confidence) {
  if (payload == nullptr || length == 0) {
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    return false;
  }

  if (binIndex != nullptr) {
    *binIndex = doc["bin_index"] | -1;
  }

  if (category != nullptr && maxCategoryLen > 0) {
    const char *catStr = doc["category"] | "none";
    strncpy(category, catStr, maxCategoryLen - 1);
    category[maxCategoryLen - 1] = '\0';
  }

  if (confidence != nullptr) {
    *confidence = doc["confidence"] | 0.0f;
  }

  return true;
}
