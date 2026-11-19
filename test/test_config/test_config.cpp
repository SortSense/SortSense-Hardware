/**
 * @file test_config.cpp
 * @brief Unit tests for SortSense hardware configuration, pinout safety, and
 * parameter invariants.
 *
 * Verifies all 14 test cases (TC_CONF_01 to TC_CONF_14) formulated by
 * Explorer 3.
 */

#include <set>
#include <string.h>
#include <type_traits>
#include <unity.h>
#include <vector>

#include "config.h"

void setUp(void) {}
void tearDown(void) {}

// Helper to gather all 25 active GPIO assignments
static std::vector<int8_t> get_all_active_gpios() {
  return {
      // Camera pins (14 active)
      CAM_PIN_XCLK, CAM_PIN_SIOD, CAM_PIN_SIOC, CAM_PIN_Y9, CAM_PIN_Y8,
      CAM_PIN_Y7, CAM_PIN_Y6, CAM_PIN_Y5, CAM_PIN_Y4, CAM_PIN_Y3, CAM_PIN_Y2,
      CAM_PIN_VSYNC, CAM_PIN_HREF, CAM_PIN_PCLK,
      // Shared I2C Bus (2 pins)
      static_cast<int8_t>(PIN_I2C_SDA), static_cast<int8_t>(PIN_I2C_SCL),
      // 4x ToF XSHUT pins
      static_cast<int8_t>(PIN_TOF_XSHUT_1),
      static_cast<int8_t>(PIN_TOF_XSHUT_2),
      static_cast<int8_t>(PIN_TOF_XSHUT_3),
      static_cast<int8_t>(PIN_TOF_XSHUT_4),
      // 4x IR break-beam sensor pins
      static_cast<int8_t>(PIN_IR_BEAM_1), static_cast<int8_t>(PIN_IR_BEAM_2),
      static_cast<int8_t>(PIN_IR_BEAM_3), static_cast<int8_t>(PIN_IR_BEAM_4),
      // WS2812B LED data pin
      static_cast<int8_t>(PIN_WS2812B_DATA)};
}

/**
 * TC_CONF_01: Verify all 25 active pin assignments are strictly unique (no
 * collisions).
 */
void test_pin_collision_none(void) {
  const auto gpios = get_all_active_gpios();
  TEST_ASSERT_EQUAL_UINT32(25, gpios.size());

  std::set<int8_t> unique_pins(gpios.begin(), gpios.end());
  TEST_ASSERT_EQUAL_UINT32(25, unique_pins.size());
}

/**
 * TC_CONF_02: Verify inactive camera pins (PWDN, RESET) are disabled (-1).
 */
void test_inactive_camera_pins(void) {
  TEST_ASSERT_EQUAL_INT8(-1, CAM_PIN_PWDN);
  TEST_ASSERT_EQUAL_INT8(-1, CAM_PIN_RESET);
}

/**
 * TC_CONF_03: Verify all 25 active GPIOs map within valid ESP32-S3 silicon
 * range (0 <= GPIO <= 48).
 */
void test_gpio_valid_range(void) {
  const auto gpios = get_all_active_gpios();
  for (int8_t pin : gpios) {
    TEST_ASSERT_GREATER_OR_EQUAL_INT8(0, pin);
    TEST_ASSERT_LESS_OR_EQUAL_INT8(48, pin);
  }
}

/**
 * TC_CONF_04: Verify active GPIOs avoid omitted silicon pins (GPIO 22..25).
 */
void test_omitted_silicon_gpios(void) {
  const std::set<int8_t> omitted = {22, 23, 24, 25};
  const auto gpios = get_all_active_gpios();
  for (int8_t pin : gpios) {
    TEST_ASSERT_FALSE_MESSAGE(
        omitted.count(pin) > 0,
        "Collides with non-existent silicon GPIO (22-25)");
  }
}

/**
 * TC_CONF_05: Verify active GPIOs avoid internal Octal PSRAM bus (GPIO 33..37).
 */
void test_octal_psram_isolation(void) {
  const std::set<int8_t> octal_psram = {33, 34, 35, 36, 37};
  const auto gpios = get_all_active_gpios();
  for (int8_t pin : gpios) {
    TEST_ASSERT_FALSE_MESSAGE(octal_psram.count(pin) > 0,
                              "Collides with internal Octal PSRAM bus (33-37)");
  }
}

/**
 * TC_CONF_06: Verify active GPIOs avoid internal SPI Flash bus (GPIO 26..32).
 */
void test_spi_flash_isolation(void) {
  const std::set<int8_t> spi_flash = {26, 27, 28, 29, 30, 31, 32};
  const auto gpios = get_all_active_gpios();
  for (int8_t pin : gpios) {
    TEST_ASSERT_FALSE_MESSAGE(spi_flash.count(pin) > 0,
                              "Collides with internal SPI Flash bus (26-32)");
  }
}

/**
 * TC_CONF_07: Verify active GPIOs avoid native USB CDC lines (GPIO 19, 20).
 */
void test_usb_cdc_isolation(void) {
  const std::set<int8_t> usb_pins = {19, 20};
  const auto gpios = get_all_active_gpios();
  for (int8_t pin : gpios) {
    TEST_ASSERT_FALSE_MESSAGE(usb_pins.count(pin) > 0,
                              "Collides with native USB CDC D-/D+ (19, 20)");
  }
}

/**
 * TC_CONF_08: Verify active GPIOs avoid UART0 debug console pins (GPIO 43, 44).
 */
void test_uart0_isolation(void) {
  const std::set<int8_t> uart0_pins = {43, 44};
  const auto gpios = get_all_active_gpios();
  for (int8_t pin : gpios) {
    TEST_ASSERT_FALSE_MESSAGE(uart0_pins.count(pin) > 0,
                              "Collides with UART0 TXD/RXD (43, 44)");
  }
}

/**
 * TC_CONF_09: Verify active GPIOs avoid critical strapping pins (GPIO 0, 3, 45,
 * 46).
 */
void test_strapping_pins_avoidance(void) {
  const std::set<int8_t> strapping_pins = {0, 3, 45, 46};
  const auto gpios = get_all_active_gpios();
  for (int8_t pin : gpios) {
    TEST_ASSERT_FALSE_MESSAGE(
        strapping_pins.count(pin) > 0,
        "Collides with boot strapping pin (0, 3, 45, 46)");
  }
}

/**
 * TC_CONF_10: Verify LM393 IR break-beam sensor pins are distinct and interrupt
 * capable.
 */
void test_ir_interrupt_capabilities(void) {
  TEST_ASSERT_EQUAL_UINT8(14, PIN_IR_BEAM_1);
  TEST_ASSERT_EQUAL_UINT8(38, PIN_IR_BEAM_2);
  TEST_ASSERT_EQUAL_UINT8(39, PIN_IR_BEAM_3);
  TEST_ASSERT_EQUAL_UINT8(40, PIN_IR_BEAM_4);

  // Verify all 4 pins are unique
  std::set<uint8_t> ir_pins = {PIN_IR_BEAM_1, PIN_IR_BEAM_2, PIN_IR_BEAM_3,
                               PIN_IR_BEAM_4};
  TEST_ASSERT_EQUAL_UINT32(4, ir_pins.size());
}

/**
 * TC_CONF_11: Verify camera acquisition settings and parameters.
 */
void test_camera_parameters_validity(void) {
  TEST_ASSERT_EQUAL(FRAMESIZE_HVGA, CAM_FRAME_SIZE);
  TEST_ASSERT_EQUAL(PIXFORMAT_YUV422, CAM_PIXEL_FORMAT);
  TEST_ASSERT_EQUAL_UINT8(12, CAM_JPEG_QUALITY);
  TEST_ASSERT_EQUAL_UINT32(2, CAM_FB_COUNT);
  TEST_ASSERT_EQUAL(CAMERA_FB_IN_PSRAM, CAM_FB_LOCATION);
  TEST_ASSERT_EQUAL(CAMERA_GRAB_LATEST, CAM_GRAB_MODE);
  TEST_ASSERT_EQUAL_UINT32(20000000, CAM_XCLK_FREQ_HZ);
}

/**
 * TC_CONF_12: Verify peripheral specifications and bus configurations.
 */
void test_peripheral_constants_validity(void) {
  TEST_ASSERT_EQUAL_UINT32(400000, I2C_BUS_FREQ_HZ);
  TEST_ASSERT_EQUAL_UINT8(4, TOF_SENSOR_COUNT);
  TEST_ASSERT_EQUAL_UINT8(0x30, TOF_I2C_ADDR_BASE);
  TEST_ASSERT_EQUAL_UINT8(4, IR_BEAM_COUNT);
  TEST_ASSERT_EQUAL_UINT16(12, WS2812B_NUM_LEDS);
}

/**
 * TC_CONF_13: Verify system, networking, and FreeRTOS task concurrency
 * settings.
 */
void test_system_and_freertos_constants(void) {
  TEST_ASSERT_EQUAL_UINT16(80, HTTP_SERVER_PORT);
  TEST_ASSERT_EQUAL_UINT32(460800, SERIAL_BAUD_RATE);
  TEST_ASSERT_EQUAL_UINT8(0, CORE_0_NETWORK);
  TEST_ASSERT_EQUAL_UINT8(1, CORE_1_HARDWARE);
  TEST_ASSERT_NOT_EQUAL(CORE_0_NETWORK, CORE_1_HARDWARE);

  // Verify task priorities avoid priority inversion
  TEST_ASSERT_GREATER_THAN_UINT8(TASK_PRIORITY_NETWORK, TASK_PRIORITY_HARDWARE);

  // Verify task stack allocations
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(8192, TASK_STACK_NETWORK);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(8192, TASK_STACK_HARDWARE);

  // Dynamic Wi-Fi provisioning and camera mutex timeout
  TEST_ASSERT_EQUAL_UINT32(1000, CAMERA_MUTEX_TIMEOUT_MS);
  TEST_ASSERT_EQUAL_UINT32(180, WIFI_PORTAL_TIMEOUT_SEC);
  TEST_ASSERT_EQUAL_STRING("SortSense-AP", WIFI_AP_NAME);
}

/**
 * TC_CONF_14: Verify strict fixed-width types across all configurations.
 */
void test_fixed_width_type_safety(void) {
  static_assert(std::is_same<decltype(CAM_PIN_XCLK), const int8_t>::value,
                "CAM_PIN_XCLK must be int8_t");
  static_assert(std::is_same<decltype(PIN_I2C_SDA), const uint8_t>::value,
                "PIN_I2C_SDA must be uint8_t");
  static_assert(std::is_same<decltype(PIN_WS2812B_DATA), const uint8_t>::value,
                "PIN_WS2812B_DATA must be uint8_t");
  static_assert(std::is_same<decltype(HTTP_SERVER_PORT), const uint16_t>::value,
                "HTTP_SERVER_PORT must be uint16_t");
  static_assert(std::is_same<decltype(SERIAL_BAUD_RATE), const uint32_t>::value,
                "SERIAL_BAUD_RATE must be uint32_t");
  static_assert(std::is_same<decltype(CAM_FB_COUNT), const size_t>::value,
                "CAM_FB_COUNT must be size_t");
  static_assert(std::is_same<decltype(CAM_JPEG_QUALITY), const uint8_t>::value,
                "CAM_JPEG_QUALITY must be uint8_t");

  TEST_ASSERT_EQUAL_UINT32(1, sizeof(CAM_PIN_XCLK));
  TEST_ASSERT_EQUAL_UINT32(1, sizeof(PIN_I2C_SDA));
  TEST_ASSERT_EQUAL_UINT32(2, sizeof(HTTP_SERVER_PORT));
  TEST_ASSERT_EQUAL_UINT32(4, sizeof(SERIAL_BAUD_RATE));
  TEST_ASSERT_EQUAL_UINT32(sizeof(size_t), sizeof(CAM_FB_COUNT));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pin_collision_none);
  RUN_TEST(test_inactive_camera_pins);
  RUN_TEST(test_gpio_valid_range);
  RUN_TEST(test_omitted_silicon_gpios);
  RUN_TEST(test_octal_psram_isolation);
  RUN_TEST(test_spi_flash_isolation);
  RUN_TEST(test_usb_cdc_isolation);
  RUN_TEST(test_uart0_isolation);
  RUN_TEST(test_strapping_pins_avoidance);
  RUN_TEST(test_ir_interrupt_capabilities);
  RUN_TEST(test_camera_parameters_validity);
  RUN_TEST(test_peripheral_constants_validity);
  RUN_TEST(test_system_and_freertos_constants);
  RUN_TEST(test_fixed_width_type_safety);
  return UNITY_END();
}
