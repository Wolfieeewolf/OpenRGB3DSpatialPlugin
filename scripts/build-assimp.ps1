# Build a slim static Assimp into dependencies/assimp-install.
# Formats: OBJ, STL, 3MF, glTF/GLB (common maker + modern interchange).
# Usage (Developer PowerShell recommended):
#   powershell -File scripts/build-assimp.ps1

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$AssimpSrc = Join-Path $Root "dependencies\assimp"
$BuildDir = Join-Path $Root "dependencies\assimp-build"
$InstallDir = Join-Path $Root "dependencies\assimp-install"
$CmakeCandidates = @(
    (Join-Path $Root "dependencies\cmake\bin\cmake.exe"),
    "${env:ProgramFiles}\CMake\bin\cmake.exe",
    "cmake"
)

if (-not (Test-Path (Join-Path $AssimpSrc "CMakeLists.txt"))) {
    Write-Error "Assimp sources missing at $AssimpSrc (git submodule update --init dependencies/assimp)"
}

$Cmake = $null
foreach ($c in $CmakeCandidates) {
    if ($c -eq "cmake") {
        $cmd = Get-Command cmake -ErrorAction SilentlyContinue
        if ($cmd) { $Cmake = $cmd.Source; break }
    } elseif (Test-Path $c) {
        $Cmake = $c
        break
    }
}
if (-not $Cmake) {
    Write-Error "cmake not found. Install CMake or extract it under dependencies/cmake"
}

if (Test-Path $BuildDir) { Remove-Item $BuildDir -Recurse -Force }
if (Test-Path $InstallDir) { Remove-Item $InstallDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

$generatorArgs = @()
if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $generatorArgs = @("-G", "Ninja")
}

& $Cmake -S $AssimpSrc -B $BuildDir @generatorArgs `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_INSTALL_PREFIX="$InstallDir" `
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

& $Cmake --build $BuildDir --config Release --parallel
& $Cmake --install $BuildDir --config Release

Write-Host "Slim Assimp (OBJ/STL/3MF/GLTF) installed to $InstallDir"
Get-ChildItem $InstallDir -Recurse -Include *.lib,*.dll,*.a | ForEach-Object {
    "{0}  ({1:N0} bytes)" -f $_.FullName, $_.Length
}
