/**
 * @file tof_handler.cpp
 * @brief 4x VL53L0X Time-of-Flight fill-level sensor subsystem for SortSense.
 */

#include "tof_handler.h"
#include "config.h"

#include <Arduino.h>
#include <VL53L0X.h>
#include <Wire.h>

#ifndef UNIT_TEST
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif

// =============================================================================
// STATIC STATE & SENSOR INSTANCES
// =============================================================================
static const uint8_t s_xshutPins[TOF_SENSOR_COUNT] = {
    PIN_TOF_XSHUT_1, PIN_TOF_XSHUT_2, PIN_TOF_XSHUT_3, PIN_TOF_XSHUT_4};

static const char *const s_binNames[TOF_SENSOR_COUNT] = {
    BIN_NAME_PLASTIC, BIN_NAME_PAPER, BIN_NAME_METAL, BIN_NAME_GENERAL};

static VL53L0X s_sensors[TOF_SENSOR_COUNT];
static bool s_sensorActive[TOF_SENSOR_COUNT] = {false, false, false, false};

// =============================================================================
// PUBLIC INTERFACES
// =============================================================================
esp_err_t tof_init(void) {
  // Step 1: Put all sensors into hardware shutdown by asserting XSHUT LOW
  for (uint8_t i = 0; i < TOF_SENSOR_COUNT; ++i) {
    pinMode(s_xshutPins[i], OUTPUT);
    digitalWrite(s_xshutPins[i], LOW);
    s_sensorActive[i] = false;
  }
  delay(20); // Allow sensors to enter power-down state

  // Step 2: Initialize shared I2C bus
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_BUS_FREQ_HZ);
  Wire.setTimeOut(TOF_TIMEOUT_MS);

  // Step 3: Sequentially wake each sensor, verify communication, and assign I2C
  // address
  uint8_t activeCount = 0;

  for (uint8_t i = 0; i < TOF_SENSOR_COUNT; ++i) {
    // Wake up sensor i
    digitalWrite(s_xshutPins[i], HIGH);
    delay(15); // Wait for sensor MCU boot to default address 0x29

#ifdef UNIT_TEST
    Wire.mock_current_sensor_idx = i;
#endif

    // Probe default address 0x29 to verify sensor is physically present
    Wire.beginTransmission(0x29);
    uint8_t probeErr = Wire.endTransmission();
    if (probeErr != 0) {
      Serial.printf("[ToF] Sensor %u (%s, GPIO %u) not detected at 0x29 (I2C "
                    "code %u). Skipping.\n",
                    i, s_binNames[i], s_xshutPins[i], probeErr);
      s_sensorActive[i] = false;
      digitalWrite(s_xshutPins[i],
                   LOW); // Return to shutdown to prevent bus noise
      continue;
    }

    s_sensors[i].setBus(&Wire);
    s_sensors[i].setTimeout(TOF_TIMEOUT_MS);

    if (!s_sensors[i].init()) {
      Serial.printf("[ToF] Sensor %u (%s) init() failed. Marking inactive.\n",
                    i, s_binNames[i]);
      s_sensorActive[i] = false;
      digitalWrite(s_xshutPins[i], LOW);
      continue;
    }

    s_sensors[i].setMeasurementTimingBudget(TOF_TIMING_BUDGET_US);

    // Assign unique I2C address (0x30 + i) so default 0x29 is freed for the
    // next sensor
    uint8_t newAddr = static_cast<uint8_t>(TOF_I2C_ADDR_BASE + i);
    s_sensors[i].setAddress(newAddr);
    s_sensorActive[i] = true;
    activeCount++;

    Serial.printf(
        "[ToF] Sensor %u (%s, GPIO %u) online at I2C address 0x%02X\n", i,
        s_binNames[i], s_xshutPins[i], newAddr);
  }

  Serial.printf("[ToF] Initialized %u of %u ToF sensors.\n", activeCount,
                TOF_SENSOR_COUNT);

  return (activeCount > 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t tof_deinit(void) {
  for (uint8_t i = 0; i < TOF_SENSOR_COUNT; ++i) {
    digitalWrite(s_xshutPins[i], LOW);
    s_sensorActive[i] = false;
  }
  Serial.println("[ToF] Asserted XSHUT LOW on all sensors.");
  return ESP_OK;
}

bool tof_is_sensor_active(uint8_t binIndex) {
  if (binIndex >= TOF_SENSOR_COUNT) {
    return false;
  }
  return s_sensorActive[binIndex];
}

uint16_t tof_read_distance_mm(uint8_t binIndex) {
  if (binIndex >= TOF_SENSOR_COUNT || !s_sensorActive[binIndex]) {
    return TOF_DISTANCE_INVALID;
  }

  uint16_t dist = s_sensors[binIndex].readRangeSingleMillimeters();
  if (s_sensors[binIndex].timeoutOccurred() || dist >= TOF_DISTANCE_INVALID) {
    return TOF_DISTANCE_INVALID;
  }
  return dist;
}

uint8_t tof_calculate_fill_percentage(uint16_t distanceMm) {
  if (distanceMm == TOF_DISTANCE_INVALID || distanceMm >= BIN_EMPTY_MM) {
    return 0;
  }
  if (distanceMm <= BIN_FULL_MM) {
    return 100;
  }

  uint32_t usableSpan = BIN_EMPTY_MM - BIN_FULL_MM;
  if (usableSpan == 0) {
    return 0;
  }

  uint32_t fillMm = BIN_EMPTY_MM - distanceMm;
  uint32_t percent = (fillMm * 100 + usableSpan / 2) / usableSpan;
  return (percent > 100) ? 100 : static_cast<uint8_t>(percent);
}

bool tof_read_all_fill_levels(uint8_t outFillLevels[TOF_SENSOR_COUNT]) {
  if (outFillLevels == nullptr) {
    return false;
  }

  bool anyValid = false;
  for (uint8_t i = 0; i < TOF_SENSOR_COUNT; ++i) {
    if (s_sensorActive[i]) {
      uint16_t dist = tof_read_distance_mm(i);
      if (dist != TOF_DISTANCE_INVALID) {
        outFillLevels[i] = tof_calculate_fill_percentage(dist);
        anyValid = true;
      } else {
        outFillLevels[i] = 0;
      }
    } else {
      outFillLevels[i] = 0;
    }
  }
  return anyValid;
}

bool tof_send_telemetry(const uint8_t fillLevels[TOF_SENSOR_COUNT]) {
  if (fillLevels == nullptr) {
    return false;
  }

#ifdef UNIT_TEST
  return true;
#else
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ToF] Wi-Fi disconnected; unable to send telemetry.");
    return false;
  }

  HTTPClient http;
  bool isHttps = (strncmp(CLOUD_TELEMETRY_URL, "https://", 8) == 0);
  bool beginOk = false;

  WiFiClientSecure secureClient;
  WiFiClient plainClient;

  if (isHttps) {
    secureClient.setInsecure();
    beginOk = http.begin(secureClient, CLOUD_TELEMETRY_URL);
  } else {
    beginOk = http.begin(plainClient, CLOUD_TELEMETRY_URL);
  }

  if (!beginOk) {
    Serial.printf("[ToF] Failed to initialize HTTP client for %s\n",
                  CLOUD_TELEMETRY_URL);
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  char payload[192];
  snprintf(payload, sizeof(payload),
           "{\"station_id\":\"%s\",\"fill_levels\":{\"plastic\":%u,\"paper\":%"
           "u,\"metal\":%u,\"general\":%u}}",
           STATION_ID, fillLevels[BIN_INDEX_PLASTIC],
           fillLevels[BIN_INDEX_PAPER], fillLevels[BIN_INDEX_METAL],
           fillLevels[BIN_INDEX_GENERAL]);

  int httpCode = http.POST(payload);
  bool success = (httpCode >= 200 && httpCode < 300);

  if (success) {
    Serial.printf(
        "[ToF HTTP] Telemetry sent: Plastic:%u%% Paper:%u%% Metal:%u%% "
        "General:%u%% (HTTP %d)\n",
        fillLevels[BIN_INDEX_PLASTIC], fillLevels[BIN_INDEX_PAPER],
        fillLevels[BIN_INDEX_METAL], fillLevels[BIN_INDEX_GENERAL], httpCode);
  } else {
    Serial.printf("[ToF HTTP] Telemetry POST to %s returned HTTP %d\n",
                  CLOUD_TELEMETRY_URL, httpCode);
  }

  http.end();
  return success;
#endif
}
