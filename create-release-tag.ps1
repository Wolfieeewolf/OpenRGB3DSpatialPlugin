# PowerShell script to create a release tag with correct format
# Format: vYY.MM.DD.V (e.g. v26.02.03.1) — YY=year, MM=month, DD=day, V=version number
# Without -Date: uses today in AUS Eastern. With -Date: use that date so the tag is correct regardless of timezone.
# Usage: .\create-release-tag.ps1 [version_number] [commit_message]
#        .\create-release-tag.ps1 -Date "2026-02-03" [version_number] [commit_message]
# If version_number is 0 or not provided, auto-increments from existing tags for that date.

param(
    [string]$Date = "",
    [int]$VersionNumber = 0,
    [string]$Message = ""
)

# Resolve year, month, day: from -Date or from today in AUS Eastern
if ([string]::IsNullOrWhiteSpace($Date)) {
    $tz = [TimeZoneInfo]::FindSystemTimeZoneById("AUS Eastern Standard Time")
    $dateObj = [TimeZoneInfo]::ConvertTimeFromUtc((Get-Date).ToUniversalTime(), $tz)
    $year = $dateObj.ToString("yy")
    $month = $dateObj.ToString("MM")
    $day = $dateObj.ToString("dd")
} else {
    # -Date: accept YYYY-MM-DD or YY.MM.DD (e.g. 2026-02-03 or 26.02.03)
    $Date = $Date.Trim()
    if ($Date -match '^(\d{4})-(\d{2})-(\d{2})$') {
        $year = $Matches[1].Substring(2, 2)
        $month = $Matches[2]
        $day = $Matches[3]
    } elseif ($Date -match '^(\d{2})\.(\d{2})\.(\d{2})$') {
        $year = $Matches[1]
        $month = $Matches[2]
        $day = $Matches[3]
    } else {
        Write-Host "ERROR: -Date must be YYYY-MM-DD (e.g. 2026-02-03) or YY.MM.DD (e.g. 26.02.03)" -ForegroundColor Red
        exit 1
    }
}

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
    Write-Host "Existing tags for this date:" -ForegroundColor Yellow
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
    Write-Host "Auto-incremented version number for this date" -ForegroundColor Cyan
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
