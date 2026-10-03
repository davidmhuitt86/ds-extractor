# EKE-DX-WIRE release validation pipeline
# GUI -> close -> pull main -> configure -> build -> test -> reopen GUI
# This pipeline intentionally does NOT commit, push, or create a pull request.
# The PowerShell window remains open after the GUI is relaunched.

[CmdletBinding()]
param(
    # Normal releases use the existing build tree and compile only what CMake/MSBuild
    # determines is stale. Use -CleanFirst for an explicit clean rebuild.
    [switch]$CleanFirst
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

    # This workflow only ever produces disposable local file changes (extraction
    # output, logs, review artifacts written by running the tool) - never source
    # edits, and never local commits (extraction-review publishing pushes to the
    # dedicated extraction-results branch, not main). Those disposable working-tree
    # changes must never block an automatic release, so they are discarded here
    # unconditionally before pulling.
    Step "Discarding local working-tree changes"
    Invoke-Checked "git" @("reset", "--hard", "HEAD")

    # --ff-only is still required on the pull itself: a plain "git pull" silently
    # creates a merge commit (i.e. it DOES commit) whenever local main has actual
    # commits origin/main does not. That indicates something committed to main
    # outside this workflow (exactly what an older, since-fixed version of
    # dx-publish-review.ps1 used to do) and should stop the release and be
    # investigated, not be silently merged away.
    Step "Pulling latest main"
    Invoke-Checked "git" @("pull", "--ff-only", "origin", "main")

    Step "Configuring Release build"
    Invoke-Checked "cmake" @("-S", ".", "-B", "build")

    if ($CleanFirst) {
        Step "Building Release (clean-first)"
        $buildArguments = @("--build", "build", "--config", "Release", "--clean-first", "--parallel", "1")
    }
    else {
        Step "Building Release (incremental)"
        $buildArguments = @("--build", "build", "--config", "Release", "--parallel", "1")
    }

    Invoke-Checked "cmake" $buildArguments

    Step "Running Release tests"
    & ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        $testExitCode = $LASTEXITCODE
        Write-Host ""
        Write-Host "[DX-RELEASE] Re-running failed tests verbosely for diagnostics" -ForegroundColor Yellow
        & ctest --test-dir build -C Release --rerun-failed --output-on-failure -VV
        throw ("Release test suite failed with exit code {0}." -f $testExitCode)
    }

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
    if ($CleanFirst) {
        Write-Host "Clean Release build completed."
    }
    else {
        Write-Host "Incremental Release build completed."
    }
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
