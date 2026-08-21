$Dir = $PSScriptRoot
$Bin = Join-Path $Dir "bin\unlock"

function Die($msg) {
    Write-Host "(!) $msg" -ForegroundColor Red
    exit 1
}

Clear-Host

Write-Host "Amazon Fire TV Stick 4K 2nd Gen series unlock @ by R0rt1z2" -ForegroundColor Magenta
Write-Host ""

Write-Host "This procedure carries a real chance of bricking the device." -ForegroundColor Yellow
Write-Host "You accept that risk, the developer takes no responsibility." -ForegroundColor Yellow
Write-Host ""
Write-Host "Do you want to continue? [y/N] " -ForegroundColor White -NoNewline
$ans = Read-Host

if ($ans -notmatch '^(y|yes)$') { exit 1 }

if (-not (Test-Path $Bin)) { Die "missing $Bin" }

$found = Get-Command adb -ErrorAction SilentlyContinue
if ($found) {
    $Adb = $found.Source
} else {
    $Adb = Join-Path $Dir "bin\adb.exe"
    if (-not (Test-Path $Adb)) { Die "missing $Adb" }
}

Write-Host ""
Write-Host "Waiting for device..."
Write-Host ""

$state = $null
while (-not $state) {
    $out = & $Adb get-state 2>&1
    if ($LASTEXITCODE -eq 0) {
        $state = ($out | Select-Object -First 1).ToString().Trim()
    } else {
        Start-Sleep -Seconds 1
    }
}

if ($state -eq "recovery") {
    Write-Host "Device is in TWRP, it is already unlocked." -ForegroundColor Green
    exit 0
}

$id = & $Adb shell "su -c id" 2>&1
if ($id -notmatch 'uid=0') { Die "Please root your device with GhostLock first" }

Clear-Host

& $Adb push $Bin /data/local/tmp/unlock 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { Die "Failed to push unlock" }

& $Adb shell "chmod 755 /data/local/tmp/unlock" 2>&1 | Out-Null

& $Adb shell "su -c /data/local/tmp/unlock"

Write-Host "Done, device should reboot to TWRP." -ForegroundColor Green

