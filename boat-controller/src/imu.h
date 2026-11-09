#pragma once
// ============================================================
// IMU / Compass — BNO055 (or MPU9250 with magnetometer)
// ============================================================

#include <Wire.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_Sensor.h>
#include "config.h"

static Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);
static bool imuReady = false;
static float imuHeading = 0.0f;
static uint32_t imuLastReadMs = 0;

static void imuSetup() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!bno.begin()) {
        DBG("IMU: BNO055 NOT FOUND! Check wiring.");
        imuReady = false;
        return;
    }

    bno.setExtCrystalUse(true);
    imuReady = true;
    DBG("IMU: BNO055 initialized (fusion mode)");
}

// --- Call in main loop (non-blocking, ~10ms per read) ---
static void imuUpdate() {
    if (!imuReady) return;

    sensors_event_t event;
    if (!bno.getEvent(&event)) {
        imuReady = false;
        return;
    }

    // BNO055 returns heading in event.orientation.x (0–360 degrees)
    if (isfinite(event.orientation.x) && event.orientation.x >= 0.0f && event.orientation.x < 360.0f) {
        imuHeading = event.orientation.x;
        imuLastReadMs = millis();
    } else {
        imuReady = false;
    }
}

// --- Get current compass heading (0–359 degrees, clockwise from North) ---
static float imuGetHeading() {
    return imuHeading;
}

// --- Check if IMU is calibrated ---
static bool imuIsCalibrated() {
    if (!imuReady) return false;

    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);

    // Magnetometer calibration is most important for heading
    return (mag >= 2);
}

static bool imuHealthy() {
    return imuReady && imuLastReadMs != 0 && millis() - imuLastReadMs < 500;
}
