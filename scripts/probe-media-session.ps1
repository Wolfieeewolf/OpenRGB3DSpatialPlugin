# Probe everything Windows' media controls (System Media Transport Controls) report while
# something plays: app, every media property (title, subtitle, artist, album, genres, track
# numbers, playback type), the thumbnail image, play state, controls, timeline and position.
# Used to see what streaming apps (Netflix / Prime Video / Crunchyroll Store apps, browsers)
# expose for Movie Sync.
#
# Run with Windows PowerShell 5.1 (not PowerShell 7, which cannot call these WinRT APIs):
#   powershell.exe -ExecutionPolicy Bypass -File scripts\probe-media-session.ps1
# Prints only when something changes. Thumbnails and a full JSON report go to
#   %TEMP%\orgb-media-probe\   (report.json is rewritten on every change)
# Press Ctrl+C to stop.

Add-Type -AssemblyName System.Runtime.WindowsRuntime
$null = [Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager, Windows.Media.Control, ContentType = WindowsRuntime]
$null = [Windows.Storage.Streams.IRandomAccessStreamWithContentType, Windows.Storage.Streams, ContentType = WindowsRuntime]

$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
        $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and
        $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'
    })[0]

function Await($operation, [Type]$resultType, [int]$timeoutMs = 2000)
{
    $task = $asTaskGeneric.MakeGenericMethod($resultType).Invoke($null, @($operation))
    if(-not $task.Wait($timeoutMs))
    {
        throw "timed out"
    }
    $task.Result
}

$outDir = Join-Path $env:TEMP 'orgb-media-probe'
$null = New-Item -ItemType Directory -Force $outDir

$manager = Await ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager]::RequestAsync()) `
                 ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager])

# Save a session's thumbnail once per title; returns "file (type, bytes)" or why not.
$thumbs = @{}
function Save-Thumbnail($app, $props)
{
    $key = "$app|$($props.Title)|$($props.Artist)|$($props.AlbumTitle)"
    if($thumbs.ContainsKey($key) -and $thumbs[$key] -notlike 'none*')
    {
        return $thumbs[$key]
    }
    $result = 'none (the app gives no thumbnail)'
    try
    {
        if($props.Thumbnail)
        {
            $stream = Await ($props.Thumbnail.OpenReadAsync()) ([Windows.Storage.Streams.IRandomAccessStreamWithContentType])
            $type = $stream.ContentType
            $ext = switch -Wildcard ($type) { '*png' { 'png' } '*jpeg' { 'jpg' } '*jpg' { 'jpg' } '*bmp' { 'bmp' } '*webp' { 'webp' } default { 'bin' } }
            $safe = ($key -replace '[^\w\-]+', '_')
            if($safe.Length -gt 80) { $safe = $safe.Substring(0, 80) }
            $file = Join-Path $outDir ("thumb_{0}.{1}" -f $safe, $ext)
            $net = [System.IO.WindowsRuntimeStreamExtensions]::AsStreamForRead($stream)
            $fs = [System.IO.File]::Create($file)
            $net.CopyTo($fs)
            $fs.Close()
            $net.Close()
            $size = (Get-Item $file).Length
            $dim = ''
            try
            {
                Add-Type -AssemblyName System.Drawing
                $img = [System.Drawing.Image]::FromFile($file)
                $dim = " {0}x{1}" -f $img.Width, $img.Height
                $img.Dispose()
            }
            catch {}
            $result = "{0} ({1}, {2} bytes{3})" -f $file, $type, $size, $dim
        }
    }
    catch
    {
        $result = "none (could not read: $($_.Exception.Message))"
    }
    $thumbs[$key] = $result
    return $result
}

function Read-Session($s)
{
    $o = [ordered]@{ app = $s.SourceAppUserModelId }
    try
    {
        $props = Await ($s.TryGetMediaPropertiesAsync()) `
                       ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionMediaProperties]) 1000
        $o.title = $props.Title
        $o.subtitle = $props.Subtitle
        $o.artist = $props.Artist
        $o.albumTitle = $props.AlbumTitle
        $o.albumArtist = $props.AlbumArtist
        $o.albumTrackCount = $props.AlbumTrackCount
        $o.trackNumber = $props.TrackNumber
        $o.genres = @($props.Genres)
        $o.playbackType = if($props.PlaybackType) { "$($props.PlaybackType.Value)" } else { '' }
        $o.thumbnail = Save-Thumbnail $s.SourceAppUserModelId $props
    }
    catch
    {
        $o.properties = "could not read ($($_.Exception.Message))"
    }
    try
    {
        $p = $s.GetPlaybackInfo()
        $c = $p.Controls
        $o.status = "$($p.PlaybackStatus)"
        $o.playbackTypeInfo = if($p.PlaybackType) { "$($p.PlaybackType.Value)" } else { '' }
        $o.rate = if($p.PlaybackRate) { $p.PlaybackRate.Value } else { $null }
        $o.shuffle = if($p.IsShuffleActive) { $p.IsShuffleActive.Value } else { $null }
        $o.repeat = if($p.AutoRepeatMode) { "$($p.AutoRepeatMode.Value)" } else { '' }
        $o.controls = (@('IsPlayEnabled', 'IsPauseEnabled', 'IsStopEnabled', 'IsNextEnabled', 'IsPreviousEnabled',
                'IsFastForwardEnabled', 'IsRewindEnabled', 'IsPlaybackPositionEnabled', 'IsPlaybackRateEnabled',
                'IsChannelUpEnabled', 'IsChannelDownEnabled', 'IsRepeatEnabled', 'IsShuffleEnabled') |
            Where-Object { $c.$_ } | ForEach-Object { $_ -replace '^Is|Enabled$', '' }) -join ','
    }
    catch
    {
        $o.playback = "could not read ($($_.Exception.Message))"
    }
    try
    {
        $t = $s.GetTimelineProperties()
        $o.position = "$($t.Position)"
        $o.start = "$($t.StartTime)"
        $o.end = "$($t.EndTime)"
        $o.minSeek = "$($t.MinSeekTime)"
        $o.maxSeek = "$($t.MaxSeekTime)"
        $o.lastUpdated = $t.LastUpdatedTime.ToLocalTime().ToString('HH:mm:ss.fff')
    }
    catch
    {
        $o.timeline = "could not read ($($_.Exception.Message))"
    }
    return $o
}

Write-Host "Watching media sessions (prints on change). Output folder: $outDir"
Write-Host "Play something, pause, seek, change episode, then Ctrl+C."
$last = ''
while($true)
{
    $current = try { $manager.GetCurrentSession().SourceAppUserModelId } catch { '' }
    $all = @()
    foreach($s in $manager.GetSessions())
    {
        $all += Read-Session $s
    }
    # Change detection ignores the moving position.
    $sig = ($all | ForEach-Object {
            $src = $_
            $x = [ordered]@{}
            foreach($k in $src.Keys) { if($k -notin 'position', 'lastUpdated') { $x[$k] = $src[$k] } }
            $x | ConvertTo-Json -Compress
        }) -join '|'
    if($sig -ne $last)
    {
        $last = $sig
        Write-Host ("==== {0}  current session: {1}" -f (Get-Date -Format 'HH:mm:ss'), $current)
        if($all.Count -eq 0)
        {
            Write-Host "(no media sessions reported)"
        }
        foreach($o in $all)
        {
            Write-Host $o.app
            foreach($k in $o.Keys)
            {
                if($k -ne 'app')
                {
                    Write-Host ("    {0,-17} {1}" -f ($k + ':'), ($o[$k] -join ', '))
                }
            }
        }
        [ordered]@{ at = (Get-Date).ToString('o'); current = $current; sessions = $all } |
            ConvertTo-Json -Depth 5 | Set-Content (Join-Path $outDir 'report.json') -Encoding UTF8
    }
    Start-Sleep -Milliseconds 1000
}
