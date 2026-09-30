# AP-DIAG-017A: shared integrity check for the extraction publication
# pipeline. Dot-sourced by both tools/dx-publish-review.ps1 (to gate a real
# publish) and tools/tests/test-dx-publish-review-integrity.ps1 (to exercise
# it against constructed fixtures), so the production check and its test
# coverage can never drift apart into two different implementations.
#
# Root cause this guards against (AP-DIAG-017A investigation): the visual
# review tree (artifacts/extraction_review/*, written by
# ReviewArtifactWriter::write) and the structured audit
# (artifacts/audit/extraction_audit.json, written by
# ExtractionAuditExporter::export_json) are two independent write
# operations against the local filesystem, both driven from the same
# WireModel in the same ExtractionArtifactWriter::write() call - but
# neither write is atomic. ReviewArtifactWriter::write() clears its output
# directory with fs::remove_all() and then writes 15 images plus a
# manifest one at a time; if any single write throws partway through (a
# render failure, a full disk, an antivirus lock, anything), the directory
# is left cleared-but-incomplete while extraction_audit.json - written
# earlier in the same call - remains fully intact from that same run, or
# vice versa on a subsequent run. Nothing before this fix ever checked that
# the two trees a publish was about to commit actually came from the same
# run, so a partial local failure could silently propagate into a
# published extraction-results commit containing one fresh tree and one
# stale/empty one.
#
# This module does not repair or reconcile a mismatch - it only detects
# one. Per AP-DIAG-017A's explicit design constraint, a mismatch must fail
# publication, never be silently patched over by copying an audit from a
# different run.

function Get-RequiredReviewLayerFiles {
    # The 15 canonical review images plus the manifest itself. Kept as a
    # literal list (not a directory listing) so a partially-written
    # directory missing files is detected as "files are missing", not
    # quietly accepted because "whatever is present is present".
    @(
        "00_source.png",
        "01_wires.png",
        "02_wire_colors.png",
        "03_symbols.png",
        "04_terminals.png",
        "05_connectors.png",
        "06_splices.png",
        "07_grounds.png",
        "08_labels.png",
        "09_topology.png",
        "10_component_bounds.png",
        "11_endpoint_debug.png",
        "12_recognition.png",
        "13_combined.png",
        "14_symbol_geometry.png",
        "review_manifest.json"
    )
}

function Test-ExtractionArtifactIntegrity {
    <#
    .SYNOPSIS
    Verifies a local artifacts/ tree has a complete review artifact set and
    a structured extraction audit that agree with each other - i.e. both
    came from the same completed WireModel in the same extraction run.

    .OUTPUTS
    A hashtable: @{ Ok = [bool]; Reason = [string] }. Reason is empty when
    Ok is $true, and a human-readable explanation of the first failure
    found otherwise.

    .NOTES
    generated_at in the review manifest is deliberately never read here -
    it is informational only. Identity is determined solely from
    extraction/source/model data (source_id, page, image dimensions) and
    from population counts both documents independently derive from the
    same WireModel collections.
    #>
    param(
        [Parameter(Mandatory = $true)] [string] $ReviewDir,
        [Parameter(Mandatory = $true)] [string] $AuditPath
    )

    foreach ($file in (Get-RequiredReviewLayerFiles)) {
        $candidate = Join-Path $ReviewDir $file
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return @{ Ok = $false; Reason = "Missing review artifact: $candidate" }
        }
    }

    if (-not (Test-Path -LiteralPath $AuditPath -PathType Leaf)) {
        return @{ Ok = $false; Reason = "Missing structured extraction audit: $AuditPath" }
    }

    $manifestPath = Join-Path $ReviewDir "review_manifest.json"

    try {
        $manifestRaw = Get-Content -Raw -LiteralPath $manifestPath
        $manifest = $manifestRaw | ConvertFrom-Json
    }
    catch {
        return @{ Ok = $false; Reason = "Unable to parse review manifest ($manifestPath): $($_.Exception.Message)" }
    }

    try {
        $auditRaw = Get-Content -Raw -LiteralPath $AuditPath
        $audit = $auditRaw | ConvertFrom-Json
    }
    catch {
        return @{ Ok = $false; Reason = "Unable to parse extraction audit ($AuditPath): $($_.Exception.Message)" }
    }

    # Identity: both files must describe the same source, page, and image
    # geometry. This is the canonical "same run" test - not generated_at.
    $identityChecks = @(
        @{ Name = "source_id";    Manifest = [string]$manifest.source_id;     Audit = [string]$audit.source.source_id }
        @{ Name = "page";         Manifest = [int]$manifest.page;             Audit = [int]$audit.source.page }
        @{ Name = "image_width";  Manifest = [int]$manifest.image_width;      Audit = [int]$audit.source.image_width }
        @{ Name = "image_height"; Manifest = [int]$manifest.image_height;     Audit = [int]$audit.source.image_height }
    )
    foreach ($check in $identityChecks) {
        if ($check.Manifest -ne $check.Audit) {
            return @{ Ok = $false; Reason = ("Source identity mismatch on {0}: review_manifest={1} extraction_audit={2}" -f $check.Name, $check.Manifest, $check.Audit) }
        }
    }

    # Population counts: review_manifest.json and extraction_audit.json
    # expose the same underlying WireModel collections under different
    # names/nesting. Both must agree, or one of the two artifacts came
    # from a different run than the other.
    $countChecks = @(
        @{ Name = "wires";               Manifest = [int]$manifest.layers.wires;              Audit = [int]$audit.counts.wires }
        @{ Name = "symbols/components";  Manifest = [int]$manifest.layers.symbols;             Audit = [int]$audit.counts.components }
        @{ Name = "terminals";           Manifest = [int]$manifest.layers.terminals;           Audit = [int]$audit.counts.terminal_candidates }
        @{ Name = "connectors";          Manifest = [int]$manifest.layers.connectors;          Audit = [int]$audit.counts.connectors }
        @{ Name = "connector_terminals"; Manifest = [int]$manifest.layers.connector_terminals; Audit = [int]$audit.counts.connector_terminals }
        @{ Name = "topology_edges";      Manifest = [int]$manifest.layers.topology_edges;      Audit = [int]$audit.counts.topology_edges }
        @{ Name = "electrical_nets";     Manifest = [int]$manifest.electrical_nets;            Audit = [int]$audit.counts.electrical_nets }
        @{ Name = "validation_errors";   Manifest = [int]$manifest.validation_errors;          Audit = [int]$audit.validation.errors }
        @{ Name = "validation_warnings"; Manifest = [int]$manifest.validation_warnings;         Audit = [int]$audit.validation.warnings }
    )
    foreach ($check in $countChecks) {
        if ($check.Manifest -ne $check.Audit) {
            return @{ Ok = $false; Reason = ("Population mismatch on {0}: review_manifest={1} extraction_audit={2}" -f $check.Name, $check.Manifest, $check.Audit) }
        }
    }

    return @{ Ok = $true; Reason = "" }
}
