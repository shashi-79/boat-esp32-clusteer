# Rescue Boat — System Architecture (Dual-ESP Direct Wi-Fi)

## System Overview

A dual-ESP rescue boat controlled directly from a smartphone over Wi-Fi:
- **Sensor Controller (classic ESP32 / WROOM-32U profile):** Hosts the Wi-Fi SoftAP (`RescueBoat-GCS`, `192.168.4.1`) and compact WebSocket endpoint, parses 25Hz joystick controls, runs motors, gimbal, GPS, IMU, radar, battery monitoring, and RTH. `nrflash` auto-detected the attached controller as classic ESP32 behind a CP2102 bridge.
- **Camera Streamer (ESP32-CAM):** Connects to the controller's SoftAP with static IP `192.168.4.2` and serves an independent MJPEG stream on port 81 (`/stream`).
- **GCS HUD (Phone Browser):** The phone serves `web-ui/index.html` from a local HTTP server; the page connects directly to the boat WebSocket at `192.168.4.1` and the ESP32-CAM video at `192.168.4.2:81`.

The controller does not send the HUD HTML to the phone. It serves a small diagnostic response at `/` and WebSocket control at `/ws`. For a Termux phone, run `python -m http.server 8000 --directory web-ui` from the repository root, connect the phone to `RescueBoat-GCS`, then open `http://127.0.0.1:8000` in the phone browser. The control page defaults to `192.168.4.1`; use `?boat=<controller-ip>` only when the AP address differs.

## Sensor Availability and Failsafe Behavior

- Arming requires a recent GPS fix with at least six satellites and a valid 3D fix, plus a responding BNO055 heading.
- GPS fixes older than 2 seconds are treated as unavailable. Losing GPS or valid IMU readings while armed disarms the boat and commands neutral motors; RTH is not attempted with stale navigation data.
- Phone control packets time out after 1.5 seconds. With valid GPS and IMU data, the boat enters RTH; otherwise it disarms and holds neutral.
- INA219 absence falls back to the configured motor voltage ADC divider. The radar and camera are advisory/video subsystems and do not block arming.
- A motor pack reading below 5 V is treated as missing; arming is rejected and a loss while armed disarms to neutral.
- E-stop remains latched until a disarm command is sent, then the operator may arm again after the normal checks pass.
- Only the first WebSocket controller is accepted. Control packets must contain all expected fields in range; disconnect clears drive commands. CP2102 uses UART0 GPIO 1/3; GPS and radar use UART1/UART2 on GPIO 16–19.

## Hardware Topology

```text
 ┌─────────────────────────── RESCUE BOAT ───────────────────────────┐
 │                                                                   │
 │  ┌─────────────────────────┐            ┌──────────────────────┐  │
 │  │        ESP32-CAM        │ MJPEG HTTP │ Classic ESP32 WROOM │  │
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
                                           │ • Local HUD:         │
                                           │   http://127.0.0.1:8000│
                                           │ • Control:           │
                                           │   ws://192.168.4.1/ws│
                                           │ • Video: MJPEG layer │
                                           │ • Left: Drive/Yaw    │
                                           │ • Right: Camera      │
                                           │ • Minimap: Dual-Gyro │
                                           │   (Boat + Phone Hdg) │
                                           └──────────────────────┘
```

## Classic ESP32-WROOM-32U Pin Allocation (Sensor Controller)

| GPIO | Function             | Protocol    | Notes                          |
|-----:|:---------------------|:------------|:-------------------------------|
|   13 | Future Servo 1       | LEDC PWM    | Reserved                       |
|   14 | Future Servo 2       | LEDC PWM    | Reserved                       |
|   16 | GPS RX (from GPS TX) | HW UART1    | NEO-M8N                        |
|   17 | GPS TX (to GPS RX)   | HW UART1    | NEO-M8N                        |
|   18 | Radar RX             | HW UART2    | HLK-LD2450                     |
|   19 | Radar TX             | HW UART2    | HLK-LD2450                     |
|   21 | I²C SDA              | I²C Bus     | INA219 (0x40) + BNO055 (0x28)  |
|   22 | I²C SCL              | I²C Bus     | INA219 (0x40) + BNO055 (0x28)  |
|   23 | Future Servo 3       | LEDC PWM    | Reserved                       |
|   25 | Left ESC Signal      | LEDC PWM    | Bidirectional, 1500µs neutral  |
|   26 | Right ESC Signal     | LEDC PWM    | Bidirectional, 1500µs neutral  |
|   27 | Camera Pan Servo     | LEDC PWM    | 500–2500µs range               |
|   32 | Camera Tilt Servo    | LEDC PWM    | 500–2500µs range               |
|   34 | Motor Battery ADC    | ADC1 input  | Divider; input only            |
|   35 | Logic Battery ADC    | ADC1 input  | Divider; input only            |

> GPIO 6–11 are reserved for module flash. GPIO 1/3 are used by the CP2102. GPIO 34/35 are input-only ADC1 pins. This pin map assumes a WROOM module without PSRAM.

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
- **Logic Battery (2S LiPo or 5V Power Bank):** Feeds the classic ESP32 controller and 3.3V sensors.
- **Common Ground:** ESC signal GND, BEC GND, and ESP32 GND must be joined together.

## Dual-Gyroscope Minimap

The top-right tactical radar minimap integrates two live sensor streams:
1. **Boat Heading (Green Arrow):** Sourced from the boat's BNO055 9-DOF IMU. Points along the boat's bow.
2. **Phone Heading (Amber Sector / Icon):** Sourced from the operator's smartphone gyroscope/compass API (`deviceorientation`). Points in the direction the phone is held.
3. **Home & Distance:** A dashed line connects the boat and the operator station with real-time distance and bearing.
4. **24GHz mmWave Radar:** Targets within 8 meters render as pulsing red obstacle blips.
