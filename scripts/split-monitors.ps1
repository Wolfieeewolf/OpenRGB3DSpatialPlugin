# Split ui/monitors.json into ui/monitors/*.json (one file per monitor)
$ErrorActionPreference = "Stop"
$monitorsJson = Join-Path $PSScriptRoot "..\ui\monitors.json"
$monitorsDir = Join-Path $PSScriptRoot "..\ui\monitors"
if (-not (Test-Path $monitorsJson)) {
    Write-Host "monitors.json not found: $monitorsJson"
    exit 1
}
$array = Get-Content $monitorsJson -Raw | ConvertFrom-Json
if (-not (Test-Path $monitorsDir)) {
    New-Item -ItemType Directory -Path $monitorsDir | Out-Null
}
foreach ($entry in $array) {
    $id = $entry.id
    if (-not $id) { continue }
    $path = Join-Path $monitorsDir "$id.json"
    $entry | ConvertTo-Json -Depth 4 | Set-Content $path -Encoding UTF8
}
Write-Host "Wrote $($array.Count) monitor files to $monitorsDir"
