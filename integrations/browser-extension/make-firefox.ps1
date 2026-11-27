# Builds the Firefox copy of the Movie Sync extension in dist/firefox: the same files, with the
# background script declared the way Firefox wants ("scripts" instead of "service_worker").
# Load dist/firefox/manifest.json from about:debugging -> This Firefox -> Load Temporary Add-on.
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$out = Join-Path $root 'dist\firefox'
if(Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item (Join-Path $root '*.js') $out
$manifest = Get-Content (Join-Path $root 'manifest.json') -Raw | ConvertFrom-Json
$overrides = Get-Content (Join-Path $root 'firefox\manifest-overrides.json') -Raw | ConvertFrom-Json
$manifest.background = $overrides.background
$manifest | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
Write-Host "Firefox build: $out"
