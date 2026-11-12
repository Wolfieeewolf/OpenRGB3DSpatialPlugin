# Discover Steam library games that use Chroma and install the OpenRGB shim into each.
# Safe: only installs where Colore / Chroma editor / existing Rz|Razer Chroma DLL is present,
# or the folder name fuzzy-matches chroma_workshop_games.txt AND a primary exe exists.
param(
    [string[]]$SteamRoots = @(),
    [string]$WorkshopList = "",
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"
$installOne = Join-Path $PSScriptRoot "install_chroma_shim.ps1"
if (-not (Test-Path $installOne)) {
    throw "Missing install_chroma_shim.ps1 next to this script."
}

if (-not $WorkshopList) {
    $WorkshopList = Join-Path $PSScriptRoot "chroma_workshop_games.txt"
}
$workshopNames = @()
if (Test-Path $WorkshopList) {
    $workshopNames = Get-Content $WorkshopList |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith("#") }
}

function Normalize-Name([string]$s) {
    ($s.ToLowerInvariant() -replace "[^a-z0-9]+", "")
}

$workshopNorm = @{}
foreach ($n in $workshopNames) {
    $workshopNorm[(Normalize-Name $n)] = $n
}

function Get-SteamLibraryRoots {
    $roots = New-Object System.Collections.Generic.List[string]
    $candidates = @(
        "D:\Steam",
        "C:\Program Files (x86)\Steam",
        "C:\Program Files\Steam",
        "$env:ProgramFiles(x86)\Steam",
        "$env:ProgramFiles\Steam"
    )
    foreach ($r in $SteamRoots) { $candidates += $r }
    foreach ($steam in ($candidates | Select-Object -Unique)) {
        if (-not (Test-Path $steam)) { continue }
        $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
        if (Test-Path $vdf) {
            $text = Get-Content $vdf -Raw -ErrorAction SilentlyContinue
            if ($text) {
                [regex]::Matches($text, '"path"\s+"([^"]+)"') | ForEach-Object {
                    $p = $_.Groups[1].Value -replace "\\\\", "\"
                    if (Test-Path $p) { [void]$roots.Add($p) }
                }
            }
        }
        if (Test-Path (Join-Path $steam "steamapps\common")) {
            [void]$roots.Add($steam)
        }
    }
    return $roots | Select-Object -Unique
}

function Test-ChromaMarkers([string]$dir) {
    $markers = @(
        "Corale.Colore.dll",
        "CChromaEditorLibrary.dll",
        "CChromaEditorLibrary64.dll",
        "UnityNativeChromaSDK.dll",
        "UnityNativeChromaSDK3.dll",
        "RzChromaSDK64.dll",
        "RzChromaSDK.dll",
        "RazerChromaSDK64.dll",
        "RazerChromaSDK.dll"
    )
    foreach ($m in $markers) {
        if (Get-ChildItem -Path $dir -Recurse -Filter $m -ErrorAction SilentlyContinue | Select-Object -First 1) {
            return $true
        }
    }
    return $false
}

function Find-PrimaryExe([string]$dir, [string]$folderName) {
    $preferred = @(
        "$folderName.exe",
        ($folderName -replace "\s+", "") + ".exe",
        "DOOMx64vk.exe",
        "DOOMx64.exe",
        "DubWars.exe",
        "AssettoCorsa.exe",
        "acs.exe",
        "Cat Quest II.exe",
        "Cat Quest.exe"
    )
    foreach ($name in $preferred) {
        $p = Join-Path $dir $name
        if (Test-Path $p) { return $p }
    }
    $exes = Get-ChildItem -Path $dir -File -Filter "*.exe" -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -notmatch "(?i)(unins|crash|redist|vcredist|dxsetup|unitycrash|crashpad|installer|setup|launcher_helper|content manager|cm_)"
        } |
        Sort-Object Length -Descending
    if ($exes) { return $exes[0].FullName }
    return $null
}

$roots = Get-SteamLibraryRoots
if (-not $roots) {
    Write-Error "No Steam libraries found. Pass -SteamRoots 'D:\Steam' (etc)."
}

Write-Host "Steam libraries:"
$roots | ForEach-Object { Write-Host "  $_" }

$targets = New-Object System.Collections.Generic.List[object]
foreach ($root in $roots) {
    $common = Join-Path $root "steamapps\common"
    if (-not (Test-Path $common)) { continue }
    Get-ChildItem $common -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        $folder = $_
        $norm = Normalize-Name $folder.Name
        $workshopExact = $workshopNorm.ContainsKey($norm)
        $workshopSoft = $false
        if (-not $workshopExact) {
            foreach ($key in $workshopNorm.Keys) {
                if ($key.Length -ge 6 -and $norm.Length -ge 6 -and ($norm.Contains($key) -or $key.Contains($norm))) {
                    $workshopSoft = $true
                    break
                }
            }
        }
        $markers = Test-ChromaMarkers $folder.FullName
        # Soft workshop match alone is too noisy (launchers / unrelated folders).
        if (-not $markers -and -not $workshopExact) { return }
        $exe = Find-PrimaryExe $folder.FullName $folder.Name
        if (-not $exe) { return }
        $targets.Add([pscustomobject]@{
            Folder   = $folder.FullName
            Exe      = $exe
            Markers  = $markers
            Workshop = ($workshopExact -or $workshopSoft)
        })
    }
}

Write-Host ""
Write-Host ("Found {0} candidate game(s)." -f $targets.Count)
$installed = 0
$skipped = 0
$failed = 0
$report = @()

foreach ($t in $targets) {
    $why = @()
    if ($t.Markers) { $why += "chroma-marker" }
    if ($t.Workshop) { $why += "workshop-list" }
    $label = "{0}  [{1}]" -f $t.Exe, ($why -join ",")
    if ($WhatIf) {
        Write-Host "WHATIF: $label"
        $report += [pscustomobject]@{ Exe = $t.Exe; Status = "whatif"; Why = ($why -join ",") }
        continue
    }
    Write-Host "INSTALL: $label"
    try {
        & powershell -NoProfile -ExecutionPolicy Bypass -File $installOne -GameExePath $t.Exe | Out-Host
        $installed++
        $report += [pscustomobject]@{ Exe = $t.Exe; Status = "ok"; Why = ($why -join ",") }
    } catch {
        Write-Warning "Failed $($t.Exe): $($_.Exception.Message)"
        $failed++
        $report += [pscustomobject]@{ Exe = $t.Exe; Status = "fail"; Why = ($why -join ",") }
    }
}

$outDir = Join-Path $env:TEMP "openrgb_gw_install_report"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$csv = Join-Path $outDir ("install_all_{0:yyyyMMdd_HHmmss}.csv" -f (Get-Date))
$report | Export-Csv -Path $csv -NoTypeInformation -Encoding UTF8
Write-Host ""
Write-Host "Done. installed=$installed failed=$failed skipped=$skipped"
Write-Host "Report: $csv"
Write-Host "Capture logs (full dump by default): $env:TEMP\openrgb_gw_capture_*.log"
Write-Host "Pointer file: $env:TEMP\openrgb_gw_capture_latest.log"
