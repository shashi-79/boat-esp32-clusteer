#pragma once
// ============================================================
// Boat Controller — AsyncWebServer & WebSocket Control Bridge
// (Runs directly on the sensor controller SoftAP)
// ============================================================

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include "config.h"

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
static volatile uint32_t controllerClientId = 0;
static portMUX_TYPE controlMux = portMUX_INITIALIZER_UNLOCKED;

struct ControlSnapshot {
    int16_t throttle, yaw, camPan, camTilt, heading;
    uint8_t arm, estop, mode;
};

static ControlSnapshot webControlSnapshot() {
    ControlSnapshot s;
    portENTER_CRITICAL(&controlMux);
    s = {ctrlThrottle, ctrlYaw, ctrlCamPan, ctrlCamTilt, phoneHeading, ctrlArm, ctrlEstop, ctrlMode};
    portEXIT_CRITICAL(&controlMux);
    return s;
}

static bool webPacketFieldInRange(JsonVariantConst value, int low, int high) {
    if (!value.is<int>()) return false;
    const int parsed = value.as<int>();
    return parsed >= low && parsed <= high;
}

// --- Handle WebSocket Events from Phone ---
static void onWsEvent(AsyncWebSocket *srv, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        if (controllerClientId != 0) {
            client->close(1008, "Controller already connected");
            return;
        }
        controllerClientId = client->id();
        DBG("WebSocket: Phone connected from %s (id: %u)",
            client->remoteIP().toString().c_str(), client->id());
    }
    else if (type == WS_EVT_DISCONNECT) {
        DBG("WebSocket: Phone disconnected (id: %u)", client->id());
        if (client->id() == controllerClientId) {
            controllerClientId = 0;
            portENTER_CRITICAL(&controlMux);
            ctrlThrottle = ctrlYaw = ctrlCamPan = ctrlCamTilt = 0;
            ctrlMode = ctrlEstop = 0;
            lastPhonePacketTime = 0;
            portEXIT_CRITICAL(&controlMux);
        }
    }
    else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            char json[256];
            if (len == 0 || len >= sizeof(json) || client->id() != controllerClientId) return;
            size_t copyLen = len;
            memcpy(json, data, copyLen);
            json[copyLen] = '\0';

            // Parse: {"t":0,"y":0,"p":0,"i":0,"a":0,"e":0,"m":0,"ph":0}
            StaticJsonDocument<192> doc;
            DeserializationError err = deserializeJson(doc, json);
            if (err) return;

            if (!webPacketFieldInRange(doc["t"], -1000, 1000) ||
                !webPacketFieldInRange(doc["y"], -1000, 1000) ||
                !webPacketFieldInRange(doc["p"], -1000, 1000) ||
                !webPacketFieldInRange(doc["i"], -1000, 1000) ||
                !webPacketFieldInRange(doc["a"], 0, 1) ||
                !webPacketFieldInRange(doc["e"], 0, 1) ||
                !webPacketFieldInRange(doc["m"], MODE_MANUAL, MODE_RTH) ||
                !webPacketFieldInRange(doc["ph"], 0, 360)) return;

            int ph = doc["ph"].as<int>();
            if (ph >= 360) ph = 0;

            portENTER_CRITICAL(&controlMux);
            ctrlThrottle = doc["t"].as<int>();
            ctrlYaw      = doc["y"].as<int>();
            ctrlCamPan   = doc["p"].as<int>();
            ctrlCamTilt  = doc["i"].as<int>();
            ctrlArm      = doc["a"].as<int>();
            ctrlEstop    = doc["e"].as<int>();
            ctrlMode     = doc["m"].as<int>();
            phoneHeading = ph;
            lastPhonePacketTime = millis();
            portEXIT_CRITICAL(&controlMux);
        }
    }
}

// --- Check if Phone Connection is Healthy ---
static bool webIsPhoneConnected() {
    return controllerClientId != 0 && lastPhonePacketTime != 0 &&
           ((millis() - lastPhonePacketTime) < FAILSAFE_TIMEOUT_MS);
}

// --- Get Connected Station Signal Strength (RSSI in dBm) ---
static int8_t webGetClientRssi() {
    wifi_sta_list_t sta_list;
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK && sta_list.num > 0) {
        // Return the lowest (weakest) RSSI among connected stations for link safety
        int8_t min_rssi = 0;
        for (int i = 0; i < sta_list.num; i++) {
            if (i == 0 || sta_list.sta[i].rssi < min_rssi) {
                min_rssi = sta_list.sta[i].rssi;
            }
        }
        return min_rssi;
    }
    return 0; // 0 indicates unknown or no station connected
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
    double   lat,
    double   lon,
    float    homeDist,
    int16_t  homeBearing,
    float    radarDist,
    int16_t  radarAngle,
    uint8_t  armed,
    uint8_t  mode,
    bool gpsHealthy,
    bool imuHealthy,
    bool batteryHealthy,
    bool radarHealthy,
    bool logicBatteryHealthy,
    bool ina219Present,
    int8_t   rssi = 0
) {
    if (wsocket.count() == 0) return;

    StaticJsonDocument<512> doc;
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
    doc["gpsok"] = gpsHealthy;
    doc["imuok"] = imuHealthy;
    doc["battok"] = batteryHealthy;
    doc["radarok"] = radarHealthy;
    doc["logicok"] = logicBatteryHealthy;
    doc["ina"] = ina219Present;
    doc["rssi"] = rssi;

    char buf[512];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    wsocket.textAll(buf, len);
}

// --- Initialize Web Server ---
static void webSetup() {
    // Keep the controller HTTP handler small; the phone hosts the HUD locally.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain",
            "Rescue Boat controller online. Serve web-ui/index.html locally on the phone.\n"
            "WebSocket control endpoint: ws://192.168.4.1/ws\n");
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
