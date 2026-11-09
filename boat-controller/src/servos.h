#pragma once
// ============================================================
// Camera Pan/Tilt Servo Control
// ============================================================

#include <Arduino.h>
#include "config.h"

static void servosCenterAll();

static void servosSetup() {
    // Camera servos
    ledcSetup(LEDC_CH_PAN,  SERVO_FREQ_HZ, 16);
    ledcSetup(LEDC_CH_TILT, SERVO_FREQ_HZ, 16);
    ledcAttachPin(PIN_SERVO_PAN,  LEDC_CH_PAN);
    ledcAttachPin(PIN_SERVO_TILT, LEDC_CH_TILT);

    // Future expansion servos
    ledcSetup(LEDC_CH_AUX1, SERVO_FREQ_HZ, 16);
    ledcSetup(LEDC_CH_AUX2, SERVO_FREQ_HZ, 16);
    ledcSetup(LEDC_CH_AUX3, SERVO_FREQ_HZ, 16);
    ledcAttachPin(PIN_SERVO_AUX1, LEDC_CH_AUX1);
    ledcAttachPin(PIN_SERVO_AUX2, LEDC_CH_AUX2);
    ledcAttachPin(PIN_SERVO_AUX3, LEDC_CH_AUX3);

    // Center all servos
    servosCenterAll();

    DBG("Servos: Pan/Tilt + 3x Aux initialized (center=%dµs)", SERVO_CENTER_US);
}

// --- Convert microseconds to 16-bit LEDC duty ---
static uint32_t servoUsToDuty(uint16_t us) {
    return (uint32_t)us * 65535UL / 20000UL;
}

// --- Write raw microseconds to a servo channel ---
static void servoWriteUs(uint8_t channel, uint16_t us) {
    us = constrain(us, SERVO_MIN_US, SERVO_MAX_US);
    ledcWrite(channel, servoUsToDuty(us));
}

// --- Map ±1000 input to servo microseconds ---
// -1000 → SERVO_MIN_US (500), 0 → 1500, +1000 → SERVO_MAX_US (2500)
static void servoSetFromInput(uint8_t channel, int16_t input) {
    input = constrain(input, -1000, 1000);
    uint16_t us = map(input, -1000, 1000, SERVO_MIN_US, SERVO_MAX_US);
    servoWriteUs(channel, us);
}

// --- Set camera pan and tilt from joystick ---
static void servosSetCamera(int16_t pan, int16_t tilt) {
    servoSetFromInput(LEDC_CH_PAN,  pan);
    servoSetFromInput(LEDC_CH_TILT, tilt);
}

// --- Center all servos ---
static void servosCenterAll() {
    servoWriteUs(LEDC_CH_PAN,  SERVO_CENTER_US);
    servoWriteUs(LEDC_CH_TILT, SERVO_CENTER_US);
    servoWriteUs(LEDC_CH_AUX1, SERVO_CENTER_US);
    servoWriteUs(LEDC_CH_AUX2, SERVO_CENTER_US);
    servoWriteUs(LEDC_CH_AUX3, SERVO_CENTER_US);
}
