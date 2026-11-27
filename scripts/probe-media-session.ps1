# Probe what Windows' media controls (System Media Transport Controls) report while
# something plays: app, title, play state and playback position. Used to check whether
# streaming apps (Netflix / Prime Video Store apps, browsers) expose enough timing for
# Movie Sync cue scripts.
#
# Run with Windows PowerShell 5.1 (not PowerShell 7, which cannot call these WinRT APIs):
#   powershell.exe -ExecutionPolicy Bypass -File scripts\probe-media-session.ps1
# Press Ctrl+C to stop.

Add-Type -AssemblyName System.Runtime.WindowsRuntime
$null = [Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager, Windows.Media.Control, ContentType = WindowsRuntime]

$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
        $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and
        $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'
    })[0]

function Await($operation, [Type]$resultType)
{
    $task = $asTaskGeneric.MakeGenericMethod($resultType).Invoke($null, @($operation))
    $null = $task.Wait(-1)
    $task.Result
}

$manager = Await ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager]::RequestAsync()) `
                 ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager])

Write-Host "Watching media sessions. Play something in Netflix / Prime Video, pause, seek, then Ctrl+C."
while($true)
{
    $sessions = $manager.GetSessions()
    if($sessions.Count -eq 0)
    {
        Write-Host "(no media sessions reported)"
    }
    foreach($s in $sessions)
    {
        try
        {
            $props = Await ($s.TryGetMediaPropertiesAsync()) `
                           ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionMediaProperties])
            $timeline = $s.GetTimelineProperties()
            $playback = $s.GetPlaybackInfo()
            # The app reports a position at LastUpdatedTime; while playing, now is later than that.
            $estimate = $timeline.Position
            if($playback.PlaybackStatus -eq 'Playing' -and $timeline.LastUpdatedTime.Year -gt 2000)
            {
                $estimate = $timeline.Position + ([DateTimeOffset]::Now - $timeline.LastUpdatedTime)
            }
            "{0}`n    title: '{1}'  artist: '{2}'`n    state: {3}  position: {4}  (estimated now: {5})  length: {6}  last update: {7}" -f `
                $s.SourceAppUserModelId, $props.Title, $props.Artist, $playback.PlaybackStatus,
                $timeline.Position, $estimate, $timeline.EndTime, $timeline.LastUpdatedTime.ToLocalTime().ToString('HH:mm:ss')
        }
        catch
        {
            "{0}: could not read ({1})" -f $s.SourceAppUserModelId, $_.Exception.Message
        }
    }
    Write-Host "----"
    Start-Sleep -Seconds 1
}
