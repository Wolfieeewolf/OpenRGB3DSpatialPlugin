# Install OpenRGB Chroma proxy next to a game executable (Artemis-style per-game deploy).
# Aurora's Chroma UI instead installs the Razer SDK system-wide and reads via RazerSdkReader — different path.
param(
    [Parameter(Mandatory = $true)]
    [string]$GameExePath,
    [string]$ShimDllPath = ""
)

$ErrorActionPreference = "Stop"
$GameExePath = (Resolve-Path $GameExePath).Path
$gameDir = Split-Path -Parent $GameExePath

function Get-PeArchitecture([string]$path) {
    $bytes = [byte[]](Get-Content -Path $path -Encoding Byte -TotalCount 256)
    if ($bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) { throw "Not a PE file: $path" }
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    $fs = [System.IO.File]::OpenRead($path)
    try {
        $fs.Position = $peOffset + 4
        $buf = New-Object byte[] 2
        [void]$fs.Read($buf, 0, 2)
        $machine = [BitConverter]::ToUInt16($buf, 0)
    } finally { $fs.Close() }
    switch ($machine) {
        0x014c { return "x86" }
        0x8664 { return "x64" }
        default { throw "Unknown PE machine type 0x{0:X}" -f $machine }
    }
}

$arch = Get-PeArchitecture $GameExePath
$targetName = if ($arch -eq "x64") { "RazerChromaSDK64.dll" } else { "RazerChromaSDK.dll" }
$targetPath = Join-Path $gameDir $targetName

if (-not $ShimDllPath) {
    $binDir = Join-Path $PSScriptRoot "chroma_shim\build\bin\Release"
    if (-not (Test-Path $binDir)) {
        $binDir = Join-Path $PSScriptRoot "chroma_shim\build\bin"
    }
    $ShimDllPath = Join-Path $binDir $targetName
    if (-not (Test-Path $ShimDllPath)) {
        $ShimDllPath = Join-Path $binDir "RazerChromaSDK64.dll"
    }
}

if (-not (Test-Path $ShimDllPath)) {
    Write-Error "Shim not found. Build first: cmake -S tools/game_wrappers/chroma_shim -B tools/game_wrappers/chroma_shim/build -A Win32 (or x64) && cmake --build tools/game_wrappers/chroma_shim/build --config Release`nThen pass -ShimDllPath to this script."
}

if (Test-Path $targetPath) {
    $backup = "$targetPath.openrgb.bak"
    if (-not (Test-Path $backup)) {
        Copy-Item $targetPath $backup -Force
        Write-Host "Backed up existing $targetName -> $(Split-Path -Leaf $backup)"
    }
}

Copy-Item $ShimDllPath $targetPath -Force
Write-Host "Installed Chroma shim ($arch):"
Write-Host "  $targetPath"
Write-Host "Start OpenRGB with 3D Spatial plugin, add Game -> Game wrapper capture, then launch the game."
