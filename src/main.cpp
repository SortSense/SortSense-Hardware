/**
 * @file main.cpp
 * @brief Live Video Streaming & Web Dashboard with OV5640 RGB565 -> JPEG Pipeline
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <esp_camera.h>
#include <img_converters.h>
#include <esp_heap_caps.h>

#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD     4
#define CAM_PIN_SIOC     5

#define CAM_PIN_D7      16
#define CAM_PIN_D6      17
#define CAM_PIN_D5      18
#define CAM_PIN_D4      12
#define CAM_PIN_D3      10
#define CAM_PIN_D2       8
#define CAM_PIN_D1       9
#define CAM_PIN_D0      11

#define CAM_PIN_VSYNC    6
#define CAM_PIN_HREF     7
#define CAM_PIN_PCLK    13

static AsyncWebServer server(80);
static SemaphoreHandle_t cameraMutex = nullptr;
static bool g_cameraOnline = false;
static uint32_t g_frameCount = 0;

/**
 * @brief 32-bit endianness byte swap on RGB565 buffer
 */
static inline void fix_rgb565_endianness(uint8_t* buf, size_t len) {
    if (!buf || len < 4) return;
    uint32_t* p32 = reinterpret_cast<uint32_t*>(buf);
    size_t count32 = len / 4;
    for (size_t i = 0; i < count32; i++) {
        uint32_t w = p32[i];
        p32[i] = ((w & 0x00FF00FF) << 8) | ((w & 0xFF00FF00) >> 8);
    }
}

class AsyncJpegResponse : public AsyncAbstractResponse {
private:
    uint8_t* _buf;
    size_t _readLength;
    SemaphoreHandle_t _mutex;

public:
    AsyncJpegResponse(uint8_t* buf, size_t len, SemaphoreHandle_t mutex)
        : AsyncAbstractResponse(nullptr), _buf(buf), _readLength(0), _mutex(mutex) {
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

    size_t _fillBuffer(uint8_t* data, size_t len) override {
        if (_buf == nullptr) return 0;
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

    bool _sourceValid() const override {
        return (_buf != nullptr);
    }
};

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>SortSense - OV5640 Live Stream</title>
  <style>
    body { background: #0b1329; color: white; text-align: center; font-family: sans-serif; padding: 20px; }
    .badge { display: inline-block; background: #0284c7; padding: 4px 12px; border-radius: 999px; font-size: 13px; font-weight: bold; margin-bottom: 12px; }
    .container { background: #1e293b; max-width: 680px; margin: 0 auto; border-radius: 12px; padding: 16px; border: 1px solid #334155; }
    img { max-width: 640px; width: 100%; border-radius: 8px; border: 2px solid #334155; display: block; margin: 0 auto; }
    .stats { display: flex; justify-content: space-around; margin-top: 14px; font-size: 14px; color: #38bdf8; }
  </style>
</head>
<body>
  <h1>SortSense OV5640 Camera Feed</h1>
  <div class="badge" id="sensorBadge">ESP32-S3 OV5640 Online</div>

  <div class="container">
    <img id="stream" src="/capture" />
    <div class="stats">
      <div>Status: <span style="color: #4ade80;">ONLINE</span></div>
      <div>Res: VGA (640x480)</div>
      <div>Frames: <span id="fc">0</span></div>
    </div>
  </div>

  <script>
    const img = document.getElementById('stream');
    let frames = 0;
    let inFlight = false;

    function loadNext() {
      if (inFlight) return;
      inFlight = true;
      const nextImg = new Image();
      nextImg.onload = function() {
        img.src = this.src;
        frames++;
        document.getElementById('fc').innerText = frames;
        inFlight = false;
        setTimeout(loadNext, 30);
      };
      nextImg.onerror = function() {
        inFlight = false;
        setTimeout(loadNext, 200);
      };
      nextImg.src = '/capture?t=' + Date.now();
    }
    loadNext();
  </script>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- Starting Web Camera Streamer ---");

    if (!psramFound()) {
        Serial.println("[ERROR] PSRAM required!");
        return;
    }

    cameraMutex = xSemaphoreCreateMutex();

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

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_whitebal(s, 1);
        s->set_exposure_ctrl(s, 1);
        s->set_aec2(s, 0); // attempted manual evaluative metering
        s->set_gain_ctrl(s, 1);
        s->set_denoise(s, 2);
        s->set_vflip(s, 1);
    }
    g_cameraOnline = true;

    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-S3-CAM-TEST");
    Serial.printf("[WIFI] SoftAP IP: %s\n", WiFi.softAPIP().toString().c_str());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", INDEX_HTML);
    });

    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!g_cameraOnline) {
            req->send(503, "text/plain", "Camera offline");
            return;
        }
        if (xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
            req->send(503, "text/plain", "Camera busy");
            return;
        }
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            xSemaphoreGive(cameraMutex);
            req->send(503, "text/plain", "Capture failed");
            return;
        }

        fix_rgb565_endianness(fb->buf, fb->len);
        uint8_t* jpg_buf = nullptr;
        size_t jpg_len = 0;
        bool ok = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
        esp_camera_fb_return(fb);

        if (!ok || !jpg_buf) {
            xSemaphoreGive(cameraMutex);
            req->send(500, "text/plain", "JPEG failed");
            return;
        }

        AsyncJpegResponse* resp = new AsyncJpegResponse(jpg_buf, jpg_len, cameraMutex);
        resp->addHeader("Cache-Control", "no-cache");
        req->send(resp);
    });

    server.begin();
    Serial.println("[HTTP] Server ready on http://192.168.4.1/");
}

void loop() {
    delay(50);
}
