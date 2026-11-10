#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

echo "================================================="
echo " Rescue Boat Dual-ESP Deployment Tool"
echo "================================================="

BUILD_ONLY=false
BOAT_PORT=""
CAM_PORT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-only|-BuildOnly) BUILD_ONLY=true; shift ;;
        --boat-port) BOAT_PORT="$2"; shift 2 ;;
        --cam-port) CAM_PORT="$2"; shift 2 ;;
        *) shift ;;
    esac
done

if [ "$BUILD_ONLY" = true ]; then
    echo "--- Building Boat Firmware ---"
    "$SCRIPT_DIR/flash_boat.sh" --build-only
    echo "--- Building Camera Firmware ---"
    "$SCRIPT_DIR/flash_cam.sh" --build-only
    echo "[ALL BUILDS SUCCEEDED]"
    exit 0
fi

echo "[STEP 1] Flashing classic ESP32 sensor controller..."
if [ -n "$BOAT_PORT" ]; then
    "$SCRIPT_DIR/flash_boat.sh" --port "$BOAT_PORT"
else
    "$SCRIPT_DIR/flash_boat.sh"
fi

echo "[STEP 2] Flashing Camera Streamer (ESP32-CAM)..."
echo "Connect GPIO0 to GND on the ESP32-CAM before plugging in USB!"
if [ -n "$CAM_PORT" ]; then
    "$SCRIPT_DIR/flash_cam.sh" --port "$CAM_PORT"
else
    "$SCRIPT_DIR/flash_cam.sh"
fi

echo "================================================="
echo " Dual-ESP Flash Complete!"
echo " 1. Remove Camera GPIO0-to-GND jumper"
echo " 2. Power on both boards"
echo " 3. Connect phone to 'RescueBoat-GCS'"
echo " 4. Open HUD: python -m http.server 8000 --directory web-ui"
echo "================================================="
