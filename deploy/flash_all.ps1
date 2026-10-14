param (
    [string]$BoatPort = "",
    [string]$CamPort = "",
    [switch]$BuildOnly
)

$ErrorActionPreference = "Stop"

Write-Host "=================================================" -ForegroundColor Cyan
Write-Host " Rescue Boat Dual-ESP Deployment Tool" -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Cyan

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

if ($BuildOnly) {
    Write-Host "--- Building Boat Firmware ---" -ForegroundColor Yellow
    & "$scriptDir\flash_boat.ps1" -BuildOnly
    Write-Host "--- Building Camera Firmware ---" -ForegroundColor Yellow
    & "$scriptDir\flash_cam.ps1" -BuildOnly
    Write-Host "[ALL BUILDS SUCCEEDED]" -ForegroundColor Green
    exit 0
}

Write-Host "This script flashes both the Boat Controller (ESP32-S3) and Camera (ESP32-CAM)." -ForegroundColor White

# Flash Boat S3
Write-Host "`n[STEP 1] Flashing Boat Controller (ESP32-S3)..." -ForegroundColor Yellow
if ($BoatPort -eq "") {
    $choice = Read-Host "Is the Boat ESP32-S3 board plugged in? (Y/N)"
    if ($choice -ne "Y" -and $choice -ne "y") {
        Write-Host "Plug in Boat board and re-run." -ForegroundColor Yellow
        exit 0
    }
    & "$scriptDir\flash_boat.ps1"
} else {
    & "$scriptDir\flash_boat.ps1" -Port $BoatPort
}

# Flash ESP32-CAM
Write-Host "`n[STEP 2] Flashing Camera Streamer (ESP32-CAM)..." -ForegroundColor Yellow
Write-Host "Connect GPIO0 to GND on the ESP32-CAM before plugging in USB!" -ForegroundColor Magenta
if ($CamPort -eq "") {
    $choice = Read-Host "Is the ESP32-CAM connected with GPIO0 grounded? (Y/N)"
    if ($choice -ne "Y" -and $choice -ne "y") {
        Write-Host "Prepare ESP32-CAM and re-run." -ForegroundColor Yellow
        exit 0
    }
    & "$scriptDir\flash_cam.ps1"
} else {
    & "$scriptDir\flash_cam.ps1" -Port $CamPort
}

Write-Host "`n=================================================" -ForegroundColor Green
Write-Host " Both Boards Successfully Flashed!" -ForegroundColor Green
Write-Host " 1. Power on boat electronics (S3 + ESP32-CAM)." -ForegroundColor Green
Write-Host " 2. Connect phone to WiFi: RescueBoat-GCS (pass: rescue2026)" -ForegroundColor Green
Write-Host " 3. Open browser: http://192.168.4.1" -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor Green
