param(
    [string]$Version = "0.2.1",
    [string]$BuildDirectory = "switch"
)
$ErrorActionPreference = "Stop"
if ($Version -notmatch '^\d+\.\d+(\.\d+)?$') { throw "Invalid version" }
$repo = Split-Path -Parent $PSScriptRoot
$nro = Join-Path $repo "build/$BuildDirectory/NxNow.nro"
if (-not (Test-Path -LiteralPath $nro)) { throw "Missing NRO: $nro" }
& python "$repo/scripts/verify-nro-assets.py" $nro --resources "$repo/resources"
if ($LASTEXITCODE -ne 0) { throw "NRO verification failed" }
$destination = Join-Path (Split-Path -Parent $repo) "NxNow$Version"
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -LiteralPath $nro -Destination (Join-Path $destination "NxNow.nro") -Force
Write-Host "Packaged standalone NRO: $destination/NxNow.nro"
