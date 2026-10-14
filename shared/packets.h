#pragma once
// ============================================================
// Shared Packet Definitions — Used by BOTH Boat and Shore
// ============================================================

#include <stdint.h>

// --- Control Packet (Shore → Boat, sent at 25 Hz) ---
typedef struct __attribute__((packed)) {
    int16_t throttle;   // -1000 to +1000 (forward/reverse)
    int16_t yaw;        // -1000 to +1000 (left/right differential)
    int16_t cam_pan;    // -1000 to +1000 (camera horizontal)
    int16_t cam_tilt;   // -1000 to +1000 (camera vertical)
    uint8_t arm;        // 0 = DISARMED, 1 = ARMED
    uint8_t estop;      // 1 = EMERGENCY STOP (instant neutral)
    uint8_t mode;       // 0 = MANUAL, 1 = RTH
    uint8_t checksum;   // XOR of all preceding bytes
} ControlPacket;

// --- Telemetry Packet (Boat → Shore, sent at 5 Hz) ---
typedef struct __attribute__((packed)) {
    float    lat;           // GPS latitude (degrees)
    float    lon;           // GPS longitude (degrees)
    int16_t  heading;       // Compass heading 0–359 degrees
    int16_t  speed;         // Ground speed in cm/s
    uint16_t v_motor_mv;    // Motor battery voltage in millivolts
    uint16_t v_logic_mv;    // Logic battery voltage in millivolts
    int16_t  current_ma;    // Motor battery live current in mA (from INA219)
    uint8_t  batt_percent;  // Live battery percentage 0-100% (non-linear LiPo curve)
    float    radar_dist;    // Nearest obstacle distance (meters)
    int16_t  radar_angle;   // Nearest obstacle angle (degrees)
    uint8_t  sats;          // GPS satellite count
    uint8_t  fix_type;      // 0=none, 2=2D, 3=3D
    uint8_t  armed;         // Current arm state
    uint8_t  mode;          // 0=MANUAL, 1=RTH, 2=FAILSAFE
    float    home_dist;     // Distance to Home (meters)
    int16_t  home_bearing;  // Bearing to Home (degrees)
} TelemetryPacket;

// --- Packet Checksum Utility ---
inline uint8_t calcChecksum(const uint8_t* data, size_t len) {
    uint8_t cs = 0;
    for (size_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

// --- Constants ---
#define ESPNOW_CHANNEL       1
#define CONTROL_SEND_HZ      25
#define TELEMETRY_SEND_HZ    5
#define FAILSAFE_TIMEOUT_MS  1500
#define RTH_CRUISE_PERCENT   30
#define RTH_ARRIVAL_RADIUS_M 3.0f
#define LOW_BATT_3S_MV       10500
#define LOW_BATT_4S_MV       14000

// --- Modes ---
#define MODE_MANUAL    0
#define MODE_RTH       1
#define MODE_FAILSAFE  2
