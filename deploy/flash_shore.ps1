param (
    [string]$Port = "",
    [switch]$Monitor,
    [switch]$BuildOnly,
    [switch]$SkipUICompress
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Shore Station Flash Tool (ESP32-WROOM-32U)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Split-Path -Parent $scriptDir

Set-Location $rootDir

if (-not $SkipUICompress) {
    Write-Host "[1/4] Compressing Web UI to PROGMEM..." -ForegroundColor Yellow
    & "$scriptDir\compress_ui.ps1"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "UI compression failed."
        exit $LASTEXITCODE
    }
}

function Get-PioCommand {
    if (Get-Command pio -ErrorAction SilentlyContinue) { return "pio" }
    if (Get-Command platformio -ErrorAction SilentlyContinue) { return "platformio" }
    $pioPenv = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pio.exe"
    if (Test-Path $pioPenv -ErrorAction SilentlyContinue) { return $pioPenv }
    return "pio"
}

$pio = Get-PioCommand

Write-Host "[2/4] Building Shore Station Firmware..." -ForegroundColor Yellow
if ($BuildOnly) {
    & $pio run -e shore
    Write-Host "[OK] Build completed successfully." -ForegroundColor Green
    exit 0
} else {
    & $pio run -e shore
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "[3/4] Uploading to ESP32 WROOM-32U..." -ForegroundColor Yellow
$uploadArgs = @("run", "-e", "shore", "-t", "upload")
if ($Port -ne "") {
    $uploadArgs += @("--upload-port", $Port)
}

& $pio @uploadArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "[WARN] Upload failed." -ForegroundColor Red
    Write-Host "Tip: Hold the BOOT button (IO0) on the board while 'Connecting...' is shown." -ForegroundColor Yellow
    exit $LASTEXITCODE
}

Write-Host "[OK] Shore station firmware flashed successfully!" -ForegroundColor Green

if ($Monitor) {
    Write-Host "[4/4] Opening Serial Monitor (115200 baud)..." -ForegroundColor Yellow
    $monArgs = @("device", "monitor", "-e", "shore", "-b", "115200")
    if ($Port -ne "") {
        $monArgs += @("--port", $Port)
    }
    & $pio @monArgs
}
