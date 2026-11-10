#pragma once
// ============================================================
// Return-to-Home — PID Compass Navigation
// ============================================================

#include <Arduino.h>
#include "config.h"

static float rthIntegral   = 0.0f;
static float rthPrevError  = 0.0f;
static uint32_t rthLastTime = 0;
static bool rthFirstStep   = true;

// --- Reset PID state (call when entering RTH mode) ---
static void rthReset() {
    rthIntegral  = 0.0f;
    rthPrevError = 0.0f;
    rthFirstStep = true;
    rthLastTime  = millis();
    DBG("RTH: PID reset");
}

// --- Normalize angle to -180..+180 ---
static float rthNormalizeAngle(float angle) {
    while (angle > 180.0f)  angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

// --- Compute RTH motor commands ---
// bearing:  bearing to Home (0–360 degrees, from GPS)
// heading:  current compass heading (0–360 degrees, from IMU)
// distance: distance to Home (meters)
//
// Returns: yaw correction value (-1000 to +1000)
//          throttle is fixed at RTH_CRUISE_PERCENT of max
static int16_t rthComputeYaw(float bearing, float heading, float distance) {
    // If we've arrived, stop
    if (distance < RTH_ARRIVAL_RADIUS_M) {
        rthIntegral = 0.0f;
        return 0;
    }

    // Heading error (positive = need to turn right)
    float error = rthNormalizeAngle(bearing - heading);

    // Time delta
    uint32_t now = millis();
    float dt = (now - rthLastTime) / 1000.0f;
    if (dt <= 0.0f || dt > 1.0f) dt = 0.02f;  // Default to 50Hz interval
    rthLastTime = now;

    if (rthFirstStep) {
        rthPrevError = error;
        rthFirstStep = false;
    }

    // PID
    rthIntegral += error * dt;
    rthIntegral = constrain(rthIntegral, -500.0f, 500.0f);  // Anti-windup

    float derivative = (error - rthPrevError) / dt;
    rthPrevError = error;

    float output = RTH_KP * error + RTH_KI * rthIntegral + RTH_KD * derivative;

    // Clamp to ±1000
    return (int16_t)constrain(output, -1000.0f, 1000.0f);
}

// --- RTH throttle (fixed cruise speed) ---
static int16_t rthGetThrottle(float distance) {
    if (distance < RTH_ARRIVAL_RADIUS_M) return 0;

    // Slow down as we approach home (within 10m)
    float factor = 1.0f;
    if (distance < 10.0f) {
        factor = distance / 10.0f;
        factor = constrain(factor, 0.3f, 1.0f);
    }

    return (int16_t)(RTH_CRUISE_PERCENT * 10.0f * factor);  // 30% of 1000 = 300
}

// --- Check if RTH has arrived ---
static bool rthHasArrived(float distance) {
    return distance < RTH_ARRIVAL_RADIUS_M;
}
