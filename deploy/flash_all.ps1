param (
    [string]$BoatPort = "",
    [string]$ShorePort = "",
    [switch]$BuildOnly
)

$ErrorActionPreference = "Stop"

Write-Host "=================================================" -ForegroundColor Cyan
Write-Host " Rescue Boat Dual Board Deployment Tool" -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Cyan

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

if ($BuildOnly) {
    Write-Host "--- Building Boat Firmware ---" -ForegroundColor Yellow
    & "$scriptDir\flash_boat.ps1" -BuildOnly
    Write-Host "--- Building Shore Firmware ---" -ForegroundColor Yellow
    & "$scriptDir\flash_shore.ps1" -BuildOnly
    Write-Host "[ALL BUILDS SUCCEEDED]" -ForegroundColor Green
    exit 0
}

Write-Host "This script will flash both the Boat ESP32-S3 and Shore ESP32." -ForegroundColor White

# Flash Boat
Write-Host "`n[STEP 1] Flashing Boat Controller (ESP32-S3)..." -ForegroundColor Yellow
if ($BoatPort -eq "") {
    $choice = Read-Host "Is the Boat board plugged in? (Y/N)"
    if ($choice -ne "Y" -and $choice -ne "y") {
        Write-Host "Plug in Boat board and re-run." -ForegroundColor Yellow
        exit 0
    }
    & "$scriptDir\flash_boat.ps1"
} else {
    & "$scriptDir\flash_boat.ps1" -Port $BoatPort
}

# Flash Shore
Write-Host "`n[STEP 2] Flashing Shore Station (ESP32 WROOM-32U)..." -ForegroundColor Yellow
if ($ShorePort -eq "") {
    $choice = Read-Host "Now plug in the Shore Station board. Ready to flash? (Y/N)"
    if ($choice -ne "Y" -and $choice -ne "y") {
        Write-Host "Plug in Shore board and re-run." -ForegroundColor Yellow
        exit 0
    }
    & "$scriptDir\flash_shore.ps1"
} else {
    & "$scriptDir\flash_shore.ps1" -Port $ShorePort
}

Write-Host "`n=================================================" -ForegroundColor Green
Write-Host " Both Firmwares Successfully Flashed!" -ForegroundColor Green
Write-Host " 1. Boat AP / Shore AP ready." -ForegroundColor Green
Write-Host " 2. Connect phone to WiFi: RescueBoat_Shore" -ForegroundColor Green
Write-Host " 3. Open browser: http://192.168.4.1" -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor Green
