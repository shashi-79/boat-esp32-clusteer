#pragma once
// ============================================================
// Shore Station — Configuration
// ============================================================

#include "../../shared/packets.h"

// --- Wi-Fi SoftAP (phone connects to this) ---
// Note: Must match ESPNOW_CHANNEL because ESP32 radio shares one channel in AP+STA mode
#define WIFI_SSID       "RescueBoat-GCS"
#define WIFI_PASSWORD   "rescue2026"
#define WIFI_CHANNEL    ESPNOW_CHANNEL

// --- Web Server ---
#define WEB_PORT        80
#define WS_PATH         "/ws"

// --- Control Send Rate ---
#define CONTROL_INTERVAL_MS   (1000 / CONTROL_SEND_HZ)  // 40ms = 25 Hz
#define TELEMETRY_LOG_INTERVAL_MS  1000  // Debug print rate

// --- Debug ---
#ifdef DEBUG_ENABLED
  #define DBG(fmt, ...) Serial.printf("[DBG] " fmt "\n", ##__VA_ARGS__)
#else
  #define DBG(fmt, ...)
#endif
