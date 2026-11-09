#pragma once
// ============================================================
// Battery Monitor — INA219 I2C High-Side Current & Voltage
// (With ADC Resistor Divider Fallback & Piecewise LiPo Curve)
// ============================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "config.h"

static Adafruit_INA219 ina219(INA219_I2C_ADDR);
static bool     ina219Ready     = false;

static uint16_t battMotorMv     = 0;
static uint16_t battLogicMv     = 0;
static int16_t  battCurrentMa   = 0;
static uint8_t  battPercent     = 0;

// Exponential moving average smoothing filters
static float    battMotorSmooth = 0.0f;
static float    battLogicSmooth = 0.0f;
static float    battCurSmooth   = 0.0f;
static const float BATT_ALPHA   = 0.15f;  // Filter factor (0.15 gives smooth response without lag)

// --- Piecewise LiPo Single-Cell Discharge Curve ---
// Real non-linear LiPo discharge table [cell_voltage, remaining_percent]
static const float LIPO_CURVE[][2] = {
    {4.20f, 100.0f},
    {4.12f,  90.0f},
    {4.02f,  80.0f},
    {3.92f,  70.0f},
    {3.85f,  60.0f},
    {3.81f,  50.0f},
    {3.77f,  40.0f},
    {3.74f,  30.0f},
    {3.69f,  20.0f},
    {3.61f,  10.0f},
    {3.50f,   5.0f},
    {3.30f,   0.0f}
};
static const size_t LIPO_CURVE_POINTS = sizeof(LIPO_CURVE) / sizeof(LIPO_CURVE[0]);

// --- Compute True Battery Percentage from Pack Voltage ---
static uint8_t calculateLipoPercent(uint16_t packMv, uint8_t cells) {
    if (cells == 0 || packMv == 0) return 0;
    float cellV = (packMv / 1000.0f) / (float)cells;

    if (cellV >= 4.20f) return 100;
    if (cellV <= 3.30f) return 0;

    for (size_t i = 0; i < LIPO_CURVE_POINTS - 1; i++) {
        if (cellV <= LIPO_CURVE[i][0] && cellV >= LIPO_CURVE[i + 1][0]) {
            float vHigh = LIPO_CURVE[i][0];
            float vLow  = LIPO_CURVE[i + 1][0];
            float pHigh = LIPO_CURVE[i][1];
            float pLow  = LIPO_CURVE[i + 1][1];
            float fraction = (cellV - vLow) / (vHigh - vLow);
            return (uint8_t)constrain(pLow + fraction * (pHigh - pLow), 0.0f, 100.0f);
        }
    }
    return 0;
}

static void batterySetup() {
    // 1. Initialize ADC for Logic Battery (and ADC fallback)
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    // 2. Initialize INA219 via shared I2C bus
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (ina219.begin(&Wire)) {
        ina219Ready = true;
        // Default calibration: 32V, 2A range, 0.1 ohm shunt (0.1mA resolution)
        ina219.setCalibration_32V_2A();
        DBG("Battery: INA219 I2C module initialized at 0x%02X", INA219_I2C_ADDR);
    } else {
        ina219Ready = false;
        DBG("Battery: INA219 NOT FOUND at 0x%02X — using ADC fallback on pin %d",
            INA219_I2C_ADDR, PIN_BATT_MOTOR);
    }

    // Prime the filter
    uint32_t rawLogic = analogReadMilliVolts(PIN_BATT_LOGIC);
    battLogicSmooth = rawLogic * BATT_LOGIC_RATIO;

    if (ina219Ready) {
        battMotorSmooth = ina219.getBusVoltage_V() * 1000.0f;
    } else {
        uint32_t rawMotor = analogReadMilliVolts(PIN_BATT_MOTOR);
        battMotorSmooth = rawMotor * BATT_MOTOR_RATIO;
    }
}

// --- Call in main loop at ~10 Hz ---
static void batteryUpdate() {
    float rawMotorMv = 0.0f;
    float rawCurMa   = 0.0f;

    if (ina219Ready) {
        // High-precision I2C measurement: bus voltage + shunt current
        float busVoltageV = ina219.getBusVoltage_V();
        if (isfinite(busVoltageV) && busVoltageV >= 5.0f && busVoltageV <= 32.0f) {
            rawMotorMv = busVoltageV * 1000.0f;
            rawCurMa   = ina219.getCurrent_mA();
        } else {
            ina219Ready = false;
            DBG("Battery: INA219 reading invalid; switching to ADC fallback");
        }
    }

    if (!ina219Ready) {
        // Fallback: ADC resistor divider
        uint32_t rawMotorAdc = analogReadMilliVolts(PIN_BATT_MOTOR);
        rawMotorMv = rawMotorAdc * BATT_MOTOR_RATIO;
        rawCurMa   = 0.0f;
    }

    // Logic Battery ADC reading (2S power)
    uint32_t rawLogicAdc = analogReadMilliVolts(PIN_BATT_LOGIC);
    float rawLogicMv = rawLogicAdc * BATT_LOGIC_RATIO;

    // Apply Exponential Moving Average filter (filters out motor throttle sag)
    battMotorSmooth = battMotorSmooth * (1.0f - BATT_ALPHA) + rawMotorMv * BATT_ALPHA;
    battLogicSmooth = battLogicSmooth * (1.0f - BATT_ALPHA) + rawLogicMv * BATT_ALPHA;
    battCurSmooth   = battCurSmooth   * (1.0f - BATT_ALPHA) + rawCurMa   * BATT_ALPHA;

    battMotorMv   = (uint16_t)battMotorSmooth;
    battLogicMv   = (uint16_t)battLogicSmooth;
    battCurrentMa = (int16_t)battCurSmooth;

    // Compute live percentage from filtered pack voltage
    battPercent   = calculateLipoPercent(battMotorMv, LIPO_CELL_COUNT);
}

// --- Public Getters ---
static uint16_t batteryMotorMv()     { return battMotorMv; }
static uint16_t batteryLogicMv()     { return battLogicMv; }
static int16_t  batteryCurrentMa()   { return battCurrentMa; }
static uint8_t  batteryPercentage()  { return battPercent; }
static bool     batteryHasIna219()   { return ina219Ready; }
static bool     batteryMotorReadingValid() { return battMotorMv >= 5000; }
static bool     batteryLogicReadingValid() { return battLogicMv >= 5000; }

// --- Safety Alarms ---
static bool batteryMotorLow() {
    if (LIPO_CELL_COUNT == 4) {
        return battMotorMv > 0 && battMotorMv < LOW_BATT_4S_MV;
    }
    return battMotorMv > 0 && battMotorMv < LOW_BATT_3S_MV;
}

static bool batteryLogicLow() {
    return battLogicMv > 0 && battLogicMv < 6400; // < 6.4V for 2S logic
}
