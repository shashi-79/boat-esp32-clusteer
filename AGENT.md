# Rescue Boat — Agent Guide (Dual-ESP Direct Wi-Fi)

## Project Summary

Dual-ESP rescue boat with direct smartphone Wi-Fi connection (40–70m range):
- **ESP32-S3:** Hosts SoftAP `RescueBoat-GCS`, serves Web HUD via AsyncWebServer, parses 25Hz WebSocket joysticks, controls dual ESC motors with differential mixing, pan/tilt camera gimbal, GPS (NEO-M8N), compass IMU (BNO055), 24GHz mmWave radar (HLK-LD2450), INA219 battery telemetry, and autonomous Return-To-Home (RTH) on signal loss (>1.5s).
- **ESP32-CAM:** Connects to S3 SoftAP as client (`192.168.4.2`), streams real-time MJPEG video on port 81 (`/stream`).
- **Web GCS HUD:** Displays dual touch joysticks, live camera backdrop, and a dual-gyroscope minimap rendering both the **Boat heading** (from IMU) and **Phone heading** (from phone sensor).

## Repository Structure

```
boat-esp32-clusteer/
├── ARCHITECTURE.md          # System design, pin maps, dual-ESP topology
├── AGENT.md                 # Project guide for agents
├── TEST.md                  # Testing procedures and checklists
├── platformio.ini           # PlatformIO multi-environment config (boat, cam)
│
├── boat-controller/         # ESP32-S3 Master Firmware
│   └── src/
│       ├── main.cpp          # Setup, SoftAP, cooperative loop, failsafe
│       ├── config.h          # Pin definitions, Wi-Fi credentials, tuning
│       ├── webserver.h       # AsyncWebServer + WebSocket 25Hz control bridge
│       ├── web_ui.h          # PROGMEM embedded tactical Web HUD
│       ├── motors.h          # Dual ESC PWM mixing & neutral latching
│       ├── servos.h          # Camera pan/tilt gimbal PWM control
│       ├── gps.h             # NEO-M8N NMEA parser & home vector calculation
│       ├── imu.h             # BNO055 9-DOF compass heading fusion
│       ├── radar.h           # HLK-LD2450 24GHz mmWave obstacle parser
│       ├── battery.h         # INA219 I2C live current/voltage & LiPo curve
│       ├── safety.h          # Failsafe watchdog, arming gate, low-batt RTH
│       └── rth.h             # Compass PID Return-To-Home navigation
│
├── cam-streamer/            # ESP32-CAM Video Streamer Firmware
│   └── src/
│       └── main.cpp          # OV2640 camera init + port 81 MJPEG server
│
├── web-ui/                  # Standalone Browser HUD (dev source)
│   ├── index.html            # Joysticks, live camera backdrop, dual-gyro minimap
│   └── compress.py           # Embeds index.html into boat web_ui.h
│
└── deploy/                  # Flash and deploy scripts
    ├── flash_boat.ps1        # Build + flash Boat ESP32-S3
    ├── flash_cam.ps1         # Build + flash Camera ESP32-CAM
    ├── flash_all.ps1         # Sequentially flashes S3 and CAM
    └── compress_ui.ps1       # Compresses index.html into web_ui.h
```

## Build & Flash Commands

```powershell
# Build boat firmware
pio run -e boat

# Build camera firmware
pio run -e cam

# Flash boat (auto-detect COM port)
.\deploy\flash_boat.ps1 -Monitor

# Flash camera (IO0 bridged to GND)
.\deploy\flash_cam.ps1 -Monitor

# Flash both boards sequentially
.\deploy\flash_all.ps1
```

## Dual-Gyroscope Minimap Operation

1. **Boat Heading (Green):** Drawn in the center of the minimap rotating with the boat's physical BNO055 compass orientation.
2. **Phone Heading (Amber):** Drawn at the operator position (`hx, hy`) with a phone icon and a 40° field-of-view cone pointing in the direction the phone is held.
3. **Range Vectors:** A dashed line connects the boat and phone with live distance in meters.
