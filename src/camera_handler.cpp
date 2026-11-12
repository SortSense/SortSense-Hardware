/**
 * @file camera_handler.cpp
 * @brief Implementation of OV5640 camera initialization and frame acquisition routines.
 * @target ESP32-S3 WROOM CAM (ESP32-S3-WROOM-1-N16R8) with OV5640 Image Sensor
 */

#include "camera_handler.h"
#include "config.h"

#include <Arduino.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

static const char* TAG = "camera_handler";

/// Tracks the active initialization status of the camera driver
static bool s_camera_initialized = false;

esp_err_t camera_init(void) {
    if (s_camera_initialized) {
        ESP_LOGW(TAG, "Camera driver is already initialized.");
        return ESP_OK;
    }

    // Verify external PSRAM availability (required for CAM_FB_IN_PSRAM)
    if (!psramFound()) {
        ESP_LOGE(TAG, "External PSRAM not detected! Camera frame buffers require PSRAM.");
        return ESP_ERR_NOT_FOUND;
    }

    const size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM verified. Free PSRAM heap: %u bytes", static_cast<uint32_t>(free_psram));

    camera_config_t config = {};

    // Control and Clock Pins
    config.pin_pwdn     = CAM_PIN_PWDN;
    config.pin_reset    = CAM_PIN_RESET;
    config.pin_xclk     = CAM_PIN_XCLK;

    // SCCB (I2C) Interface Pins
    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;

    // 8-bit DVP Data Pins (Y9 down to Y2 mapped to D7 down to D0)
    config.pin_d7       = CAM_PIN_Y9;
    config.pin_d6       = CAM_PIN_Y8;
    config.pin_d5       = CAM_PIN_Y7;
    config.pin_d4       = CAM_PIN_Y6;
    config.pin_d3       = CAM_PIN_Y5;
    config.pin_d2       = CAM_PIN_Y4;
    config.pin_d1       = CAM_PIN_Y3;
    config.pin_d0       = CAM_PIN_Y2;

    // Synchronization Pins
    config.pin_vsync    = CAM_PIN_VSYNC;
    config.pin_href     = CAM_PIN_HREF;
    config.pin_pclk     = CAM_PIN_PCLK;

    // Clock and Signal Generation
    config.xclk_freq_hz = static_cast<int32_t>(CAM_XCLK_FREQ_HZ);
    config.ledc_timer   = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;

    // Image Format and Resolution
    config.pixel_format = CAM_PIXEL_FORMAT;
    config.frame_size   = CAM_FRAME_SIZE;
    config.jpeg_quality = static_cast<int32_t>(CAM_JPEG_QUALITY);
    config.fb_count     = CAM_FB_COUNT;
    config.fb_location  = CAM_FB_LOCATION;
    config.grab_mode    = CAM_GRAB_MODE;

    // Use default SCCB software I2C implementation on configured pins
    config.sccb_i2c_port = -1;

    ESP_LOGI(TAG, "Initializing esp_camera with frame_size=%d, jpeg_quality=%u, fb_count=%u",
             static_cast<int32_t>(config.frame_size),
             CAM_JPEG_QUALITY,
             static_cast<uint32_t>(config.fb_count));

    const esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: 0x%x (%s)", err, esp_err_to_name(err));
        s_camera_initialized = false;
        return err;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s != nullptr) {
        ESP_LOGI(TAG, "Camera sensor detected successfully (PID=0x%04x, MID=0x%04x)",
                 s->id.PID,
                 static_cast<uint16_t>((static_cast<uint16_t>(s->id.MIDH) << 8) | s->id.MIDL));
    } else {
        ESP_LOGW(TAG, "Camera driver initialized, but sensor_t pointer could not be retrieved.");
    }

    s_camera_initialized = true;
    ESP_LOGI(TAG, "OV5640 Camera initialized successfully in PSRAM.");
    return ESP_OK;
}

esp_err_t camera_deinit(void) {
    if (!s_camera_initialized) {
        ESP_LOGW(TAG, "camera_deinit called but camera is not initialized.");
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t err = esp_camera_deinit();
    if (err == ESP_OK) {
        s_camera_initialized = false;
        ESP_LOGI(TAG, "Camera driver deinitialized successfully.");
    } else {
        ESP_LOGE(TAG, "esp_camera_deinit failed: 0x%x (%s)", err, esp_err_to_name(err));
    }
    return err;
}

camera_fb_t* camera_acquire_frame(void) {
    if (!s_camera_initialized) {
        ESP_LOGE(TAG, "camera_acquire_frame failed: camera driver is not initialized.");
        return nullptr;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr) {
        ESP_LOGE(TAG, "Camera frame capture failed: esp_camera_fb_get returned nullptr.");
        return nullptr;
    }

    return fb;
}

void camera_release_frame(camera_fb_t* fb) {
    if (fb == nullptr) {
        ESP_LOGW(TAG, "camera_release_frame called with nullptr fb; ignoring.");
        return;
    }

    esp_camera_fb_return(fb);
}

bool is_camera_initialized(void) {
    return s_camera_initialized;
}
