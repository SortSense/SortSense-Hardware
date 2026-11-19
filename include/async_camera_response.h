/**
 * @file async_camera_response.h
 * @brief Asynchronous HTTP streaming responses for camera frame buffers and
 * JPEG images with RAII cleanup.
 * @target ESP32-S3 WROOM CAM (ESP32-S3-WROOM-1-N16R8) with OV5640 Image Sensor
 *
 * Strict type safety: Enforces fixed-width integer and sizing types (uint8_t,
 * size_t).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "camera_handler.h"
#include "esp_camera.h"

/**
 * @class AsyncCameraResponse
 * @brief Streams camera frame buffer directly to the async TCP socket and
 * guarantees frame buffer return and mutex release upon completion or
 * disconnection.
 */
class AsyncCameraResponse : public AsyncAbstractResponse {
private:
  camera_fb_t *_fb;
  size_t _readLength;
  SemaphoreHandle_t _mutex;

public:
  AsyncCameraResponse(camera_fb_t *fb, SemaphoreHandle_t mutex)
      : AsyncAbstractResponse(nullptr), _fb(fb), _readLength(0), _mutex(mutex) {
    _code = 200;
    _contentType = "image/jpeg";
    _contentLength = (fb != nullptr) ? fb->len : 0;
  }

  ~AsyncCameraResponse() override {
    if (_fb != nullptr) {
      camera_release_frame(_fb);
      _fb = nullptr;
    }
    if (_mutex != nullptr) {
      xSemaphoreGive(_mutex);
    }
  }

  size_t _fillBuffer(uint8_t *data, size_t len) override {
    if (_fb == nullptr) {
      return 0;
    }
    size_t left = _contentLength - _readLength;
    if (left > len) {
      memcpy(data, _fb->buf + _readLength, len);
      _readLength += len;
      return len;
    }
    memcpy(data, _fb->buf + _readLength, left);
    _readLength += left;
    return left;
  }

  bool _sourceValid() const override { return (_fb != nullptr); }
};

/**
 * @class AsyncJpegResponse
 * @brief Streams dynamically allocated JPEG buffer to the client and releases
 * heap memory and mutex on destruction.
 */
class AsyncJpegResponse : public AsyncAbstractResponse {
private:
  uint8_t *_buf;
  size_t _readLength;
  SemaphoreHandle_t _mutex;

public:
  AsyncJpegResponse(uint8_t *buf, size_t len, SemaphoreHandle_t mutex)
      : AsyncAbstractResponse(nullptr), _buf(buf), _readLength(0),
        _mutex(mutex) {
    _code = 200;
    _contentType = "image/jpeg";
    _contentLength = len;
  }

  ~AsyncJpegResponse() override {
    if (_buf != nullptr) {
      free(_buf);
      _buf = nullptr;
    }
    if (_mutex != nullptr) {
      xSemaphoreGive(_mutex);
    }
  }

  size_t _fillBuffer(uint8_t *data, size_t len) override {
    if (_buf == nullptr) {
      return 0;
    }
    size_t left = _contentLength - _readLength;
    if (left > len) {
      memcpy(data, _buf + _readLength, len);
      _readLength += len;
      return len;
    }
    memcpy(data, _buf + _readLength, left);
    _readLength += left;
    return left;
  }

  bool _sourceValid() const override { return (_buf != nullptr); }
};
