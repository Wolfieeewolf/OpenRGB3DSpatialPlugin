# GitLab SaaS Windows build — install Qt (aqt), ensure MSVC, build plugin.
# Requires env: QT_AQT_VERSION, QT_AQT_ARCH, QT_BIN_DIR, QT_MSVC_BOOTSTRAP_PACKAGE

$ErrorActionPreference = 'Stop'

function Get-VsInstallPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($path) { return $path.Trim() }
        $path = & $vswhere -latest -products * -property installationPath 2>$null
        if ($path) { return $path.Trim() }
    }
    $candidates = @(
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\BuildTools'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Community'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Enterprise'),
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2019\BuildTools'),
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2019\Community')
    )
    foreach ($root in $candidates) {
        if (Test-Path (Join-Path $root 'VC\Auxiliary\Build\vcvars64.bat')) {
            return $root
        }
    }
    return $null
}

function Ensure-MsvcToolchain {
    $vs = Get-VsInstallPath
    if ($vs) { return $vs }

    if (-not (Get-Command choco.exe -ErrorAction SilentlyContinue)) {
        throw 'Chocolatey is not available and MSVC was not found on this runner.'
    }

    $package = $env:QT_MSVC_BOOTSTRAP_PACKAGE
    if (-not $package) {
        throw 'QT_MSVC_BOOTSTRAP_PACKAGE is not set (e.g. visualstudio2019buildtools or visualstudio2022buildtools).'
    }

    Write-Host "Installing $package with C++ workload (first pipeline run may take a while)..."
    & choco.exe install $package -y --no-progress --execution-timeout 7200 `
        --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --quiet --norestart"

    $vs = Get-VsInstallPath
    if (-not $vs) {
        throw 'MSVC toolchain not found after Visual Studio Build Tools install.'
    }
    return $vs
}

if (-not $env:QT_AQT_VERSION -or -not $env:QT_AQT_ARCH -or -not $env:QT_BIN_DIR) {
    throw 'Set QT_AQT_VERSION, QT_AQT_ARCH, and QT_BIN_DIR for this job.'
}

if (-not (Test-Path (Join-Path $env:QT_BIN_DIR 'qmake.exe'))) {
    python -m pip install --quiet aqtinstall
    python -m aqt install-qt windows desktop $env:QT_AQT_VERSION $env:QT_AQT_ARCH -O C:\Qt
}

$vs = Ensure-MsvcToolchain
$qtBin = $env:QT_BIN_DIR
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'

# Run vcvars + qmake + nmake in ONE cmd session so cl.exe stays on PATH.
$batchFile = Join-Path $env:TEMP ("orgb3d_build_" + [guid]::NewGuid().ToString() + ".cmd")
@"
@echo off
call "$vcvars"
if errorlevel 1 exit /b 1
set "PATH=$qtBin;%PATH%"
where cl
if errorlevel 1 (
  echo cl.exe not found after vcvars
  exit /b 1
)
qmake -v
qmake OpenRGB3DSpatialPlugin.pro CONFIG+=release
if errorlevel 1 exit /b 1
nmake
if errorlevel 1 exit /b 1
"@ | Set-Content -Path $batchFile -Encoding ASCII

Write-Host "Running build batch: $batchFile"
cmd /c $batchFile
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Remove-Item $batchFile -Force -ErrorAction SilentlyContinue

$qtMajor = if ($env:QT_MAJOR) { $env:QT_MAJOR } else { '6' }
$env:QT_MAJOR = $qtMajor
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'package-windows.ps1')
