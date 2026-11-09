// ============================================================
// BOAT SENSOR CONTROLLER — Main Entry Point (Classic ESP32)
// ============================================================
// Architecture:
//   1. Hosts Wi-Fi SoftAP ("RescueBoat-GCS") for direct Phone & ESP32-CAM connection
//   2. Serves the control WebSocket; the phone hosts the HUD locally
//   3. Non-blocking cooperative loop: IMU, GPS, mmWave Radar, Battery, Servos, Motors
//   4. Autonomous Safety & Return-To-Home (RTH) on phone disconnect (>1.5s)
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "webserver.h"
#include "motors.h"
#include "servos.h"
#include "gps.h"
#include "imu.h"
#include "radar.h"
#include "battery.h"
#include "safety.h"
#include "rth.h"

// --- Timing intervals (milliseconds) ---
static const uint32_t INTERVAL_TELEMETRY = 200;  // 5 Hz
static const uint32_t INTERVAL_BATTERY   = 100;  // 10 Hz
static const uint32_t INTERVAL_IMU       = 20;   // 50 Hz
static const uint32_t INTERVAL_MOTORS    = 20;   // 50 Hz
static const uint32_t INTERVAL_CLEANUP   = 1000; // 1 Hz

static uint32_t lastTelemetry = 0;
static uint32_t lastBattery   = 0;
static uint32_t lastImu       = 0;
static uint32_t lastMotors    = 0;
static uint32_t lastCleanup   = 0;

static bool rthActive = false;

void setup() {
    Serial.begin(115200);
    delay(500);  // USB CDC settling

    Serial.println("========================================");
    Serial.println("  RESCUE BOAT — ESP32 Sensor Controller v2.0");
    Serial.println("========================================");
    Serial.printf("[MEM] flash=%u sketch=%u freeSketch=%u freeHeap=%u\n",
        static_cast<unsigned>(ESP.getFlashChipSize()),
        static_cast<unsigned>(ESP.getSketchSize()),
        static_cast<unsigned>(ESP.getFreeSketchSpace()),
        static_cast<unsigned>(ESP.getFreeHeap()));

    // 1. Start Wi-Fi SoftAP for Phone and ESP32-CAM
    WiFi.mode(WIFI_AP);
    bool apStarted = false;
    for (uint8_t attempt = 0; attempt < 3 && !apStarted; ++attempt) {
        apStarted = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, 0, WIFI_MAX_CONN);
        if (!apStarted) delay(500);
    }
    IPAddress apIP = WiFi.softAPIP();
    if (apStarted) {
        Serial.printf("[WIFI] SoftAP '%s' ready | IP: %s\n",
            WIFI_SSID, apIP.toString().c_str());
    } else {
        Serial.printf("[WIFI] ERROR: SoftAP '%s' failed to start after 3 attempts\n", WIFI_SSID);
    }

    // 2. Start Web Server and WebSocket
    webSetup();

    // 3. Initialize Boat Hardware Modules
    motorsSetup();
    servosSetup();
    gpsSetup();
    imuSetup();
    radarSetup();
    batterySetup();

    Serial.println("[BOOT] All systems initialized.");
    Serial.printf("[BOOT] Connect phone to '%s' and open http://%s\n",
        WIFI_SSID, apIP.toString().c_str());
}

void loop() {
    uint32_t now = millis();

    // --- Always-run: GPS and Radar byte streaming ---
    gpsUpdate();
    radarUpdate();

    // --- 50 Hz: IMU / Compass heading update ---
    if (now - lastImu >= INTERVAL_IMU) {
        lastImu = now;
        imuUpdate();
    }

    // --- 10 Hz: Battery voltage and current monitor ---
    if (now - lastBattery >= INTERVAL_BATTERY) {
        lastBattery = now;
        batteryUpdate();
    }

    // --- 50 Hz: Motor & Servo Control Loop ---
    if (now - lastMotors >= INTERVAL_MOTORS) {
        lastMotors = now;
        const ControlSnapshot control = webControlSnapshot();

        // Safety layer determines active mode (handles phone disconnect failsafe)
        uint8_t mode = safetyProcess(
            control.arm,
            control.estop,
            control.mode,
            webIsPhoneConnected(),
            gpsHasFix(),
            gpsSats(),
            gpsFixType(),
            batteryMotorLow(),
            imuHealthy(),
            batteryMotorReadingValid()
        );

        // Record Home GPS coordinate upon ARM
        if (safetyIsArmed() && !homeSet) {
            gpsSetHome();
        }

        // Execute mode-based motor commands
        switch (mode) {
            case MODE_MANUAL:
                if (safetyIsArmed()) {
                    motorsMix(control.throttle, control.yaw);
                    servosSetCamera(control.camPan, control.camTilt);
                } else {
                    motorsSetNeutral();
                    servosSetCamera(control.camPan, control.camTilt);
                }
                rthActive = false;
                break;

            case MODE_RTH:
                if (!rthActive) {
                    rthReset();
                    rthActive = true;
                    DBG("Safety: Entering autonomous RTH");
                }
                {
                    float dist    = gpsDistanceToHome();
                    float bearing = gpsBearingToHome();
                    float heading = imuGetHeading();

                    int16_t yaw      = rthComputeYaw(bearing, heading, dist);
                    int16_t throttle = rthGetThrottle(dist);

                    if (rthHasArrived(dist)) {
                        motorsSetNeutral();
                        DBG("RTH: Arrived within 3m radius");
                    } else {
                        motorsMix(throttle, yaw);
                    }
                }
                servosCenterAll();
                break;

            case MODE_FAILSAFE:
            default:
                motorsSetNeutral();
                servosCenterAll();
                rthActive = false;
                break;
        }
    }

    // --- 5 Hz: Send Telemetry to Phone via WebSocket ---
    if (now - lastTelemetry >= INTERVAL_TELEMETRY) {
        lastTelemetry = now;

        webSendTelemetry(
            batteryMotorMv(),
            batteryLogicMv(),
            batteryCurrentMa(),
            batteryPercentage(),
            gpsSats(),
            gpsFixType(),
            (int16_t)imuGetHeading(),
            (int16_t)gpsSpeed(),
            gpsLat(),
            gpsLon(),
            gpsDistanceToHome(),
            (int16_t)gpsBearingToHome(),
            radarGetDist(),
            radarGetAngle(),
            safetyIsArmed() ? 1 : 0,
            safetyGetMode(),
            gpsHasFix(),
            imuHealthy(),
            batteryMotorReadingValid(),
            radarIsHealthy(),
            batteryLogicReadingValid(),
            batteryHasIna219()
        );
    }

    // --- 1 Hz: Cleanup Dead WebSocket Clients ---
    if (now - lastCleanup >= INTERVAL_CLEANUP) {
        lastCleanup = now;
        webCleanup();
    }
}
