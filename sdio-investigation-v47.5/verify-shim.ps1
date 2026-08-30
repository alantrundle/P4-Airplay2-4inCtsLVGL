param([string]$ProjectRoot = "")
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

if (!$ProjectRoot) { $ProjectRoot = Find-ProjectRoot $PSScriptRoot }
if (!$ProjectRoot) { throw "Could not locate project root." }

$map = Join-Path $ProjectRoot ".pio\build\esp32-p4-wifi6\firmware.map"
$marker = Join-Path $ProjectRoot "src\esp_hosted_sdio_shim\SHIM_INSTALLED.txt"

Write-Host "=== ESP-Hosted SDIO v47.5 verifier ===" -ForegroundColor Cyan
if (!(Test-Path $marker)) { throw "Shim marker missing." }
Get-Content $marker
if (!(Test-Path $map)) { throw "firmware.map not found. Complete a successful build first." }

$projectHits = Select-String -Path $map -Pattern "src[/\\]esp_hosted_sdio_shim[/\\]sdio_drv.c.o"
$stockHits = Select-String -Path $map -Pattern "libespressif__esp_hosted\.a\(sdio_drv\.c\.obj\)"

Write-Host ""
Write-Host "Project replacement references: $($projectHits.Count)"
$projectHits | Select-Object -First 15 | ForEach-Object { $_.Line }

Write-Host ""
Write-Host "Stock archive SDIO references: $($stockHits.Count)"
$stockHits | Select-Object -First 15 | ForEach-Object { $_.Line }

if (!$projectHits) {
    Write-Host "FAIL: project sdio_drv.c object was not linked." -ForegroundColor Red
    exit 3
}
if ($stockHits) {
    Write-Host "FAIL: stock sdio_drv.c.obj was also extracted. Do not flash." -ForegroundColor Red
    exit 2
}
Write-Host ""
Write-Host "PASS: custom SDIO replacement linked; stock SDIO object not extracted." -ForegroundColor Green
