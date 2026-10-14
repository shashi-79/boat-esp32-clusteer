#pragma once
// ============================================================
// Safety Layer — Failsafe, E-STOP, Low Battery, Arming
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "../../shared/packets.h"

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

// --- Process incoming control packet and determine safe mode ---
// Called every time a new ControlPacket is received.
// Returns the effective mode the boat should operate in.
static uint8_t safetyProcess(
    const ControlPacket& cmd,
    bool  linkAlive,
    bool  gpsFix,
    uint8_t sats,
    uint8_t fixType,
    bool  motorBattLow
) {
    // --- E-STOP: highest priority, overrides everything ---
    if (cmd.estop == 1) {
        safetyEstop = true;
        safetyArmed = false;
        safetyMode  = MODE_FAILSAFE;
        DBG("Safety: E-STOP ACTIVATED!");
        return MODE_FAILSAFE;
    }

    // --- Clear E-STOP only by explicit DISARM then re-ARM ---
    // (E-STOP stays latched until power cycle or fresh ARM sequence)

    // --- ARM/DISARM ---
    if (cmd.arm == 1 && !safetyArmed) {
        if (safetyCanArm(sats, fixType) && !safetyEstop) {
            safetyArmed = true;
            DBG("Safety: ARMED (sats=%d)", sats);
        } else {
            DBG("Safety: ARM REJECTED (sats=%d, fix=%d, estop=%d)",
                sats, fixType, safetyEstop);
        }
    } else if (cmd.arm == 0 && safetyArmed) {
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
    if (motorBattLow && gpsFix) {
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
    if (cmd.mode == MODE_RTH) {
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
