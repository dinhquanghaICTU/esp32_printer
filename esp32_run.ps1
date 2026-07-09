param(
    [string]$Port = "COM10",
    [int]$Baud = 115200,
    [string]$IdfPath = "C:\esp\v6.0.2\esp-idf",
    [switch]$NoMonitor,
    [switch]$Flash,
    [switch]$Erase,
    [switch]$InIdfEnv
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $IdfPath)) {
    Write-Error "ESP-IDF path not found: $IdfPath"
}

if (-not $InIdfEnv) {
    $argsList = @(
        "-ExecutionPolicy", "Bypass",
        "-NoProfile",
        "-File", $PSCommandPath,
        "-InIdfEnv",
        "-Port", $Port,
        "-Baud", $Baud,
        "-IdfPath", $IdfPath
    )

    if ($NoMonitor) {
        $argsList += "-NoMonitor"
    }

    if ($Flash) {
        $argsList += "-Flash"
    }

    & powershell.exe @argsList
    exit $LASTEXITCODE
}

Set-Location $PSScriptRoot

Write-Host "Loading ESP-IDF from $IdfPath" -ForegroundColor Cyan
$env:PATH = "C:\Users\mylap\AppData\Local\Programs\Python\Python313;C:\Users\mylap\AppData\Local\Programs\Python\Python313\Scripts;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0"
$env:IDF_PATH = $IdfPath
. "$IdfPath\export.ps1"


if ($Erase) {
    Write-Host "Erasing flash on $Port at $Baud..." -ForegroundColor Yellow
    idf.py -p $Port -b $Baud erase-flash
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    if (-not $Flash -and $NoMonitor) {
        exit 0
    }
}
Write-Host "Building ESP32 project..." -ForegroundColor Cyan
idf.py build
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($Flash) {
    Write-Host "Flashing to $Port at $Baud..." -ForegroundColor Cyan
    idf.py -p $Port -b $Baud flash
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not $NoMonitor) {
    Write-Host "Opening monitor on $Port... Press Ctrl+] to exit." -ForegroundColor Cyan
    idf.py -p $Port monitor
}




