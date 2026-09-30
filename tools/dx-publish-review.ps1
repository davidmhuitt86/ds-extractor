# Publish the current extraction review artifacts to the dedicated extraction-results branch.
# This publisher is launched automatically by the GUI after every successful extraction.
# The main working tree is never switched, committed, or pushed.
# extraction-results contains ONLY the current artifacts/extraction_review tree.

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$resultsBranch = "extraction-results"
$reviewRelative = "artifacts\extraction_review"
$reviewPath = Join-Path $repoRoot $reviewRelative
$worktreePath = Join-Path ([System.IO.Path]::GetTempPath()) ("dx-extraction-results-" + [guid]::NewGuid().ToString("N"))
$worktreeAdded = $false

function Fail([string]$Message) {
    Write-Host "[DX-REVIEW] FAILED: $Message" -ForegroundColor Red
    exit 1
}

function Invoke-Checked([string]$File, [string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("Command failed with exit code {0}: {1} {2}" -f $LASTEXITCODE, $File, ($Arguments -join " "))
    }
}

try {
    $branch = (git rev-parse --abbrev-ref HEAD).Trim()
    if ($branch -ne "main") {
        Fail "Review publishing must run from main. Current branch: $branch"
    }

    if (-not (Test-Path $reviewPath)) {
        Fail "Review directory does not exist: $reviewPath"
    }

    Write-Host "[DX-REVIEW] Fetching $resultsBranch"
    Invoke-Checked "git" @("fetch", "origin", $resultsBranch)

    if (git show-ref --verify --quiet ("refs/heads/" + $resultsBranch)) {
        Write-Host "[DX-REVIEW] Creating temporary worktree from local $resultsBranch"
        Invoke-Checked "git" @("worktree", "add", $worktreePath, $resultsBranch)
    }
    else {
        Write-Host "[DX-REVIEW] Creating local tracking branch $resultsBranch"
        Invoke-Checked "git" @("worktree", "add", "-b", $resultsBranch, $worktreePath, ("origin/" + $resultsBranch))
    }
    $worktreeAdded = $true

    $destination = Join-Path $worktreePath $reviewRelative
    if (Test-Path $destination) {
        Remove-Item -LiteralPath $destination -Recurse -Force
    }
    New-Item -ItemType Directory -Path $destination -Force | Out-Null

    Write-Host "[DX-REVIEW] Replacing extraction results"
    Copy-Item -LiteralPath (Join-Path $reviewPath "*") -Destination $destination -Recurse -Force

    Push-Location $worktreePath
    try {
        & git add -A -- $reviewRelative
        if ($LASTEXITCODE -ne 0) {
            throw "git add failed."
        }

        & git diff --cached --quiet
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[DX-REVIEW] No extraction-result changes to publish." -ForegroundColor Yellow
            return
        }

        $stamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        $message = "DX-REVIEW: replace extraction results $stamp"

        Write-Host "[DX-REVIEW] Committing current extraction results"
        Invoke-Checked "git" @("commit", "-m", $message)

        Write-Host "[DX-REVIEW] Pushing $resultsBranch"
        Invoke-Checked "git" @("push", "origin", $resultsBranch)

        $sha = (git rev-parse HEAD).Trim()
        Write-Host "[DX-REVIEW] Published extraction results: $sha" -ForegroundColor Green
        Write-Host "[DX-REVIEW] Main was not modified."
    }
    finally {
        Pop-Location
    }
}
catch {
    Fail $_.Exception.Message
}
finally {
    if ($worktreeAdded) {
        try {
            git worktree remove --force $worktreePath 2>$null
        }
        catch {
            Write-Host "[DX-REVIEW] WARNING: temporary worktree cleanup failed: $worktreePath" -ForegroundColor Yellow
        }
    }
}
