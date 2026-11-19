/**
 * @file test_camera_handler.cpp
 * @brief Unit tests for SortSense OV5640 camera handler lifecycle and buffer
 * management.
 *
 * Verifies all 12 test cases (TC_CAM_01 to TC_CAM_12) formulated by Explorer 3.
 */

#include <string.h>
#include <unity.h>

#include "camera_handler.h"
#include "config.h"

// Mock control interfaces from test/mocks/
#include "Arduino.h"
#include "esp_camera.h"
#include "esp_err.h"

static camera_fb_t s_mock_fb;
static uint8_t s_dummy_pixels[128];

void setUp(void) {
  mock_camera_reset();
  mock_set_psram_found(true);
  s_mock_fb.buf = s_dummy_pixels;
  s_mock_fb.len = sizeof(s_dummy_pixels);
  s_mock_fb.width = 640;
  s_mock_fb.height = 480;
  s_mock_fb.format = PIXFORMAT_JPEG;
}

void tearDown(void) {
  // If the camera is left in initialized state, tear it down cleanly
  if (is_camera_initialized()) {
    mock_set_camera_deinit_result(ESP_OK);
    camera_deinit();
  }
}

/**
 * TC_CAM_01: Verify initial uninitialized state invariants.
 */
void test_initial_state_uninitialized(void) {
  TEST_ASSERT_FALSE(is_camera_initialized());
  TEST_ASSERT_NULL(camera_acquire_frame());
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, camera_deinit());
}

/**
 * TC_CAM_02: Verify PSRAM missing guard aborts with ESP_ERR_NOT_FOUND.
 */
void test_init_missing_psram_failure(void) {
  mock_set_psram_found(false);

  const esp_err_t err = camera_init();
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);
  TEST_ASSERT_FALSE(is_camera_initialized());
  TEST_ASSERT_EQUAL_UINT32(0, mock_get_init_call_count());
}

/**
 * TC_CAM_03: Verify successful initialization and complete config mapping.
 */
void test_init_success_and_config_mapping(void) {
  mock_set_camera_init_result(ESP_OK);

  const esp_err_t err = camera_init();
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_TRUE(is_camera_initialized());
  TEST_ASSERT_EQUAL_UINT32(1, mock_get_init_call_count());

  // Inspect mapped camera_config_t structure
  const camera_config_t &cfg = mock_get_captured_config();
  TEST_ASSERT_EQUAL_INT(CAM_PIN_PWDN, cfg.pin_pwdn);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_RESET, cfg.pin_reset);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_XCLK, cfg.pin_xclk);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_SIOD, cfg.pin_sccb_sda);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_SIOC, cfg.pin_sccb_scl);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_Y9, cfg.pin_d7);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_Y8, cfg.pin_d6);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_Y7, cfg.pin_d5);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_Y6, cfg.pin_d4);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_Y5, cfg.pin_d3);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_Y4, cfg.pin_d2);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_Y3, cfg.pin_d1);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_Y2, cfg.pin_d0);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_VSYNC, cfg.pin_vsync);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_HREF, cfg.pin_href);
  TEST_ASSERT_EQUAL_INT(CAM_PIN_PCLK, cfg.pin_pclk);

  TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(CAM_XCLK_FREQ_HZ),
                          cfg.xclk_freq_hz);
  TEST_ASSERT_EQUAL(CAM_PIXEL_FORMAT, cfg.pixel_format);
  TEST_ASSERT_EQUAL(CAM_FRAME_SIZE, cfg.frame_size);
  TEST_ASSERT_EQUAL_INT(static_cast<int32_t>(CAM_JPEG_QUALITY),
                        cfg.jpeg_quality);
  TEST_ASSERT_EQUAL_UINT32(CAM_FB_COUNT, cfg.fb_count);
  TEST_ASSERT_EQUAL(CAM_FB_LOCATION, cfg.fb_location);
  TEST_ASSERT_EQUAL(CAM_GRAB_MODE, cfg.grab_mode);
  TEST_ASSERT_EQUAL_INT(-1, cfg.sccb_i2c_port);
}

/**
 * TC_CAM_04: Verify initialization idempotency when already initialized.
 */
void test_init_idempotency(void) {
  mock_set_camera_init_result(ESP_OK);
  TEST_ASSERT_EQUAL(ESP_OK, camera_init());
  TEST_ASSERT_EQUAL_UINT32(1, mock_get_init_call_count());

  // Subsequent initialization call should return ESP_OK without calling
  // esp_camera_init again
  TEST_ASSERT_EQUAL(ESP_OK, camera_init());
  TEST_ASSERT_EQUAL_UINT32(1, mock_get_init_call_count());
  TEST_ASSERT_TRUE(is_camera_initialized());
}

/**
 * TC_CAM_05: Verify driver failure propagates error and preserves uninitialized
 * state.
 */
void test_init_driver_hardware_failure(void) {
  mock_set_camera_init_result(ESP_ERR_CAMERA_NOT_DETECTED);

  const esp_err_t err = camera_init();
  TEST_ASSERT_EQUAL(ESP_ERR_CAMERA_NOT_DETECTED, err);
  TEST_ASSERT_FALSE(is_camera_initialized());
}

/**
 * TC_CAM_06: Verify successful frame buffer acquisition.
 */
void test_acquire_frame_success(void) {
  mock_set_camera_init_result(ESP_OK);
  TEST_ASSERT_EQUAL(ESP_OK, camera_init());

  mock_set_fb(&s_mock_fb);
  camera_fb_t *fb = camera_acquire_frame();
  TEST_ASSERT_NOT_NULL(fb);
  TEST_ASSERT_EQUAL_PTR(&s_mock_fb, fb);
  TEST_ASSERT_EQUAL_UINT32(sizeof(s_dummy_pixels), fb->len);
  TEST_ASSERT_EQUAL_PTR(s_dummy_pixels, fb->buf);
  TEST_ASSERT_EQUAL_UINT32(1, mock_get_fb_get_call_count());
}

/**
 * TC_CAM_07: Verify driver returning nullptr is handled gracefully.
 */
void test_acquire_frame_driver_null_failure(void) {
  mock_set_camera_init_result(ESP_OK);
  TEST_ASSERT_EQUAL(ESP_OK, camera_init());

  mock_set_fb(nullptr);
  camera_fb_t *fb = camera_acquire_frame();
  TEST_ASSERT_NULL(fb);
  TEST_ASSERT_EQUAL_UINT32(1, mock_get_fb_get_call_count());
}

/**
 * TC_CAM_08: Verify acquiring frame when uninitialized returns nullptr
 * immediately.
 */
void test_acquire_frame_when_uninitialized(void) {
  TEST_ASSERT_FALSE(is_camera_initialized());
  mock_set_fb(&s_mock_fb);

  camera_fb_t *fb = camera_acquire_frame();
  TEST_ASSERT_NULL(fb);
  // esp_camera_fb_get must not be invoked
  TEST_ASSERT_EQUAL_UINT32(0, mock_get_fb_get_call_count());
}

/**
 * TC_CAM_09: Verify releasing valid frame buffer delegates to
 * esp_camera_fb_return.
 */
void test_release_frame_valid(void) {
  camera_release_frame(&s_mock_fb);
  TEST_ASSERT_EQUAL_UINT32(1, mock_get_fb_return_call_count());
  TEST_ASSERT_EQUAL_PTR(&s_mock_fb, mock_get_returned_fb());
}

/**
 * TC_CAM_10: Verify releasing nullptr does not crash and ignores call.
 */
void test_release_frame_nullptr_safe(void) {
  camera_release_frame(nullptr);
  TEST_ASSERT_EQUAL_UINT32(0, mock_get_fb_return_call_count());
}

/**
 * TC_CAM_11: Verify clean deinitialization resets operational state.
 */
void test_deinit_success(void) {
  mock_set_camera_init_result(ESP_OK);
  TEST_ASSERT_EQUAL(ESP_OK, camera_init());
  TEST_ASSERT_TRUE(is_camera_initialized());

  mock_set_camera_deinit_result(ESP_OK);
  const esp_err_t err = camera_deinit();
  TEST_ASSERT_EQUAL(ESP_OK, err);
  TEST_ASSERT_FALSE(is_camera_initialized());
  TEST_ASSERT_EQUAL_UINT32(1, mock_get_deinit_call_count());

  // Subsequent frame acquisition must be rejected
  TEST_ASSERT_NULL(camera_acquire_frame());
}

/**
 * TC_CAM_12: Verify deinitialization failure propagates driver error code.
 */
void test_deinit_driver_failure(void) {
  mock_set_camera_init_result(ESP_OK);
  TEST_ASSERT_EQUAL(ESP_OK, camera_init());

  mock_set_camera_deinit_result(ESP_FAIL);
  const esp_err_t err = camera_deinit();
  TEST_ASSERT_EQUAL(ESP_FAIL, err);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_initial_state_uninitialized);
  RUN_TEST(test_init_missing_psram_failure);
  RUN_TEST(test_init_success_and_config_mapping);
  RUN_TEST(test_init_idempotency);
  RUN_TEST(test_init_driver_hardware_failure);
  RUN_TEST(test_acquire_frame_success);
  RUN_TEST(test_acquire_frame_driver_null_failure);
  RUN_TEST(test_acquire_frame_when_uninitialized);
  RUN_TEST(test_release_frame_valid);
  RUN_TEST(test_release_frame_nullptr_safe);
  RUN_TEST(test_deinit_success);
  RUN_TEST(test_deinit_driver_failure);
  return UNITY_END();
}
