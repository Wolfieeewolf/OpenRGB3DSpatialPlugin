# Probe the text a streaming app window exposes: its window title and the text Windows
# accessibility (UI Automation) can read inside it (what a screen reader would hear).
# Used to look for the show / episode name in apps that do not report it to the media
# controls (the Netflix Store app). Read-only.
#
# Run with Windows PowerShell 5.1 while the app is open and playing:
#   powershell.exe -ExecutionPolicy Bypass -File scripts\probe-app-text.ps1 -Process Netflix
param(
    [string]$Process = 'Netflix',
    [int]$MaxNodes = 4000
)

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

$procs = Get-Process | Where-Object { $_.ProcessName -like "*$Process*" -or $_.MainWindowTitle -like "*$Process*" }
Write-Host "Processes matching '$Process':"
$procs | ForEach-Object { "  {0} (pid {1})  window title: '{2}'" -f $_.ProcessName, $_.Id, $_.MainWindowTitle }

# Store apps run inside ApplicationFrameHost; find top-level windows whose name mentions the app.
$root = [System.Windows.Automation.AutomationElement]::RootElement
$windows = $root.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition) |
    Where-Object { $_.Current.Name -like "*$Process*" -or ($procs | Where-Object Id -eq $_.Current.ProcessId) }

foreach($w in $windows)
{
    Write-Host ("`n=== Window: '{0}'  class: {1}  pid {2}" -f $w.Current.Name, $w.Current.ClassName, $w.Current.ProcessId)
    $walker = [System.Windows.Automation.TreeWalker]::RawViewWalker
    $stack = New-Object System.Collections.Stack
    $stack.Push(@($w, 0))
    $seen = 0
    $texts = New-Object System.Collections.Generic.List[string]
    while($stack.Count -gt 0 -and $seen -lt $MaxNodes)
    {
        $item = $stack.Pop()
        $el = $item[0]
        $depth = $item[1]
        $seen++
        try
        {
            $name = $el.Current.Name
            $type = $el.Current.ControlType.ProgrammaticName -replace 'ControlType\.', ''
            if($name -and $name.Trim().Length -gt 1)
            {
                $texts.Add(("{0}[{1}] {2}" -f (' ' * [Math]::Min($depth, 20)), $type, $name.Trim()))
            }
            $child = $walker.GetFirstChild($el)
            while($child)
            {
                $stack.Push(@($child, $depth + 1))
                $child = $walker.GetNextSibling($child)
            }
        }
        catch { }
    }
    Write-Host ("  {0} elements walked, {1} with text:" -f $seen, $texts.Count)
    $texts | Select-Object -First 300 | ForEach-Object { "  $_" }
}
