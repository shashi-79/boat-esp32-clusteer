#pragma once
// ============================================================
// Safety Layer — Failsafe, E-STOP, Low Battery, Arming
// ============================================================

#include <Arduino.h>
#include "config.h"

static uint8_t safetyMode   = MODE_MANUAL;
static bool    safetyArmed  = false;
static bool    safetyEstop  = false;

// --- Check arming conditions ---
// Returns true only if GPS has enough satellites and fix quality
static bool safetyCanArm(uint8_t sats, uint8_t fixType) {
    if (safetyEstop) return false;
    if (sats < ARM_MIN_SATS) return false;
    if (fixType < 3) return false;  // Need 3D fix
    return true;
}

// --- Process incoming control and determine safe mode ---
static uint8_t safetyProcess(
    uint8_t armCmd,
    uint8_t estopCmd,
    uint8_t modeCmd,
    bool    linkAlive,
    bool    gpsFix,
    uint8_t sats,
    uint8_t fixType,
    bool    motorBattLow,
    bool    imuHealthy,
    bool    motorBattHealthy
) {
    // --- E-STOP: highest priority, overrides everything ---
    if (estopCmd == 1) {
        safetyEstop = true;
        safetyArmed = false;
        safetyMode  = MODE_FAILSAFE;
        DBG("Safety: E-STOP ACTIVATED!");
        return MODE_FAILSAFE;
    }

    // E-stop is latched until an explicit disarm packet arrives.
    if (safetyEstop && armCmd == 0) {
        safetyEstop = false;
        safetyMode = MODE_MANUAL;
        return MODE_MANUAL;
    }

    // --- ARM/DISARM ---
    if (armCmd == 1 && !safetyArmed) {
        if (gpsFix && safetyCanArm(sats, fixType) && imuHealthy && motorBattHealthy && !safetyEstop) {
            safetyArmed = true;
            DBG("Safety: ARMED (sats=%d)", sats);
        } else {
            DBG("Safety: ARM REJECTED (sats=%d, fix=%d, estop=%d)",
                sats, fixType, safetyEstop);
        }
    } else if (armCmd == 0 && safetyArmed) {
        safetyArmed = false;
        safetyEstop = false;  // Clear E-STOP latch on explicit disarm
        safetyMode  = MODE_MANUAL;
        DBG("Safety: DISARMED");
        return MODE_MANUAL;
    }

    // --- If not armed, always manual (motors won't spin) ---
    if (!safetyArmed) {
        safetyMode = MODE_MANUAL;
        return MODE_MANUAL;
    }

    // --- Low battery: force RTH ---
    if (!imuHealthy || !gpsFix || !motorBattHealthy) {
        safetyMode = MODE_FAILSAFE;
        safetyArmed = false;
        DBG("Safety: required navigation sensor lost — disarming");
        return MODE_FAILSAFE;
    }

    if (motorBattLow) {
        safetyMode = MODE_RTH;
        DBG("Safety: LOW BATTERY → RTH forced");
        return MODE_RTH;
    }

    // --- Signal loss: failsafe → RTH if GPS available ---
    if (!linkAlive) {
        if (gpsFix) {
            safetyMode = MODE_RTH;
            DBG("Safety: SIGNAL LOST → RTH");
        } else {
            safetyMode = MODE_FAILSAFE;
            DBG("Safety: SIGNAL LOST + NO GPS → FAILSAFE (neutral)");
        }
        return safetyMode;
    }

    // --- Manual RTH request from operator ---
    if (modeCmd == MODE_RTH) {
        safetyMode = MODE_RTH;
        return MODE_RTH;
    }

    // --- Normal manual operation ---
    safetyMode = MODE_MANUAL;
    return MODE_MANUAL;
}

// --- Getters ---
static uint8_t safetyGetMode()  { return safetyMode; }
static bool    safetyIsArmed()  { return safetyArmed; }
static bool    safetyIsEstop()  { return safetyEstop; }
