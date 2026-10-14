# Rescue Boat — Test Plan

## Phase 1: Bench Tests (No Water, No Motors)

### T1.1 ESP-NOW LR Link Test
- [ ] Flash both boards with radio-only firmware.
- [ ] Shore sends ControlPacket every 40ms (25 Hz).
- [ ] Boat receives and echoes TelemetryPacket back.
- [ ] Verify: Serial monitor shows round-trip packets.
- [ ] Verify: Packet loss < 1% at 1 meter distance.
- [ ] Walk 50m apart → verify link holds.
- **Pass criteria:** Bidirectional packets flowing, checksum valid.

### T1.2 Failsafe Timeout Test
- [ ] Boat receiving packets normally.
- [ ] Disconnect Shore ESP32 (power off).
- [ ] Verify: Boat serial prints "SIGNAL LOST" within 1500ms.
- [ ] Verify: Motor PWM outputs go to 1500µs (neutral).
- [ ] Reconnect Shore → verify boat resumes normal mode.
- **Pass criteria:** Failsafe triggers in < 2 seconds, recovers on reconnect.

### T1.3 ESC PWM Output Test (No Propellers!)
- [ ] ESCs powered, propellers REMOVED.
- [ ] Send throttle=0, yaw=0 → both ESCs read 1500µs.
- [ ] Send throttle=500, yaw=0 → both ESCs read ~1750µs (forward).
- [ ] Send throttle=-500, yaw=0 → both ESCs read ~1250µs (reverse).
- [ ] Send throttle=0, yaw=500 → left=1750, right=1250 (spin right).
- [ ] Send E-STOP → both ESCs snap to 1500µs.
- **Pass criteria:** Differential mixing math correct, E-STOP instant.

### T1.4 Servo Test
- [ ] Send cam_pan sweep -1000 to +1000.
- [ ] Send cam_tilt sweep -1000 to +1000.
- [ ] Verify: Servos move smoothly across full range.
- **Pass criteria:** No jitter, no buzzing at endpoints.

### T1.5 GPS Lock Test
- [ ] Place GPS antenna near window / outdoors.
- [ ] Verify: Serial prints satellite count increasing.
- [ ] Wait for fix_type=3 (3D fix) and sats >= 6.
- [ ] Verify: Lat/Lon values are sane (not 0.0 / NaN).
- **Pass criteria:** 3D fix within 60 seconds outdoors.

### T1.6 Compass Heading Test
- [ ] Read heading while rotating board 360 degrees slowly.
- [ ] Verify: Heading increases 0 → 90 → 180 → 270 → 360.
- [ ] Verify: No stuck values or sudden jumps > 30 degrees.
- **Pass criteria:** Smooth monotonic heading rotation.

### T1.7 Radar Obstacle Test
- [ ] Place a cardboard box 3 meters in front of radar.
- [ ] Verify: Serial prints distance ~3.0m ± 0.5m.
- [ ] Move box to 1 meter → verify ~1.0m.
- [ ] Remove box → verify no target or > 8m.
- **Pass criteria:** Distance reads within ±0.5m accuracy.

### T1.8 Battery ADC Test
- [ ] Connect known voltage source (e.g., bench supply at 11.1V).
- [ ] Verify: Serial prints voltage within ±0.2V of actual.
- [ ] Test low-battery threshold trigger.
- **Pass criteria:** Voltage accurate, low-batt alarm fires correctly.

### T1.9 Web UI + WebSocket Test
- [ ] Phone connects to Shore ESP32 Wi-Fi.
- [ ] Open browser → 192.168.4.1.
- [ ] Verify: Page loads fully (joysticks visible, minimap renders).
- [ ] Touch left joystick → verify Shore serial shows outgoing ControlPacket.
- [ ] Verify: Telemetry HUD updates (battery, GPS, heading values change).
- **Pass criteria:** Full round-trip UI → Shore → Boat → Shore → UI.

## Phase 2: Range Tests (Outdoor, No Water)

### T2.1 Open-Field Range Test
- [ ] Both boards with external IPEX antennas.
- [ ] Walk boat board away from shore board in open field.
- [ ] Log packet loss at: 100m, 300m, 500m, 800m, 1000m, 1500m.
- [ ] Note distance where packet loss exceeds 5%.
- **Pass criteria:** Usable link (< 5% loss) at ≥ 1000m.

### T2.2 RTH Simulation (Walking Test)
- [ ] ARM the system, set Home point.
- [ ] Walk boat board 200m away.
- [ ] Disconnect Shore ESP32 (simulate signal loss).
- [ ] Observe serial: RTH mode triggers, bearing calculation starts.
- [ ] Verify: Bearing to Home is correct direction.
- **Pass criteria:** RTH math produces correct bearing and distance.

## Phase 3: Water Tests

### T3.1 Static Float Test
- [ ] Boat hull sealed, all electronics mounted.
- [ ] Place boat in still water for 30 minutes.
- [ ] Check for any water ingress.
- **Pass criteria:** Zero water inside hull.

### T3.2 Motor Thrust Test
- [ ] Propellers attached, boat in water.
- [ ] 25% throttle forward → verify boat moves forward.
- [ ] 25% throttle reverse → verify boat moves backward.
- [ ] Yaw left/right → verify boat turns.
- **Pass criteria:** Differential steering works correctly.

### T3.3 Full Integration Test
- [ ] Shore station on bank, boat in water.
- [ ] Drive boat 50m out → verify video + controls working.
- [ ] Trigger E-STOP from phone → boat stops.
- [ ] Re-ARM → resume driving.
- [ ] Drive 200m+ out → check telemetry + minimap on phone.
- **Pass criteria:** All systems nominal at 200m range.

### T3.4 RTH Water Test
- [ ] ARM, set Home near shore.
- [ ] Drive boat 100m out.
- [ ] Power off Shore ESP32.
- [ ] Observe: Boat should autonomously return toward Home.
- **Pass criteria:** Boat arrives within 5m of Home point.
