# Rescue Boat — System Architecture (Dual-ESP Direct Wi-Fi)

## System Overview

A dual-ESP rescue boat controlled directly from a smartphone over Wi-Fi:
- **Boat Controller (ESP32-S3):** Hosts the Wi-Fi SoftAP (`RescueBoat-GCS`, `192.168.4.1`), serves the Web GCS HUD, parses 25Hz WebSocket joystick controls, runs the differential motor mixer, gimbal servos, GPS, compass IMU, 24GHz mmWave radar, INA219 battery monitor, and autonomous Return-To-Home (RTH) failsafe.
- **Camera Streamer (ESP32-CAM):** Connects to the S3's SoftAP with static IP `192.168.4.2` and serves an independent low-latency MJPEG video stream on port 81 (`/stream`).
- **GCS HUD (Phone Browser):** Renders dual touch joysticks, battery gauge with live voltage and current, camera video backdrop, and a tactical dual-gyroscope minimap showing both the **Boat heading** (BNO055) and **Phone heading** (phone sensor).

## Hardware Topology

```text
 ┌─────────────────────────── RESCUE BOAT ───────────────────────────┐
 │                                                                   │
 │  ┌─────────────────────────┐            ┌──────────────────────┐  │
 │  │        ESP32-CAM        │ MJPEG HTTP │    ESP32-S3 (N8R8)   │  │
 │  │     Camera Streamer     │  (Port 81) │   MASTER CONTROLLER  │  │
 │  │                         │───────────►│                      │  │
 │  │ • AI-Thinker OV2640     │            │ • Wi-Fi AP:          │  │
 │  │ • Client IP 192.168.4.2 │            │   192.168.4.1        │  │
 │  │ • Powered: 5V BEC       │            │ • AsyncWebServer     │  │
 │  │   (isolated power)      │            │ • WebSocket (25 Hz)  │  │
 │  └─────────────────────────┘            │ • 2x Bidirect ESCs   │  │
 │                                         │ • 2x Camera Servos   │  │
 │                                         │ • BNO055 IMU (I²C)   │  │
 │                                         │ • NEO-M8N GPS (UART1)│  │
 │                                         │ • HLK-LD2450 (UART2) │  │
 │                                         │ • INA219 Batt (I²C)  │  │
 │                                         │ • External Antenna   │  │
 │                                         └──────────┬───────────┘  │
 └────────────────────────────────────────────────────┼──────────────┘
                                                      │ 802.11b/g/n Wi-Fi
                                                      │ (40–70m range)
                                                      ▼
                                           ┌──────────────────────┐
                                           │     MOBILE PHONE     │
                                           │                      │
                                           │ • Browser:           │
                                           │   http://192.168.4.1 │
                                           │ • Video: MJPEG layer │
                                           │ • Left: Drive/Yaw    │
                                           │ • Right: Camera      │
                                           │ • Minimap: Dual-Gyro │
                                           │   (Boat + Phone Hdg) │
                                           └──────────────────────┘
```

## ESP32-S3 Pin Allocation (Boat Controller)

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

## ESP32-CAM Pin Allocation (Camera Node)

| Pin | Function | Notes |
|:---|:---|:---|
| **5V / GND** | Power Input | Connect to isolated 5V 2A BEC |
| **GPIO 0** | Flash / Run Mode | Bridge to GND to flash; open to run |
| **U0TXD / U0RXD** | Programming UART | Used only for flashing firmware |
| **All Other GPIOs** | Dedicated to OV2640 | Managed automatically by camera driver |

## Power Architecture

- **Motor Battery (3S / 4S LiPo):** Feeds dual ESCs through the INA219 high-side shunt.
- **5V BEC 1:** Steps down motor LiPo to 5V (2A) for ESP32-CAM and servos.
- **Logic Battery (2S LiPo or 5V Power Bank):** Feeds ESP32-S3 and 3.3V sensors.
- **Common Ground:** ESC signal GND, BEC GND, and ESP32 GND must be joined together.

## Dual-Gyroscope Minimap

The top-right tactical radar minimap integrates two live sensor streams:
1. **Boat Heading (Green Arrow):** Sourced from the boat's BNO055 9-DOF IMU. Points along the boat's bow.
2. **Phone Heading (Amber Sector / Icon):** Sourced from the operator's smartphone gyroscope/compass API (`deviceorientation`). Points in the direction the phone is held.
3. **Home & Distance:** A dashed line connects the boat and the operator station with real-time distance and bearing.
4. **24GHz mmWave Radar:** Targets within 8 meters render as pulsing red obstacle blips.
