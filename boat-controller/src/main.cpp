// ============================================================
// BOAT CONTROLLER — Main Entry Point (ESP32-S3)
// ============================================================
// Architecture: Non-blocking cooperative loop
// Each module has setup() and update() — NO delay() calls.
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "../../shared/packets.h"
#include "radio.h"
#include "motors.h"
#include "servos.h"
#include "gps.h"
#include "imu.h"
#include "radar.h"
#include "battery.h"
#include "safety.h"
#include "rth.h"

// --- Timing intervals (milliseconds) ---
static const uint32_t INTERVAL_TELEMETRY  = 1000 / TELEMETRY_SEND_HZ;   // 200ms
static const uint32_t INTERVAL_BATTERY    = 100;                          // 10 Hz
static const uint32_t INTERVAL_IMU        = 20;                           // 50 Hz
static const uint32_t INTERVAL_MOTORS     = 20;                           // 50 Hz

static uint32_t lastTelemetry = 0;
static uint32_t lastBattery   = 0;
static uint32_t lastImu       = 0;
static uint32_t lastMotors    = 0;

// Track whether RTH PID was just activated
static bool rthActive = false;

void setup() {
    Serial.begin(115200);
    delay(500);  // Allow USB CDC to connect

    Serial.println("========================================");
    Serial.println("  RESCUE BOAT — Boat Controller v1.0");
    Serial.println("========================================");

    radioSetup();
    motorsSetup();
    servosSetup();
    gpsSetup();
    imuSetup();
    radarSetup();
    batterySetup();

    Serial.println("[BOOT] All systems initialized.");
    Serial.println("[BOOT] Waiting for shore station link...");
}

void loop() {
    uint32_t now = millis();

    // --- Always-run: GPS and Radar feed (byte-by-byte, non-blocking) ---
    gpsUpdate();
    radarUpdate();

    // --- 50 Hz: IMU heading update ---
    if (now - lastImu >= INTERVAL_IMU) {
        lastImu = now;
        imuUpdate();
    }

    // --- 10 Hz: Battery voltage reading ---
    if (now - lastBattery >= INTERVAL_BATTERY) {
        lastBattery = now;
        batteryUpdate();
    }

    // --- 50 Hz: Motor / Servo control loop ---
    if (now - lastMotors >= INTERVAL_MOTORS) {
        lastMotors = now;

        // Run safety layer to determine current mode
        uint8_t mode = safetyProcess(
            rxControl,
            radioIsLinked(),
            gpsHasFix(),
            gpsSats(),
            gpsFixType(),
            batteryMotorLow()
        );

        // --- ARM event: set Home ---
        if (safetyIsArmed() && !homeSet) {
            gpsSetHome();
        }

        // --- Mode-based motor control ---
        switch (mode) {
            case MODE_MANUAL:
                if (safetyIsArmed()) {
                    // Pass joystick inputs through to motors
                    motorsMix(rxControl.throttle, rxControl.yaw);
                    // Camera servos always respond
                    servosSetCamera(rxControl.cam_pan, rxControl.cam_tilt);
                } else {
                    // Disarmed: motors neutral, servos still work
                    motorsSetNeutral();
                    servosSetCamera(rxControl.cam_pan, rxControl.cam_tilt);
                }
                rthActive = false;
                break;

            case MODE_RTH:
                if (!rthActive) {
                    rthReset();
                    rthActive = true;
                    DBG("Main: RTH mode ACTIVATED");
                }
                {
                    float dist    = gpsDistanceToHome();
                    float bearing = gpsBearingToHome();
                    float heading = imuGetHeading();

                    int16_t yaw      = rthComputeYaw(bearing, heading, dist);
                    int16_t throttle = rthGetThrottle(dist);

                    if (rthHasArrived(dist)) {
                        motorsSetNeutral();
                        DBG("Main: RTH ARRIVED — motors neutral");
                    } else {
                        motorsMix(throttle, yaw);
                    }
                }
                // Camera centers during RTH
                servosCenterAll();
                break;

            case MODE_FAILSAFE:
            default:
                // All motors and servos to safe state
                motorsSetNeutral();
                servosCenterAll();
                rthActive = false;
                break;
        }
    }

    // --- 5 Hz: Send telemetry to shore ---
    if (now - lastTelemetry >= INTERVAL_TELEMETRY) {
        lastTelemetry = now;

        // Populate telemetry packet
        txTelemetry.lat          = gpsLat();
        txTelemetry.lon          = gpsLon();
        txTelemetry.heading      = (int16_t)imuGetHeading();
        txTelemetry.speed        = (int16_t)gpsSpeed();
        txTelemetry.v_motor_mv   = batteryMotorMv();
        txTelemetry.v_logic_mv   = batteryLogicMv();
        txTelemetry.current_ma   = batteryCurrentMa();
        txTelemetry.batt_percent = batteryPercentage();
        txTelemetry.radar_dist   = radarGetDist();
        txTelemetry.radar_angle  = radarGetAngle();
        txTelemetry.sats         = gpsSats();
        txTelemetry.fix_type     = gpsFixType();
        txTelemetry.armed        = safetyIsArmed() ? 1 : 0;
        txTelemetry.mode         = safetyGetMode();
        txTelemetry.home_dist    = gpsDistanceToHome();
        txTelemetry.home_bearing = (int16_t)gpsBearingToHome();

        radioSendTelemetry();

        // Debug print
        DBG("Telem: sats=%d fix=%d armed=%d mode=%d batt=%dmV (%d%%, %dmA) heading=%d dist=%.1fm",
            txTelemetry.sats, txTelemetry.fix_type,
            txTelemetry.armed, txTelemetry.mode,
            txTelemetry.v_motor_mv, txTelemetry.batt_percent, txTelemetry.current_ma,
            txTelemetry.heading, txTelemetry.home_dist);
    }
}
