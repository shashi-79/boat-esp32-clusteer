param (
    [string]$Port = "",
    [switch]$Monitor,
    [switch]$BuildOnly
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Boat Controller Flash Tool (ESP32-S3)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

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

Write-Host "[1/3] Building Boat Firmware..." -ForegroundColor Yellow
if ($BuildOnly) {
    & $pio run -e boat
    Write-Host "[OK] Build completed successfully." -ForegroundColor Green
    exit 0
} else {
    & $pio run -e boat
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "[2/3] Uploading to ESP32-S3..." -ForegroundColor Yellow
$uploadArgs = @("run", "-e", "boat", "-t", "upload")
if ($Port -ne "") {
    $uploadArgs += @("--upload-port", $Port)
}

& $pio @uploadArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "[WARN] Upload failed." -ForegroundColor Red
    Write-Host "Tip: Hold the BOOT button on the ESP32-S3 board while plugging in USB, then re-run." -ForegroundColor Yellow
    exit $LASTEXITCODE
}

Write-Host "[OK] Boat firmware flashed successfully!" -ForegroundColor Green

if ($Monitor) {
    Write-Host "[3/3] Opening Serial Monitor (115200 baud)..." -ForegroundColor Yellow
    $monArgs = @("device", "monitor", "-e", "boat", "-b", "115200")
    if ($Port -ne "") {
        $monArgs += @("--port", $Port)
    }
    & $pio @monArgs
}
