/**
 * @file test_led_handler.cpp
 * @brief Unit tests for onboard WS2812 RGB LED manager for SortSense visual UX.
 */

#include <unity.h>

#include "Arduino.h"
#include "config.h"
#include "led_handler.h"

void setUp(void) {
  mock_arduino_reset();
  led_init();
}

void tearDown(void) { led_turn_off(); }

/**
 * TC_LED_01: Verify initialization targets onboard RGB LED on GPIO 48 and sets
 * default idle state.
 */
void test_led_init_defaults(void) {
  esp_err_t err = led_init();
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_EQUAL_INT8(BIN_INDEX_NONE, led_get_current_bin());
  TEST_ASSERT_EQUAL_UINT8(PIN_RGB_LED, mock_neopixel_pin);
}

/**
 * TC_LED_02: Verify bin category to RGB color mappings.
 */
void test_led_set_category_mappings(void) {
  // Bin 0: Plastic -> Blue
  led_set_category(BIN_INDEX_PLASTIC);
  TEST_ASSERT_EQUAL_INT8(BIN_INDEX_PLASTIC, led_get_current_bin());
  TEST_ASSERT_EQUAL_UINT8(0, mock_neopixel_r);
  TEST_ASSERT_TRUE(mock_neopixel_b > mock_neopixel_g);

  // Bin 1: Paper -> Green
  led_set_category(BIN_INDEX_PAPER);
  TEST_ASSERT_EQUAL_INT8(BIN_INDEX_PAPER, led_get_current_bin());
  TEST_ASSERT_EQUAL_UINT8(0, mock_neopixel_r);
  TEST_ASSERT_TRUE(mock_neopixel_g > mock_neopixel_b);

  // Bin 2: Metal -> Yellow/Amber (R and G both high, B zero)
  led_set_category(BIN_INDEX_METAL);
  TEST_ASSERT_EQUAL_INT8(BIN_INDEX_METAL, led_get_current_bin());
  TEST_ASSERT_TRUE(mock_neopixel_r > 0);
  TEST_ASSERT_TRUE(mock_neopixel_g > 0);
  TEST_ASSERT_EQUAL_UINT8(0, mock_neopixel_b);

  // Bin 3: General -> White (R, G, B all balanced)
  led_set_category(BIN_INDEX_GENERAL);
  TEST_ASSERT_EQUAL_INT8(BIN_INDEX_GENERAL, led_get_current_bin());
  TEST_ASSERT_TRUE(mock_neopixel_r > 0);
  TEST_ASSERT_EQUAL_UINT8(mock_neopixel_r, mock_neopixel_g);
  TEST_ASSERT_EQUAL_UINT8(mock_neopixel_g, mock_neopixel_b);

  // Idle: None -> Dim Cyan (R=0, G>0, B>0)
  led_set_category(BIN_INDEX_NONE);
  TEST_ASSERT_EQUAL_INT8(BIN_INDEX_NONE, led_get_current_bin());
  TEST_ASSERT_EQUAL_UINT8(0, mock_neopixel_r);
  TEST_ASSERT_TRUE(mock_neopixel_b > 0);
}

/**
 * TC_LED_03: Verify manual RGB assignment.
 */
void test_led_set_rgb(void) {
  led_set_rgb(100, 150, 200);
  // Values scaled by RGB_LED_BRIGHTNESS (64/255)
  uint8_t expR = (100 * RGB_LED_BRIGHTNESS) / 255;
  uint8_t expG = (150 * RGB_LED_BRIGHTNESS) / 255;
  uint8_t expB = (200 * RGB_LED_BRIGHTNESS) / 255;
  TEST_ASSERT_EQUAL_UINT8(expR, mock_neopixel_r);
  TEST_ASSERT_EQUAL_UINT8(expG, mock_neopixel_g);
  TEST_ASSERT_EQUAL_UINT8(expB, mock_neopixel_b);
}

/**
 * TC_LED_04: Verify drop confirmation flash and automated category restoration.
 */
void test_led_flash_drop_confirmation_lifecycle(void) {
  led_set_category(BIN_INDEX_PLASTIC); // Active blue
  uint8_t blueVal = mock_neopixel_b;

  // Trigger drop flash
  mock_millis_val = 2000;
  led_flash_drop_confirmed();

  // Should immediately show full white flash
  uint8_t flashVal = (255 * RGB_LED_BRIGHTNESS) / 255;
  TEST_ASSERT_EQUAL_UINT8(flashVal, mock_neopixel_r);
  TEST_ASSERT_EQUAL_UINT8(flashVal, mock_neopixel_g);
  TEST_ASSERT_EQUAL_UINT8(flashVal, mock_neopixel_b);

  // During the 600ms window, led_loop maintains flash
  mock_millis_val = 2300; // +300ms
  led_loop();
  TEST_ASSERT_EQUAL_UINT8(flashVal, mock_neopixel_r);

  // After 600ms window expires, led_loop restores category (Plastic/Blue)
  mock_millis_val = 2650; // +650ms
  led_loop();
  TEST_ASSERT_EQUAL_UINT8(0, mock_neopixel_r);
  TEST_ASSERT_EQUAL_UINT8(blueVal, mock_neopixel_b);
}

/**
 * TC_LED_05: Verify full warning override and restoration.
 */
void test_led_full_warning_override(void) {
  led_set_category(BIN_INDEX_PAPER); // Active green

  // Activate full warning
  led_set_full_warning(true);
  uint8_t expRed = (255 * RGB_LED_BRIGHTNESS) / 255;
  TEST_ASSERT_EQUAL_UINT8(expRed, mock_neopixel_r);
  TEST_ASSERT_EQUAL_UINT8(0, mock_neopixel_g);
  TEST_ASSERT_EQUAL_UINT8(0, mock_neopixel_b);

  // Deactivate full warning -> restores Paper Green
  led_set_full_warning(false);
  TEST_ASSERT_EQUAL_UINT8(0, mock_neopixel_r);
  TEST_ASSERT_TRUE(mock_neopixel_g > 0);
}

/**
 * TC_LED_06: Verify turn off clears all channels.
 */
void test_led_turn_off(void) {
  led_set_category(BIN_INDEX_GENERAL);
  TEST_ASSERT_TRUE(mock_neopixel_r > 0);

  led_turn_off();
  TEST_ASSERT_EQUAL_UINT8(0, mock_neopixel_r);
  TEST_ASSERT_EQUAL_UINT8(0, mock_neopixel_g);
  TEST_ASSERT_EQUAL_UINT8(0, mock_neopixel_b);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  UNITY_BEGIN();

  RUN_TEST(test_led_init_defaults);
  RUN_TEST(test_led_set_category_mappings);
  RUN_TEST(test_led_set_rgb);
  RUN_TEST(test_led_flash_drop_confirmation_lifecycle);
  RUN_TEST(test_led_full_warning_override);
  RUN_TEST(test_led_turn_off);

  return UNITY_END();
}
