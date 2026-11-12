# Summarize an OpenRGB Chroma capture log (device/effect/colour patterns).
param(
    [string]$LogPath = ""
)

$ErrorActionPreference = "Stop"
if (-not $LogPath) {
    $latest = Join-Path $env:TEMP "openrgb_gw_capture_latest.log"
    if (-not (Test-Path $latest)) { throw "No latest capture pointer at $latest" }
    $LogPath = (Get-Content $latest -TotalCount 1).Trim()
}
if (-not (Test-Path $LogPath)) { throw "Capture log not found: $LogPath" }

Write-Host "Capture: $LogPath"
Get-Content $LogPath -TotalCount 5 | Where-Object { $_ -like '#*' } | ForEach-Object { Write-Host $_ }

$rows = Get-Content $LogPath | Where-Object { $_ -and ($_ -notlike '#*') } | ForEach-Object {
    $p = $_.Split("`t")
    if ($p.Count -lt 6) { return }
    [pscustomobject]@{
        Kind   = $p[2]
        Device = $p[3]
        Effect = $p[4]
        Rows   = $p[5]
        Cols   = $p[6]
        Lit    = $p[7]
        AvgR   = $p[8]
        AvgG   = $p[9]
        AvgB   = $p[10]
    }
}

Write-Host ""
Write-Host "=== calls by device / kind / effect ==="
$rows | Group-Object Device, Kind, Effect | Sort-Object Count -Descending |
    Select-Object Count, @{n='Device';e={$_.Group[0].Device}}, @{n='Kind';e={$_.Group[0].Kind}}, @{n='Effect';e={$_.Group[0].Effect}} |
    Format-Table -AutoSize

Write-Host "=== unique avg colours (non-black) by device ==="
$rows | Where-Object { $_.AvgR -or $_.AvgG -or $_.AvgB } |
    Group-Object Device, AvgR, AvgG, AvgB |
    Sort-Object { $_.Group[0].Device }, Count -Descending |
    Select-Object -First 40 Count, @{n='Device';e={$_.Group[0].Device}}, @{n='RGB';e={ '{0},{1},{2}' -f $_.Group[0].AvgR,$_.Group[0].AvgG,$_.Group[0].AvgB }} |
    Format-Table -AutoSize
