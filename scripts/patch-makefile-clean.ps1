# Patches qmake-generated nmake/jom Makefiles so `clean` is a short directory wipe
# instead of a multi-KB per-file `del` list (Qt Creator "Discarding excessive output").
param(
    [Parameter(Mandatory = $true)][string]$OutDir,
    [Parameter(Mandatory = $true)][string]$CleanScript
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path -LiteralPath $OutDir)) {
    exit 0
}

$cleanCmd = "cmd /c $CleanScript $OutDir"
$replacement = "clean: FORCE`r`n`t$cleanCmd`r`n`r`n"

foreach ($name in @("Makefile", "Makefile.Release", "Makefile.Debug")) {
    $path = Join-Path $OutDir $name
    if (-not (Test-Path -LiteralPath $path)) {
        continue
    }
    $text = [System.IO.File]::ReadAllText($path)

    # Already patched (clean target itself calls qt-win-clean) — leave alone.
    if ($text -match '(?m)^clean:.*?[\r\n]+\t[^\r\n]*qt-win-clean\.cmd') {
        continue
    }

    # Replace the `clean:` recipe (dependency + all DEL_FILE lines) up to distclean/next target.
    $patched = [regex]::Replace(
        $text,
        '(?ms)^clean:[^\r\n]*\r?\n(?:\t[^\r\n]*\r?\n)*\r?\n?',
        $replacement,
        1
    )
    if ($patched -eq $text) {
        Write-Host "WARN: could not patch clean in $name"
        continue
    }
    [System.IO.File]::WriteAllText($path, $patched)
    Write-Host "Patched clean target in $name"
}
