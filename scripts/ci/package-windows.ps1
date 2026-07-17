# Package Windows plugin DLL as a zip under dist/.
# Env: QT_MAJOR (default 6), optional DIST_VARIANT (e.g. Windows_64)

$ErrorActionPreference = 'Stop'
$Root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $Root

$QtMajor = if ($env:QT_MAJOR) { $env:QT_MAJOR } else { '6' }
$Variant = if ($env:DIST_VARIANT) { $env:DIST_VARIANT } else { 'Windows_64' }

$DllName = 'OpenRGB3DSpatialPlugin.dll'
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

Requires a Qt6 build of OpenRGB.
Restart OpenRGB after installing the plugin.
"@ | Set-Content -Path (Join-Path $Stage 'INSTALL.txt') -Encoding UTF8

$DistDir = Join-Path $Root 'dist'
New-Item -ItemType Directory -Path $DistDir -Force | Out-Null
$ZipPath = Join-Path $DistDir "OpenRGB3DSpatialPlugin_${Variant}.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath $ZipPath

Remove-Item $Stage -Recurse -Force
Write-Host "Packaged: $ZipPath"
