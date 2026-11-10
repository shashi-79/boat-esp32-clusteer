// ============================================================
// CAMERA STREAMER — ESP32-CAM (AI-Thinker OV2640 Module)
// ============================================================
// Connects to the sensor controller SoftAP ("RescueBoat-GCS") as client 192.168.4.2
// Serves low-latency MJPEG video stream on port 81 (/stream)
// Serves dynamic control and status REST APIs on port 80 and port 81 (/control, /status)
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <ctype.h>
#include "esp_camera.h"
#include "esp_http_server.h"

// --- AI-Thinker ESP32-CAM Pin Definitions ---
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// --- Wi-Fi Credentials (Connects to sensor controller SoftAP) ---
#define WIFI_SSID         "RescueBoat-GCS"
#define WIFI_PASSWORD     "rescue2026"

// Static IP configuration on controller AP network
static IPAddress local_IP(192, 168, 4, 2);
static IPAddress gateway(192, 168, 4, 1);
static IPAddress subnet(255, 255, 255, 0);

// Dual HTTP server handles
static httpd_handle_t camera_httpd = NULL; // Port 80 (Control & Status API)
static httpd_handle_t stream_httpd = NULL; // Port 81 (MJPEG Stream & Fallback API)

// FreeRTOS Mutex to synchronize frame capture and dynamic sensor reconfigurations
static SemaphoreHandle_t s_camera_mutex = NULL;

// Target streaming frame rate (frames per second) and real-time measured transmitter FPS
static volatile uint8_t s_target_fps = 25; // Default 25 FPS for smooth low-latency FPV
static volatile float s_live_fps = 0.0f;   // Real-time measured transmitter FPS

static bool cameraReady = false;
static uint32_t lastWifiAttemptMs = 0;
static uint32_t lastCameraAttemptMs = 0;
static uint32_t lastServerAttemptMs = 0;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// Helper to validate supported framesizes
static bool is_supported_framesize(int fs) {
    switch (fs) {
        case FRAMESIZE_QQVGA: // 1 (160x120)
        case FRAMESIZE_QVGA:  // 5 (320x240)
        case FRAMESIZE_CIF:   // 6 (400x296)
        case FRAMESIZE_VGA:   // 8 (640x480)
        case FRAMESIZE_SVGA:  // 9 (800x600)
            return true;
        default:
            return false;
    }
}

// Helper to parse framesize from integer or string literal
static int parse_framesize_value(const char *val) {
    if (!val || !val[0]) return -1;
    if (isdigit((unsigned char)val[0])) {
        return atoi(val);
    }
    if (strcasecmp(val, "svga") == 0 || strcasecmp(val, "framesize_svga") == 0) return FRAMESIZE_SVGA;
    if (strcasecmp(val, "vga") == 0 || strcasecmp(val, "framesize_vga") == 0) return FRAMESIZE_VGA;
    if (strcasecmp(val, "cif") == 0 || strcasecmp(val, "framesize_cif") == 0) return FRAMESIZE_CIF;
    if (strcasecmp(val, "qvga") == 0 || strcasecmp(val, "framesize_qvga") == 0) return FRAMESIZE_QVGA;
    if (strcasecmp(val, "qqvga") == 0 || strcasecmp(val, "framesize_qqvga") == 0) return FRAMESIZE_QQVGA;
    return -1;
}

// --- CORS OPTIONS Preflight Handler ---
static esp_err_t options_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// --- Status HTTP Handler (/status) ---
static esp_err_t status_handler(httpd_req_t *req) {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"Sensor not found\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    uint8_t fs = 0;
    uint8_t q = 0;
    if (s_camera_mutex != NULL && xSemaphoreTake(s_camera_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        fs = s->status.framesize;
        q = s->status.quality;
        xSemaphoreGive(s_camera_mutex);
    } else {
        fs = s->status.framesize;
        q = s->status.quality;
    }

    char response[192];
    snprintf(response, sizeof(response),
             "{\"status\":\"ok\",\"framesize\":%u,\"quality\":%u,\"fps\":%.1f,\"target_fps\":%u,\"psram\":%s,\"rssi\":%d}",
             fs, q, s_live_fps, (unsigned)s_target_fps, psramFound() ? "true" : "false", (int)WiFi.RSSI());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// --- Control HTTP Handler (/control) ---
static esp_err_t control_handler(httpd_req_t *req) {
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len == 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"Missing query parameters\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    char *buf = (char *)malloc(query_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, query_len + 1) != ESP_OK) {
        free(buf);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int new_framesize = -1;
    int new_quality = -1;
    int new_fps = -1;
    char param_buf[32];

    // 1. Check for ?framesize=<val>
    if (httpd_query_key_value(buf, "framesize", param_buf, sizeof(param_buf)) == ESP_OK) {
        new_framesize = parse_framesize_value(param_buf);
    }

    // 2. Check for ?quality=<val>
    if (httpd_query_key_value(buf, "quality", param_buf, sizeof(param_buf)) == ESP_OK) {
        new_quality = atoi(param_buf);
    }

    // 3. Check for ?fps=<val>
    if (httpd_query_key_value(buf, "fps", param_buf, sizeof(param_buf)) == ESP_OK) {
        new_fps = atoi(param_buf);
    }

    // 4. Check for ?var=framesize|quality|fps&val=<val>
    char var_buf[32];
    if (httpd_query_key_value(buf, "var", var_buf, sizeof(var_buf)) == ESP_OK &&
        httpd_query_key_value(buf, "val", param_buf, sizeof(param_buf)) == ESP_OK) {
        if (strcmp(var_buf, "framesize") == 0) {
            new_framesize = parse_framesize_value(param_buf);
        } else if (strcmp(var_buf, "quality") == 0) {
            new_quality = atoi(param_buf);
        } else if (strcmp(var_buf, "fps") == 0) {
            new_fps = atoi(param_buf);
        }
    }

    free(buf);

    if (new_framesize == -1 && new_quality == -1 && new_fps == -1) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"No valid parameters provided\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // Validate FPS (1 to 30)
    if (new_fps != -1) {
        if (new_fps < 1 || new_fps > 30) {
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"FPS out of range (1-30)\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        s_target_fps = (uint8_t)new_fps;
    }

    // Validate framesize
    if (new_framesize != -1) {
        if (!is_supported_framesize(new_framesize)) {
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"Unsupported framesize (supported: 1, 5, 6, 8, 9)\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        if (!psramFound() && new_framesize > FRAMESIZE_CIF) {
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"Resolution exceeds non-PSRAM limit (CIF)\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }

    // Validate quality range (10 to 63)
    if (new_quality != -1) {
        if (new_quality < 10 || new_quality > 63) {
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"Quality out of range (10-63)\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"Sensor not found\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // Acquire mutex before changing sensor configuration
    if (s_camera_mutex == NULL || xSemaphoreTake(s_camera_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"Camera mutex timeout\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    if (new_framesize != -1) {
        s->set_framesize(s, (framesize_t)new_framesize);
    }
    if (new_quality != -1) {
        s->set_quality(s, new_quality);
    }

    // Delay 40ms to allow sensor PLL and registers to stabilize
    vTaskDelay(pdMS_TO_TICKS(40));

    // Drain 2 stale in-flight frames from DMA buffers to ensure fresh stream delivery
    for (int i = 0; i < 2; i++) {
        camera_fb_t *stale = esp_camera_fb_get();
        if (stale) {
            esp_camera_fb_return(stale);
        }
    }

    uint8_t cur_fs = s->status.framesize;
    uint8_t cur_q = s->status.quality;

    xSemaphoreGive(s_camera_mutex);

    Serial.printf("[CAM] Control applied: framesize=%u, quality=%u, target_fps=%u\n", cur_fs, cur_q, (unsigned)s_target_fps);

    char response[192];
    snprintf(response, sizeof(response),
             "{\"status\":\"ok\",\"framesize\":%u,\"quality\":%u,\"fps\":%.1f,\"target_fps\":%u,\"rssi\":%d}",
             cur_fs, cur_q, s_live_fps, (unsigned)s_target_fps, (int)WiFi.RSSI());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// --- Hardened MJPEG HTTP Stream Handler ---
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char part_buf[64];
    int consecutive_drops = 0;

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    // Keep the stream usable from the phone's locally hosted HUD page
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    uint32_t window_start_ms = millis();
    uint32_t window_frames = 0;

    while (true) {
        int64_t frame_start_us = esp_timer_get_time();
        // Acquire mutex around frame acquisition
        if (s_camera_mutex != NULL) {
            if (xSemaphoreTake(s_camera_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                fb = esp_camera_fb_get();
                xSemaphoreGive(s_camera_mutex);
            } else {
                fb = NULL;
            }
        } else {
            fb = esp_camera_fb_get();
        }

        // Handle transient NULL frames during dynamic resolution switches
        if (!fb) {
            consecutive_drops++;
            Serial.printf("[CAM] Frame acquisition transient drop (%d/10)\n", consecutive_drops);
            if (consecutive_drops < 10) {
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            } else {
                Serial.println("[CAM] Maximum consecutive frame drops exceeded — terminating stream");
                res = ESP_FAIL;
                break;
            }
        }
        consecutive_drops = 0;

        if (fb->format != PIXFORMAT_JPEG) {
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if (s_camera_mutex != NULL && xSemaphoreTake(s_camera_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                esp_camera_fb_return(fb);
                xSemaphoreGive(s_camera_mutex);
            } else {
                esp_camera_fb_return(fb);
            }
            fb = NULL;
            if (!jpeg_converted) {
                res = ESP_FAIL;
                break;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }

        if (fb) {
            if (s_camera_mutex != NULL && xSemaphoreTake(s_camera_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                esp_camera_fb_return(fb);
                xSemaphoreGive(s_camera_mutex);
            } else {
                esp_camera_fb_return(fb);
            }
            fb = NULL;
            _jpg_buf = NULL;
        } else if (_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        if (res != ESP_OK) break;

        // Measure actual transmitted frame rate live
        window_frames++;
        uint32_t now_ms = millis();
        uint32_t elapsed_ms = now_ms - window_start_ms;
        if (elapsed_ms >= 1000) {
            s_live_fps = (window_frames * 1000.0f) / elapsed_ms;
            window_frames = 0;
            window_start_ms = now_ms;
        }

        // Dynamic frame rate pacing:
        // Calculate remaining budget to meet s_target_fps rather than always sending with the same fixed delay
        int64_t frame_duration_us = esp_timer_get_time() - frame_start_us;
        uint32_t target_interval_ms = (s_target_fps > 0 && s_target_fps <= 60) ? (1000 / s_target_fps) : 40;
        int32_t remaining_ms = (int32_t)target_interval_ms - (int32_t)(frame_duration_us / 1000);
        uint32_t delay_ms = (remaining_ms > 10) ? (uint32_t)remaining_ms : 10;
        vTaskDelay(pdMS_TO_TICKS(delay_ms)); // Yield to lwIP & Wi-Fi tasks to prevent buffer bloat
    }

    return res;
}

static void stopCameraServer() {
    if (camera_httpd != NULL) {
        httpd_stop(camera_httpd);
        camera_httpd = NULL;
    }
    if (stream_httpd != NULL) {
        httpd_stop(stream_httpd);
        stream_httpd = NULL;
    }
}

static bool startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;

    httpd_config_t stream_config = HTTPD_DEFAULT_CONFIG();
    stream_config.server_port = 81;
    stream_config.ctrl_port = 32769;

    httpd_uri_t control_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = control_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t control_options_uri = {
        .uri       = "/control",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t status_options_uri = {
        .uri       = "/status",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };

    Serial.println("[CAM] Starting dual HTTP servers...");

    // Start Port 80: Dedicated Control & Status Server (never blocked by active stream)
    if (camera_httpd == NULL) {
        if (httpd_start(&camera_httpd, &config) == ESP_OK) {
            httpd_register_uri_handler(camera_httpd, &control_uri);
            httpd_register_uri_handler(camera_httpd, &status_uri);
            httpd_register_uri_handler(camera_httpd, &control_options_uri);
            httpd_register_uri_handler(camera_httpd, &status_options_uri);
            Serial.println("[CAM] Control server started at http://192.168.4.2/control and /status");
        } else {
            Serial.println("[ERR] Failed to start control server on port 80");
            camera_httpd = NULL;
            return false;
        }
    }

    // Start Port 81: Dedicated MJPEG Stream Server (+ fallback control/status endpoints)
    if (stream_httpd == NULL) {
        if (httpd_start(&stream_httpd, &stream_config) == ESP_OK) {
            httpd_register_uri_handler(stream_httpd, &stream_uri);
            httpd_register_uri_handler(stream_httpd, &control_uri);
            httpd_register_uri_handler(stream_httpd, &status_uri);
            httpd_register_uri_handler(stream_httpd, &control_options_uri);
            httpd_register_uri_handler(stream_httpd, &status_options_uri);
            Serial.println("[CAM] Stream server started at http://192.168.4.2:81/stream");
        } else {
            Serial.println("[ERR] Failed to start stream server on port 81");
            stream_httpd = NULL;
            stopCameraServer();
            return false;
        }
    }

    return true;
}

static bool initCamera() {
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_SVGA;      // Pre-allocate PSRAM buffers for max operational resolution (SVGA 800x600)
    config.jpeg_quality = 12;               // 0-63 (lower = higher quality)
    config.fb_count = 2;                    // Double buffering in PSRAM
    config.grab_mode = CAMERA_GRAB_LATEST;

    // Check PSRAM presence
    if (psramFound()) {
        config.fb_count = 2;
        Serial.println("[CAM] PSRAM detected — double buffering enabled (max SVGA)");
    } else {
        config.frame_size = FRAMESIZE_CIF;  // Fallback to 400x296 if no PSRAM
        config.fb_count = 1;
        Serial.println("[WARN] No PSRAM — dropping resolution to CIF");
    }

    // Init Camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[ERR] Camera init failed with error 0x%x\n", err);
        return false;
    }
    Serial.println("[CAM] OV2640 sensor initialized");

    // Immediately step down to default operational resolution FRAMESIZE_VGA and quality 12
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        if (psramFound()) {
            s->set_framesize(s, FRAMESIZE_VGA);
        }
        s->set_quality(s, 12);
        Serial.printf("[CAM] Initial operational mode set: framesize=%u (VGA), quality=%u\n",
                      s->status.framesize, s->status.quality);
    }

    return true;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("========================================");
    Serial.println("  RESCUE BOAT — Camera Node (OV2640)");
    Serial.println("========================================");

    // 0. Initialize camera synchronization mutex
    if (s_camera_mutex == NULL) {
        s_camera_mutex = xSemaphoreCreateMutex();
    }

    // 1. Camera Configuration
    cameraReady = initCamera();
    lastCameraAttemptMs = millis();

    // 2. Connect to Boat Controller SoftAP
    WiFi.mode(WIFI_STA);
    if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("[WARN] Static IP configuration failed");
    }
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("[WIFI] Connecting to controller SoftAP");
    uint32_t startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt < 15000)) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" Connected!");
        Serial.printf("[WIFI] Cam IP: %s\n", WiFi.localIP().toString().c_str());
        if (cameraReady) {
            startCameraServer();
            lastServerAttemptMs = millis();
        }
    } else {
        Serial.println("\n[WARN] Connection timeout — will retry in loop");
    }
}

void loop() {
    const uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED && now - lastWifiAttemptMs >= 5000) {
        Serial.println("[WIFI] Reconnecting to controller...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        lastWifiAttemptMs = now;
    }

    if (!cameraReady && now - lastCameraAttemptMs >= 10000) {
        lastCameraAttemptMs = now;
        cameraReady = initCamera();
    }

    if (WiFi.status() == WL_CONNECTED && cameraReady &&
        (camera_httpd == NULL || stream_httpd == NULL) &&
        now - lastServerAttemptMs >= 5000) {
        lastServerAttemptMs = now;
        if (!startCameraServer()) {
            Serial.println("[WARN] Server start failed; will retry");
        }
    }
    delay(20);
}
