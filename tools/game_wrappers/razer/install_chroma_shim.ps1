# Install OpenRGB Chroma proxy next to a game executable (Artemis-style per-game deploy).
# Always installs as RzChromaSDK*.dll (the LoadLibrary name Colore / UnityNative / CChromaEditor use).
# Also replaces any leftover RazerChromaSDK*.dll beside the exe.
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
        default { throw ("Unknown PE machine type 0x{0:X}" -f $machine) }
    }
}

function Install-One([string]$from, [string]$toPath) {
    $name = Split-Path -Leaf $toPath
    if (Test-Path $toPath) {
        $backup = "$toPath.openrgb.bak"
        if (-not (Test-Path $backup)) {
            Copy-Item $toPath $backup -Force
            Write-Host "Backed up existing $name -> $(Split-Path -Leaf $backup)"
        }
    }
    try {
        Copy-Item $from $toPath -Force
        Write-Host "  to   $toPath"
    } catch {
        Write-Warning "Could not write $toPath (game running?): $($_.Exception.Message)"
    }
}

$arch = Get-PeArchitecture $GameExePath
$primaryName = if ($arch -eq "x64") { "RzChromaSDK64.dll" } else { "RzChromaSDK.dll" }

if (-not $ShimDllPath) {
    $binCandidates = @()
    if ($arch -eq "x86") {
        $binCandidates += @(
            (Join-Path $PSScriptRoot "chroma_shim\build32\bin\Release"),
            (Join-Path $PSScriptRoot "chroma_shim\build32\bin"),
            (Join-Path $PSScriptRoot "chroma_shim\build_win32\bin\Release")
        )
    }
    $binCandidates += @(
        (Join-Path $PSScriptRoot "chroma_shim\build\bin\Release"),
        (Join-Path $PSScriptRoot "chroma_shim\build\bin"),
        (Join-Path $PSScriptRoot "chroma_shim\build\Release"),
        (Join-Path $PSScriptRoot "chroma_shim\build")
    )
    $builtNames = if ($arch -eq "x64") {
        @("RzChromaSDK64.dll", "chroma_shim.dll")
    } else {
        @("RzChromaSDK.dll", "chroma_shim.dll")
    }
    foreach ($binDir in $binCandidates) {
        if (-not (Test-Path $binDir)) { continue }
        foreach ($name in $builtNames) {
            $candidate = Join-Path $binDir $name
            if (Test-Path $candidate) {
                $ShimDllPath = $candidate
                break
            }
        }
        if ($ShimDllPath) { break }
    }
}

if (-not $ShimDllPath -or -not (Test-Path $ShimDllPath)) {
    Write-Error "Shim not found for $arch. Build chroma_shim (cmake -A $(if ($arch -eq 'x64') { 'x64' } else { 'Win32' })), then re-run."
}

Write-Host "Installed Chroma shim ($arch):"
Write-Host "  from $ShimDllPath"
Install-One $ShimDllPath (Join-Path $gameDir $primaryName)

# Cover stale dual-name installs from older shims
$legacyName = if ($arch -eq "x64") { "RazerChromaSDK64.dll" } else { "RazerChromaSDK.dll" }
$legacyPath = Join-Path $gameDir $legacyName
if (Test-Path $legacyPath) {
    Install-One $ShimDllPath $legacyPath
}

$regPath = "HKLM:\SOFTWARE\WOW6432Node\Razer Chroma SDK"
$needReg = $true
if (Test-Path $regPath) {
    $en = (Get-ItemProperty $regPath -Name Enable -ErrorAction SilentlyContinue).Enable
    if ($en -eq 1) { $needReg = $false }
}
if ($needReg) {
    Write-Host "Chroma SDK needs HKLM registry Enable=1 (admin). Writing keys..."
    $regScript = Join-Path $env:TEMP "openrgb_gw_colore_reg.ps1"
    $regBody = @(
        "`$p = '$regPath'"
        "New-Item -Path `$p -Force | Out-Null"
        "New-ItemProperty -Path `$p -Name Enable -PropertyType DWord -Value 1 -Force | Out-Null"
        "New-ItemProperty -Path `$p -Name MajorVersion -PropertyType DWord -Value 1 -Force | Out-Null"
        "New-ItemProperty -Path `$p -Name MinorVersion -PropertyType DWord -Value 10 -Force | Out-Null"
        "New-ItemProperty -Path `$p -Name RevisionNumber -PropertyType DWord -Value 6 -Force | Out-Null"
        "Write-Host 'Wrote Chroma SDK registry keys.'"
    ) -join "`r`n"
    Set-Content -Path $regScript -Value $regBody -Encoding ASCII
    try {
        Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$regScript`"" -Wait | Out-Null
    } catch {
        Write-Warning "Could not elevate to write registry: $($_.Exception.Message)"
    }
} else {
    Write-Host "SDK registry already enabled at $regPath"
}

Write-Host "Relaunch the game with the OpenRGB plugin running (DeviceGrid on UDP :9877)."
