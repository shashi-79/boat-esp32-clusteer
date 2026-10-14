#pragma once
// ============================================================
// GPS Module — NEO-M8N via TinyGPSPlus
// ============================================================

#include <TinyGPSPlus.h>
#include "config.h"

static TinyGPSPlus gps;
static HardwareSerial GPSSerial(1);  // UART1

// Home coordinates (set on ARM)
static float homeLat = 0.0f;
static float homeLon = 0.0f;
static bool  homeSet = false;

static void gpsSetup() {
    GPSSerial.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    DBG("GPS: UART1 initialized at %d baud", GPS_BAUD);
}

// --- Call this in main loop to feed NMEA data ---
static void gpsUpdate() {
    while (GPSSerial.available() > 0) {
        gps.encode(GPSSerial.read());
    }
}

// --- Getters ---
static bool gpsHasFix() {
    return gps.location.isValid() && gps.satellites.value() >= ARM_MIN_SATS;
}

static float gpsLat()    { return gps.location.lat(); }
static float gpsLon()    { return gps.location.lng(); }
static uint8_t gpsSats() { return gps.satellites.value(); }
static float gpsSpeed()  { return gps.speed.cmps(); }  // cm/s

static uint8_t gpsFixType() {
    if (!gps.location.isValid()) return 0;
    if (gps.altitude.isValid())  return 3;  // 3D fix
    return 2;  // 2D fix
}

// --- Set Home Position (called on ARM) ---
static void gpsSetHome() {
    if (gpsHasFix()) {
        homeLat = gpsLat();
        homeLon = gpsLon();
        homeSet = true;
        DBG("GPS: Home set at %.6f, %.6f (%d sats)",
            homeLat, homeLon, gpsSats());
    }
}

// --- Haversine Distance to Home (meters) ---
static float gpsDistanceToHome() {
    if (!homeSet) return 9999.0f;
    return (float)TinyGPSPlus::distanceBetween(
        gpsLat(), gpsLon(), homeLat, homeLon);
}

// --- Bearing to Home (degrees, 0-360) ---
static float gpsBearingToHome() {
    if (!homeSet) return 0.0f;
    return (float)TinyGPSPlus::courseTo(
        gpsLat(), gpsLon(), homeLat, homeLon);
}
