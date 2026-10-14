#pragma once
// ============================================================
// ESP-NOW Long-Range Radio — Shore Side (Sender + Receiver)
// ============================================================

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "../../shared/packets.h"
#include "config.h"

// Boat ESP32-S3 MAC address (auto-discovered)
static uint8_t boatMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool boatPaired = false;

static volatile bool      shoreLinked = false;
static volatile uint32_t  shoreLastRxTime = 0;
static ControlPacket      txControl;
static TelemetryPacket    rxTelemetry;
static esp_now_peer_info_t shorePeerInfo;

// --- Callback: Telemetry Received from Boat ---
static void onShoreDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(TelemetryPacket)) return;

    memcpy(&rxTelemetry, data, sizeof(TelemetryPacket));
    shoreLastRxTime = millis();
    shoreLinked = true;

    // Auto-pair with boat MAC on first valid telemetry
    if (!boatPaired) {
        uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        if (esp_now_is_peer_exist(bcast)) {
            esp_now_del_peer(bcast);
        }
        memcpy(boatMac, mac, 6);
        memcpy(shorePeerInfo.peer_addr, mac, 6);
        if (!esp_now_is_peer_exist(shorePeerInfo.peer_addr)) {
            esp_now_add_peer(&shorePeerInfo);
        }
        boatPaired = true;
        DBG("Shore Radio: paired with boat %02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

static void onShoreDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    (void)mac;
    (void)status;
}

// --- Initialize ESP-NOW LR on Shore ---
// Note: Shore runs BOTH Wi-Fi AP (for phone) AND ESP-NOW simultaneously.
// ESP32 supports AP+STA coexistence which allows this.
static void shoreRadioSetup() {
    // Wi-Fi mode set to AP+STA in main setup (for WebServer + ESP-NOW)
    // Enable LR protocol on STA interface
    esp_wifi_set_protocol(WIFI_IF_STA,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);

    if (esp_now_init() != ESP_OK) {
        DBG("Shore Radio: ESP-NOW init FAILED!");
        return;
    }

    esp_now_register_recv_cb(onShoreDataRecv);
    esp_now_register_send_cb(onShoreDataSent);

    // Add broadcast peer (will be replaced on first boat response)
    memset(&shorePeerInfo, 0, sizeof(shorePeerInfo));
    memcpy(shorePeerInfo.peer_addr, boatMac, 6);
    shorePeerInfo.channel = ESPNOW_CHANNEL;
    shorePeerInfo.encrypt = false;
    esp_now_add_peer(&shorePeerInfo);

    // Initialize control packet to safe defaults
    memset(&txControl, 0, sizeof(txControl));

    DBG("Shore Radio: ESP-NOW LR initialized");
}

// --- Send Control Packet to Boat ---
static void shoreSendControl() {
    // Calculate checksum
    txControl.checksum = calcChecksum(
        (const uint8_t*)&txControl, sizeof(txControl) - 1);

    esp_err_t result = esp_now_send(boatMac,
        (const uint8_t*)&txControl, sizeof(txControl));

    if (result != ESP_OK) {
        DBG("Shore Radio: control send failed (%d)", result);
    }
}

// --- Check if boat link is alive ---
static bool shoreIsLinked() {
    return shoreLinked && ((millis() - shoreLastRxTime) < 3000);
}
