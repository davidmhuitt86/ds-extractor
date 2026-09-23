# EKE-DX-WIRE release pipeline
# Build -> Test -> Commit -> Push main
# Intended to be launched from the development GUI or directly from PowerShell.\n# Repository policy: development and release commits remain on main; no feature branches or PRs.

[CmdletBinding()]
param(
    [switch]$BuildOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Step([string]$Message) {
    Write-Host ""
    Write-Host ("[DX-RELEASE] " + $Message) -ForegroundColor Cyan
}

function Fail([string]$Message) {
    Write-Host ""
    Write-Host ("[DX-RELEASE] FAILED: " + $Message) -ForegroundColor Red
    Write-Host ""
    exit 1
}

function Invoke-Checked([string]$File, [string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("Command failed with exit code {0}: {1} {2}" -f $LASTEXITCODE, $File, ($Arguments -join " "))
    }
}

try {
    Step "Repository: $repoRoot"

    if (-not (Test-Path (Join-Path $repoRoot ".git"))) {
        Fail "Repository root could not be verified."
    }

    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        Fail "git.exe was not found on PATH."
    }

    $branch = (git rev-parse --abbrev-ref HEAD).Trim()
    if ([string]::IsNullOrWhiteSpace($branch) -or $branch -eq "HEAD") {
        Fail "Repository is in detached HEAD state."
    }

    Write-Host "Current branch: $branch"

    Step "Configuring Release build"
    Invoke-Checked "cmake" @("-S", ".", "-B", "build")

    Step "Building Release"
    Invoke-Checked "cmake" @("--build", "build", "--config", "Release", "--parallel", "1")

    Step "Running Release tests"
    Invoke-Checked "ctest" @("--test-dir", "build", "-C", "Release", "--output-on-failure")

    if ($BuildOnly) {
        Write-Host ""
        Write-Host "[DX-RELEASE] BUILD + TEST COMPLETE" -ForegroundColor Green
        exit 0
    }

    Step "Checking GitHub CLI"
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
        Fail "GitHub CLI (gh.exe) is required for automatic PR creation."
    }
    gh auth status
    if ($LASTEXITCODE -ne 0) {
        Fail "GitHub CLI is not authenticated. Run: gh auth login"
    }

    Step "Checking working tree"
    $status = @(git status --short)
    if ($status.Count -eq 0) {
        Write-Host "Working tree is clean."
    } else {
        Write-Host "Changes to be committed:"
        $status | ForEach-Object { Write-Host "  $_" }
    }

    if ($branch -ne "main") {
        Fail "This repository is main-only. Switch to main before running the release pipeline."
    }

    $commitMessage = Read-Host "Commit message (Enter = use current branch name)"
    if ([string]::IsNullOrWhiteSpace($commitMessage)) {
        $commitMessage = $branch
    }

    Step "Creating commit"
    Invoke-Checked "git" @("add", "-A")

    $staged = @(git diff --cached --name-only)
    if ($staged.Count -eq 0) {
        Write-Host "No new staged changes. Continuing with existing branch commits."
    } else {
        Invoke-Checked "git" @("commit", "-m", $commitMessage)
    }

    Step "Pushing branch"
    Invoke-Checked "git" @("push", "-u", "origin", $branch)

    $headSha = (git rev-parse HEAD).Trim()

    Write-Host ""
    Write-Host "[DX-RELEASE] Main-only workflow: no feature branch and no pull request." -ForegroundColor Yellow

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host " DX-EXTRACTOR RELEASE COMPLETE" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Branch: $branch"
    Write-Host "Commit: $headSha"
    Write-Host "Push:   main"
    Write-Host "PR:     not applicable (main-only workflow)"
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
}
catch {
    Fail $_.Exception.Message
}
