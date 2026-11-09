// ============================================================
// CAMERA STREAMER — ESP32-CAM (AI-Thinker OV2640 Module)
// ============================================================
// Connects to the sensor controller SoftAP ("RescueBoat-GCS") as client 192.168.4.2
// Serves low-latency MJPEG video stream on port 81 (/stream)
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
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

static httpd_handle_t stream_httpd = NULL;
static bool cameraReady = false;
static uint32_t lastWifiAttemptMs = 0;
static uint32_t lastCameraAttemptMs = 0;
static uint32_t lastServerAttemptMs = 0;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// --- MJPEG HTTP Handler ---
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char part_buf[64];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    // Keep the stream usable from the phone's locally hosted HUD page.
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("[CAM] Frame buffer acquisition failed");
            res = ESP_FAIL;
            break;
        }

        if (fb->format != PIXFORMAT_JPEG) {
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            esp_camera_fb_return(fb);
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
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if (_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        if (res != ESP_OK) break;
    }

    return res;
}

static bool startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 81;
    config.ctrl_port = 32768;

    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        if (httpd_register_uri_handler(stream_httpd, &stream_uri) != ESP_OK) {
            httpd_stop(stream_httpd);
            stream_httpd = NULL;
            return false;
        }
        Serial.println("[CAM] Stream server started at http://192.168.4.2:81/stream");
        return true;
    }
    return false;
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
    config.frame_size = FRAMESIZE_VGA;      // 640x480 (optimal balance for low latency Wi-Fi)
    config.jpeg_quality = 12;               // 0-63 (lower = higher quality)
    config.fb_count = 2;                    // Double buffering in PSRAM
    config.grab_mode = CAMERA_GRAB_LATEST;

    // Check PSRAM presence
    if (psramFound()) {
        config.fb_count = 2;
        Serial.println("[CAM] PSRAM detected — double buffering enabled");
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
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("========================================");
    Serial.println("  RESCUE BOAT — Camera Node (OV2640)");
    Serial.println("========================================");

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

    if (WiFi.status() == WL_CONNECTED && cameraReady && stream_httpd == NULL &&
        now - lastServerAttemptMs >= 5000) {
        lastServerAttemptMs = now;
        if (!startCameraServer()) Serial.println("[WARN] Stream server start failed; will retry");
    }
    delay(20);
}
