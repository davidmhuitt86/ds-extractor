# Publish the current extraction review artifacts to the dedicated extraction-results branch.
# This publisher is launched automatically by the GUI after every successful extraction.
# The main working tree is never switched, committed, or pushed.
# extraction-results contains ONLY the current extraction artifacts needed
# for review:
#   artifacts/extraction_review
#   artifacts/audit/extraction_audit.json

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

# AP-DIAG-017A: shared integrity check (Test-ExtractionArtifactIntegrity)
# verifying the review tree and the structured audit came from the same
# extraction run, before anything here is allowed to touch git.
. (Join-Path $PSScriptRoot "lib/dx-artifact-integrity.ps1")

$resultsBranch = "extraction-results"
# Forward slashes throughout, not backslashes: these paths are also used
# as git pathspecs (git treats backslash as an escape character in a
# pathspec, not a separator, outside Windows), and forward slashes work
# identically in Join-Path and Test-Path on both Windows PowerShell and
# PowerShell 7+.
$reviewRelative = "artifacts/extraction_review"
$reviewPath = Join-Path $repoRoot $reviewRelative
$auditRelative = "artifacts/audit/extraction_audit.json"
$auditPath = Join-Path $repoRoot $auditRelative
$worktreePath = Join-Path ([System.IO.Path]::GetTempPath()) ("dx-extraction-results-" + [guid]::NewGuid().ToString("N"))
$publishBranch = "dx-publish-" + [guid]::NewGuid().ToString("N")
$worktreeAdded = $false

# The GUI launches this script in a normal PowerShell window. Keep the window
# open after completion so the extraction/publish log remains visible.
# A transcript is also retained for postmortem diagnosis.
$logPath = Join-Path ([System.IO.Path]::GetTempPath()) "dx-publish-review.log"
Start-Transcript -Path $logPath -Append | Out-Null

function Wait-ForClose([string]$Message) {
    Write-Host ""
    Write-Host $Message -ForegroundColor Yellow
    Read-Host "Press Enter to close"
}

function Fail([string]$Message) {
    Write-Host "[DX-REVIEW] FAILED: $Message" -ForegroundColor Red
    Write-Host "[DX-REVIEW] Full log: $logPath" -ForegroundColor Yellow
    Wait-ForClose "Review publisher stopped with an error."
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

    if (-not (Test-Path $auditPath -PathType Leaf)) {
        Fail "Structured extraction audit does not exist: $auditPath"
    }

    # AP-DIAG-017A integrity guard: the review tree and the structured audit
    # must be internally complete and must agree with each other (same
    # source identity, same population counts) before anything is staged,
    # committed, or pushed. A mismatch means the two artifacts came from
    # different runs (e.g. a partial local write from an interrupted
    # extraction) and publication must fail outright - never reconcile or
    # patch over it.
    Write-Host "[DX-REVIEW] Verifying review/audit integrity"
    $integrity = Test-ExtractionArtifactIntegrity -ReviewDir $reviewPath -AuditPath $auditPath
    if (-not $integrity.Ok) {
        Fail "Artifact integrity check failed: $($integrity.Reason)"
    }
    Write-Host "[DX-REVIEW] Integrity check passed: review and audit agree" -ForegroundColor Green

    # Never use a pre-existing local extraction-results branch. Always build the
    # temporary worktree from the freshly fetched remote ref so stale local refs
    # cannot publish old results or cause a non-fast-forward push.
    Write-Host "[DX-REVIEW] Fetching latest remote refs"
    Invoke-Checked "git" @("fetch", "origin", "--prune")

    $remoteRef = "refs/remotes/origin/$resultsBranch"
    git show-ref --verify --quiet $remoteRef
    $remoteExists = ($LASTEXITCODE -eq 0)

    if ($remoteExists) {
        Write-Host "[DX-REVIEW] Creating temporary worktree from origin/$resultsBranch"
        Invoke-Checked "git" @("worktree", "add", "-b", $publishBranch, $worktreePath, ("origin/" + $resultsBranch))
    }
    else {
        Write-Host "[DX-REVIEW] Remote $resultsBranch does not exist; creating initial publication from main"
        Invoke-Checked "git" @("worktree", "add", "-b", $publishBranch, $worktreePath, "HEAD")
    }
    $worktreeAdded = $true

    ${destination} = Join-Path $worktreePath $reviewRelative
    $auditDestination = Join-Path $worktreePath $auditRelative
    if (Test-Path $destination) {
        Remove-Item -LiteralPath $destination -Recurse -Force
    }
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    New-Item -ItemType Directory -Path (Split-Path -Parent $auditDestination) -Force | Out-Null

    Write-Host "[DX-REVIEW] Replacing extraction results"
    # -LiteralPath disables wildcard expansion entirely, so a trailing "*"
    # with it is never reliably a directory-contents copy - use -Path,
    # which expands wildcards consistently on both Windows PowerShell and
    # PowerShell 7+.
    Copy-Item -Path (Join-Path $reviewPath "*") -Destination $destination -Recurse -Force
    Copy-Item -LiteralPath $auditPath -Destination $auditDestination -Force

    Push-Location $worktreePath
    try {
        & git add -A -f -- $reviewRelative $auditRelative
        if ($LASTEXITCODE -ne 0) {
            throw "git add failed."
        }

        & git diff --cached --quiet -- $reviewRelative $auditRelative
        if ($LASTEXITCODE -eq 0) {
            # No diff is a legitimate, expected outcome (e.g. a code change that
            # doesn't alter extraction output) - not a failure. But without any
            # pause, this looks identical to the window closing before reaching
            # any output at all (a genuine silent failure). A short visible
            # delay makes the two distinguishable without requiring a keypress
            # on every single extraction.
            Write-Host "[DX-REVIEW] No extraction-result changes to publish." -ForegroundColor Yellow
            Wait-ForClose "Extraction results are already current."
            return
        }

        $stamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        $message = "DX-REVIEW: replace extraction results $stamp"

        Write-Host "[DX-REVIEW] Committing current extraction results"
        Invoke-Checked "git" @("commit", "-m", $message)

        Write-Host "[DX-REVIEW] Pushing $resultsBranch"
        Invoke-Checked "git" @("push", "origin", ("HEAD:refs/heads/" + $resultsBranch))

        $sha = (git rev-parse HEAD).Trim()
        Write-Host "[DX-REVIEW] Published extraction results: $sha" -ForegroundColor Green
        Write-Host "[DX-REVIEW] Main was not modified." -ForegroundColor Green
        Wait-ForClose "Extraction results published successfully."
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

        try {
            git branch -D $publishBranch 2>$null
        }
        catch {
            Write-Host "[DX-REVIEW] WARNING: temporary branch cleanup failed: $publishBranch" -ForegroundColor Yellow
        }
    }
    Stop-Transcript | Out-Null
}
