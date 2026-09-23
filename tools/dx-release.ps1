# EKE-DX-WIRE release validation pipeline
# GUI -> close -> pull main -> configure -> build -> test -> reopen GUI
# This pipeline intentionally does NOT commit, push, or create a pull request.
# The PowerShell window remains open after the GUI is relaunched.

[CmdletBinding()]
param()

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
    Read-Host "Press Enter to close"
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
    if ($branch -ne "main") {
        Fail "The release workflow is main-only. Current branch: $branch"
    }

    Step "Checking working tree before pull"
    $status = @(git status --short)
    if ($status.Count -ne 0) {
        Write-Host "The working tree contains local changes:" -ForegroundColor Yellow
        $status | ForEach-Object { Write-Host "  $_" }
        Fail "Commit or discard local changes before running Release."
    }

    Step "Pulling latest main"
    Invoke-Checked "git" @("pull", "origin", "main")

    Step "Configuring Release build"
    Invoke-Checked "cmake" @("-S", ".", "-B", "build")

    Step "Building Release"
    Invoke-Checked "cmake" @("--build", "build", "--config", "Release", "--parallel", "1")

    Step "Running Release tests"
    Invoke-Checked "ctest" @("--test-dir", "build", "-C", "Release", "--output-on-failure")

    $guiPath = Join-Path $repoRoot "build\Release\dx-extractor-gui.exe"
    if (-not (Test-Path $guiPath)) {
        Fail "Release GUI was not produced: $guiPath"
    }

    $headSha = (git rev-parse HEAD).Trim()

    Step "Build and tests passed"
    Write-Host "Main:   $headSha" -ForegroundColor Green
    Write-Host "GUI:    $guiPath" -ForegroundColor Green
    Write-Host "Commit: NOT modified"
    Write-Host "Push:   NOT performed"
    Write-Host "PR:     NOT created"

    Step "Reopening DX-Extractor GUI"
    Start-Process -FilePath $guiPath -WorkingDirectory $repoRoot

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host " DX-EXTRACTOR RELEASE VALIDATION COMPLETE" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Pulled latest main."
    Write-Host "Release build completed."
    Write-Host "All Release tests passed."
    Write-Host "GUI relaunched."
    Write-Host ""
    Write-Host "This PowerShell window will remain open." -ForegroundColor Yellow
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
}
catch {
    Fail $_.Exception.Message
}
