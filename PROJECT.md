# Project: Boat ESP32 Cluster — Range-Based Adaptive Camera & Project Cleanup

## Architecture
- **boat-controller** (ESP32-WROOM-32U): Runs sensor integration, GPS Haversine distance-to-home calculation, IMU, battery telemetry, motor safety, SoftAP (`192.168.4.1`), and WebSocket server (`ws://192.168.4.1/ws` at 5Hz).
- **cam-streamer** (AI-Thinker ESP32-CAM): Wi-Fi client station (`192.168.4.2`), HTTP servers (Port 80 for `/control` and `/status`, Port 81 for `/stream`), FreeRTOS mutex synchronization, double-buffered PSRAM capture, and dynamic SCCB framesize/quality adjustment without stream termination.
- **web-ui** (Standalone HTML5/PWA GCS HUD): Connects to boat-controller via WebSocket (`lastTelemetry.hd`) and cam-streamer via HTTP MJPEG (`http://192.168.4.2:81/stream`) and REST API (`http://192.168.4.2/control`). Implements 4-tier range-based adaptive resolution scaling, hysteresis deadband, rate limiting, HUD badges, and manual override controls.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | ESP32-S3 Cleanup | Remove stale ESP32-S3 flags and references from compile_commands.json, web-ui, and configs | M1 | Survey / R3 [DONE] |
| 2 | Obsolete Files Cleanup | Remove deprecated deploy batch files (.cmd) and dead cached libraries | M1 | Survey / R3 [DONE] |
| 3 | Tooling Compliance | Verify all PlatformIO operations use `tpio` wrapper and `./deploy/flash_all.sh --build-only` | M1 | Survey / R4 [DONE] |
| 4 | Safe Frame Buffer Allocation | Pre-allocate PSRAM buffers for worst-case frame size at boot to prevent reallocation crashes | M2 | Survey / R2 [DONE] |
| 5 | Non-blocking Stream Handler | Handle transient NULL frames during sensor PLL settling without terminating MJPEG stream | M2 | Survey / R2 [DONE] |
| 6 | Cam-Streamer REST Control API | Implement `/control` and `/status` endpoints with CORS on dedicated HTTP server (Port 80/81) | M2 | Survey / R2 [DONE] |
| 7 | Sensor Mutex Synchronization | Protect sensor parameter modifications and frame acquisitions with FreeRTOS mutex | M2 | Survey / R2 [DONE] |
| 8 | Range-Based Adaptive Scaling | Automatically scale resolution and compression quality based on GPS distance (`hd`) | M3 | Survey / R1 [DONE] |
| 9 | Hysteresis & Rate-Limiting | 2.5m deadband and 2.5s hold-off timer to prevent thrashing from GPS noise | M3 | Survey / R1 [DONE] |
| 10 | HUD UI Controls & Badges | Active resolution badge, dashboard readout, Auto/Manual toggle, manual selectors in settings | M3 | Survey / R1 [DONE] |
| 11 | Stream Auto-Reconnect | Silent automatic reconnection loop on `fpvStream.onerror` during transient switches | M3 | Survey / R1 [DONE] |
| 12 | End-to-End Verification & Audit | E2E validation of builds, scripts, telemetry-driven scaling, and forensic integrity audit | M4 | Acceptance Criteria [DONE] |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | M1: Legacy Cleanup & Tooling | Regenerate compile_commands.json via tpio, remove S3 references and obsolete files, verify tpio builds | none | DONE |
| 2 | M2: Cam-Streamer Dynamic Resolution | Implement safe framesize/quality switching, dual HTTP server, mutex synchronization, buffer draining | M1 | DONE |
| 3 | M3: Web-UI HUD Adaptive & Manual Controls | Add range-adaptive state machine, HUD UI badges, settings controls, auto-reconnect, REST calls | M2 | DONE |
| 4 | M4: E2E Integration & Verification | Full verification with tpio builds, script testing, UI verification, Challenger testing & Forensic Audit | M3 | DONE |

## Interface Contracts

### Web-UI ↔ Cam-Streamer REST API
- **Endpoint**: `GET http://192.168.4.2/control` (and port 81 fallback)
- **Parameters**:
  - `framesize`: integer (9 = SVGA 800x600, 8 = VGA 640x480, 6 = CIF 400x296, 5 = QVGA 320x240, 1 = QQVGA 160x120)
  - `quality`: integer (10 to 63, lower is higher quality)
  - Alternatively: `var=framesize&val=<num>` or `var=quality&val=<num>`
- **Response**: HTTP 200 OK
  - Headers: `Access-Control-Allow-Origin: *`, `Content-Type: application/json`
  - Body: `{"status":"ok","framesize":8,"quality":12}`
- **Endpoint**: `GET http://192.168.4.2/status`
  - Response: `{"status":"ok","framesize":8,"quality":12,"psram":true}`

### Boat-Controller ↔ Web-UI Telemetry
- **Endpoint**: `ws://192.168.4.1/ws`
- **Payload**: JSON packet broadcast at 5Hz containing `"hd"` (float: distance to home in meters, computed via Haversine), `"gpsok"` (boolean), and `"rssi"` (int8: connected station signal strength in dBm via `esp_wifi_ap_get_sta_list`).

### Range & Signal Adaptive Scaling State Matrix (Dual-Sensor Fusion)
- **Distance & Dynamic FPS Tiers**:
  - Tier 1: $0.0\text{ m} \le d < 18.0\text{ m}$ $\rightarrow$ VGA (640x480, framesize 8), Quality 12, Target 25 FPS
  - Tier 2: $18.0\text{ m} \le d < 38.0\text{ m}$ $\rightarrow$ CIF (400x296, framesize 6), Quality 14, Target 20 FPS
  - Tier 3: $38.0\text{ m} \le d < 55.0\text{ m}$ $\rightarrow$ QVGA (320x240, framesize 5), Quality 18, Target 15 FPS
  - Tier 4: $d \ge 55.0\text{ m}$ $\rightarrow$ QQVGA (160x120, framesize 1), Quality 24, Target 10 FPS
  - Distance Hysteresis: $\pm 2.5\text{ m}$ on transitions.
- **Signal Strength (RSSI) Tiers**:
  - Tier 1: $\text{RSSI} \ge -65.0\text{ dBm}$ (Strong RF link) $\rightarrow$ VGA, Q12, 25 FPS
  - Tier 2: $-73.0\text{ dBm} \le \text{RSSI} < -65.0\text{ dBm}$ (Good RF link) $\rightarrow$ CIF, Q14, 20 FPS
  - Tier 3: $-80.0\text{ dBm} \le \text{RSSI} < -73.0\text{ dBm}$ (Fair RF link) $\rightarrow$ QVGA, Q18, 15 FPS
  - Tier 4: $\text{RSSI} < -80.0\text{ dBm}$ (Weak / Fringe RF link) $\rightarrow$ QQVGA, Q24, 10 FPS
  - RSSI Hysteresis: $\pm 3.0\text{ dBm}$ on transitions.
- **Dynamic FPS Pacing & Live Reporting**:
  - `stream_handler` in ESP32-CAM computes dynamic task delay budgets based on `s_target_fps` instead of a static fixed delay, cutting bandwidth by >60% at long range.
  - Measures real-time transmitted FPS (`s_live_fps`) across a 1-second rolling window and reports it via `/status` and `/control`.
  - Web-UI HUD badge and telemetry display the real-time live FPS (`📶 -64dBm · VGA · Q12 · 25fps (AUTO)` and `#val-cam-fps`).
  - Settings drawer allows manual FPS slider adjustment (5–30 FPS) when in manual override mode.
- **Sensor Fusion Logic**:
  - Worst-case defense principle: $\max(\text{Tier}_{\text{dist}}, \text{Tier}_{\text{rssi}})$. Prevents video buffer starvation when GPS drifts or when obstacles attenuate the Wi-Fi link.
  - GPS Error / Degraded Fallback: When GPS fix is lost (`!gpsok` or satellite count < 6), the system automatically uses Signal Strength (RSSI) as the primary scaling metric instead of failing.
  - Rate Limiting Hold-Off: $2500\text{ ms}$ minimum between transitions.
  - Modes Available in UI Settings: `Auto (GPS + Signal Fusion)`, `Signal RSSI Only`, `GPS Range Only`, `Manual Override`.

## Code Layout
- `boat-controller/src/`: Boat controller firmware (main.cpp, gps.h, webserver.h, etc.)
- `cam-streamer/src/`: Camera streaming firmware (main.cpp)
- `web-ui/`: Ground control station web interface (index.html, service-worker.js, manifest.webmanifest)
- `deploy/`: Deployment and flashing scripts (flash_all.sh, flash_boat.sh, flash_cam.sh)
- `platformio.ini`: PlatformIO project configuration for `boat` and `cam` environments
- `compile_commands.json`: Compilation database generated via `tpio run -t compiledb`
