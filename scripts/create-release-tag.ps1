# PowerShell script to create a release tag in the format YY.MM.DD.version
# Usage: .\scripts\create-release-tag.ps1 [version_number]
# If version_number is not provided, it will auto-increment based on existing tags for today

param(
    [int]$VersionNumber = 0
)

# Get today's date in YY.MM.DD format
$today = Get-Date -Format "yy.MM.dd"
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

# Push tag
Write-Host "Pushing tag to remote..." -ForegroundColor Green
git push origin $tagName

Write-Host "`nTag created and pushed successfully: $tagName" -ForegroundColor Green
Write-Host "Format: YY.MM.DD.version (e.g., v26.01.26.1)" -ForegroundColor Cyan
