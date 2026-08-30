param(
    [string]$ProjectRoot = "",
    [string]$HostedSource = ""
)

$ErrorActionPreference = "Stop"

function Find-ProjectRoot {
    param([string]$Start)
    $p = (Resolve-Path $Start).Path
    while ($true) {
        if (Test-Path (Join-Path $p "platformio.ini")) { return $p }
        $parent = Split-Path $p -Parent
        if (!$parent -or $parent -eq $p) { break }
        $p = $parent
    }
    return $null
}

function Test-HostedRoot {
    param([string]$Root)
    if ([string]::IsNullOrWhiteSpace($Root)) { return $false }
    return (Test-Path (Join-Path $Root "host\drivers\transport\sdio\sdio_drv.h"))
}

function Find-HostedRoot {
    param([string]$Project)
    $projects = Split-Path $Project -Parent
    $candidates = @(
        (Join-Path $Project "components\esp_hosted_mcu"),
        (Join-Path $Project "managed_components\espressif__esp_hosted"),
        (Join-Path $projects "esp-hosted-mcu-main"),
        (Join-Path $projects "Airplay2Receiver\components\esp_hosted_mcu"),
        (Join-Path $projects "Airplay2Receiver\managed_components\espressif__esp_hosted")
    )
    foreach ($c in $candidates) {
        if (Test-HostedRoot $c) { return (Resolve-Path $c).Path }
    }

    Write-Host "Searching PlatformIO projects for ESP-Hosted source..." -ForegroundColor DarkGray
    $hit = Get-ChildItem $projects -Recurse -File -Filter "sdio_drv.h" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "\\host\\drivers\\transport\\sdio\\sdio_drv\.h$" } |
        Select-Object -First 1
    if ($hit) {
        $d = $hit.Directory
        1..4 | ForEach-Object { $d = $d.Parent }
        if ($d -and (Test-HostedRoot $d.FullName)) { return $d.FullName }
    }
    return $null
}

function Write-Utf8NoBom {
    param([string]$Path, [string]$Text)
    $enc = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $enc)
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Find-ProjectRoot $PSScriptRoot
}
if (!$ProjectRoot) { throw "Could not find project root containing platformio.ini." }
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

Write-Host "=== ESP-Hosted SDIO v47.5 shim installer ===" -ForegroundColor Cyan
Write-Host "Project: $ProjectRoot"

$patched = Join-Path $PSScriptRoot "patched\sdio_drv.c"
if (!(Test-Path $patched)) { throw "Missing patched driver: $patched" }

if (![string]::IsNullOrWhiteSpace($HostedSource)) {
    if (!(Test-HostedRoot $HostedSource)) {
        throw "HostedSource is not a valid ESP-Hosted component root: $HostedSource"
    }
    $HostedSource = (Resolve-Path $HostedSource).Path
} else {
    $HostedSource = Find-HostedRoot $ProjectRoot
}
if (!$HostedSource) { throw "Could not locate ESP-Hosted source tree." }
Write-Host "ESP-Hosted source: $HostedSource" -ForegroundColor Green

# CRITICAL: PlatformIO recursively compiles everything under src.
# Move any old shim backups OUT of src before building.
$backupRoot = Join-Path $ProjectRoot "sdio-shim-backups"
New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
Get-ChildItem (Join-Path $ProjectRoot "src") -Directory -Filter "esp_hosted_sdio_shim.backup-*" -ErrorAction SilentlyContinue |
    ForEach-Object {
        $dest = Join-Path $backupRoot $_.Name
        if (Test-Path $dest) { $dest += "-$(Get-Date -Format HHmmssfff)" }
        Move-Item $_.FullName $dest
        Write-Host "Moved old src backup out of build tree: $dest" -ForegroundColor Yellow
    }

$shim = Join-Path $ProjectRoot "src\esp_hosted_sdio_shim"
if (Test-Path $shim) { Remove-Item $shim -Recurse -Force }
New-Item -ItemType Directory -Force -Path $shim | Out-Null

# ONLY the replacement C source lives under src.
Copy-Item $patched (Join-Path $shim "sdio_drv.c") -Force

@"
CUSTOM_SDIO_SHIM=v47.5-rx-stability-20260827
HOSTED_SOURCE=$HostedSource
PROJECT_ROOT=$ProjectRoot
INSTALLED=$(Get-Date -Format o)
"@ | Set-Content (Join-Path $shim "SHIM_INSTALLED.txt") -Encoding ASCII

# Build include list against the external ESP-Hosted tree. The source tree is
# NOT copied under src, so PlatformIO will not try to compile the whole component.
$includeDirs = Get-ChildItem $HostedSource -Recurse -Directory -ErrorAction SilentlyContinue |
    Where-Object {
        $_.FullName -notmatch "\\(build|\.git|examples|slave)(\\|$)" -and
        (Get-ChildItem $_.FullName -File -Filter "*.h" -ErrorAction SilentlyContinue | Select-Object -First 1)
    } |
    ForEach-Object { $_.FullName } |
    Sort-Object -Unique

# Ensure the most important host dirs are first.
$priority = @(
    (Join-Path $HostedSource "host\drivers\transport\sdio"),
    (Join-Path $HostedSource "host\drivers\transport"),
    (Join-Path $HostedSource "host\drivers\serial"),
    (Join-Path $HostedSource "host\drivers\bt"),
    (Join-Path $HostedSource "host\drivers\power_save"),
    (Join-Path $HostedSource "host\api"),
    (Join-Path $HostedSource "host\include"),
    (Join-Path $HostedSource "common\mempool"),
    (Join-Path $HostedSource "common\utils")
) | Where-Object { Test-Path $_ }

$ordered = @()
foreach ($d in $priority) { if ($ordered -notcontains $d) { $ordered += $d } }
foreach ($d in $includeDirs) { if ($ordered -notcontains $d) { $ordered += $d } }

# Generate explicit -I build flags. We intentionally do NOT use a GCC @response
# file here: PlatformIO/SCons did not propagate that response file to the C
# compile command in the previous integration.
$includeFlags = @()
foreach ($d in $ordered) {
    $norm = ($d -replace '\\','/')
    $includeFlags += "    -I`"$norm`""
}

$pio = Join-Path $ProjectRoot "platformio.ini"
$pioText = [System.IO.File]::ReadAllText($pio).TrimStart([char]0xFEFF)

# Remove every previous v47 integration, including stale response-file lines.
$pioText = [regex]::Replace($pioText, "(?m)^\s*@.*hosted-includes\.rsp\s*\r?\n", "")
$pioText = [regex]::Replace($pioText, "(?ms)^; BEGIN ESP_HOSTED_SDIO_SHIM_V47.*?^; END ESP_HOSTED_SDIO_SHIM_V47\s*\r?\n?", "")
$pioText = [regex]::Replace($pioText, "(?ms)^; BEGIN ESP_HOSTED_SDIO_INCLUDE_FLAGS_V47.*?^; END ESP_HOSTED_SDIO_INCLUDE_FLAGS_V47\s*\r?\n?", "")

$envHeader = "[env:esp32-p4-wifi6]"
$envPos = $pioText.IndexOf($envHeader)
if ($envPos -lt 0) { throw "Could not find $envHeader in platformio.ini" }
$nextEnv = $pioText.IndexOf("`n[", $envPos + $envHeader.Length)
if ($nextEnv -lt 0) { $nextEnv = $pioText.Length }
$envBlock = $pioText.Substring($envPos, $nextEnv - $envPos)

# Append explicit include flags to the existing build_flags block.
if ($envBlock -notmatch "(?m)^build_flags\s*=") {
    $envBlock += "`r`nbuild_flags =`r`n"
}

$marker = @()
$marker += "; BEGIN ESP_HOSTED_SDIO_INCLUDE_FLAGS_V47"
$marker += "; v47.5 private ESP-Hosted include paths"
$marker += $includeFlags
$marker += "; END ESP_HOSTED_SDIO_INCLUDE_FLAGS_V47"
$markerText = ($marker -join "`r`n") + "`r`n"

# The marker itself sits inside the env block. Because continuation lines under
# build_flags must remain indented, insert the -I lines immediately before the
# marker as continuation lines and retain comments for uninstaller discovery.
$envBlock += "`r`n" + $markerText

$pioText = $pioText.Substring(0,$envPos) + $envBlock + $pioText.Substring($nextEnv)
Write-Utf8NoBom $pio $pioText

Write-Host ""
Write-Host "Installed replacement source:" -ForegroundColor Green
Write-Host "  $shim\sdio_drv.c"
Write-Host "ESP-Hosted private include directories added explicitly to platformio.ini."
Write-Host ""
Write-Host "No ESP-Hosted source tree is copied under src." -ForegroundColor Green
Write-Host "Old shim backups were moved outside src so PlatformIO cannot compile them." -ForegroundColor Green
Write-Host ""
Write-Host "Now clean build ONLY:" -ForegroundColor Cyan
Write-Host "  Remove-Item -Recurse -Force `"$ProjectRoot\.pio\build\esp32-p4-wifi6`" -ErrorAction SilentlyContinue"
Write-Host "  & `"$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe`" run -e esp32-p4-wifi6"
