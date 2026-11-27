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
# UTF-8 without a byte-order mark (Windows PowerShell 5.1's -Encoding utf8 adds one).
[IO.File]::WriteAllText((Join-Path $out 'manifest.json'), ($manifest | ConvertTo-Json -Depth 10), (New-Object System.Text.UTF8Encoding($false)))
Write-Host "Firefox build: $out"
