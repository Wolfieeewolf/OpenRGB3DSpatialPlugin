# Push the current branch (or given ref) to both GitLab (origin) and GitHub.
# Does not modify git config.
#
# Usage:
#   .\scripts\dual-push.ps1
#   .\scripts\dual-push.ps1 -Ref HEAD:main
#   .\scripts\dual-push.ps1 -Tags
#   .\scripts\dual-push.ps1 -Ref v0.1.0

[CmdletBinding()]
param(
    [string]$Ref = '',
    [switch]$Tags
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
Set-Location $Root

if (-not $Ref) {
    $Ref = (git rev-parse --abbrev-ref HEAD).Trim()
    if ($Ref -eq 'HEAD') {
        throw 'Detached HEAD — pass -Ref (e.g. HEAD:main or v0.1.0)'
    }
}

foreach ($remote in @('origin', 'github')) {
    git remote get-url $remote 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Missing git remote: $remote"
    }
}

Write-Host "Pushing $Ref → origin (GitLab)"
git push origin $Ref
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Pushing $Ref → github"
git push github $Ref
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Tags) {
    Write-Host 'Pushing tags → origin'
    git push origin --tags
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host 'Pushing tags → github'
    git push github --tags
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host 'Dual-push complete.'
