# Rescue Boat — Test Plan (Dual-ESP Direct Wi-Fi)

## Phase 1: Bench Tests (No Water, No Motors)

### T1.1 SoftAP & Web GCS Link Test
- [ ] Power the classic ESP32 sensor controller through its CP2102-connected board.
- [ ] On smartphone, scan Wi-Fi networks → verify `RescueBoat-GCS` appears.
- [ ] Connect with password `rescue2026`.
- [ ] Serve `web-ui/` locally on the phone and open `http://127.0.0.1:8000`.
- [ ] Verify: Web GCS HUD loads with joysticks, battery gauges, and radar disc.
- [ ] Verify: Link badge turns green (**CONNECTED**).
- **Pass criteria:** Web UI loads within 2 seconds; WebSocket handshake succeeds.

### T1.2 ESP32-CAM Video Stream Test
- [ ] Power ESP32-CAM module (dedicated 5V BEC).
- [ ] Verify controller serial log shows camera client connected (`192.168.4.2`).
- [ ] In phone browser, check FPV video backdrop renders behind HUD.
- [ ] Tap **CAM: ON / OFF** in header → verify video hides and restores.
- **Pass criteria:** Live video stream displays smoothly with < 300ms latency.

### T1.3 Dual-Gyroscope Minimap Test
- [ ] Rotate boat board physically 360 degrees:
  - Verify: Green boat arrow in center of radar disc rotates smoothly to match compass.
  - Verify: Serial shows heading increasing 0° → 90° → 180° → 270° → 360°.
- [ ] Hold phone and rotate body 360 degrees:
  - Verify: Amber phone icon and 40° vision cone rotate to match phone's physical direction.
  - Verify: Phone heading stat (`Phone Gyro`) updates in real time.
- **Pass criteria:** Both boat and phone headings track physical movements independently without lag or drift.

### T1.4 Wi-Fi Disconnect Failsafe Test
- [ ] Connect phone, verify system is receiving packets.
- [ ] Disconnect phone Wi-Fi or walk out of range.
- [ ] Observe controller CP2102 serial monitor:
  - Within 1500ms: serial logs `Safety: SIGNAL LOST`.
  - Mode switches from `MODE_MANUAL` to `MODE_RTH` (or `MODE_FAILSAFE` if no GPS fix).
  - Motor outputs immediately command neutral (1500µs).
- [ ] Reconnect phone Wi-Fi → verify boat resumes normal communication.
- **Pass criteria:** Failsafe engages within 1.5 seconds of signal loss.

### T1.5 ESC PWM Output Test (No Propellers!)
- [ ] ESCs powered, propellers REMOVED.
- [ ] Drag throttle forward → both ESCs output > 1500µs.
- [ ] Drag throttle reverse → both ESCs output < 1500µs.
- [ ] Steer left/right → differential mixing modulates left and right outputs.
- [ ] Tap **E-STOP** on phone → both ESCs immediately lock to neutral (1500µs).
- **Pass criteria:** Differential mixing math correct, E-STOP instant.

### T1.6 Gimbal Servo Test
- [ ] Drag right joystick pan (X) sweep -1000 to +1000.
- [ ] Drag right joystick tilt (Y) sweep -1000 to +1000.
- [ ] Verify: Servos move smoothly across full range without jitter.
- **Pass criteria:** Smooth servo movement, camera centers when released or in RTH.

### T1.7 GPS Lock & Home Vector Test
- [ ] Place GPS antenna outdoors with clear sky view.
- [ ] Wait for `Fix: 3D` and `Sats >= 6`.
- [ ] Tap **ARM** button on phone:
  - Verify: Home coordinates are captured at current GPS position.
  - Verify: Home distance reads `0m`.
- [ ] Walk board 20m away → verify Home Distance reads ~20m and bearing points back to start.
- **Pass criteria:** 3D fix within 60s, Home waypoint captured on ARM.

### T1.8 24GHz mmWave Radar Obstacle Test
- [ ] Place cardboard box 2 meters in front of radar sensor.
- [ ] Verify: Radar stat shows `~2.0m` and a red obstacle blip renders on the minimap.
- [ ] Move box closer to 1m → verify distance decreases.
- [ ] Remove box → verify radar stat shows `CLEAR`.
- **Pass criteria:** Distance reads within ±0.3m.

### T1.9 INA219 Battery Monitor Test
- [ ] Power system from charged 3S LiPo pack (~12.4V).
- [ ] Check HUD battery panel:
  - Voltage reads accurate within ±0.05V (e.g., `12.4V`).
  - True percentage reflects LiPo curve (e.g., `92%`).
  - Current shows live motor draw in Amps.
- **Pass criteria:** Voltage, current, and non-linear percentage calculate accurately.

### T1.10 Sensor Disconnect and E-STOP Recovery

- [ ] Verify arming is rejected without a recent GPS 3D fix or a responding BNO055.
- [ ] While safely armed with propellers removed, disconnect GPS; verify motors go neutral and the boat disarms.
- [ ] Restore GPS; verify the operator must explicitly arm again.
- [ ] Trigger E-STOP, send disarm, then verify re-arming is possible only after normal sensor checks pass.
- [ ] Stop sending control packets; verify the 1.5-second watchdog selects RTH only while GPS and IMU remain healthy, otherwise neutral/disarm.
- [ ] Disconnect INA219 with the motor ADC divider connected; verify ADC voltage fallback. Disconnect both sensing paths and verify arming is rejected.
- [ ] Verify HUD sensor status reports GPS, IMU, battery, and radar health; unplug each and confirm its status changes.
- [ ] Connect a second browser; verify it is rejected while the active controller retains control.
- [ ] Send malformed and out-of-range control fields; verify they are rejected and the link watchdog expires normally if valid packets stop.
- [ ] Verify the camera recovers after camera initialization failure and after a Wi-Fi reconnect.
- [ ] Serve `web-ui/` from a local HTTP server on the phone, connect to the boat AP, and open the HUD from `127.0.0.1`.
- [ ] Verify the phone page connects to `ws://192.168.4.1/ws`, video loads directly from `192.168.4.2:81`, and the controller root returns only a short diagnostic message.

---

## Phase 2: Walking Range Tests (Outdoor, No Water)

### T2.1 Wi-Fi Boundary & Failsafe Check
- [ ] Classic ESP32 controller and separate ESP32-CAM powered on boat chassis.
- [ ] Walk away with smartphone in open area.
- [ ] Monitor HUD link badge and video stream:
  - Note distance where video frame rate drops.
  - Note distance where Wi-Fi disconnects (expected 40–70m).
- [ ] Verify boat enters RTH mode immediately upon disconnect.
- **Pass criteria:** Reliable control up to 40m+; instant RTH upon signal loss.

---

## Phase 3: Water Tests

### T3.1 Static Hull Sealing Test
- [ ] Hull sealed, all electronics mounted.
- [ ] Place boat in water for 20 minutes without driving.
- [ ] Inspect hull interior for any water ingress.
- **Pass criteria:** 100% dry interior.

### T3.2 Low-Speed Differential Drive Test
- [ ] Propellers attached, boat in water.
- [ ] Forward 25% → verify forward thrust.
- [ ] Reverse 25% → verify reverse thrust.
- [ ] Turn left / right → verify boat turns using differential thrust.
- **Pass criteria:** Responsive steering and neutral stopping.

### T3.3 Full Integration & Camera FPV Test
- [ ] Stand on shore, drive boat 20–30m out.
- [ ] Observe live FPV camera feed and dual-gyro minimap on phone screen.
- [ ] Trigger **E-STOP** from phone → verify boat stops immediately.
- [ ] Re-ARM → resume driving.
- **Pass criteria:** Video, controls, and telemetry operate concurrently.

### T3.4 Water RTH Test
- [ ] ARM boat near shore (Home point set).
- [ ] Drive boat 30m out into water.
- [ ] Turn off phone Wi-Fi (simulating signal loss).
- [ ] Observe boat: boat should turn autonomously toward Home point using compass PID and slow to a stop within 3m of shore.
- **Pass criteria:** Autonomous return within 3m of Home waypoint.
