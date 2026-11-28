# Builds the Movie Sync extension for every major browser, into dist/:
#   dist/chromium/                 Chrome, Edge, Brave, Opera, Vivaldi (load unpacked)
#   dist/firefox/                  Firefox 128+ (about:debugging -> Load Temporary Add-on)
#   dist/movie-sync-chromium-<v>.zip   for the Chrome Web Store / Edge Add-ons, or to share
#   dist/movie-sync-firefox-<v>.zip    for addons.mozilla.org (signing), or to share
# Firefox declares the background script as "scripts" instead of a service worker; nothing
# else differs. Run: powershell -ExecutionPolicy Bypass -File build.ps1
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$root = $PSScriptRoot
$dist = Join-Path $root 'dist'
$utf8 = New-Object System.Text.UTF8Encoding($false)
$manifest = Get-Content (Join-Path $root 'manifest.json') -Raw | ConvertFrom-Json
$version = $manifest.version

function Build-Target([string]$name, $m)
{
    $out = Join-Path $dist $name
    if(Test-Path $out) { Remove-Item $out -Recurse -Force }
    New-Item -ItemType Directory -Force $out | Out-Null
    Copy-Item (Join-Path $root '*.js') $out
    Copy-Item (Join-Path $root 'icons') (Join-Path $out 'icons') -Recurse
    [IO.File]::WriteAllText((Join-Path $out 'manifest.json'), ($m | ConvertTo-Json -Depth 10), $utf8)
    # Zip with plain names at the top level (stores and Firefox reject backslash paths).
    $zip = Join-Path $dist ("movie-sync-{0}-{1}.zip" -f $name, $version)
    if(Test-Path $zip) { Remove-Item $zip -Force }
    $archive = [IO.Compression.ZipFile]::Open($zip, 'Create')
    try
    {
        foreach($f in Get-ChildItem $out -File -Recurse)
        {
            $entry = $f.FullName.Substring($out.Length + 1).Replace('\', '/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $f.FullName, $entry) | Out-Null
        }
    }
    finally { $archive.Dispose() }
    Write-Host ("{0,-9} {1}`n          {2}" -f $name, $out, $zip)
}

New-Item -ItemType Directory -Force $dist | Out-Null

# Chromium: the manifest as it is, minus Firefox's own section (Chrome warns about it).
$chromium = Get-Content (Join-Path $root 'manifest.json') -Raw | ConvertFrom-Json
$chromium.PSObject.Properties.Remove('browser_specific_settings')
Build-Target 'chromium' $chromium

# Firefox: background as scripts.
$firefox = Get-Content (Join-Path $root 'manifest.json') -Raw | ConvertFrom-Json
$overrides = Get-Content (Join-Path $root 'manifest-firefox-overrides.json') -Raw | ConvertFrom-Json
$firefox.background = $overrides.background
Build-Target 'firefox' $firefox
