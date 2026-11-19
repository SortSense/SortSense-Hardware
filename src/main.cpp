/**
 * @file main.cpp
 * @brief System startup, dual-core FreeRTOS task scheduling, non-blocking
 * Wi-Fi, and asynchronous HTTP server endpoints for SortSense with OV5640
 * camera.
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
// GLOBAL INSTANCES & SYNCHRONIZATION PRIMITIVES
// =============================================================================
static SemaphoreHandle_t cameraMutex = nullptr;
static TaskHandle_t networkTaskHandle = nullptr;
static TaskHandle_t hardwareTaskHandle = nullptr;

static WiFiManager wm;
static AsyncWebServer server(HTTP_SERVER_PORT);
static bool isServerRunning = false;
static uint32_t g_frameCount = 0;

// =============================================================================
// EMBEDDED WEB DASHBOARD (HTML / CSS / JS)
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
        <div class="stat-val">HVGA (480x320)</div>
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
  // Route 2: GET /capture - Acquire and stream JPEG snapshot via YUV422->JPEG
  // -------------------------------------------------------------------------
  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (cameraMutex == nullptr) {
      request->send(500, "text/plain", "Camera mutex uninitialized\n");
      return;
    }

    // Attempt mutex acquisition with bounded timeout
    if (xSemaphoreTake(cameraMutex, CAMERA_MUTEX_TIMEOUT) != pdTRUE) {
      request->send(503, "text/plain",
                    "Camera busy - lock acquisition timeout\n");
      return;
    }

    // Verify camera operational state
    if (!is_camera_initialized()) {
      xSemaphoreGive(cameraMutex);
      request->send(500, "text/plain", "Camera hardware not initialized\n");
      return;
    }

    // Acquire frame buffer from hardware driver (YUV422 in PSRAM)
    camera_fb_t *fb = camera_acquire_frame();
    if (fb == nullptr) {
      xSemaphoreGive(cameraMutex);
      request->send(503, "text/plain", "Failed to capture frame from camera\n");
      return;
    }

    g_frameCount++;

    // Convert YUV422 DMA buffer to compressed JPEG
    uint8_t *jpg_buf = nullptr;
    size_t jpg_len = 0;
    bool ok = frame2jpg(fb, 70, &jpg_buf, &jpg_len);
    camera_release_frame(fb); // Release DMA buffer back to driver immediately

    if (!ok || jpg_buf == nullptr) {
      xSemaphoreGive(cameraMutex);
      request->send(500, "text/plain", "JPEG compression failed\n");
      return;
    }

    // Wrap dynamically allocated JPEG buffer in RAII streaming response
    AsyncJpegResponse *response =
        new (std::nothrow) AsyncJpegResponse(jpg_buf, jpg_len, cameraMutex);
    if (response == nullptr) {
      free(jpg_buf);
      xSemaphoreGive(cameraMutex);
      request->send(500, "text/plain", "Failed to allocate HTTP response\n");
      return;
    }

    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request->send(response);
  });

  // -------------------------------------------------------------------------
  // Route 3: GET /status - Return JSON system telemetry and heap diagnostics
  // -------------------------------------------------------------------------
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    char statusJson[256];
    snprintf(statusJson, sizeof(statusJson),
             "{\"status\":\"ok\",\"ip\":\"%s\",\"wifi_connected\":%s,\"camera_"
             "initialized\":%s,\"frame_count\":%u,\"free_heap\":%u,\"free_"
             "psram\":%u,\"uptime_ms\":%lu}",
             (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString().c_str()
                                             : "0.0.0.0",
             (WiFi.status() == WL_CONNECTED) ? "true" : "false",
             is_camera_initialized() ? "true" : "false", g_frameCount,
             static_cast<uint32_t>(ESP.getFreeHeap()),
             static_cast<uint32_t>(ESP.getFreePsram()),
             static_cast<unsigned long>(millis()));
    request->send(200, "application/json", statusJson);
  });

  // -------------------------------------------------------------------------
  // Route 4: 404 Handler - Unmatched routes
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

  // 2. Initialize Camera Driver via modular camera_handler interface (Clean
  // YUV422)
  Serial.println("[Init] Initializing OV5640 Camera Driver...");
  esp_err_t camErr = camera_init();
  if (camErr != ESP_OK) {
    Serial.printf("[WARN] Camera initialization failed (0x%x). Operating in "
                  "degraded mode.\n",
                  camErr);
  } else {
    Serial.println("[Init] OV5640 Camera initialized successfully.");
  }

  // 3. Configure HTTP routes on AsyncWebServer instance
  setupServerRoutes();

  // 4. Configure WiFiManager in non-blocking mode
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
          uint8_t *jpg_buf = nullptr;
          size_t jpg_len = 0;
          bool ok = frame2jpg(fb, 75, &jpg_buf, &jpg_len);
          camera_release_frame(fb);

          if (ok && jpg_buf != nullptr) {
            Serial.println("---BEGIN_PHOTO---");
            static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefgh"
                                            "ijklmnopqrstuvwxyz0123456789+/";
            for (size_t i = 0; i < jpg_len; i += 3) {
              uint32_t a = (i < jpg_len) ? jpg_buf[i] : 0;
              uint32_t b = (i + 1 < jpg_len) ? jpg_buf[i + 1] : 0;
              uint32_t c_byte = (i + 2 < jpg_len) ? jpg_buf[i + 2] : 0;
              uint32_t triple = (a << 16) + (b << 8) + c_byte;
              Serial.print(b64_chars[(triple >> 18) & 0x3F]);
              Serial.print(b64_chars[(triple >> 12) & 0x3F]);
              Serial.print((i + 1 < jpg_len) ? b64_chars[(triple >> 6) & 0x3F]
                                             : '=');
              Serial.print((i + 2 < jpg_len) ? b64_chars[triple & 0x3F] : '=');
              if ((i / 3) % 18 == 17)
                Serial.println();
            }
            Serial.println("\n---END_PHOTO---");
            free(jpg_buf);
          } else {
            Serial.println("[ERROR] frame2jpg failed!");
          }
        } else {
          Serial.println("[ERROR] camera_acquire_frame failed!");
        }
        xSemaphoreGive(cameraMutex);
      } else {
        Serial.println("[ERROR] Camera mutex busy!");
      }
    }
  }

  // Yield execution slice to idle task and watchdog on Core 1
  vTaskDelay(pdMS_TO_TICKS(20));
}
