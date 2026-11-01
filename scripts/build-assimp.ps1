# Build a slim static Assimp (OBJ / STL / 3MF / glTF).
# Default dirs: dependencies/assimp-build + dependencies/assimp-install.
# Override with env ASSIMP_BUILD_DIR / ASSIMP_INSTALL_DIR (qmake uses OUT_PWD/_deps).
# Usage:
#   powershell -File scripts/build-assimp.ps1
#   powershell -File scripts/build-assimp.ps1 -Clean
# Idempotent: skips when install\lib\assimp.lib already exists (unless -Clean).

param(
    [switch]$Clean,
    [string]$BuildDir = "",
    [string]$InstallDir = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$AssimpSrc = Join-Path $Root "dependencies\assimp"
if (-not $BuildDir) {
    $BuildDir = if ($env:ASSIMP_BUILD_DIR) { $env:ASSIMP_BUILD_DIR } else { Join-Path $Root "dependencies\assimp-build" }
}
if (-not $InstallDir) {
    $InstallDir = if ($env:ASSIMP_INSTALL_DIR) { $env:ASSIMP_INSTALL_DIR } else { Join-Path $Root "dependencies\assimp-install" }
}
$StableLib = Join-Path $InstallDir "lib\assimp.lib"

if (-not (Test-Path (Join-Path $AssimpSrc "CMakeLists.txt"))) {
    Write-Error "Assimp sources missing at $AssimpSrc — run: git submodule update --init dependencies/assimp"
}

function Invoke-VsWhere {
    param([string]$Property)
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }
    # Insiders / prerelease first (VS 2026), then stable.
    foreach ($pre in @($true, $false)) {
        $args = @("-latest", "-products", "*")
        if ($pre) { $args += "-prerelease" }
        $args += @("-property", $Property)
        $value = & $vswhere @args 2>$null
        if ($value) { return "$value" }
    }
    return $null
}

function Get-VsInstallPath { Invoke-VsWhere -Property "installationPath" }
function Get-VsProductLine { Invoke-VsWhere -Property "catalog_productLineVersion" }

function Find-Cmake {
    # Prefer newer CMake: Qt Maintenance Tool ships 3.30 which may not know VS 2026 yet.
    $candidates = [System.Collections.Generic.List[string]]::new()

    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { $candidates.Add($cmd.Source) }

    foreach ($c in @(
        (Join-Path $Root "dependencies\cmake\bin\cmake.exe"),
        "${env:ProgramFiles}\CMake\bin\cmake.exe"
    )) {
        if (Test-Path $c) { $candidates.Add($c) }
    }

    $vsInstall = Get-VsInstallPath
    if ($vsInstall) {
        $vsCmake = Join-Path $vsInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $vsCmake) { $candidates.Add($vsCmake) }
    }

    $qtRoots = @()
    if ($env:QTDIR) { $qtRoots += (Split-Path (Split-Path $env:QTDIR -Parent) -Parent) }
    if ($env:QT_ROOT) { $qtRoots += $env:QT_ROOT }
    $qtRoots += @("C:\Qt", "${env:USERPROFILE}\Qt", "${env:ProgramFiles}\Qt")
    foreach ($qr in $qtRoots) {
        if (-not $qr -or -not (Test-Path $qr)) { continue }
        Get-ChildItem -Path (Join-Path $qr "Tools") -Directory -Filter "CMake*" -ErrorAction SilentlyContinue | ForEach-Object {
            $exe = Join-Path $_.FullName "bin\cmake.exe"
            if (Test-Path $exe) { $candidates.Add($exe) }
        }
    }

    $productLine = Get-VsProductLine
    $neededGen = switch ($productLine) {
        "18" { "Visual Studio 18 2026" }
        "17" { "Visual Studio 17 2022" }
        "16" { "Visual Studio 16 2019" }
        default { $null }
    }

    $unique = $candidates | Select-Object -Unique
    if ($neededGen) {
        foreach ($exe in $unique) {
            $help = & $exe --help 2>&1 | Out-String
            if ($help -match [regex]::Escape($neededGen)) {
                return $exe
            }
        }
    }
    if ($unique.Count -gt 0) { return $unique[0] }
    return $null
}

$Cmake = Find-Cmake
if (-not $Cmake) {
    Write-Error @"
cmake not found. Install one of:
  - Qt Maintenance Tool → Build Tools → CMake
  - Visual Studio Installer → C++ CMake tools for Windows
  - Or put cmake on PATH
"@
}

if ($Clean) {
    if (Test-Path $BuildDir) { Remove-Item $BuildDir -Recurse -Force }
    if (Test-Path $InstallDir) { Remove-Item $InstallDir -Recurse -Force }
}

if ((Test-Path $StableLib) -and -not $Clean) {
    Write-Host "Assimp already installed: $StableLib"
    exit 0
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

function Invoke-CmakeChecked {
    & $Cmake @args
    if ($LASTEXITCODE -ne 0) {
        Write-Error "cmake failed (exit $LASTEXITCODE): $Cmake $($args -join ' ')"
    }
}

# Prefer the Visual Studio generator so Assimp builds without nmake/vcvars in PATH
# (Qt Creator and plain shells). Use Ninja only when cl.exe is already available.
$generatorArgs = @()
$hasCl = [bool](Get-Command cl -ErrorAction SilentlyContinue)
$hasNinja = [bool](Get-Command ninja -ErrorAction SilentlyContinue)
if ($hasNinja -and $hasCl) {
    $generatorArgs = @("-G", "Ninja")
} else {
    $productLine = Get-VsProductLine
    $vsGenerator = switch ("$productLine") {
        "18" { "Visual Studio 18 2026" }
        "17" { "Visual Studio 17 2022" }
        "16" { "Visual Studio 16 2019" }
        "15" { "Visual Studio 15 2017" }
        default { "Visual Studio 17 2022" }
    }
    $generatorArgs = @("-G", $vsGenerator, "-A", "x64")
}

Invoke-CmakeChecked -S $AssimpSrc -B $BuildDir @generatorArgs `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_INSTALL_PREFIX="$InstallDir" `
    -DCMAKE_INSTALL_LIBDIR=lib `
    -DASSIMP_BUILD_TESTS=OFF `
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF `
    -DASSIMP_BUILD_SAMPLES=OFF `
    -DASSIMP_INSTALL_PDB=OFF `
    -DBUILD_SHARED_LIBS=OFF `
    -DASSIMP_BUILD_ZLIB=ON `
    -DASSIMP_NO_EXPORT=ON `
    -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF `
    -DASSIMP_BUILD_OBJ_IMPORTER=ON `
    -DASSIMP_BUILD_STL_IMPORTER=ON `
    -DASSIMP_BUILD_3MF_IMPORTER=ON `
    -DASSIMP_BUILD_GLTF_IMPORTER=ON `
    -DASSIMP_WARNINGS_AS_ERRORS=OFF `
    -DASSIMP_INJECT_DEBUG_POSTFIX=OFF

Invoke-CmakeChecked --build $BuildDir --config Release --parallel
Invoke-CmakeChecked --install $BuildDir --config Release

$libDir = Join-Path $InstallDir "lib"
New-Item -ItemType Directory -Force -Path $libDir | Out-Null
if (-not (Test-Path $StableLib)) {
    $candidate = Get-ChildItem $libDir -Filter "assimp-vc*-mt.lib" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $candidate) {
        $candidate = Get-ChildItem $libDir -Filter "assimp*.lib" -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ne "assimp.lib" } |
            Select-Object -First 1
    }
    if ($candidate) {
        Copy-Item $candidate.FullName $StableLib -Force
        Write-Host "Stabilized link name: $($candidate.Name) → assimp.lib"
    }
}

if (-not (Test-Path $StableLib)) {
    Write-Error "Assimp install finished but $StableLib is missing"
}

Write-Host "Slim Assimp (OBJ/STL/3MF/GLTF) installed to $InstallDir"
Get-ChildItem $libDir -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in ".lib", ".a" } | ForEach-Object {
    "{0}  ({1:N0} bytes)" -f $_.FullName, $_.Length
}
