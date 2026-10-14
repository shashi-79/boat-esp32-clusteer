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
    bno.getEvent(&event);

    // BNO055 returns heading in event.orientation.x (0–360 degrees)
    imuHeading = event.orientation.x;
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
