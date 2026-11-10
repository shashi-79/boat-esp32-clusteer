#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

PIO_CMD="pio"
if command -v tpio >/dev/null 2>&1; then
    PIO_CMD="tpio"
elif command -v pio >/dev/null 2>&1; then
    PIO_CMD="pio"
elif command -v platformio >/dev/null 2>&1; then
    PIO_CMD="platformio"
fi

echo "=========================================="
echo " Camera Streamer Flash Tool (ESP32-CAM)"
echo "=========================================="

BUILD_ONLY=false
PORT=""
MONITOR=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-only|-BuildOnly) BUILD_ONLY=true; shift ;;
        --port|-Port) PORT="$2"; shift 2 ;;
        --monitor|-Monitor) MONITOR=true; shift ;;
        *) shift ;;
    esac
done

echo "[1/3] Building ESP32-CAM Firmware..."
$PIO_CMD run -e cam

if [ "$BUILD_ONLY" = true ]; then
    echo "[OK] Build completed successfully."
    exit 0
fi

echo "[2/3] Uploading to ESP32-CAM..."
echo "NOTE: Ensure IO0 is connected to GND and press RESET before flashing!"
UPLOAD_ARGS=("$PIO_CMD" "run" "-e" "cam" "-t" "upload")
if [ -n "$PORT" ]; then
    UPLOAD_ARGS+=("--upload-port" "$PORT")
fi

"${UPLOAD_ARGS[@]}"

echo "[2/3] Remove IO0 jumper from GND and press RESET to boot normal mode!"

if [ "$MONITOR" = true ]; then
    echo "[3/3] Opening Serial Monitor (Ctrl+C to exit)..."
    MONITOR_ARGS=("$PIO_CMD" "device" "monitor" "-e" "cam")
    if [ -n "$PORT" ]; then
        MONITOR_ARGS+=("--port" "$PORT")
    fi
    "${MONITOR_ARGS[@]}"
else
    echo "[3/3] Flash complete."
fi
