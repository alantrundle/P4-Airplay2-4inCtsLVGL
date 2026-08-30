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
function Write-Utf8NoBom {
    param([string]$Path, [string]$Text)
    $enc = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $enc)
}
if (!$ProjectRoot) { $ProjectRoot = Find-ProjectRoot $PSScriptRoot }
if (!$ProjectRoot) { throw "Could not locate project root." }

$shim = Join-Path $ProjectRoot "src\esp_hosted_sdio_shim"
if (Test-Path $shim) { Remove-Item $shim -Recurse -Force }

$pio = Join-Path $ProjectRoot "platformio.ini"
$t = [System.IO.File]::ReadAllText($pio).TrimStart([char]0xFEFF)
$t = [regex]::Replace($t, "(?m)^\s*@.*hosted-includes\.rsp\s*\r?\n", "")
$t = [regex]::Replace($t, "(?ms)^; BEGIN ESP_HOSTED_SDIO_SHIM_V47.*?^; END ESP_HOSTED_SDIO_SHIM_V47\s*\r?\n?", "")
Write-Utf8NoBom $pio $t

Write-Host "Removed project SDIO shim. Clean the build directory before rebuilding." -ForegroundColor Green

$t = [regex]::Replace($t, "(?ms)^; BEGIN ESP_HOSTED_SDIO_INCLUDE_FLAGS_V47.*?^; END ESP_HOSTED_SDIO_INCLUDE_FLAGS_V47\s*\r?\n?", "")
Write-Utf8NoBom $pio $t
