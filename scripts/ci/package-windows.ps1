# Package Windows plugin DLL as a zip under dist/.
# Env: QT_MAJOR (5 or 6), optional DIST_VARIANT (e.g. Windows_64, Windows_64_Qt6)

$ErrorActionPreference = 'Stop'
$Root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $Root

$QtMajor = if ($env:QT_MAJOR) { $env:QT_MAJOR } else { '5' }
$Variant = if ($env:DIST_VARIANT) { $env:DIST_VARIANT } else {
    if ($QtMajor -eq '6') { 'Windows_64_Qt6' } else { 'Windows_64' }
}

$DllName = if ($QtMajor -eq '6') { 'OpenRGB3DSpatialPlugin-qt6.dll' } else { 'OpenRGB3DSpatialPlugin.dll' }
$SrcDll = Join-Path $Root 'release\OpenRGB3DSpatialPlugin.dll'
if (-not (Test-Path $SrcDll)) {
    throw "Build output not found: $SrcDll"
}

$Stage = Join-Path $env:TEMP ("orgb3d_pkg_" + [guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $Stage -Force | Out-Null
Copy-Item $SrcDll (Join-Path $Stage $DllName)

@"
OpenRGB 3D Spatial Plugin — Windows install
=========================================
Copy $($DllName) into the plugins folder next to OpenRGB.exe
(typically OpenRGB\plugins\ or the path shown in OpenRGB Settings → Plugins).

Use the Qt5 build with Qt5 OpenRGB; use the Qt6 (-qt6) build with Qt6 OpenRGB.
Restart OpenRGB after installing the plugin.
"@ | Set-Content -Path (Join-Path $Stage 'INSTALL.txt') -Encoding UTF8

$DistDir = Join-Path $Root 'dist'
New-Item -ItemType Directory -Path $DistDir -Force | Out-Null
$ZipPath = Join-Path $DistDir "OpenRGB3DSpatialPlugin_${Variant}.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath $ZipPath

Remove-Item $Stage -Recurse -Force
Write-Host "Packaged: $ZipPath"
