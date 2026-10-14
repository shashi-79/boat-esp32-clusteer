# Rescue Boat — Agent Guide

## Project Summary

2-ESP32 rescue boat with 1.5–2 km range. Control via ESP-NOW LR, video via 5.8GHz analog FPV.
Phone connects to Shore ESP32 Wi-Fi, opens browser HUD with joysticks, telemetry, and game-style minimap.

## Repository Structure

```
boat-esp32-clusteer/
├── ARCHITECTURE.md          # System design, pin maps, packet formats
├── AGENT.md                 # This file — project guide for agents
├── TEST.md                  # Testing procedures and checklists
├── platformio.ini           # PlatformIO multi-environment config
│
├── shared/                  # Shared headers used by both firmwares
│   └── packets.h            # ControlPacket & TelemetryPacket structs
│
├── boat-controller/         # ESP32-S3 boat-side firmware
│   └── src/
│       ├── main.cpp          # Setup + main loop
│       ├── config.h          # Pin definitions, constants, tuning
│       ├── radio.h / .cpp    # ESP-NOW LR receive + send
│       ├── motors.h / .cpp   # ESC PWM output + differential mixing
│       ├── servos.h / .cpp   # Camera pan/tilt servo control
│       ├── gps.h / .cpp      # NEO-M8N NMEA parser (TinyGPSPlus)
│       ├── imu.h / .cpp      # BNO055/MPU9250 compass heading
│       ├── radar.h / .cpp    # HLK-LD2450 obstacle parser
│       ├── battery.h / .cpp  # Dual ADC voltage monitors
│       ├── safety.h / .cpp   # Failsafe watchdog, E-STOP, low-batt
│       └── rth.h / .cpp      # Return-to-Home PID navigation
│
├── shore-station/            # Shore ESP32 firmware
│   └── src/
│       ├── main.cpp          # Setup + main loop
│       ├── config.h          # Wi-Fi SSID, passwords, constants
│       ├── radio.h / .cpp    # ESP-NOW LR send + receive
│       ├── webserver.h/.cpp  # AsyncWebServer + WebSocket handler
│       └── web_ui.h          # Gzipped HTML/CSS/JS in PROGMEM
│
├── web-ui/                   # Source for browser HUD (dev only)
│   ├── index.html            # Single-file app (joysticks + minimap)
│   └── compress.py           # Script to gzip HTML → C header
│
└── deploy/                   # Flash and deploy scripts
    ├── flash_boat.ps1        # PowerShell: build + flash boat firmware
    ├── flash_shore.ps1       # PowerShell: build + flash shore firmware
    ├── flash_all.ps1         # PowerShell: flash both boards sequentially
    └── compress_ui.ps1       # PowerShell: compress web UI → PROGMEM header
```

## Build Commands

```powershell
# Build boat firmware only
pio run -e boat

# Build shore firmware only
pio run -e shore

# Flash boat (auto-detect COM port)
pio run -e boat -t upload

# Flash shore (auto-detect COM port)
pio run -e shore -t upload

# Monitor boat serial output
pio device monitor -e boat -b 115200

# Monitor shore serial output
pio device monitor -e shore -b 115200
```

## Key Design Decisions

1. **ESP-NOW LR (not Wi-Fi or LoRa)** for 1.5–2 km control range.
2. **5.8GHz Analog FPV** for video (not Wi-Fi streaming).
3. **Bidirectional ESCs** with differential thrust (no rudder servo).
4. **Dual batteries** (motor power isolated from logic power).
5. **Manual ARM required** (GPS lock ≥6 sats before motors enable).
6. **Autonomous RTH** on signal loss (PID compass navigation).
7. **24GHz mmWave radar** for obstacle detection on minimap.
8. **100% offline Web UI** served from PROGMEM (no CDN, no internet).

## Coding Conventions

- C++17, Arduino framework via PlatformIO.
- All timing via `millis()`, no `delay()` in main loop.
- Non-blocking architecture: each module has `setup()` and `update()`.
- Shared packet structs in `shared/packets.h` (included by both firmwares).
- Serial debug prints gated behind `#define DEBUG_ENABLED`.
