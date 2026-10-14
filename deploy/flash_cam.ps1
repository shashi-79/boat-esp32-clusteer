param (
    [string]$Port = "",
    [switch]$Monitor,
    [switch]$BuildOnly
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Camera Streamer Flash Tool (ESP32-CAM)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Split-Path -Parent $scriptDir

Set-Location $rootDir

function Get-PioCommand {
    if (Get-Command pio -ErrorAction SilentlyContinue) { return "pio" }
    if (Get-Command platformio -ErrorAction SilentlyContinue) { return "platformio" }
    $pioPenv = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pio.exe"
    if (Test-Path $pioPenv -ErrorAction SilentlyContinue) { return $pioPenv }
    return "pio"
}

$pio = Get-PioCommand

Write-Host "[1/3] Building ESP32-CAM Firmware..." -ForegroundColor Yellow
if ($BuildOnly) {
    & $pio run -e cam
    Write-Host "[OK] Build completed successfully." -ForegroundColor Green
    exit 0
} else {
    & $pio run -e cam
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "[2/3] Uploading to ESP32-CAM..." -ForegroundColor Yellow
Write-Host "NOTE: Ensure IO0 is connected to GND and press RESET before flashing!" -ForegroundColor Magenta
$uploadArgs = @("run", "-e", "cam", "-t", "upload")
if ($Port -ne "") {
    $uploadArgs += @("--upload-port", $Port)
}

& $pio @uploadArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "[WARN] Upload failed." -ForegroundColor Red
    Write-Host "Tip: Connect GPIO0 to GND, press the RST button on the ESP32-CAM, and retry." -ForegroundColor Yellow
    exit $LASTEXITCODE
}

Write-Host "[OK] ESP32-CAM firmware flashed successfully!" -ForegroundColor Green
Write-Host "Remember: Disconnect GPIO0 from GND and press RESET to boot into run mode!" -ForegroundColor Magenta

if ($Monitor) {
    Write-Host "[3/3] Opening Serial Monitor (115200 baud)..." -ForegroundColor Yellow
    $monArgs = @("device", "monitor", "-e", "cam", "-b", "115200")
    if ($Port -ne "") {
        $monArgs += @("--port", $Port)
    }
    & $pio @monArgs
}
