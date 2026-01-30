# PowerShell script to create a release tag with correct format
# Format: vYY.MM.DD.version (e.g., v26.01.30.1)
# Date is always today in Australian timezone (AUS Eastern Standard Time).
# Usage: .\create-release-tag.ps1 [version_number] [commit_message]
# If version_number is 0 or not provided, auto-increments from existing tags for today

param(
    [int]$VersionNumber = 0,
    [string]$Message = ""
)

# Get current date in Australian timezone (AUS Eastern - Sydney/Melbourne)
$tz = [TimeZoneInfo]::FindSystemTimeZoneById("AUS Eastern Standard Time")
$date = [TimeZoneInfo]::ConvertTimeFromUtc((Get-Date).ToUniversalTime(), $tz)
$year = $date.ToString("yy")
$month = $date.ToString("MM")
$day = $date.ToString("dd")

# Auto-increment version if not specified
if ($VersionNumber -eq 0) {
    $todayPattern = "^v$year\.$month\.$day\.(\d+)$"
    $todayTags = git tag | Where-Object { $_ -match $todayPattern } | Sort-Object -Descending
    
    if ($todayTags.Count -gt 0) {
        $latestTag = $todayTags[0]
        if ($latestTag -match $todayPattern) {
            $VersionNumber = [int]$matches[1] + 1
        } else {
            $VersionNumber = 1
        }
    } else {
        $VersionNumber = 1
    }
}

# Build tag name
$tagName = "v$year.$month.$day.$VersionNumber"

# Check if tag already exists
$existingTags = git tag | Select-String "^$tagName$"
if ($existingTags) {
    Write-Host "ERROR: Tag $tagName already exists!" -ForegroundColor Red
    Write-Host "Existing tags for today:" -ForegroundColor Yellow
    git tag | Select-String "^v$year\.$month\.$day\." | ForEach-Object { Write-Host "  $_" }
    exit 1
}

# Get commit message if not provided
if ([string]::IsNullOrWhiteSpace($Message)) {
    $lastCommitMessage = git log -1 --pretty=%B
    $Message = $lastCommitMessage
}

Write-Host "Creating tag: $tagName" -ForegroundColor Green
if ($VersionNumber -eq 0 -or $VersionNumber -gt 1) {
    Write-Host "Auto-incremented version number for today" -ForegroundColor Cyan
}
Write-Host "Message: $Message" -ForegroundColor Cyan

# Create annotated tag
git tag -a $tagName -m $Message

# Ask for confirmation before pushing (pushing triggers auto-release on GitHub and GitLab)
Write-Host "`nTag created locally. Push to remotes? (Y/N) - Pushing triggers auto-release on GitHub and GitLab" -ForegroundColor Yellow
$response = Read-Host
if ($response -eq "Y" -or $response -eq "y") {
    git push origin $tagName
    Write-Host "Tag pushed to origin (GitHub): $tagName" -ForegroundColor Green
    $gitlabRemote = git remote | Select-String "^gitlab$"
    if ($gitlabRemote) {
        git push gitlab $tagName
        Write-Host "Tag pushed to gitlab: $tagName" -ForegroundColor Green
    }
    Write-Host "Auto-release will run on both pipelines (GitHub Actions + GitLab CI)." -ForegroundColor Cyan
} else {
    Write-Host "Tag created locally only. Push manually: git push origin $tagName; git push gitlab $tagName" -ForegroundColor Yellow
}
