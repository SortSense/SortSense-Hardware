/**
 * @file async_camera_response.h
 * @brief Asynchronous HTTP streaming response for camera frame buffers with RAII cleanup.
 * @target ESP32-S3 WROOM CAM (ESP32-S3-WROOM-1-N16R8) with OV5640 Image Sensor
 * 
 * Strict type safety: Enforces fixed-width integer and sizing types (uint8_t, size_t).
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <ESPAsyncWebServer.h>

#include "esp_camera.h"
#include "camera_handler.h"

/**
 * @class AsyncCameraResponse
 * @brief Streams camera frame buffer directly to the async TCP socket and guarantees
 *        frame buffer return and mutex release upon completion or disconnection.
 */
class AsyncCameraResponse : public AsyncAbstractResponse {
private:
    camera_fb_t* _fb;
    size_t _readLength;
    SemaphoreHandle_t _mutex;

public:
    AsyncCameraResponse(camera_fb_t* fb, SemaphoreHandle_t mutex)
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

    size_t _fillBuffer(uint8_t* data, size_t len) override {
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

    bool _sourceValid() const override {
        return (_fb != nullptr);
    }
};
