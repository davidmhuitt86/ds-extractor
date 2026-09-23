# Publish the current extraction review artifacts to main.
# This script stages ONLY artifacts/extraction_review and never stages source changes.

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Fail([string]$Message) {
    Write-Host "[DX-REVIEW] FAILED: $Message" -ForegroundColor Red
    exit 1
}

try {
    $branch = (git rev-parse --abbrev-ref HEAD).Trim()
    if ($branch -ne "main") {
        Fail "Review publishing is main-only. Current branch: $branch"
    }

    $reviewPath = Join-Path $repoRoot "artifacts\extraction_review"
    if (-not (Test-Path $reviewPath)) {
        Fail "Review directory does not exist: $reviewPath"
    }

    Write-Host "[DX-REVIEW] Staging extraction review artifacts"
    & git add -f -- artifacts/extraction_review
    if ($LASTEXITCODE -ne 0) {
        Fail "git add failed."
    }

    & git diff --cached --quiet -- artifacts/extraction_review
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[DX-REVIEW] No review changes to publish." -ForegroundColor Yellow
        exit 0
    }

    $stamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $message = "DX-REVIEW: extraction review $stamp"

    Write-Host "[DX-REVIEW] Committing review artifacts"
    & git commit -m $message
    if ($LASTEXITCODE -ne 0) {
        Fail "git commit failed."
    }

    Write-Host "[DX-REVIEW] Pushing review commit to origin/main"
    & git push origin main
    if ($LASTEXITCODE -ne 0) {
        Fail "git push failed. The review commit remains local."
    }

    $sha = (git rev-parse HEAD).Trim()
    Write-Host "[DX-REVIEW] Published: $sha" -ForegroundColor Green
}
catch {
    Fail $_.Exception.Message
}
