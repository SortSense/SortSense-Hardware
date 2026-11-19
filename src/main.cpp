/**
 * @file main.cpp
 * @brief System startup, dual-core FreeRTOS task scheduling, non-blocking
 * Wi-Fi, and high-performance asynchronous HTTP streaming for SortSense with
 * OV5640.
 * @target ESP32-S3 WROOM CAM (N16R8) with OV5640 Camera
 */

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <new>

// CRITICAL INCLUDE ORDER: WiFiManager MUST precede ESPAsyncWebServer to allow
// WebServer.h to define WEBSERVER_H and avoid conflicting HTTP_GET
// enumerations.
// clang-format off
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
// clang-format on
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <img_converters.h>

#include "async_camera_response.h"
#include "camera_handler.h"
#include "config.h"

// =============================================================================
// FREERTOS CONFIGURATION & SYSTEM CONSTANTS
// =============================================================================
constexpr TickType_t NETWORK_LOOP_DELAY = pdMS_TO_TICKS(10);
constexpr TickType_t HARDWARE_LOOP_DELAY = pdMS_TO_TICKS(50);
constexpr TickType_t CAMERA_MUTEX_TIMEOUT =
    pdMS_TO_TICKS(CAMERA_MUTEX_TIMEOUT_MS);

// =============================================================================
// GLOBAL INSTANCES & RUNTIME CAMERA TUNING
// =============================================================================
static SemaphoreHandle_t cameraMutex = nullptr;
static TaskHandle_t networkTaskHandle = nullptr;
static TaskHandle_t hardwareTaskHandle = nullptr;

static WiFiManager wm;
static AsyncWebServer server(HTTP_SERVER_PORT);
static bool isServerRunning = false;
static uint32_t g_frameCount = 0;

// Runtime adjustable camera parameters for high-FPS streaming & orientation
static uint8_t g_jpegQuality = 60; // 55-65 gives ~2x faster compression than 75
static framesize_t g_frameSize = FRAMESIZE_VGA;
static int g_vflip = 1; // Invert vertical flip by default (OV5640 board mount)
static int g_hmirror = 0;

// =============================================================================
// EMBEDDED WEB DASHBOARD (HTML / CSS / JS) WITH CONTROLS & HIGH-FPS PACING
// =============================================================================
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
    img { width: 100%; max-width: 640px; height: auto; display: block; border-radius: 8px; transition: transform 0.2s ease; }
    .toolbar { display: flex; flex-wrap: wrap; gap: 8px; justify-content: center; margin-top: 14px; }
    button, select { background: #0284c7; color: white; border: none; padding: 8px 14px; border-radius: 8px; font-weight: 600; cursor: pointer; transition: background 0.2s; font-size: 13px; }
    button:hover, select:hover { background: #0369a1; }
    button.secondary { background: #475569; }
    button.secondary:hover { background: #334155; }
    button.active { background: #10b981; }
    .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 10px; margin-top: 16px; text-align: left; }
    .stat-card { background: #0f172a; padding: 10px 14px; border-radius: 8px; border: 1px solid #334155; }
    .stat-label { font-size: 11px; text-transform: uppercase; color: #94a3b8; font-weight: 700; letter-spacing: 0.5px; }
    .stat-val { font-size: 14px; font-weight: 600; color: #38bdf8; margin-top: 2px; }
    .tip { font-size: 12px; color: #94a3b8; margin-top: 12px; }
  </style>
</head>
<body>
  <h1>SortSense OV5640 Camera Feed</h1>
  <div class="badge" id="sensorBadge">ESP32-S3 OV5640 Optimized Stream</div>

  <div class="container">
    <div class="feed-wrapper">
      <img id="liveFeed" src="/capture" alt="Live Stream Loading..." />
    </div>

    <!-- Quick Controls for Orientation & Performance -->
    <div class="toolbar">
      <button id="btnVflip" onclick="toggleControl('vflip')">↕ Flip V: ON</button>
      <button id="btnHmirror" onclick="toggleControl('hmirror')">↔ Mirror H: OFF</button>
      <select id="selRes" onchange="changeResolution(this.value)">
        <option value="9" selected>VGA (640x480) - High Res</option>
        <option value="7">HVGA (480x320) - Balanced</option>
        <option value="5">QVGA (320x240) - Smooth 15+ FPS</option>
      </select>
      <button class="secondary" id="btnStream" onclick="toggleStream()">Pause Feed</button>
      <button onclick="snapshot()">Snapshot</button>
    </div>

    <div class="stats">
      <div class="stat-card">
        <div class="stat-label">Camera Status</div>
        <div class="stat-val" id="statStatus" style="color: #4ade80;">ONLINE</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Frame Rate</div>
        <div class="stat-val" id="statFps" style="color: #4ade80;">--</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Frames Streamed</div>
        <div class="stat-val" id="statFrames">0</div>
      </div>
      <div class="stat-card">
        <div class="stat-label">Resolution</div>
        <div class="stat-val" id="statRes">VGA (640x480)</div>
      </div>
    </div>

    <div class="tip">Tip: Switch to <b>QVGA</b> for ultra-fast, low-latency live video streaming (~15-20 FPS).</div>
  </div>

  <script>
    const img = document.getElementById('liveFeed');
    let frames = 0;
    let streaming = true;
    let inFlight = false;
    let vflip = 1;
    let hmirror = 0;

    function applyTransforms() {
      img.style.transform = `scale(${hmirror ? -1 : 1}, ${vflip ? -1 : 1})`;
    }

    function toggleControl(variable) {
      if (variable === 'vflip') {
        vflip = vflip ? 0 : 1;
        document.getElementById('btnVflip').innerText = '↕ Flip V: ' + (vflip ? 'ON' : 'OFF');
        fetch('/control?var=vflip&val=' + vflip).catch(console.error);
      } else if (variable === 'hmirror') {
        hmirror = hmirror ? 0 : 1;
        document.getElementById('btnHmirror').innerText = '↔ Mirror H: ' + (hmirror ? 'ON' : 'OFF');
        fetch('/control?var=hmirror&val=' + hmirror).catch(console.error);
      }
    }

    function changeResolution(val) {
      const resMap = { '5': 'QVGA (320x240)', '7': 'HVGA (480x320)', '9': 'VGA (640x480)' };
      document.getElementById('statRes').innerText = resMap[val] || 'Custom';
      fetch('/control?var=framesize&val=' + val)
        .then(() => { inFlight = false; })
        .catch(console.error);
    }

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
        // Zero delay between frames for maximum possible frame rate
        setTimeout(loadNextFrame, 0);
      };

      nextImg.onerror = function() {
        inFlight = false;
        setTimeout(loadNextFrame, 150);
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

// =============================================================================
// HTTP ROUTE DEFINITIONS
// =============================================================================
/**
 * @brief Registers all HTTP request handlers on the AsyncWebServer instance.
 */
void setupServerRoutes(void) {
  // -------------------------------------------------------------------------
  // Route 1: GET / - Live Camera Stream Web Dashboard
  // -------------------------------------------------------------------------
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  // -------------------------------------------------------------------------
  // Route 2: GET /capture - High-Performance Snapshot Streaming
  // -------------------------------------------------------------------------
  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (cameraMutex == nullptr) {
      request->send(500, "text/plain", "Camera mutex uninitialized\n");
      return;
    }

    // Bounded mutex acquisition
    if (xSemaphoreTake(cameraMutex, CAMERA_MUTEX_TIMEOUT) != pdTRUE) {
      request->send(503, "text/plain",
                    "Camera busy - lock acquisition timeout\n");
      return;
    }

    if (!is_camera_initialized()) {
      xSemaphoreGive(cameraMutex);
      request->send(500, "text/plain", "Camera hardware not initialized\n");
      return;
    }

    // 1. Acquire raw DMA frame buffer from camera driver
    camera_fb_t *fb = camera_acquire_frame();
    if (fb == nullptr) {
      xSemaphoreGive(cameraMutex);
      request->send(503, "text/plain", "Failed to capture frame from camera\n");
      return;
    }

    g_frameCount++;

    // Format handling: Direct Hardware JPEG vs Software Compression fallback
    if (fb->format == PIXFORMAT_JPEG) {
      // 1. HARDWARE JPEG: Zero-copy DMA streaming via AsyncCameraResponse
      // (Automatically releases frame buffer and cameraMutex when HTTP stream
      // completes)
      AsyncCameraResponse *response =
          new (std::nothrow) AsyncCameraResponse(fb, cameraMutex);
      if (response == nullptr) {
        camera_release_frame(fb);
        xSemaphoreGive(cameraMutex);
        request->send(500, "text/plain", "Failed to allocate HTTP response\n");
        return;
      }
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Cache-Control",
                          "no-cache, no-store, must-revalidate");
      request->send(response);
      return;
    }

    // 2. RAW FALLBACK (YUV422 / RGB565): Software JPEG conversion via frame2jpg
    uint8_t *jpg_buf = nullptr;
    size_t jpg_len = 0;
    bool ok = frame2jpg(fb, g_jpegQuality, &jpg_buf, &jpg_len);
    camera_release_frame(fb);
    xSemaphoreGive(cameraMutex);

    if (!ok || jpg_buf == nullptr) {
      request->send(500, "text/plain", "JPEG compression failed\n");
      return;
    }

    AsyncJpegResponse *response =
        new (std::nothrow) AsyncJpegResponse(jpg_buf, jpg_len);
    if (response == nullptr) {
      free(jpg_buf);
      request->send(500, "text/plain", "Failed to allocate HTTP response\n");
      return;
    }

    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request->send(response);
  });

  // -------------------------------------------------------------------------
  // Route 3: GET /control - Dynamic Sensor & Orientation Tuning
  // -------------------------------------------------------------------------
  server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request) {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
      request->send(500, "text/plain", "Camera sensor uninitialized\n");
      return;
    }

    if (request->hasParam("var") && request->hasParam("val")) {
      String var = request->getParam("var")->value();
      int val = request->getParam("val")->value().toInt();

      if (var == "vflip") {
        g_vflip = val;
        if (s->set_vflip != nullptr) {
          s->set_vflip(s, val);
        }
      } else if (var == "hmirror") {
        g_hmirror = val;
        if (s->set_hmirror != nullptr) {
          s->set_hmirror(s, val);
        }
      } else if (var == "framesize") {
        if (cameraMutex != nullptr &&
            xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          g_frameSize = static_cast<framesize_t>(val);
          if (s->set_framesize != nullptr) {
            s->set_framesize(s, g_frameSize);
          }
          xSemaphoreGive(cameraMutex);
        }
      } else if (var == "quality") {
        g_jpegQuality = constrain(val, 10, 95);
      }
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing parameter 'var' or 'val'");
    }
  });

  // -------------------------------------------------------------------------
  // Route 4: GET /status - Return JSON system telemetry and heap diagnostics
  // -------------------------------------------------------------------------
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    char statusJson[300];
    snprintf(
        statusJson, sizeof(statusJson),
        "{\"status\":\"ok\",\"ip\":\"%s\",\"wifi_connected\":%s,\"camera_"
        "initialized\":%s,\"frame_count\":%u,\"vflip\":%d,\"hmirror\":%d,"
        "\"quality\":%u,\"free_heap\":%u,\"free_psram\":%u,\"uptime_ms\":%lu}",
        (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString().c_str()
                                        : "0.0.0.0",
        (WiFi.status() == WL_CONNECTED) ? "true" : "false",
        is_camera_initialized() ? "true" : "false", g_frameCount, g_vflip,
        g_hmirror, g_jpegQuality, static_cast<uint32_t>(ESP.getFreeHeap()),
        static_cast<uint32_t>(ESP.getFreePsram()),
        static_cast<unsigned long>(millis()));
    request->send(200, "application/json", statusJson);
  });

  // -------------------------------------------------------------------------
  // Route 5: 404 Handler - Unmatched routes
  // -------------------------------------------------------------------------
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "404: Not Found\n");
  });
}

// =============================================================================
// FREERTOS TASK IMPLEMENTATIONS
// =============================================================================
/**
 * @brief Core 0 Task: Servicing WiFiManager captive portal and network state.
 */
void networkTask(void *pvParameters) {
  (void)pvParameters;
  Serial.println("[NetworkTask] Started on Core 0");

  bool previousConnectedState = false;

  for (;;) {
    // Service non-blocking captive portal DNS & HTTP requests
    wm.process();

    bool currentConnectedState = (WiFi.status() == WL_CONNECTED);

    // State transition: Wi-Fi connection established
    if (currentConnectedState && !previousConnectedState) {
      previousConnectedState = true;
      Serial.print("[WiFi] Connected! Assigned IP: ");
      Serial.println(WiFi.localIP());

      // Defer starting AsyncWebServer until Wi-Fi is established
      if (!isServerRunning) {
        server.begin();
        isServerRunning = true;
        Serial.printf(
            "[WebServer] Async HTTP Server listening on http://%s:%u/\n",
            WiFi.localIP().toString().c_str(), HTTP_SERVER_PORT);
      }
    }
    // State transition: Wi-Fi connection lost
    else if (!currentConnectedState && previousConnectedState) {
      previousConnectedState = false;
      Serial.println("[WiFi] Connection lost. Attempting reconnection...");
    }

    // Cooperative yield to reset Core 0 Task Watchdog Timer (TWDT)
    vTaskDelay(NETWORK_LOOP_DELAY);
  }
}

/**
 * @brief Core 1 Task: Hardware acquisition, sensor polling, and peripheral
 * control.
 */
void hardwareTask(void *pvParameters) {
  (void)pvParameters;
  Serial.println("[HardwareTask] Started on Core 1");

  for (;;) {
    // Hardware task loop - handles sensor polling and future peripheral
    // pipelines (4x VL53L0X ToF ranging, LM393 IR break-beam interrupts,
    // WS2812B animations)
    vTaskDelay(HARDWARE_LOOP_DELAY);
  }
}

// =============================================================================
// SYSTEM ENTRY POINTS (SETUP & LOOP)
// =============================================================================
void setup(void) {
  // Initialize Serial console for USB CDC / UART output
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000); // Allow serial connection to stabilize on boot
  Serial.println("\n=== SortSense ESP32-S3 Firmware Initializing ===");

  // 1. Create mutual exclusion semaphore for thread-safe camera access
  cameraMutex = xSemaphoreCreateMutex();
  if (cameraMutex == nullptr) {
    Serial.println("[FATAL] Failed to create cameraMutex! Halting.");
    while (true) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  // 2. Configure WiFiManager in non-blocking mode FIRST
  // (Ensures network association does not contend with camera DMA interrupts)
  Serial.println("[Init] Initializing WiFiManager (non-blocking)...");
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SEC);

  bool connected = wm.autoConnect(WIFI_AP_NAME);
  if (connected) {
    Serial.print("[WiFi] Connected to configured AP. IP: ");
    Serial.println(WiFi.localIP());
    server.begin();
    isServerRunning = true;
    Serial.printf("[WebServer] Async HTTP Server listening on http://%s:%u/\n",
                  WiFi.localIP().toString().c_str(), HTTP_SERVER_PORT);
  } else {
    Serial.println("[WiFi] Starting non-blocking connection / captive portal "
                   "'SortSense-AP'...");
  }

  // 3. Initialize Camera Driver via modular camera_handler interface
  Serial.println("[Init] Initializing OV5640 Camera Driver...");
  esp_err_t camErr = camera_init();
  if (camErr != ESP_OK) {
    Serial.printf("[WARN] Camera initialization failed (0x%x). Operating in "
                  "degraded mode.\n",
                  camErr);
  } else {
    Serial.println("[Init] OV5640 Camera initialized successfully.");
    // Set hardware orientation defaults
    sensor_t *s = esp_camera_sensor_get();
    if (s != nullptr) {
      if (s->set_vflip != nullptr)
        s->set_vflip(s, g_vflip);
      if (s->set_hmirror != nullptr)
        s->set_hmirror(s, g_hmirror);
    }

    // Warm-up flush: Clear sensor PLL sync & DMA buffers
    delay(200);
    for (int i = 0; i < 4; ++i) {
      camera_fb_t *dummy = camera_acquire_frame();
      if (dummy != nullptr) {
        camera_release_frame(dummy);
      }
      delay(30);
    }
  }

  // 4. Configure HTTP routes on AsyncWebServer instance
  setupServerRoutes();

  // 5. Pin FreeRTOS tasks to specific processor cores
  BaseType_t netTaskStatus = xTaskCreatePinnedToCore(
      networkTask, "NetworkTask", TASK_STACK_NETWORK, nullptr,
      TASK_PRIORITY_NETWORK, &networkTaskHandle, CORE_0_NETWORK);

  BaseType_t hwTaskStatus = xTaskCreatePinnedToCore(
      hardwareTask, "HardwareTask", TASK_STACK_HARDWARE, nullptr,
      TASK_PRIORITY_HARDWARE, &hardwareTaskHandle, CORE_1_HARDWARE);

  if (netTaskStatus != pdPASS || hwTaskStatus != pdPASS) {
    Serial.println("[ERROR] Failed to schedule one or more FreeRTOS tasks!");
  } else {
    Serial.println("[Init] FreeRTOS dual-core tasks scheduled successfully.");
  }

  Serial.println("=== SortSense Initialization Complete ===");
  Serial.printf("[SYSTEM] Send 'c' or 's' over Serial at %u baud to capture "
                "snapshot anytime.\n",
                SERIAL_BAUD_RATE);
}

void loop(void) {
  // High-speed Serial photo grab listener (compatible with tools/grab_test.py)
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'c' || c == 's') {
      if (cameraMutex != nullptr &&
          xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        camera_fb_t *fb = camera_acquire_frame();
        if (fb != nullptr) {
          uint8_t *send_buf = nullptr;
          size_t send_len = 0;
          bool must_free = false;

          if (fb->format == PIXFORMAT_JPEG) {
            send_buf = fb->buf;
            send_len = fb->len;
          } else {
            bool ok = frame2jpg(fb, 75, &send_buf, &send_len);
            if (ok) {
              must_free = true;
            }
          }

          if (send_buf != nullptr && send_len > 0) {
            Serial.println("---BEGIN_PHOTO---");
            static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefgh"
                                            "ijklmnopqrstuvwxyz0123456789+/";
            for (size_t i = 0; i < send_len; i += 3) {
              uint32_t a = (i < send_len) ? send_buf[i] : 0;
              uint32_t b = (i + 1 < send_len) ? send_buf[i + 1] : 0;
              uint32_t c_byte = (i + 2 < send_len) ? send_buf[i + 2] : 0;
              uint32_t triple = (a << 16) + (b << 8) + c_byte;
              Serial.print(b64_chars[(triple >> 18) & 0x3F]);
              Serial.print(b64_chars[(triple >> 12) & 0x3F]);
              Serial.print((i + 1 < send_len) ? b64_chars[(triple >> 6) & 0x3F]
                                              : '=');
              Serial.print((i + 2 < send_len) ? b64_chars[triple & 0x3F] : '=');
              if ((i / 3) % 18 == 17)
                Serial.println();
            }
            Serial.println("\n---END_PHOTO---");
            if (must_free) {
              free(send_buf);
            }
          } else {
            Serial.println("[ERROR] JPEG encoding failed!");
          }
          camera_release_frame(fb);
          xSemaphoreGive(cameraMutex);
        } else {
          xSemaphoreGive(cameraMutex);
          Serial.println("[ERROR] camera_acquire_frame failed!");
        }
      } else {
        Serial.println("[ERROR] Camera mutex busy!");
      }
    }
  }

  // Yield execution slice to idle task and watchdog on Core 1
  vTaskDelay(pdMS_TO_TICKS(20));
}
