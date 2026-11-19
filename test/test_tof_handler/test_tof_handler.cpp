/**
 * @file test_tof_handler.cpp
 * @brief Unit tests for 4x VL53L0X Time-of-Flight fill-level sensor subsystem.
 */

#include "config.h"
#include "tof_handler.h"
#include <Arduino.h>
#include <Wire.h>
#include <unity.h>

void setUp(void) {
  Wire.reset();
  mock_arduino_reset();
}

void tearDown(void) { tof_deinit(); }

/**
 * TC_TOF_01: Verify all 4 sensors initialize and receive sequential addresses.
 */
void test_tof_init_all_sensors_success(void) {
  Wire.mock_devices_present_mask = 0x0F; // All 4 sensors answer at 0x29

  esp_err_t err = tof_init();
  TEST_ASSERT_EQUAL(ESP_OK, err);

  for (uint8_t i = 0; i < TOF_SENSOR_COUNT; ++i) {
    TEST_ASSERT_TRUE(tof_is_sensor_active(i));
  }
}

/**
 * TC_TOF_02: Verify graceful single-sensor testing (only Sensor 0 connected).
 */
void test_tof_init_single_sensor_graceful(void) {
  Wire.mock_devices_present_mask = 0x01; // Only Sensor 0 present

  esp_err_t err = tof_init();
  TEST_ASSERT_EQUAL(ESP_OK, err);

  TEST_ASSERT_TRUE(tof_is_sensor_active(0));
  TEST_ASSERT_FALSE(tof_is_sensor_active(1));
  TEST_ASSERT_FALSE(tof_is_sensor_active(2));
  TEST_ASSERT_FALSE(tof_is_sensor_active(3));
}

/**
 * TC_TOF_03: Verify handling when no sensors are connected.
 */
void test_tof_init_no_sensors(void) {
  Wire.mock_devices_present_mask = 0x00; // No sensors present

  esp_err_t err = tof_init();
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);

  for (uint8_t i = 0; i < TOF_SENSOR_COUNT; ++i) {
    TEST_ASSERT_FALSE(tof_is_sensor_active(i));
  }
}

/**
 * TC_TOF_04: Verify fill percentage at empty threshold (>= BIN_EMPTY_MM).
 */
void test_tof_fill_percentage_empty(void) {
  TEST_ASSERT_EQUAL_UINT8(0, tof_calculate_fill_percentage(BIN_EMPTY_MM));
  TEST_ASSERT_EQUAL_UINT8(0, tof_calculate_fill_percentage(BIN_EMPTY_MM + 50));
  TEST_ASSERT_EQUAL_UINT8(0, tof_calculate_fill_percentage(1000));
}

/**
 * TC_TOF_05: Verify fill percentage at full threshold (<= BIN_FULL_MM).
 */
void test_tof_fill_percentage_full(void) {
  TEST_ASSERT_EQUAL_UINT8(100, tof_calculate_fill_percentage(BIN_FULL_MM));
  TEST_ASSERT_EQUAL_UINT8(100, tof_calculate_fill_percentage(BIN_FULL_MM - 20));
  TEST_ASSERT_EQUAL_UINT8(100, tof_calculate_fill_percentage(0));
}

/**
 * TC_TOF_06: Verify linear interpolation between empty and full.
 */
void test_tof_fill_percentage_linear(void) {
  // Empty = 400mm, Full = 50mm, Span = 350mm
  // Midpoint: 50 + 175 = 225mm -> 50%
  TEST_ASSERT_EQUAL_UINT8(50, tof_calculate_fill_percentage(225));

  // 75% full: 400 - (350 * 0.75) = 400 - 262.5 = 137mm (or 138mm)
  TEST_ASSERT_EQUAL_UINT8(75, tof_calculate_fill_percentage(137));

  // 25% full: 400 - (350 * 0.25) = 400 - 87.5 = 312mm (or 313mm)
  TEST_ASSERT_EQUAL_UINT8(25, tof_calculate_fill_percentage(313));
}

/**
 * TC_TOF_07: Verify invalid / out-of-range distance returns 0%.
 */
void test_tof_fill_percentage_invalid(void) {
  TEST_ASSERT_EQUAL_UINT8(0,
                          tof_calculate_fill_percentage(TOF_DISTANCE_INVALID));
}

/**
 * TC_TOF_08: Verify reading distance on active vs inactive sensors.
 */
void test_tof_read_distance_active_and_inactive(void) {
  Wire.mock_devices_present_mask = 0x05; // Sensors 0 and 2 active
  tof_init();

  TEST_ASSERT_TRUE(tof_is_sensor_active(0));
  TEST_ASSERT_FALSE(tof_is_sensor_active(1));
  TEST_ASSERT_TRUE(tof_is_sensor_active(2));
  TEST_ASSERT_FALSE(tof_is_sensor_active(3));

  // Active sensor returns distance (mock defaults to 200mm)
  TEST_ASSERT_EQUAL_UINT16(200, tof_read_distance_mm(0));

  // Inactive sensors return TOF_DISTANCE_INVALID
  TEST_ASSERT_EQUAL_UINT16(TOF_DISTANCE_INVALID, tof_read_distance_mm(1));
  TEST_ASSERT_EQUAL_UINT16(TOF_DISTANCE_INVALID, tof_read_distance_mm(3));

  // Out of bounds bin index returns TOF_DISTANCE_INVALID
  TEST_ASSERT_EQUAL_UINT16(TOF_DISTANCE_INVALID, tof_read_distance_mm(4));
  TEST_ASSERT_EQUAL_UINT16(TOF_DISTANCE_INVALID, tof_read_distance_mm(255));
}

/**
 * TC_TOF_09: Verify reading all compartment fill levels.
 */
void test_tof_read_all_fill_levels(void) {
  Wire.mock_devices_present_mask = 0x0F; // All active
  tof_init();

  uint8_t levels[TOF_SENSOR_COUNT] = {0};
  bool ok = tof_read_all_fill_levels(levels);
  TEST_ASSERT_TRUE(ok);

  // Mock distance 200mm -> (400 - 200) / 350 = 200 / 350 = 57%
  TEST_ASSERT_EQUAL_UINT8(57, levels[0]);
  TEST_ASSERT_EQUAL_UINT8(57, levels[1]);
  TEST_ASSERT_EQUAL_UINT8(57, levels[2]);
  TEST_ASSERT_EQUAL_UINT8(57, levels[3]);
}

/**
 * TC_TOF_10: Verify null pointer safety on read_all and send_telemetry.
 */
void test_tof_null_pointer_safety(void) {
  TEST_ASSERT_FALSE(tof_read_all_fill_levels(nullptr));
  TEST_ASSERT_FALSE(tof_send_telemetry(nullptr));
}

/**
 * TC_TOF_11: Verify deinit asserts XSHUT LOW and deactivates all sensors.
 */
void test_tof_deinit_resets_state(void) {
  const uint8_t xshut_pins[TOF_SENSOR_COUNT] = {
      PIN_TOF_XSHUT_1, PIN_TOF_XSHUT_2, PIN_TOF_XSHUT_3, PIN_TOF_XSHUT_4};

  Wire.mock_devices_present_mask = 0x0F;
  tof_init();

  for (uint8_t i = 0; i < TOF_SENSOR_COUNT; ++i) {
    TEST_ASSERT_TRUE(tof_is_sensor_active(i));
  }

  tof_deinit();

  for (uint8_t i = 0; i < TOF_SENSOR_COUNT; ++i) {
    TEST_ASSERT_FALSE(tof_is_sensor_active(i));
    TEST_ASSERT_EQUAL_UINT8(LOW, mock_pin_values[xshut_pins[i]]);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tof_init_all_sensors_success);
  RUN_TEST(test_tof_init_single_sensor_graceful);
  RUN_TEST(test_tof_init_no_sensors);
  RUN_TEST(test_tof_fill_percentage_empty);
  RUN_TEST(test_tof_fill_percentage_full);
  RUN_TEST(test_tof_fill_percentage_linear);
  RUN_TEST(test_tof_fill_percentage_invalid);
  RUN_TEST(test_tof_read_distance_active_and_inactive);
  RUN_TEST(test_tof_read_all_fill_levels);
  RUN_TEST(test_tof_null_pointer_safety);
  RUN_TEST(test_tof_deinit_resets_state);
  return UNITY_END();
}
