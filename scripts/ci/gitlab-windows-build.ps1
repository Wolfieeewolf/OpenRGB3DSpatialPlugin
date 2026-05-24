# GitLab SaaS Windows build — install Qt (aqt), ensure MSVC, build plugin.
# Requires env: QT_AQT_VERSION, QT_AQT_ARCH, QT_BIN_DIR, QT_MSVC_BOOTSTRAP_PACKAGE (choco package name)

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

function Invoke-VcBuild([string]$vs, [string]$command) {
    $qtBin = $env:QT_BIN_DIR
    if (-not $qtBin) { throw 'QT_BIN_DIR is not set' }
    $batch = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && set PATH=$qtBin;%PATH% && $command"
    cmd /c $batch
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (-not $env:QT_AQT_VERSION -or -not $env:QT_AQT_ARCH -or -not $env:QT_BIN_DIR) {
    throw 'Set QT_AQT_VERSION, QT_AQT_ARCH, and QT_BIN_DIR for this job.'
}

if (-not (Test-Path (Join-Path $env:QT_BIN_DIR 'qmake.exe'))) {
    python -m pip install --quiet aqtinstall
    python -m aqt install-qt windows desktop $env:QT_AQT_VERSION $env:QT_AQT_ARCH -O C:\Qt
}

$vs = Ensure-MsvcToolchain
$env:VSINSTALLPATH = $vs

Invoke-VcBuild $vs 'qmake -v'
Invoke-VcBuild $vs 'qmake OpenRGB3DSpatialPlugin.pro CONFIG+=release'
Invoke-VcBuild $vs 'nmake'
