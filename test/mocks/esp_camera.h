#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

typedef enum {
  PIXFORMAT_RGB565,
  PIXFORMAT_YUV422,
  PIXFORMAT_GRAYSCALE,
  PIXFORMAT_JPEG,
  PIXFORMAT_RGB888,
  PIXFORMAT_RAW,
  PIXFORMAT_RGB444,
  PIXFORMAT_RGB555
} pixformat_t;

typedef enum {
  FRAMESIZE_QQVGA,
  FRAMESIZE_QVGA,
  FRAMESIZE_VGA,
  FRAMESIZE_SVGA,
  FRAMESIZE_XGA,
  FRAMESIZE_SXGA,
  FRAMESIZE_UXGA
} framesize_t;

typedef enum {
  CAMERA_FB_IN_PSRAM = 0,
  CAMERA_FB_IN_DRAM = 1
} camera_fb_location_t;

typedef enum {
  CAMERA_GRAB_WHEN_EMPTY = 0,
  CAMERA_GRAB_LATEST = 1
} camera_grab_mode_t;

typedef enum {
  LEDC_TIMER_0 = 0,
  LEDC_TIMER_1,
  LEDC_TIMER_2,
  LEDC_TIMER_3
} ledc_timer_t;

typedef enum {
  LEDC_CHANNEL_0 = 0,
  LEDC_CHANNEL_1,
  LEDC_CHANNEL_2
} ledc_channel_t;

typedef struct {
  uint8_t *buf;
  size_t len;
  size_t width;
  size_t height;
  pixformat_t format;
  struct timeval timestamp;
} camera_fb_t;

typedef struct {
  uint16_t MIDH;
  uint16_t MIDL;
  uint16_t PID;
  uint8_t VER;
} sensor_id_t;

typedef struct {
  sensor_id_t id;
} sensor_t;

typedef struct {
  int pin_pwdn;
  int pin_reset;
  int pin_xclk;
  int pin_sccb_sda;
  int pin_sccb_scl;
  int pin_d7;
  int pin_d6;
  int pin_d5;
  int pin_d4;
  int pin_d3;
  int pin_d2;
  int pin_d1;
  int pin_d0;
  int pin_vsync;
  int pin_href;
  int pin_pclk;

  int xclk_freq_hz;
  ledc_timer_t ledc_timer;
  ledc_channel_t ledc_channel;
  pixformat_t pixel_format;
  framesize_t frame_size;
  int jpeg_quality;
  size_t fb_count;
  camera_fb_location_t fb_location;
  camera_grab_mode_t grab_mode;
  int sccb_i2c_port;
} camera_config_t;

// --- Mock Control & Inspection State ---
inline camera_config_t mock_captured_config = {};
inline esp_err_t mock_camera_init_result = ESP_OK;
inline esp_err_t mock_camera_deinit_result = ESP_OK;
inline sensor_t mock_sensor = {{0x56, 0x40, 0x5640, 0x01}};
inline sensor_t *mock_sensor_ptr = &mock_sensor;
inline camera_fb_t *mock_fb_ptr = nullptr;
inline camera_fb_t *mock_returned_fb_ptr = nullptr;

inline uint32_t mock_init_call_count = 0;
inline uint32_t mock_deinit_call_count = 0;
inline uint32_t mock_fb_get_call_count = 0;
inline uint32_t mock_fb_return_call_count = 0;

inline void mock_camera_reset() {
  memset(&mock_captured_config, 0, sizeof(mock_captured_config));
  mock_camera_init_result = ESP_OK;
  mock_camera_deinit_result = ESP_OK;
  mock_sensor = {{0x56, 0x40, 0x5640, 0x01}};
  mock_sensor_ptr = &mock_sensor;
  mock_fb_ptr = nullptr;
  mock_returned_fb_ptr = nullptr;
  mock_init_call_count = 0;
  mock_deinit_call_count = 0;
  mock_fb_get_call_count = 0;
  mock_fb_return_call_count = 0;
}

inline void mock_set_camera_init_result(esp_err_t err) {
  mock_camera_init_result = err;
}

inline void mock_set_camera_deinit_result(esp_err_t err) {
  mock_camera_deinit_result = err;
}

inline void mock_set_sensor_ptr(sensor_t *s) { mock_sensor_ptr = s; }

inline void mock_set_fb(camera_fb_t *fb) { mock_fb_ptr = fb; }

inline const camera_config_t &mock_get_captured_config() {
  return mock_captured_config;
}

inline uint32_t mock_get_init_call_count() { return mock_init_call_count; }

inline uint32_t mock_get_deinit_call_count() { return mock_deinit_call_count; }

inline uint32_t mock_get_fb_get_call_count() { return mock_fb_get_call_count; }

inline uint32_t mock_get_fb_return_call_count() {
  return mock_fb_return_call_count;
}

inline camera_fb_t *mock_get_returned_fb() { return mock_returned_fb_ptr; }

#ifdef __cplusplus
extern "C" {
#endif

inline esp_err_t esp_camera_init(const camera_config_t *config) {
  mock_init_call_count++;
  if (config != nullptr) {
    mock_captured_config = *config;
  }
  return mock_camera_init_result;
}

inline esp_err_t esp_camera_deinit(void) {
  mock_deinit_call_count++;
  return mock_camera_deinit_result;
}

inline camera_fb_t *esp_camera_fb_get(void) {
  mock_fb_get_call_count++;
  return mock_fb_ptr;
}

inline void esp_camera_fb_return(camera_fb_t *fb) {
  mock_fb_return_call_count++;
  mock_returned_fb_ptr = fb;
}

inline sensor_t *esp_camera_sensor_get(void) { return mock_sensor_ptr; }

#ifdef __cplusplus
}
#endif
