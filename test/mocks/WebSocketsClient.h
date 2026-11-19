/**
 * @file WebSocketsClient.h
 * @brief Mock implementation of WebSocketsClient for native unit testing.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
  WStype_ERROR,
  WStype_DISCONNECTED,
  WStype_CONNECTED,
  WStype_TEXT,
  WStype_BIN,
  WStype_FRAGMENT_TEXT_START,
  WStype_FRAGMENT_BIN_START,
  WStype_FRAGMENT,
  WStype_FRAGMENT_FIN,
  WStype_PING,
  WStype_PONG,
} WStype_t;

typedef void (*WebSocketClientEvent)(WStype_t type, uint8_t *payload,
                                     size_t length);

class WebSocketsClient {
public:
  inline static WebSocketClientEvent s_lastCallback = nullptr;
  inline static bool s_mockConnected = false;
  inline static bool s_mockSendBinResult = true;
  inline static size_t s_mockLastSentLen = 0;
  inline static uint32_t s_mockSendBinCallCount = 0;
  inline static uint32_t s_mockLoopCallCount = 0;
  inline static char s_mockHost[64] = {0};
  inline static uint16_t s_mockPort = 0;
  inline static char s_mockPath[64] = {0};

  static void mockReset() {
    s_lastCallback = nullptr;
    s_mockConnected = false;
    s_mockSendBinResult = true;
    s_mockLastSentLen = 0;
    s_mockSendBinCallCount = 0;
    s_mockLoopCallCount = 0;
    s_mockHost[0] = '\0';
    s_mockPort = 0;
    s_mockPath[0] = '\0';
  }

  static void mockTriggerEvent(WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_CONNECTED) {
      s_mockConnected = true;
    } else if (type == WStype_DISCONNECTED) {
      s_mockConnected = false;
    }
    if (s_lastCallback != nullptr) {
      s_lastCallback(type, payload, length);
    }
  }

  void begin(const char *host, uint16_t port, const char *url = "/") {
    if (host != nullptr) {
      snprintf(s_mockHost, sizeof(s_mockHost), "%s", host);
    }
    s_mockPort = port;
    if (url != nullptr) {
      snprintf(s_mockPath, sizeof(s_mockPath), "%s", url);
    }
  }

  void onEvent(WebSocketClientEvent cb) { s_lastCallback = cb; }

  void setReconnectInterval(unsigned long interval) { (void)interval; }

  void enableHeartbeat(uint32_t pingInterval, uint32_t pongTimeout,
                       uint8_t disconnectTimeoutCount) {
    (void)pingInterval;
    (void)pongTimeout;
    (void)disconnectTimeoutCount;
  }

  void loop() { s_mockLoopCallCount++; }

  bool isConnected() { return s_mockConnected; }

  bool sendBIN(const uint8_t *data, size_t len) {
    (void)data;
    s_mockLastSentLen = len;
    s_mockSendBinCallCount++;
    return s_mockSendBinResult;
  }

  bool sendTXT(const char *data) {
    (void)data;
    return true;
  }
};
