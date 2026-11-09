# Rescue Boat — Agent Guide (Dual-ESP Direct Wi-Fi)

## Project Summary

Dual-ESP rescue boat with direct smartphone Wi-Fi connection (40–70m range):
- **Classic ESP32-WROOM-32U sensor controller:** Hosts SoftAP `RescueBoat-GCS` and a compact control WebSocket, parses 25Hz phone commands, controls motors and gimbal, reads GPS/IMU/radar/battery sensors, and handles RTH on signal loss. Program it through the CP2102 UART bridge with `nrflash`.
- **ESP32-CAM:** Connects to the sensor controller SoftAP as a separate client (`192.168.4.2`), streams real-time MJPEG video on port 81 (`/stream`).
- **Web GCS HUD:** Displays dual touch joysticks, live camera backdrop, and a dual-gyroscope minimap rendering both the **Boat heading** (from IMU) and **Phone heading** (from phone sensor).

## Repository Structure

```
boat-esp32-clusteer/
├── ARCHITECTURE.md          # System design, pin maps, dual-ESP topology
├── CIRCUIT.md               # Circuit diagrams, full pinout & wiring schematics
├── AGENT.md                 # Project guide for agents
├── TEST.md                  # Testing procedures and checklists
├── platformio.ini           # PlatformIO multi-environment config (boat, cam)
│
├── boat-controller/         # Classic ESP32-WROOM-32U Sensor Controller
│   └── src/
│       ├── main.cpp          # Setup, SoftAP, cooperative loop, failsafe
│       ├── config.h          # Pin definitions, Wi-Fi credentials, tuning
│       ├── webserver.h       # AsyncWebServer + WebSocket 25Hz control bridge
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
├── web-ui/                  # HUD served locally by the phone
│   ├── index.html            # Joysticks, live camera backdrop, dual-gyro minimap
│
└── deploy/                  # Flash scripts
    ├── flash_boat.ps1        # Build + flash classic ESP32 controller
    ├── flash_cam.ps1         # Build + flash Camera ESP32-CAM
    ├── flash_all.ps1         # Sequentially flashes controller and CAM
```

## Build & Flash Commands

```powershell
# Build boat firmware
pio run -e boat

# Build camera firmware
pio run -e cam

# Flash boat controller (CP2102 UART bridge)
.\deploy\flash_boat.ps1 -Monitor

# Flash camera (IO0 bridged to GND)
.\deploy\flash_cam.ps1 -Monitor

# Flash both boards sequentially
.\deploy\flash_all.ps1

# Build the sensor-controller image
tpio run -e boat
```

### Run the HUD on the Phone

The HUD is not embedded or served by the controller. On the phone in Termux, from the repository root, run:

```sh
python -m http.server 8000 --directory web-ui
```

Connect the phone to `RescueBoat-GCS`, then open `http://127.0.0.1:8000` in its browser. The page sends control to the controller WebSocket and loads video directly from the ESP32-CAM.

## Dual-Gyroscope Minimap Operation

1. **Boat Heading (Green):** Drawn in the center of the minimap rotating with the boat's physical BNO055 compass orientation.
2. **Phone Heading (Amber):** Drawn at the operator position (`hx, hy`) with a phone icon and a 40° field-of-view cone pointing in the direction the phone is held.
3. **Range Vectors:** A dashed line connects the boat and phone with live distance in meters.
