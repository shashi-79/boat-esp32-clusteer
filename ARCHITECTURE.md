# Rescue Boat — System Architecture

## System Overview

A remotely operated rescue boat with 1.5–2 km operational range using a split-channel RF architecture:
- **Control/Telemetry:** ESP-NOW Long-Range mode (2.4 GHz, ~250 kbps)
- **Live Video:** 5.8 GHz Analog FPV (independent, zero ESP32 CPU load)

## Hardware Topology

```text
┌────────────────────────────────────────────────────────────────┐
│                          RESCUE BOAT                           │
│                                                                │
│  ┌─────────────────────────┐     ┌──────────────────────────┐  │
│  │   5.8GHz FPV Payload    │     │      ESP32-S3 (N8R8)     │  │
│  │                         │     │    BOAT CONTROLLER        │  │
│  │ • 1200TVL FPV Camera    │     │                          │  │
│  │ • TS5828 600mW VTX      │     │ • ESP-NOW LR Transceiver │  │
│  │ • Cloverleaf Antenna    │     │ • 2x Bidirect ESC (PWM)  │  │
│  │ • Powered: Motor LiPo   │     │ • 2x Camera Servos (PWM) │  │
│  │   via LC Filter + BEC   │     │ • 3x Future Servos (PWM) │  │
│  └─────────────────────────┘     │ • BNO055/MPU9250 (I²C)   │  │
│                                  │ • NEO-M8N GPS (UART1)    │  │
│                                  │ • HLK-LD2450 Radar(UART2)│  │
│                                  │ • 2x Battery ADC          │  │
│                                  │ • IPEX Ext. Antenna       │  │
│                                  └──────────────┬───────────┘  │
└──────────────────────────────────────────────────┼──────────────┘
                                                   │
                                    ESP-NOW LR (2.4GHz, 1.5–2 km)
                                                   │
┌──────────────────────────────────────────────────┼──────────────┐
│                        SHORE STATION                            │
│                                                                 │
│  ┌──────────────────────┐          ┌──────────────────────────┐ │
│  │ 5.8GHz USB OTG       │          │    Shore ESP32 Board     │ │
│  │ Receiver (ROTG01)    │          │                          │ │
│  │                      │          │ • ESP-NOW LR Client      │ │
│  │ • Patch Antenna      │          │ • Wi-Fi SoftAP (local)   │ │
│  │ • USB-C to Phone     │          │ • AsyncWebServer + WS    │ │
│  └──────────┬───────────┘          │ • Serves Web HUD from    │ │
│             │ USB-C                │   PROGMEM (offline)      │ │
│             ▼                      │ • High-Gain Ext. Antenna │ │
│  ┌──────────────────────────────┐  └──────────────┬───────────┘ │
│  │       MOBILE PHONE          │                  │ Local Wi-Fi │
│  │                              │◄─────────────────┘             │
│  │ • Browser: 192.168.4.1      │                                │
│  │ • Video: getUserMedia(USB)  │                                │
│  │ • Left Stick: Boat          │                                │
│  │ • Right Stick: Camera       │                                │
│  │ • HUD: Telemetry + Minimap  │                                │
│  └──────────────────────────────┘                                │
└──────────────────────────────────────────────────────────────────┘
```

## ESP32-S3 Pin Allocation (Boat)

| GPIO | Function             | Protocol    | Notes                          |
|-----:|:---------------------|:------------|:-------------------------------|
|    1 | Motor Batt Fallback  | ADC1        | Resistor divider (fallback)    |
|    2 | Logic Battery ADC    | ADC1        | Voltage divider 10k/2k         |
|    4 | Left ESC Signal      | LEDC PWM    | Bidirectional, 1500µs neutral  |
|    5 | Right ESC Signal     | LEDC PWM    | Bidirectional, 1500µs neutral  |
|    6 | Camera Pan Servo     | LEDC PWM    | 500–2500µs range               |
|    7 | Camera Tilt Servo    | LEDC PWM    | 500–2500µs range               |
|    8 | I²C SDA              | I²C Bus     | INA219 (0x40) + BNO055 (0x28)  |
|    9 | I²C SCL              | I²C Bus     | INA219 (0x40) + BNO055 (0x28)  |
|   15 | Future Servo 1       | LEDC PWM    | Reserved                       |
|   16 | Future Servo 2       | LEDC PWM    | Reserved                       |
|   17 | Future Servo 3       | LEDC PWM    | Reserved                       |
|   18 | GPS RX (from GPS TX) | HW UART1    | NEO-M8N                        |
|   19 | GPS TX (to GPS RX)   | HW UART1    | NEO-M8N                        |
|   20 | Radar RX             | HW UART2    | HLK-LD2450                     |
|   21 | Radar TX             | HW UART2    | HLK-LD2450                     |

> **BLOCKED PINS (N8R8 Octal PSRAM):** GPIO 33, 34, 35, 36, 37 — never use.

## Power Architecture

- **Battery 1 (Motor LiPo 3S/4S):** Feeds ESCs and VTX (via LC filter + 5V BEC).
- **Battery 2 (Logic LiPo 2S or 5V Power Bank):** Feeds ESP32-S3, sensors, servos.
- **Grounding:** ESC signal GND connects to ESP32-S3 GND. ESC red 5V wire DISCONNECTED.

## Communication Protocol

### Control Packet (Shore → Boat, 25 Hz, 12 bytes)

```c
typedef struct __attribute__((packed)) {
    int16_t throttle;   // -1000 to +1000
    int16_t yaw;        // -1000 to +1000
    int16_t cam_pan;    // -1000 to +1000
    int16_t cam_tilt;   // -1000 to +1000
    uint8_t arm;        // 0=DISARMED, 1=ARMED
    uint8_t estop;      // 1=EMERGENCY STOP
    uint8_t mode;       // 0=MANUAL, 1=RTH
    uint8_t checksum;
} ControlPacket;
```

### Telemetry Packet (Boat → Shore, 5 Hz, 32 bytes)

```c
typedef struct __attribute__((packed)) {
    float    lat;           // GPS latitude
    float    lon;           // GPS longitude
    int16_t  heading;       // Compass heading 0–359
    int16_t  speed;         // cm/s from GPS
    uint16_t v_motor_mv;    // Motor battery mV
    uint16_t v_logic_mv;    // Logic battery mV
    float    radar_dist;    // Nearest obstacle meters
    int16_t  radar_angle;   // Obstacle angle degrees
    uint8_t  sats;          // GPS satellite count
    uint8_t  fix_type;      // 0=none, 2=2D, 3=3D
    uint8_t  armed;         // Current arm state
    uint8_t  mode;          // Current mode
    float    home_dist;     // Distance to home meters
    int16_t  home_bearing;  // Bearing to home degrees
} TelemetryPacket;
```

## Failsafe Logic

1. **Signal Loss (>1500 ms no ControlPacket):**
   - If ARMED and GPS fix >= 6 sats → Enter RTH mode (cruise 30%, PID compass nav).
   - If DISARMED or no GPS fix → Motors neutral (1500µs), wait.
2. **Low Battery (Motor < 10.5V for 3S / 14.0V for 4S):**
   - Force RTH regardless of signal.
3. **E-STOP received:**
   - Immediate motors neutral, DISARM, ignore all further commands until re-armed.
4. **RTH Arrival (< 3m from Home):**
   - Motors neutral, hold position.

## RTH Algorithm

```
1. bearing = atan2(home_lon - cur_lon, home_lat - cur_lat) → degrees
2. error  = bearing - compass_heading (normalize to ±180)
3. yaw_correction = PID(error)  // Kp=2.0, Ki=0.1, Kd=0.5
4. left_esc  = CRUISE_THROTTLE + yaw_correction
5. right_esc = CRUISE_THROTTLE - yaw_correction
6. if distance_to_home < 3m → neutral
```
