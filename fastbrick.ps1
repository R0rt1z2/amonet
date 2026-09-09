param()

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$FASTBOOT = Join-Path $ScriptDir "bin\fastboot.exe"
if (-not (Test-Path $FASTBOOT)) {
    $SystemFastboot = Get-Command fastboot -ErrorAction SilentlyContinue
    if ($SystemFastboot) {
        $FASTBOOT = $SystemFastboot.Source
    } else {
        Write-Host "Error: no fastboot binary found." -ForegroundColor Red
        Write-Host "Install fastboot, or place fastboot.exe in bin\ next to this script." -ForegroundColor Red
        exit 1
    }
}

$ProfilePath = Join-Path $ScriptDir "profile.ps1"
if (-not (Test-Path $ProfilePath)) {
    Write-Host "Error: profile.ps1 not found next to this script." -ForegroundColor Red
    Write-Host "The package is incomplete - re-extract it and try again." -ForegroundColor Red
    exit 1
}

. $ProfilePath

$DeviceMap = @{
    "BISCUIT" = "Echo Dot 2nd Generation - 2016"
}

function Run-Fastboot($arguments, $timeoutSeconds = 0) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FASTBOOT
    $psi.Arguments = $arguments
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    $process.Start() | Out-Null

    if ($timeoutSeconds -gt 0) {
        $exited = $process.WaitForExit($timeoutSeconds * 1000)
        if (-not $exited) {
            $process.Kill() | Out-Null
            return @{ Output = ""; TimedOut = $true }
        }
    } else {
        $process.WaitForExit()
    }

    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()

    return @{ Output = $stdout + $stderr; TimedOut = $false }
}

# The payload itself rejects the wrong device ("Device mismatch"), so this only
# waits for something to show up - without it the first getvar blocks silently.
function Wait-ForDevice {
    Write-Host "Make sure device is in fastboot mode and connected!" -ForegroundColor Yellow
    Write-Host ""

    do {
        $result = Run-Fastboot "getvar product"
        $output = $result.Output

        if ($output -match "waiting for any device" -or $output -match "< waiting for any device >") {
            Start-Sleep -Seconds 1
            continue
        }

        $productLine = $output | Select-String "product:"
        if (-not $productLine) {
            Start-Sleep -Seconds 1
            continue
        }

        $product = ($productLine.Line -split '\s+')[1]

        if ($DeviceMap.ContainsKey($product)) {
            $friendlyName = $DeviceMap[$product]
        } else {
            $friendlyName = "Unknown device"
        }

        Write-Host "Detected: $product - $friendlyName" -ForegroundColor Cyan
        return
    } while ($true)
}

function Check-Unlocked {
    $result = Run-Fastboot "getvar unlock_status"
    $unlockLine = $result.Output | Select-String "unlock_status:"

    if ($unlockLine) {
        $unlockStatus = ($unlockLine.Line -split '\s+')[1].Trim().ToLower()

        if ($unlockStatus -eq "true") {
            Write-Host ""
            Write-Host "Your bootloader is already unlocked!" -ForegroundColor Green
            Write-Host ""
            Write-Host "The amonet-fastbrick exploit cannot be run on a device that is" -ForegroundColor Green
            Write-Host "already unlocked. There is no need to run it again." -ForegroundColor Green
            Write-Host ""
            Write-Host "Press any key to exit..."
            $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
            exit 0
        }
    }
}

function Detect-Image {
    $image = $DefaultImage

    $result = Run-Fastboot "getvar lk_build_desc"
    $buildLine = $result.Output | Select-String "lk_build_desc:"

    if ($buildLine) {
        $buildDesc = ($buildLine.Line -split '\s+', 2)[1].Trim()
        $buildDesc = ($buildDesc -split '\r?\n')[0].Trim()
        Write-Host "LK build: $buildDesc" -ForegroundColor Cyan

        foreach ($build in $ImageMap.Keys) {
            if ($buildDesc.StartsWith($build, [System.StringComparison]::Ordinal)) {
                $image = $ImageMap[$build]
                break
            }
        }
    }

    $script:FULL_IMAGE = Join-Path $ScriptDir $image

    Write-Host "Will use payload: $(Split-Path $script:FULL_IMAGE -Leaf)" -ForegroundColor Cyan

    if (-not (Test-Path $script:FULL_IMAGE)) {
        Write-Host "Error: Payload file not found: $script:FULL_IMAGE" -ForegroundColor Red
        exit 1
    }
}

function Flash-Payload {
    Write-Host "Sending payload, this might take a few attempts..." -ForegroundColor Yellow
    $attempt = 1
    while ($true) {
        $result = Run-Fastboot "flash brick `"$($script:FULL_IMAGE)`"" 8

        if ($result.Output -match "eMMC-RO") {
            Write-Host ""
            Write-Host "eMMC is in permanent read-only mode!" -ForegroundColor Red
            Write-Host ""
            Write-Host "This device's storage has reached the end of its life and has" -ForegroundColor Red
            Write-Host "locked itself read-only. Writes report success but never persist," -ForegroundColor Red
            Write-Host "so the bootloader cannot be unlocked and nothing can be flashed." -ForegroundColor Red
            Write-Host ""

            if ($result.Output -match "boots=(\d+)") {
                $boots = [int]$matches[1]
                Write-Host "For reference, this device has booted $boots times." -ForegroundColor Yellow
                if ($boots -lt 100) {
                    Write-Host "Damn... only $boots boots and the flash already gave out." -ForegroundColor Yellow
                    Write-Host "You must've been seriously unlucky with this one." -ForegroundColor Yellow
                }
                Write-Host ""
            }

            Write-Host "This is a hardware failure - the eMMC chip would need to be" -ForegroundColor Red
            Write-Host "physically replaced. The device was not modified and is safe to" -ForegroundColor Red
            Write-Host "reboot." -ForegroundColor Red
            exit 1
        }

        if ($result.Output -match "Device mismatch") {
            Write-Host ""
            Write-Host "Device mismatch detected!" -ForegroundColor Red
            Write-Host ""
            Write-Host "The payload is not compatible with your device. You may be running" -ForegroundColor Red
            Write-Host "this script on an unsupported device or with the wrong payload." -ForegroundColor Red
            Write-Host ""
            Write-Host "Please verify your device and try again with the correct payload." -ForegroundColor Red
            exit 1
        }

        if ($result.TimedOut) {
            Write-Host "Exploit most likely successful!" -ForegroundColor Green
            return
        }

        Start-Sleep -Seconds 2
        $attempt++
    }
}

Clear-Host

Write-Host @"
                                   _
                                  | |
   __ _ _ __ ___   ___  _ __   ___| |_
  / _' | '_ ' _ \ / _ \| '_ \ / _ \ __|
 | (_| | | | | | | (_) | | | |  __/ |_  (-fastbrick)
  \__,_|_| |_| |_|\___/|_| |_|\___|\__|
            by k4y0z & r0rt1z2

"@ -ForegroundColor DarkMagenta

Wait-ForDevice
Check-Unlocked
Detect-Image

Write-Host ""
$confirmation = Read-Host 'Run amonet-fastbrick to unlock the bootloader? (Type "YES" to continue)'

if ($confirmation -eq "YES") {
    Write-Host ""
    Flash-Payload
    Write-Host ""
    Write-Host "Please wait until the process finishes and DO NOT INTERRUPT IT." -ForegroundColor Yellow
} else {
    Write-Host ""
    Write-Host "Aborting." -ForegroundColor Yellow
    exit 0
}

Write-Host ""
Write-Host "Press any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
