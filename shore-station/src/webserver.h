#pragma once
// ============================================================
// Shore Station — AsyncWebServer + WebSocket Handler
// ============================================================

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "web_ui.h"
#include "radio.h"

static AsyncWebServer server(WEB_PORT);
static AsyncWebSocket  wsocket(WS_PATH);

// --- Handle WebSocket Events ---
static void onWsEvent(AsyncWebSocket *srv, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        DBG("WebSocket: client #%u connected from %s",
            client->id(), client->remoteIP().toString().c_str());
    }
    else if (type == WS_EVT_DISCONNECT) {
        DBG("WebSocket: client #%u disconnected", client->id());
    }
    else if (type == WS_EVT_DATA) {
        // Parse incoming joystick JSON from phone
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            // Null-terminate the data
            char json[256];
            size_t copyLen = len < 255 ? len : 255;
            memcpy(json, data, copyLen);
            json[copyLen] = '\0';

            // Parse JSON: {"t":0,"y":0,"p":0,"i":0,"a":0,"e":0,"m":0}
            StaticJsonDocument<128> doc;
            DeserializationError err = deserializeJson(doc, json);
            if (err) {
                DBG("WebSocket: JSON parse error: %s", err.c_str());
                return;
            }

            // Update control packet
            txControl.throttle = doc["t"] | (int16_t)0;
            txControl.yaw      = doc["y"] | (int16_t)0;
            txControl.cam_pan  = doc["p"] | (int16_t)0;
            txControl.cam_tilt = doc["i"] | (int16_t)0;
            txControl.arm      = doc["a"] | (uint8_t)0;
            txControl.estop    = doc["e"] | (uint8_t)0;
            txControl.mode     = doc["m"] | (uint8_t)0;
        }
    }
}

// --- Send Telemetry to All WebSocket Clients ---
static void webSendTelemetry() {
    if (wsocket.count() == 0) return;

    // Build compact JSON telemetry
    StaticJsonDocument<256> doc;
    doc["vm"]  = rxTelemetry.v_motor_mv;
    doc["vl"]  = rxTelemetry.v_logic_mv;
    doc["cur"] = rxTelemetry.current_ma;
    doc["pct"] = rxTelemetry.batt_percent;
    doc["sat"] = rxTelemetry.sats;
    doc["fix"] = rxTelemetry.fix_type;
    doc["hdg"] = rxTelemetry.heading;
    doc["spd"] = rxTelemetry.speed;
    doc["lat"] = rxTelemetry.lat;
    doc["lon"] = rxTelemetry.lon;
    doc["hd"]  = rxTelemetry.home_dist;
    doc["hb"]  = rxTelemetry.home_bearing;
    doc["rd"]  = rxTelemetry.radar_dist;
    doc["ra"]  = rxTelemetry.radar_angle;
    doc["arm"] = rxTelemetry.armed;
    doc["mode"]= rxTelemetry.mode;

    char buf[256];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    wsocket.textAll(buf, len);
}

// --- Initialize Web Server ---
static void webSetup() {
    // Serve the main page from PROGMEM
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", WEB_UI_HTML, WEB_UI_HTML_LEN);
    });

    // Attach WebSocket
    wsocket.onEvent(onWsEvent);
    server.addHandler(&wsocket);

    server.begin();
    DBG("WebServer: started on port %d", WEB_PORT);
}

// --- Call periodically to clean up dead WS connections ---
static void webCleanup() {
    wsocket.cleanupClients();
}
