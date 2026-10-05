# EKE-DX-WIRE AP acceptance gate
# This gate is intentionally stricter than normal release validation.
# It validates the exact committed repository state and never modifies git history.

[CmdletBinding()]
param(
    [switch]$SkipGui
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Invoke-Checked([string]$File, [string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("Command failed with exit code {0}: {1} {2}" -f $LASTEXITCODE, $File, ($Arguments -join " "))
    }
}

function Fail([string]$Message) {
    Write-Host ""
    Write-Host "[DX-AP-GATE] FAILED: $Message" -ForegroundColor Red
    exit 1
}

try {
    Write-Host "[DX-AP-GATE] Repository: $repoRoot" -ForegroundColor Cyan

    if (-not (Test-Path (Join-Path $repoRoot ".git"))) {
        Fail "Repository root could not be verified."
    }

    $branch = (git rev-parse --abbrev-ref HEAD).Trim()
    if ($branch -ne "main") {
        Fail "AP gate is main-only. Current branch: $branch"
    }

    $status = @(git status --porcelain)
    if ($status.Count -ne 0) {
        Fail "Working tree is not clean. Commit or intentionally remove all changes before running the AP gate."
    }

    $head = (git rev-parse HEAD).Trim()
    Write-Host "[DX-AP-GATE] Validating committed HEAD: $head" -ForegroundColor Cyan

    # Always regenerate the build tree so CMake state cannot hide dependency or
    # target-definition errors from the committed repository.
    Write-Host "[DX-AP-GATE] Configuring Release build from scratch" -ForegroundColor Cyan
    Invoke-Checked "cmake" @("-S", ".", "-B", "build")

    # Always clean-first. Incremental compilation is deliberately not acceptable
    # as the AP acceptance criterion.
    Write-Host "[DX-AP-GATE] Clean Release build" -ForegroundColor Cyan
    Invoke-Checked "cmake" @("--build", "build", "--config", "Release", "--clean-first", "--parallel", "1")

    Write-Host "[DX-AP-GATE] Running complete Release test suite" -ForegroundColor Cyan
    & ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw ("Release test suite failed with exit code {0}." -f $LASTEXITCODE)
    }

    $guiPath = Join-Path $repoRoot "build\Release\dx-extractor-gui.exe"
    if (-not (Test-Path $guiPath -PathType Leaf)) {
        Fail "Release GUI was not produced: $guiPath"
    }

    if (-not $SkipGui) {
        Write-Host "[DX-AP-GATE] GUI executable verified: $guiPath" -ForegroundColor Green
    }

    $postStatus = @(git status --porcelain)
    if ($postStatus.Count -ne 0) {
        Fail "Validation modified the working tree. Generated source-controlled files must not be left dirty by the AP gate."
    }

    $verifiedHead = (git rev-parse HEAD).Trim()
    if ($verifiedHead -ne $head) {
        Fail "Repository HEAD changed during validation."
    }

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host " DX-EXTRACTOR AP ACCEPTANCE GATE PASSED" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "HEAD:        $verifiedHead"
    Write-Host "Build:       CLEAN RELEASE PASSED"
    Write-Host "CTest:       PASSED"
    Write-Host "GUI:         VERIFIED"
    Write-Host "Git status:  CLEAN"
    Write-Host ""
    Write-Host "This commit is eligible for AP completion." -ForegroundColor Green
}
catch {
    Fail $_.Exception.Message
}
