#pragma once
// ============================================================
// Boat Controller — AsyncWebServer & WebSocket Control Bridge
// (Runs directly on ESP32-S3 SoftAP)
// ============================================================

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "web_ui.h"

static AsyncWebServer server(WEB_PORT);
static AsyncWebSocket  wsocket(WS_PATH);

// Live control inputs received from phone
static volatile int16_t  ctrlThrottle = 0;
static volatile int16_t  ctrlYaw      = 0;
static volatile int16_t  ctrlCamPan   = 0;
static volatile int16_t  ctrlCamTilt  = 0;
static volatile uint8_t  ctrlArm      = 0;
static volatile uint8_t  ctrlEstop    = 0;
static volatile uint8_t  ctrlMode     = 0;
static volatile int16_t  phoneHeading = 0;  // Operator phone gyroscope/compass heading

static volatile uint32_t lastPhonePacketTime = 0;

// --- Handle WebSocket Events from Phone ---
static void onWsEvent(AsyncWebSocket *srv, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        DBG("WebSocket: Phone connected from %s (id: %u)",
            client->remoteIP().toString().c_str(), client->id());
    }
    else if (type == WS_EVT_DISCONNECT) {
        DBG("WebSocket: Phone disconnected (id: %u)", client->id());
    }
    else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            char json[256];
            size_t copyLen = len < 255 ? len : 255;
            memcpy(json, data, copyLen);
            json[copyLen] = '\0';

            // Parse: {"t":0,"y":0,"p":0,"i":0,"a":0,"e":0,"m":0,"ph":0}
            StaticJsonDocument<192> doc;
            DeserializationError err = deserializeJson(doc, json);
            if (err) return;

            ctrlThrottle = doc["t"] | (int16_t)0;
            ctrlYaw      = doc["y"] | (int16_t)0;
            ctrlCamPan   = doc["p"] | (int16_t)0;
            ctrlCamTilt  = doc["i"] | (int16_t)0;
            ctrlArm      = doc["a"] | (uint8_t)0;
            ctrlEstop    = doc["e"] | (uint8_t)0;
            ctrlMode     = doc["m"] | (uint8_t)0;
            phoneHeading = doc["ph"] | (int16_t)0;

            lastPhonePacketTime = millis();
        }
    }
}

// --- Check if Phone Connection is Healthy ---
static bool webIsPhoneConnected() {
    return (wsocket.count() > 0) && ((millis() - lastPhonePacketTime) < FAILSAFE_TIMEOUT_MS);
}

// --- Broadcast Live Telemetry to Phone at 5 Hz ---
static void webSendTelemetry(
    uint16_t vMotorMv,
    uint16_t vLogicMv,
    int16_t  curMa,
    uint8_t  pct,
    uint8_t  sats,
    uint8_t  fixType,
    int16_t  heading,
    int16_t  spdCmps,
    float    lat,
    float    lon,
    float    homeDist,
    int16_t  homeBearing,
    float    radarDist,
    int16_t  radarAngle,
    uint8_t  armed,
    uint8_t  mode
) {
    if (wsocket.count() == 0) return;

    StaticJsonDocument<288> doc;
    doc["vm"]  = vMotorMv;
    doc["vl"]  = vLogicMv;
    doc["cur"] = curMa;
    doc["pct"] = pct;
    doc["sat"] = sats;
    doc["fix"] = fixType;
    doc["hdg"] = heading;
    doc["spd"] = spdCmps;
    doc["lat"] = lat;
    doc["lon"] = lon;
    doc["hd"]  = homeDist;
    doc["hb"]  = homeBearing;
    doc["rd"]  = radarDist;
    doc["ra"]  = radarAngle;
    doc["arm"] = armed;
    doc["mode"]= mode;

    char buf[288];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    wsocket.textAll(buf, len);
}

// --- Initialize Web Server ---
static void webSetup() {
    // Serve the HUD web page from PROGMEM
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", WEB_UI_HTML, WEB_UI_HTML_LEN);
    });

    // Attach WebSocket
    wsocket.onEvent(onWsEvent);
    server.addHandler(&wsocket);

    server.begin();
    DBG("WebServer: Started on port %d", WEB_PORT);
}

// --- Periodic Cleanup ---
static void webCleanup() {
    wsocket.cleanupClients();
}
