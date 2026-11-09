#pragma once
// ============================================================
// 24GHz mmWave Radar — HLK-LD2450 Obstacle Parser
// ============================================================

#include <Arduino.h>
#include "config.h"

static HardwareSerial RadarSerial(2);  // UART2

// Parsed obstacle data (nearest target)
static float  radarDist  = 99.0f;   // meters (99 = no target)
static int16_t radarAngle = 0;       // degrees (-90 to +90)
static uint32_t radarLastFrameMs = 0;

// LD2450 frame buffer
#define RADAR_BUF_SIZE 64
#define RADAR_FRAME_SIZE 30
static uint8_t radarBuf[RADAR_BUF_SIZE];
static uint8_t radarIdx = 0;

// LD2450 frame header: AA FF 03 00
static const uint8_t RADAR_HEADER[] = {0xAA, 0xFF, 0x03, 0x00};
// LD2450 frame footer: 55 CC
static const uint8_t RADAR_FOOTER[] = {0x55, 0xCC};

static void radarSetup() {
    RadarSerial.begin(RADAR_BAUD, SERIAL_8N1, PIN_RADAR_RX, PIN_RADAR_TX);
    DBG("Radar: HLK-LD2450 UART2 initialized at %d baud", RADAR_BAUD);
}

// --- Parse a complete LD2450 target frame ---
// LD2450 reports up to 3 targets. Each target: X(2) + Y(2) + Speed(2) = 6 bytes
// We extract the nearest target.
static void radarParseFrame(const uint8_t* frame, uint8_t len) {
    // Each target block is 8 bytes after header
    // Target format: X_low, X_high, Y_low, Y_high, Speed_low, Speed_high, DistRes_low, DistRes_high
    float nearestDist = 99.0f;
    int16_t nearestAngle = 0;

    // Up to 3 targets starting at byte 4
    for (int t = 0; t < 3; t++) {
        int offset = 4 + (t * 8);
        if (offset + 7 >= len) break;

        // X and Y are in mm, signed (center = 0)
        int16_t x = (int16_t)(frame[offset] | (frame[offset + 1] << 8));
        int16_t y = (int16_t)(frame[offset + 2] | (frame[offset + 3] << 8));

        // LD2450 uses sign bit in bit 15: 1 = negative
        if (x & 0x8000) x = -(x & 0x7FFF);
        if (y & 0x8000) y = -(y & 0x7FFF);

        // Skip if target at origin (no detection)
        if (x == 0 && y == 0) continue;

        // Distance in meters
        float dist = hypotf((float)x, (float)y) / 1000.0f;
        // Angle in degrees (0 = straight ahead, + = right, - = left)
        float angle = atan2f((float)x, (float)y) * 180.0f / PI;

        if (dist < nearestDist) {
            nearestDist  = dist;
            nearestAngle = (int16_t)angle;
        }
    }

    radarDist  = nearestDist;
    radarAngle = nearestAngle;
}

// --- Call in main loop (non-blocking byte-by-byte reader) ---
static void radarUpdate() {
    while (RadarSerial.available()) {
        uint8_t b = RadarSerial.read();
        radarBuf[radarIdx++] = b;

        // Check for footer (end of frame)
        if (radarIdx >= 2 &&
            radarBuf[radarIdx - 2] == RADAR_FOOTER[0] &&
            radarBuf[radarIdx - 1] == RADAR_FOOTER[1]) {

            // Verify header
            if (radarIdx == RADAR_FRAME_SIZE &&
                radarBuf[0] == RADAR_HEADER[0] &&
                radarBuf[1] == RADAR_HEADER[1] &&
                radarBuf[2] == RADAR_HEADER[2] &&
                radarBuf[3] == RADAR_HEADER[3]) {

                radarParseFrame(radarBuf, radarIdx);
                radarLastFrameMs = millis();
            }
            radarIdx = 0;  // Reset for next frame
        }

        // Overflow protection
        if (radarIdx >= RADAR_BUF_SIZE) {
            radarIdx = 0;
        }
    }
}

// --- Getters ---
static float  radarGetDist()  {
    if (radarLastFrameMs == 0 || millis() - radarLastFrameMs > 1000) return 99.0f;
    return radarDist;
}
static bool   radarIsHealthy() { return radarLastFrameMs != 0 && millis() - radarLastFrameMs <= 1000; }
static int16_t radarGetAngle() { return radarIsHealthy() ? radarAngle : 0; }
static bool   radarHasTarget() { return radarIsHealthy() && radarDist < 8.0f; }
