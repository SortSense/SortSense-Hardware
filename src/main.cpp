/**
 * @file main.cpp
 * @brief SortSense: Clean YUV422 @ 20MHz Streamer
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
static uint16_t g_sensorPid = 0;

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
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>SortSense - OV5640 Live Feed</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0b1329; color: #f1f5f9; text-align: center; padding: 20px 10px; }
    h1 { color: #38bdf8; font-size: 26px; margin-bottom: 8px; }
    .badge { display: inline-block; background: #059669; padding: 5px 14px; border-radius: 9999px; font-size: 13px; font-weight: 600; margin-bottom: 16px; }
    .container { background: #1e293b; max-width: 720px; margin: 0 auto; border-radius: 14px; padding: 18px; box-shadow: 0 10px 30px rgba(0,0,0,0.6); border: 1px solid #334155; }
    .feed-wrapper { position: relative; background: #000; border-radius: 10px; overflow: hidden; border: 2px solid #475569; min-height: 240px; display: flex; align-items: center; justify-content: center; }
    img { width: 100%; max-width: 640px; height: auto; display: block; border-radius: 8px; }
    .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 10px; margin-top: 16px; text-align: left; }
    .stat-card { background: #0f172a; padding: 10px 14px; border-radius: 8px; border: 1px solid #334155; }
    .stat-label { font-size: 11px; text-transform: uppercase; color: #94a3b8; font-weight: 700; letter-spacing: 0.5px; }
    .stat-val { font-size: 14px; font-weight: 600; color: #38bdf8; margin-top: 2px; }
    .actions { display: flex; gap: 10px; justify-content: center; margin-top: 16px; }
    button { background: #0284c7; color: white; border: none; padding: 10px 20px; border-radius: 8px; font-weight: 600; cursor: pointer; transition: background 0.2s; font-size: 14px; }
    button:hover { background: #0369a1; }
    button.secondary { background: #475569; }
    button.secondary:hover { background: #334155; }
  </style>
</head>
<body>
  <h1>SortSense OV5640 Camera Feed</h1>
  <div class="badge" id="sensorBadge">ESP32-S3 OV5640 Clean YUV422 v5</div>

  <div class="container">
    <div class="feed-wrapper">
      <img id="liveFeed" src="/capture" alt="Live Stream Loading..." />
    </div>

    <div class="stats">
      <div class="stat-card">
        <div class="stat-label">Camera Status</div>
        <div class="stat-val" id="statStatus" style="color: #4ade80;">ONLINE (Live)</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Resolution</div>
        <div class="stat-val">VGA (640x480)</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Frames Streamed</div>
        <div class="stat-val" id="statFrames">0</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Frame Rate</div>
        <div class="stat-val" id="statFps">--</div>
      </div>
    </div>

    <div class="actions">
      <button onclick="snapshot()">Snapshot</button>
      <button class="secondary" id="btnStream" onclick="toggleStream()">Pause Feed</button>
    </div>
  </div>

  <script>
    const img = document.getElementById('liveFeed');
    let frames = 0;
    let streaming = true;
    let inFlight = false;

    function loadNextFrame() {
      if (!streaming || inFlight) return;
      inFlight = true;

      const nextImg = new Image();
      const t0 = performance.now();

      nextImg.onload = function() {
        img.src = this.src;
        frames++;
        document.getElementById('statFrames').innerText = frames;

        const now = performance.now();
        const fps = 1000 / (now - t0);
        document.getElementById('statFps').innerText = fps.toFixed(1) + ' fps';

        inFlight = false;
        setTimeout(loadNextFrame, 30);
      };

      nextImg.onerror = function() {
        inFlight = false;
        setTimeout(loadNextFrame, 200);
      };

      nextImg.src = '/capture?t=' + Date.now();
    }

    function toggleStream() {
      streaming = !streaming;
      document.getElementById('btnStream').innerText = streaming ? 'Pause Feed' : 'Resume Feed';
      if (streaming && !inFlight) {
        loadNextFrame();
      }
    }

    function snapshot() {
      const w = window.open('');
      w.document.write('<img src="' + img.src + '" style="max-width:100%;height:auto;"/>');
    }

    loadNextFrame();
  </script>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(460800);
    delay(500);

    Serial.println("\n\n==================================================");
    Serial.println("  SORTSENSE: OV5640 CLEAN YUV422 STREAMER         ");
    Serial.println("==================================================");

    if (!psramFound()) {
        Serial.println("[ERROR] PSRAM not found!");
        return;
    }
    Serial.printf("[PSRAM] Octal PSRAM Online: %u KB Free\n", ESP.getFreePsram() / 1024);

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

    // 20 MHz master clock: OV5640 native PLL operating frequency
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_YUV422;
    config.frame_size   = FRAMESIZE_VGA;     // 640x480
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_LATEST;

    Serial.println("[CAMERA] Initializing OV5640 (VGA 640x480, YUV422 @ 20MHz)...");
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[ERROR] esp_camera_init failed: 0x%04x\n", err);
        return;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s != nullptr) {
        g_sensorPid = s->id.PID;
        Serial.printf(">>> [SUCCESS] OV5640 Online! PID: 0x%04X\n", g_sensorPid);
        s->set_vflip(s, 1);
        s->set_hmirror(s, 0);
    }
    g_cameraOnline = true;

    // Warm-up flush
    delay(400);
    for (int i = 0; i < 4; i++) {
        camera_fb_t* dummy = esp_camera_fb_get();
        if (dummy) esp_camera_fb_return(dummy);
        delay(30);
    }

    // Benchmark capture
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb != nullptr) {
        uint8_t* jpg = nullptr;
        size_t jpg_len = 0;
        unsigned long t0 = millis();
        bool ok = frame2jpg(fb, 70, &jpg, &jpg_len);
        unsigned long dt = millis() - t0;
        esp_camera_fb_return(fb);

        if (ok && jpg != nullptr) {
            Serial.printf("[BENCHMARK] YUV422 JPEG: %u bytes in %lu ms\n",
                          static_cast<uint32_t>(jpg_len), dt);
            free(jpg);
        }
    }

    // Wi-Fi SoftAP
    Serial.println("\n[WIFI] Starting SoftAP 'ESP32-S3-CAM-TEST'...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-S3-CAM-TEST");
    IPAddress IP = WiFi.softAPIP();
    Serial.printf("[WIFI] Connected! Open http://%s/ in your browser\n", IP.toString().c_str());

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
        if (fb == nullptr) {
            xSemaphoreGive(cameraMutex);
            req->send(503, "text/plain", "Capture timeout");
            return;
        }

        uint8_t* jpg_buf = nullptr;
        size_t jpg_len = 0;
        bool ok = frame2jpg(fb, 70, &jpg_buf, &jpg_len);
        esp_camera_fb_return(fb);

        if (!ok || jpg_buf == nullptr) {
            xSemaphoreGive(cameraMutex);
            req->send(500, "text/plain", "Compression error");
            return;
        }

        AsyncJpegResponse* resp = new AsyncJpegResponse(jpg_buf, jpg_len, cameraMutex);
        resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        resp->addHeader("Access-Control-Allow-Origin", "*");
        req->send(resp);
    });

    server.begin();
    Serial.println("[SYSTEM] Ready! Check live stream at http://192.168.4.1/");
    Serial.println("[SYSTEM] Send 'c' over Serial at 460800 baud to grab photo anytime!\n");
}

void loop() {
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'c' || c == 's') {
            if (xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                camera_fb_t* fb = esp_camera_fb_get();
                if (fb != nullptr) {
                    uint8_t* jpg_buf = nullptr;
                    size_t jpg_len = 0;
                    bool ok = frame2jpg(fb, 75, &jpg_buf, &jpg_len);
                    esp_camera_fb_return(fb);

                    if (ok && jpg_buf != nullptr) {
                        Serial.println("---BEGIN_PHOTO---");
                        static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                        for (size_t i = 0; i < jpg_len; i += 3) {
                            uint32_t a = (i < jpg_len) ? jpg_buf[i] : 0;
                            uint32_t b = (i + 1 < jpg_len) ? jpg_buf[i + 1] : 0;
                            uint32_t c_byte = (i + 2 < jpg_len) ? jpg_buf[i + 2] : 0;
                            uint32_t triple = (a << 16) + (b << 8) + c_byte;
                            Serial.print(b64_chars[(triple >> 18) & 0x3F]);
                            Serial.print(b64_chars[(triple >> 12) & 0x3F]);
                            Serial.print((i + 1 < jpg_len) ? b64_chars[(triple >> 6) & 0x3F] : '=');
                            Serial.print((i + 2 < jpg_len) ? b64_chars[triple & 0x3F] : '=');
                            if ((i / 3) % 18 == 17) Serial.println();
                        }
                        Serial.println("\n---END_PHOTO---");
                        free(jpg_buf);
                    } else {
                        Serial.println("[ERROR] frame2jpg failed!");
                    }
                } else {
                    Serial.println("[ERROR] esp_camera_fb_get failed!");
                }
                xSemaphoreGive(cameraMutex);
            } else {
                Serial.println("[ERROR] Camera mutex busy!");
            }
        }
    }
    delay(20);
}
