# EKE-DX-WIRE AP preflight validation
# Validates the proposed working tree BEFORE an AP commit.
# Unlike dx-ap-gate.ps1, this script intentionally permits source changes.

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Fail([string]$Message) {
    Write-Host ""
    Write-Host "[DX-AP-PREFLIGHT] FAILED: $Message" -ForegroundColor Red
    exit 1
}

function Invoke-Checked([string]$File, [string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("Command failed with exit code {0}: {1} {2}" -f $LASTEXITCODE, $File, ($Arguments -join " "))
    }
}

try {
    Write-Host "[DX-AP-PREFLIGHT] Repository: $repoRoot" -ForegroundColor Cyan

    if (-not (Test-Path (Join-Path $repoRoot ".git"))) {
        Fail "Repository root could not be verified."
    }

    $branch = (git rev-parse --abbrev-ref HEAD).Trim()
    if ($branch -ne "main") {
        Fail "AP preflight is main-only. Current branch: $branch"
    }

    $head = (git rev-parse HEAD).Trim()
    $status = @(git status --porcelain)

    if ($status.Count -eq 0) {
        Fail "No working-tree changes detected. Preflight is intended for a proposed AP before commit."
    }

    Write-Host "[DX-AP-PREFLIGHT] Baseline HEAD: $head" -ForegroundColor Cyan
    Write-Host "[DX-AP-PREFLIGHT] Proposed changes detected: $($status.Count) status entries" -ForegroundColor Cyan

    Write-Host "[DX-AP-PREFLIGHT] Reconfiguring CMake" -ForegroundColor Cyan
    Invoke-Checked "cmake" @("-S", ".", "-B", "build")

    Write-Host "[DX-AP-PREFLIGHT] Clean Release build" -ForegroundColor Cyan
    Invoke-Checked "cmake" @("--build", "build", "--config", "Release", "--clean-first", "--parallel", "1")

    Write-Host "[DX-AP-PREFLIGHT] Running complete Release test suite" -ForegroundColor Cyan
    & ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw ("Release test suite failed with exit code {0}." -f $LASTEXITCODE)
    }

    $guiPath = Join-Path $repoRoot "buildReleasedx-extractor-gui.exe"
    if (-not (Test-Path $guiPath -PathType Leaf)) {
        Fail "Release GUI was not produced: $guiPath"
    }

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host " DX-EXTRACTOR AP PREFLIGHT PASSED" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Baseline:    $head"
    Write-Host "Build:       CLEAN RELEASE PASSED"
    Write-Host "CTest:       PASSED"
    Write-Host "GUI:         VERIFIED"
    Write-Host ""
    Write-Host "Review the diff, then commit the AP." -ForegroundColor Green
}
catch {
    Fail $_.Exception.Message
}
