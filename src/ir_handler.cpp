/**
 * @file ir_handler.cpp
 * @brief Interrupt-driven LM393 IR break-beam sensor manager for SortSense
 * trash drop detection.
 */

#include "ir_handler.h"
#include "config.h"

#include <Arduino.h>

#ifndef UNIT_TEST
#include <HTTPClient.h>
#include <WiFi.h>
#endif

// =============================================================================
// STATIC STATE & INTERRUPT TRACKING
// =============================================================================
static const uint8_t s_irPins[IR_BEAM_COUNT] = {PIN_IR_BEAM_1, PIN_IR_BEAM_2,
                                                PIN_IR_BEAM_3, PIN_IR_BEAM_4};

static volatile uint32_t s_lastTriggerTime[IR_BEAM_COUNT] = {0, 0, 0, 0};
static volatile bool s_dropDetected = false;
static volatile int s_lastDroppedBin = -1;
static portMUX_TYPE s_irMux = portMUX_INITIALIZER_UNLOCKED;

// =============================================================================
// INTERRUPT SERVICE ROUTINES (IRAM_ATTR)
// =============================================================================
static inline void IRAM_ATTR handle_ir_interrupt(uint8_t bin) {
  uint32_t now = static_cast<uint32_t>(millis());
  if (now - s_lastTriggerTime[bin] >= IR_DEBOUNCE_MS) {
    s_lastTriggerTime[bin] = now;
    portENTER_CRITICAL_ISR(&s_irMux);
    s_lastDroppedBin = static_cast<int>(bin);
    s_dropDetected = true;
    portEXIT_CRITICAL_ISR(&s_irMux);
  }
}

static void IRAM_ATTR isr_bin_0(void) { handle_ir_interrupt(0); }
static void IRAM_ATTR isr_bin_1(void) { handle_ir_interrupt(1); }
static void IRAM_ATTR isr_bin_2(void) { handle_ir_interrupt(2); }
static void IRAM_ATTR isr_bin_3(void) { handle_ir_interrupt(3); }

// =============================================================================
// PUBLIC INTERFACES
// =============================================================================
esp_err_t ir_init(void) {
  // Reset drop event states
  portENTER_CRITICAL(&s_irMux);
  s_dropDetected = false;
  s_lastDroppedBin = -1;
  for (uint8_t i = 0; i < IR_BEAM_COUNT; ++i) {
    s_lastTriggerTime[i] = 0;
  }
  portEXIT_CRITICAL(&s_irMux);

  // Configure all 4 IR pins as INPUT_PULLUP
  for (uint8_t i = 0; i < IR_BEAM_COUNT; ++i) {
    pinMode(s_irPins[i], INPUT_PULLUP);
  }

  // Attach FALLING edge hardware interrupts
  attachInterrupt(digitalPinToInterrupt(s_irPins[0]), isr_bin_0, FALLING);
  attachInterrupt(digitalPinToInterrupt(s_irPins[1]), isr_bin_1, FALLING);
  attachInterrupt(digitalPinToInterrupt(s_irPins[2]), isr_bin_2, FALLING);
  attachInterrupt(digitalPinToInterrupt(s_irPins[3]), isr_bin_3, FALLING);

  Serial.printf("[IR] Initialized %u LM393 sensors on GPIOs: %u, %u, %u, %u "
                "(FALLING interrupt, %ums debounce)\n",
                IR_BEAM_COUNT, s_irPins[0], s_irPins[1], s_irPins[2],
                s_irPins[3], IR_DEBOUNCE_MS);
  return ESP_OK;
}

esp_err_t ir_deinit(void) {
  for (uint8_t i = 0; i < IR_BEAM_COUNT; ++i) {
    detachInterrupt(digitalPinToInterrupt(s_irPins[i]));
  }
  portENTER_CRITICAL(&s_irMux);
  s_dropDetected = false;
  s_lastDroppedBin = -1;
  portEXIT_CRITICAL(&s_irMux);

  Serial.println("[IR] Detached all IR break-beam interrupts.");
  return ESP_OK;
}

bool ir_poll_drop_event(int *outBinIndex) {
  if (!s_dropDetected) {
    return false;
  }

  portENTER_CRITICAL(&s_irMux);
  int bin = s_lastDroppedBin;
  s_dropDetected = false;
  portEXIT_CRITICAL(&s_irMux);

  if (outBinIndex != nullptr) {
    *outBinIndex = bin;
  }
  return true;
}

bool ir_is_beam_broken(uint8_t binIndex) {
  if (binIndex >= IR_BEAM_COUNT) {
    return false;
  }
  // LM393 output goes LOW when the beam is broken/obscured
  return digitalRead(s_irPins[binIndex]) == LOW;
}

bool ir_send_verify_drop(int binIndex) {
  if (binIndex < 0 || binIndex >= IR_BEAM_COUNT) {
    return false;
  }

#ifdef UNIT_TEST
  (void)binIndex;
  return true;
#else
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[IR] Wi-Fi disconnected; unable to send verify-drop.");
    return false;
  }

  HTTPClient http;
  char url[128];
  snprintf(url, sizeof(url), "http://%s:%u%s", WS_HOST, WS_PORT,
           IR_VERIFY_DROP_PATH);

  if (!http.begin(url)) {
    Serial.printf("[IR] Failed to initialize HTTP client for %s\n", url);
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  char payload[96];
  snprintf(payload, sizeof(payload), "{\"station_id\":\"%s\",\"bin_index\":%d}",
           STATION_ID, binIndex);

  int httpCode = http.POST(payload);
  bool success = (httpCode >= 200 && httpCode < 300);

  if (success) {
    Serial.printf("[IR HTTP] Verified drop for Bin %d on server (HTTP %d)\n",
                  binIndex, httpCode);
  } else {
    Serial.printf("[IR HTTP] Drop verification returned HTTP %d\n", httpCode);
  }

  http.end();
  return success;
#endif
}
