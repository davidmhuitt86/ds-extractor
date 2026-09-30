# AP-DIAG-017A regression coverage for the publication integrity guard
# (tools/lib/dx-artifact-integrity.ps1). Exercises Test-ExtractionArtifactIntegrity
# directly against constructed fixtures - it never touches git and never
# calls dx-publish-review.ps1 itself, so these cases run in isolation from
# any real repository state.
#
# Usage:
#   pwsh -NoProfile -File tools/tests/test-dx-publish-review-integrity.ps1
#   (Windows PowerShell 5.1 works too: powershell -File ...)

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot "tools/lib/dx-artifact-integrity.ps1")

$script:failures = 0

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        Write-Host "  FAIL: $Message" -ForegroundColor Red
        $script:failures++
    }
    else {
        Write-Host "  ok: $Message" -ForegroundColor DarkGray
    }
}

# A complete, self-consistent pair of fixtures, matching the real schema
# produced by ReviewArtifactWriter::write / ExtractionAuditExporter::export_json
# (verified against a real build's output in this AP - see AP-DIAG-017A).
function New-MatchingFixture([string]$Root, [hashtable]$Overrides = @{}) {
    $reviewDir = Join-Path $Root "artifacts/extraction_review"
    $auditDir = Join-Path $Root "artifacts/audit"
    New-Item -ItemType Directory -Path $reviewDir -Force | Out-Null
    New-Item -ItemType Directory -Path $auditDir -Force | Out-Null

    foreach ($file in (Get-RequiredReviewLayerFiles | Where-Object { $_ -ne "review_manifest.json" })) {
        Set-Content -LiteralPath (Join-Path $reviewDir $file) -Value "fixture" -NoNewline
    }

    $sourceId = if ($Overrides.ContainsKey("source_id")) { $Overrides.source_id } else { "samples/trx300ODG.png" }
    $page = if ($Overrides.ContainsKey("page")) { $Overrides.page } else { 0 }
    $width = if ($Overrides.ContainsKey("image_width")) { $Overrides.image_width } else { 898 }
    $height = if ($Overrides.ContainsKey("image_height")) { $Overrides.image_height } else { 549 }
    $wires = if ($Overrides.ContainsKey("wires")) { $Overrides.wires } else { 37 }
    $symbols = if ($Overrides.ContainsKey("symbols")) { $Overrides.symbols } else { 34 }
    $terminals = if ($Overrides.ContainsKey("terminals")) { $Overrides.terminals } else { 18 }
    $connectors = if ($Overrides.ContainsKey("connectors")) { $Overrides.connectors } else { 7 }
    $connectorTerminals = if ($Overrides.ContainsKey("connector_terminals")) { $Overrides.connector_terminals } else { 6 }
    $topologyEdges = if ($Overrides.ContainsKey("topology_edges")) { $Overrides.topology_edges } else { 643 }
    $electricalNets = if ($Overrides.ContainsKey("electrical_nets")) { $Overrides.electrical_nets } else { 12 }
    $validationErrors = if ($Overrides.ContainsKey("validation_errors")) { $Overrides.validation_errors } else { 0 }
    $validationWarnings = if ($Overrides.ContainsKey("validation_warnings")) { $Overrides.validation_warnings } else { 34 }

    # review_manifest.json uses its own (audit-independent) field names:
    # symbols/terminals/topology_edges instead of
    # components/terminal_candidates/topology_edges under "counts".
    $manifestWires = if ($Overrides.ContainsKey("manifest_wires")) { $Overrides.manifest_wires } else { $wires }

    $manifest = @{
        generated_at = "2026-01-01T00:00:00Z"
        source_id = $sourceId
        page = $page
        image_width = $width
        image_height = $height
        layers = @{
            wires = $manifestWires
            wire_colors = 230
            symbols = $symbols
            terminals = $terminals
            connectors = $connectors
            connector_terminals = $connectorTerminals
            splices_and_junctions = 533
            grounds = 6
            labels = 1147
            topology_edges = $topologyEdges
            component_bounds = $symbols
            endpoint_debug = 191
            recognition = $symbols
            symbol_geometry = $symbols
        }
        electrical_nets = $electricalNets
        validation_errors = $validationErrors
        validation_warnings = $validationWarnings
        validation_warning_codes = @{}
        coverage_summary = @{}
    }
    ($manifest | ConvertTo-Json -Depth 6) | Set-Content -LiteralPath (Join-Path $reviewDir "review_manifest.json")

    $auditSourceId = if ($Overrides.ContainsKey("audit_source_id")) { $Overrides.audit_source_id } else { $sourceId }
    $auditWires = if ($Overrides.ContainsKey("audit_wires")) { $Overrides.audit_wires } else { $wires }

    $audit = @{
        schema_version = 2
        source = @{
            source_id = $auditSourceId
            page = $page
            image_width = $width
            image_height = $height
        }
        run_identity = @{ kind = "wire_model_extraction"; source_id = $auditSourceId; page = $page }
        counts = @{
            components = $symbols
            terminal_candidates = $terminals
            connectors = $connectors
            connector_terminals = $connectorTerminals
            conductor_segments = 230
            endpoint_candidates = 191
            wires = $auditWires
            topology_nodes = 533
            topology_edges = $topologyEdges
            electrical_nets = $electricalNets
        }
        objects = @{}
        validation = @{ valid = $true; errors = $validationErrors; warnings = $validationWarnings; issues = @() }
        coverage = @{}
    }
    ($audit | ConvertTo-Json -Depth 6) | Set-Content -LiteralPath (Join-Path $auditDir "extraction_audit.json")

    return @{ ReviewDir = $reviewDir; AuditPath = (Join-Path $auditDir "extraction_audit.json") }
}

$testsRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("dx-integrity-tests-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $testsRoot -Force | Out-Null

try {
    # --- Case 1: complete matching artifacts -> publish succeeds --------
    Write-Host "Case 1: complete matching artifacts"
    $c1 = Join-Path $testsRoot "case1"
    $fixture = New-MatchingFixture -Root $c1
    $result = Test-ExtractionArtifactIntegrity -ReviewDir $fixture.ReviewDir -AuditPath $fixture.AuditPath
    Assert-True $result.Ok "matching artifacts are accepted"

    # --- Case 2: audit missing -> publish fails --------------------------
    Write-Host "Case 2: audit missing"
    $c2 = Join-Path $testsRoot "case2"
    $fixture2 = New-MatchingFixture -Root $c2
    Remove-Item -LiteralPath $fixture2.AuditPath -Force
    $result2 = Test-ExtractionArtifactIntegrity -ReviewDir $fixture2.ReviewDir -AuditPath $fixture2.AuditPath
    Assert-True (-not $result2.Ok) "missing audit is rejected"
    Assert-True ($result2.Reason -like "*audit*") "reason names the missing audit"

    # --- Case 3: review manifest missing -> publish fails ----------------
    Write-Host "Case 3: review manifest missing"
    $c3 = Join-Path $testsRoot "case3"
    $fixture3 = New-MatchingFixture -Root $c3
    Remove-Item -LiteralPath (Join-Path $fixture3.ReviewDir "review_manifest.json") -Force
    $result3 = Test-ExtractionArtifactIntegrity -ReviewDir $fixture3.ReviewDir -AuditPath $fixture3.AuditPath
    Assert-True (-not $result3.Ok) "missing review manifest is rejected"

    # --- Case 3b: any other missing review layer file -> publish fails --
    Write-Host "Case 3b: a review layer image missing"
    $c3b = Join-Path $testsRoot "case3b"
    $fixture3b = New-MatchingFixture -Root $c3b
    Remove-Item -LiteralPath (Join-Path $fixture3b.ReviewDir "07_grounds.png") -Force
    $result3b = Test-ExtractionArtifactIntegrity -ReviewDir $fixture3b.ReviewDir -AuditPath $fixture3b.AuditPath
    Assert-True (-not $result3b.Ok) "missing review layer image is rejected"

    # --- Case 4: source identity mismatch -> publish fails ---------------
    Write-Host "Case 4: source identity mismatch"
    $c4 = Join-Path $testsRoot "case4"
    $fixture4 = New-MatchingFixture -Root $c4 -Overrides @{
        source_id = "trx300ODG.png"
        audit_source_id = "another-source.png"
    }
    $result4 = Test-ExtractionArtifactIntegrity -ReviewDir $fixture4.ReviewDir -AuditPath $fixture4.AuditPath
    Assert-True (-not $result4.Ok) "source identity mismatch is rejected"
    Assert-True ($result4.Reason -like "*identity*") "reason names an identity mismatch"

    # --- Case 5: population mismatch -> publish fails ---------------------
    Write-Host "Case 5: population mismatch (wires)"
    $c5 = Join-Path $testsRoot "case5"
    $fixture5 = New-MatchingFixture -Root $c5 -Overrides @{
        manifest_wires = 39
        audit_wires = 37
    }
    $result5 = Test-ExtractionArtifactIntegrity -ReviewDir $fixture5.ReviewDir -AuditPath $fixture5.AuditPath
    Assert-True (-not $result5.Ok) "population mismatch is rejected"
    Assert-True ($result5.Reason -like "*wires*" -or $result5.Reason -like "*Population*") "reason names a population mismatch"

    # --- Case 6: current generated artifacts (if present) -----------------
    Write-Host "Case 6: current generated artifacts (best-effort, repo-relative)"
    $realReview = Join-Path $repoRoot "artifacts/extraction_review"
    $realAudit = Join-Path $repoRoot "artifacts/audit/extraction_audit.json"
    if ((Test-Path $realReview) -and (Test-Path $realAudit)) {
        $real = Test-ExtractionArtifactIntegrity -ReviewDir $realReview -AuditPath $realAudit
        if ($real.Ok) {
            Write-Host "  ok: locally generated artifacts agree" -ForegroundColor DarkGray
        }
        else {
            Write-Host "  note: locally generated artifacts do not currently agree ($($real.Reason)) - run a fresh extraction to regenerate both together" -ForegroundColor Yellow
        }
    }
    else {
        Write-Host "  skipped: no locally generated artifacts/ tree present (run an extraction first to exercise this case)" -ForegroundColor DarkGray
    }
}
finally {
    Remove-Item -LiteralPath $testsRoot -Recurse -Force -ErrorAction SilentlyContinue
}

if ($script:failures -gt 0) {
    Write-Host "`n$script:failures assertion(s) failed." -ForegroundColor Red
    exit 1
}

Write-Host "`nAll publication integrity guard tests passed." -ForegroundColor Green
exit 0
