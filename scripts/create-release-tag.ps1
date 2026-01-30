# PowerShell script to create a release tag in the format YY.MM.DD.version
# Date is always today in Australian timezone (AUS Eastern Standard Time).
# Usage: .\scripts\create-release-tag.ps1 [version_number]
# If version_number is not provided, it will auto-increment based on existing tags for today

param(
    [int]$VersionNumber = 0
)

# Get today's date in Australian timezone (AUS Eastern - Sydney/Melbourne)
$tz = [TimeZoneInfo]::FindSystemTimeZoneById("AUS Eastern Standard Time")
$dateInAus = [TimeZoneInfo]::ConvertTimeFromUtc((Get-Date).ToUniversalTime(), $tz)
$today = $dateInAus.ToString("yy.MM.dd")
$datePattern = "^v$today\.(\d+)$"

# Get all tags matching today's date
$todayTags = git tag | Where-Object { $_ -match $datePattern } | Sort-Object -Descending

if ($VersionNumber -eq 0) {
    # Auto-increment: find the highest version number for today
    if ($todayTags.Count -gt 0) {
        $latestTag = $todayTags[0]
        if ($latestTag -match $datePattern) {
            $VersionNumber = [int]$matches[1] + 1
        } else {
            $VersionNumber = 1
        }
    } else {
        # No tags for today, start at 1
        $VersionNumber = 1
    }
}

$tagName = "v$today.$VersionNumber"
$commitMessage = git log -1 --pretty=format:"%s"

Write-Host "Creating tag: $tagName" -ForegroundColor Green
Write-Host "Latest commit: $commitMessage" -ForegroundColor Yellow

# Create annotated tag
git tag -a $tagName -m "$commitMessage"

# Push tag to both remotes
Write-Host "Pushing tag to remotes..." -ForegroundColor Green
git push origin $tagName

# Also push to GitLab if remote exists
$gitlabRemote = git remote | Select-String "^gitlab$"
if ($gitlabRemote) {
    Write-Host "Pushing tag to GitLab..." -ForegroundColor Green
    git push gitlab $tagName
} else {
    Write-Host "GitLab remote not found, skipping GitLab push" -ForegroundColor Yellow
}

Write-Host "`nTag created and pushed successfully: $tagName" -ForegroundColor Green
Write-Host "Format: YY.MM.DD.version (e.g., v26.01.30.1), date = Australian (AUS Eastern)" -ForegroundColor Cyan
Write-Host "Auto-release runs on push: GitHub Actions and GitLab CI will build and create releases." -ForegroundColor Cyan
