#pragma once
// ============================================================
// ESP-NOW Long-Range Radio — Boat Side (Receiver + Sender)
// ============================================================

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "../../shared/packets.h"
#include "config.h"

// Shore ESP32 MAC address (must be set after identifying shore board)
// Default: broadcast to all — will be overridden in setup
static uint8_t shoreMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static volatile bool      radioLinked = false;
static volatile uint32_t  lastRxTime  = 0;
static ControlPacket      rxControl;
static TelemetryPacket    txTelemetry;
static esp_now_peer_info_t peerInfo;

// --- Callback: Data Received from Shore ---
static void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(ControlPacket)) return;

    ControlPacket pkt;
    memcpy(&pkt, data, sizeof(pkt));

    // Verify checksum
    uint8_t expected = calcChecksum((const uint8_t*)&pkt, sizeof(pkt) - 1);
    if (pkt.checksum != expected) {
        DBG("Radio: bad checksum recv=%02X calc=%02X", pkt.checksum, expected);
        return;
    }

    memcpy(&rxControl, &pkt, sizeof(pkt));
    lastRxTime = millis();
    radioLinked = true;

    // Store shore MAC on first valid packet (auto-pair)
    if (shoreMac[0] == 0xFF) {
        uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        if (esp_now_is_peer_exist(bcast)) {
            esp_now_del_peer(bcast);
        }
        memcpy(shoreMac, mac, 6);
        memcpy(peerInfo.peer_addr, mac, 6);
        if (!esp_now_is_peer_exist(peerInfo.peer_addr)) {
            esp_now_add_peer(&peerInfo);
        }
        DBG("Radio: paired with shore %02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

// --- Callback: Data Sent Confirmation ---
static void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    // Optional: track send success rate
    (void)mac;
    (void)status;
}

// --- Initialize ESP-NOW in Long-Range Mode ---
static void radioSetup() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Enable Long-Range mode (drops throughput but extends range to ~1-1.5km)
    esp_wifi_set_protocol(WIFI_IF_STA,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);

    if (esp_now_init() != ESP_OK) {
        DBG("Radio: ESP-NOW init FAILED!");
        return;
    }

    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);

    // Add broadcast peer initially (will be replaced on first valid packet)
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, shoreMac, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    // Initialize rxControl to safe defaults
    memset(&rxControl, 0, sizeof(rxControl));
    rxControl.throttle = 0;
    rxControl.yaw      = 0;
    rxControl.cam_pan  = 0;
    rxControl.cam_tilt = 0;
    rxControl.arm      = 0;
    rxControl.estop    = 0;
    rxControl.mode     = MODE_MANUAL;

    DBG("Radio: ESP-NOW LR initialized on channel %d", ESPNOW_CHANNEL);
}

// --- Send Telemetry to Shore ---
static void radioSendTelemetry() {
    if (shoreMac[0] == 0xFF) return;  // No shore paired yet

    esp_err_t result = esp_now_send(shoreMac,
        (const uint8_t*)&txTelemetry, sizeof(txTelemetry));

    if (result != ESP_OK) {
        DBG("Radio: telemetry send failed (%d)", result);
    }
}

// --- Check Link Health ---
static bool radioIsLinked() {
    return radioLinked && ((millis() - lastRxTime) < FAILSAFE_TIMEOUT_MS);
}

static uint32_t radioTimeSinceLastRx() {
    return millis() - lastRxTime;
}
