// ============================================================
// SHORE STATION — Main Entry Point (ESP32 WROOM-32U)
// ============================================================
// Runs:
//   1. Wi-Fi SoftAP for phone connection
//   2. AsyncWebServer serving HUD from PROGMEM
//   3. WebSocket for joystick ↔ telemetry
//   4. ESP-NOW LR for 1.5–2km boat communication
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "../../shared/packets.h"
#include "radio.h"
#include "webserver.h"

static uint32_t lastControlSend  = 0;
static uint32_t lastTelemetrySend = 0;
static uint32_t lastCleanup       = 0;
static uint32_t lastDebugLog      = 0;

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("========================================");
    Serial.println("  RESCUE BOAT — Shore Station v1.0");
    Serial.println("========================================");

    // --- Wi-Fi: AP mode for phone + STA mode for ESP-NOW ---
    WiFi.mode(WIFI_AP_STA);

    // Start SoftAP
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[WIFI] AP started: SSID=%s  IP=%s\n", WIFI_SSID, apIP.toString().c_str());

    // --- ESP-NOW LR Radio ---
    shoreRadioSetup();

    // --- Web Server + WebSocket ---
    webSetup();

    Serial.println("[BOOT] Shore station ready.");
    Serial.printf("[BOOT] Connect phone to Wi-Fi '%s' and open http://%s\n",
        WIFI_SSID, apIP.toString().c_str());
}

void loop() {
    uint32_t now = millis();

    // --- 25 Hz: Send control packets to boat ---
    if (now - lastControlSend >= CONTROL_INTERVAL_MS) {
        lastControlSend = now;
        shoreSendControl();
    }

    // --- 5 Hz: Forward telemetry to phone via WebSocket ---
    if (now - lastTelemetrySend >= (1000 / TELEMETRY_SEND_HZ)) {
        lastTelemetrySend = now;
        webSendTelemetry();
    }

    // --- 1 Hz: Cleanup dead WebSocket connections ---
    if (now - lastCleanup >= 1000) {
        lastCleanup = now;
        webCleanup();
    }

    // --- 1 Hz: Debug log ---
    if (now - lastDebugLog >= TELEMETRY_LOG_INTERVAL_MS) {
        lastDebugLog = now;

        Serial.printf("[TELEM] Link=%s Sats=%d Armed=%d Mode=%d Batt=%dmV Heading=%d Home=%.0fm Radar=%.1fm\n",
            shoreIsLinked() ? "UP" : "DOWN",
            rxTelemetry.sats,
            rxTelemetry.armed,
            rxTelemetry.mode,
            rxTelemetry.v_motor_mv,
            rxTelemetry.heading,
            rxTelemetry.home_dist,
            rxTelemetry.radar_dist);
    }
}
