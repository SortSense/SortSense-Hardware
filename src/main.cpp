/**
 * @file main.cpp
 * @brief Basic OV5640 Serial Camera Test
 */

#include <Arduino.h>
#include <esp_camera.h>

#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    10
#define CAM_PIN_SIOD    40
#define CAM_PIN_SIOC    39

#define CAM_PIN_D7      48
#define CAM_PIN_D6      11
#define CAM_PIN_D5      12
#define CAM_PIN_D4      14
#define CAM_PIN_D3      16
#define CAM_PIN_D2      18
#define CAM_PIN_D1      17
#define CAM_PIN_D0      15

#define CAM_PIN_VSYNC   38
#define CAM_PIN_HREF    47
#define CAM_PIN_PCLK    13

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- OV5640 Serial Camera Test ---");

    if (!psramFound()) {
        Serial.println("[ERROR] PSRAM not detected!");
        return;
    }
    Serial.printf("[PSRAM] Found %u KB free\n", ESP.getFreePsram() / 1024);

    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

    config.pin_pwdn     = CAM_PIN_PWDN;
    config.pin_reset    = CAM_PIN_RESET;
    config.pin_xclk     = CAM_PIN_XCLK;
    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;

    config.pin_d7       = CAM_PIN_D7;
    config.pin_d6       = CAM_PIN_D6;
    config.pin_d5       = CAM_PIN_D5;
    config.pin_d4       = CAM_PIN_D4;
    config.pin_d3       = CAM_PIN_D3;
    config.pin_d2       = CAM_PIN_D2;
    config.pin_d1       = CAM_PIN_D1;
    config.pin_d0       = CAM_PIN_D0;

    config.pin_vsync    = CAM_PIN_VSYNC;
    config.pin_href     = CAM_PIN_HREF;
    config.pin_pclk     = CAM_PIN_PCLK;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size   = FRAMESIZE_VGA;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[ERROR] esp_camera_init failed: 0x%x\n", err);
        return;
    }
    Serial.println("[CAMERA] OV5640 Initialized. Send 'c' to capture.");
}

void loop() {
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'c') {
            Serial.println("[CAPTURE] Grabbing frame...");
            camera_fb_t* fb = esp_camera_fb_get();
            if (!fb) {
                Serial.println("[ERROR] Failed to get camera frame buffer!");
                return;
            }
            Serial.printf("[SUCCESS] Frame acquired: %u bytes (%dx%d)\n",
                          static_cast<uint32_t>(fb->len), fb->width, fb->height);
            esp_camera_fb_return(fb);
        }
    }
    delay(20);
}
