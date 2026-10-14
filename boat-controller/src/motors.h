#pragma once
// ============================================================
// Motor ESC Control — Differential Thrust Mixing
// ============================================================

#include <Arduino.h>
#include "config.h"

static void motorsSetup() {
    // Configure LEDC channels for ESC PWM (50 Hz, 16-bit resolution)
    ledcSetup(LEDC_CH_ESC_LEFT,  ESC_FREQ_HZ, 16);
    ledcSetup(LEDC_CH_ESC_RIGHT, ESC_FREQ_HZ, 16);
    ledcAttachPin(PIN_ESC_LEFT,  LEDC_CH_ESC_LEFT);
    ledcAttachPin(PIN_ESC_RIGHT, LEDC_CH_ESC_RIGHT);

    // Start at neutral (1500µs)
    motorsSetNeutral();

    DBG("Motors: ESC PWM initialized (neutral=%dµs)", ESC_NEUTRAL_US);
}

// --- Convert microseconds to 16-bit LEDC duty ---
// At 50 Hz, period = 20000µs. Duty = (us / 20000) * 65535
static uint32_t usToDuty(uint16_t us) {
    return (uint32_t)us * 65535UL / 20000UL;
}

// --- Write raw microseconds to ESCs ---
static void motorsWriteUs(uint16_t leftUs, uint16_t rightUs) {
    leftUs  = constrain(leftUs,  ESC_MIN_US, ESC_MAX_US);
    rightUs = constrain(rightUs, ESC_MIN_US, ESC_MAX_US);
    ledcWrite(LEDC_CH_ESC_LEFT,  usToDuty(leftUs));
    ledcWrite(LEDC_CH_ESC_RIGHT, usToDuty(rightUs));
}

// --- Set both ESCs to neutral (1500µs = stopped) ---
static void motorsSetNeutral() {
    motorsWriteUs(ESC_NEUTRAL_US, ESC_NEUTRAL_US);
}

// --- Differential Thrust Mixing ---
// throttle: -1000 (full reverse) to +1000 (full forward)
// yaw:      -1000 (hard left)    to +1000 (hard right)
static void motorsMix(int16_t throttle, int16_t yaw) {
    // Clamp inputs
    throttle = constrain(throttle, -1000, 1000);
    yaw      = constrain(yaw,      -1000, 1000);

    // Apply deadband: if both near zero, go neutral
    if (abs(throttle) < 30 && abs(yaw) < 30) {
        motorsSetNeutral();
        return;
    }

    // Mix: left motor = throttle + yaw, right motor = throttle - yaw
    int16_t leftMix  = throttle + yaw;
    int16_t rightMix = throttle - yaw;

    // Clamp to ±1000
    leftMix  = constrain(leftMix,  -1000, 1000);
    rightMix = constrain(rightMix, -1000, 1000);

    // Map ±1000 → 1000–2000µs (center = 1500µs)
    uint16_t leftUs  = map(leftMix,  -1000, 1000, ESC_MIN_US, ESC_MAX_US);
    uint16_t rightUs = map(rightMix, -1000, 1000, ESC_MIN_US, ESC_MAX_US);

    motorsWriteUs(leftUs, rightUs);
}
