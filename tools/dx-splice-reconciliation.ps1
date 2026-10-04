# EKE-DX-WIRE diagnostic: generate source-level crops for residual zero-wire splice endpoints.
# Diagnostic only. Does not modify extraction logic or production artifacts.
#
# Default source is the current extraction review source image:
#   artifacts/extraction_review/00_source.png
# Default audit is:
#   artifacts/audit/extraction_audit.json
#
# Output:
#   <OutDir>\raw\splice-001.png ...    untouched source crops
#   <OutDir>\annotated\splice-001.png annotated review crops
#   <OutDir>\splice_reconciliation_manifest.json
#   <OutDir>\splice_reconciliation_contact_sheet.png

[CmdletBinding()]
param(
    [string]$SourceImagePath = "artifacts/extraction_review/00_source.png",
    [string]$AuditPath = "artifacts/audit/extraction_audit.json",
    [string]$OutDir = "artifacts/splice_reconciliation",
    [int]$Pad = 32,
    [int]$Scale = 4
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not (Test-Path -LiteralPath $SourceImagePath -PathType Leaf)) {
    throw "Source image not found: $SourceImagePath"
}
if (-not (Test-Path -LiteralPath $AuditPath -PathType Leaf)) {
    throw "Audit JSON not found: $AuditPath"
}
if ($Pad -lt 8) { throw "Pad must be >= 8." }
if ($Scale -lt 1) { throw "Scale must be >= 1." }

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Drawing.Common -ErrorAction SilentlyContinue

$audit = Get-Content -Raw -LiteralPath $AuditPath | ConvertFrom-Json
$nodes = @($audit.objects.topology_nodes)
$edges = @($audit.objects.topology_edges)
$endpoints = @($audit.objects.endpoint_candidates)
$wires = @($audit.objects.wires)

$nodeById = @{}
foreach ($n in $nodes) { $nodeById[[string]$n.id] = $n }

$edgeById = @{}
foreach ($e in $edges) { $edgeById[[string]$e.id] = $e }

$endpointByNode = @{}
foreach ($e in $endpoints) { $endpointByNode[[string]$e.node_id] = $e }

$adj = @{}
function Add-Adj([string]$NodeId, $Edge) {
    if (-not $adj.ContainsKey($NodeId)) {
        $adj[$NodeId] = New-Object System.Collections.ArrayList
    }
    [void]$adj[$NodeId].Add($Edge)
}
foreach ($e in $edges) {
    Add-Adj ([string]$e.from_node) $e
    Add-Adj ([string]$e.to_node) $e
}

function Other-Node([string]$NodeId, $Edge) {
    if ([string]$Edge.from_node -eq $NodeId) { return [string]$Edge.to_node }
    return [string]$Edge.from_node
}

function Is-Distribution([string]$NodeId) {
    $n = $nodeById[$NodeId]
    if ($null -eq $n) { return $false }
    return ([string]$n.type -eq "splice" -or [string]$n.type -eq "junction")
}

$wireEndpointIds = @{}
foreach ($w in $wires) {
    $wireEndpointIds[[string]$w.start_endpoint] = $true
    $wireEndpointIds[[string]$w.end_endpoint] = $true
}

$zeroEndpoints = @(
    $endpoints |
        Where-Object {
            -not $wireEndpointIds.ContainsKey([string]$_.id) -and
            [string]$_.kind -ne "splice" -and
            [string]$_.kind -ne "unresolved" -and
            $adj.ContainsKey([string]$_.node_id) -and
            @($adj[[string]$_.node_id]).Count -eq 1
        } |
        Sort-Object id
)

function Find-Terminal-Splice($Endpoint) {
    $startNode = [string]$Endpoint.node_id
    $incident = @($adj[$startNode])
    if ($incident.Count -ne 1) { return $null }

    $currentNode = Other-Node $startNode $incident[0]
    $previousEdge = [string]$incident[0].id
    $visited = @{}
    $visited[$startNode] = $true

    for ($step = 0; $step -lt 1000; $step++) {
        if ($visited.ContainsKey($currentNode)) {
            return $null
        }
        $visited[$currentNode] = $true

        $current = $nodeById[$currentNode]
        if ($null -eq $current) { return $null }

        # Existing endpoint means this walk does not terminate at a residual splice.
        if ($endpointByNode.ContainsKey($currentNode)) {
            return $null
        }

        $incident = @($adj[$currentNode])
        if ($incident.Count -eq 0) { return $null }

        $others = @($incident | Where-Object { [string]$_.id -ne $previousEdge })
        $arrivalSeg = [string]$edgeById[$previousEdge].conductor_segment

        if ($incident.Count -eq 2) {
            if ($others.Count -ne 1) { return $null }
            $next = $others[0]
            if (Is-Distribution $currentNode) {
                $nextSeg = [string]$edgeById[[string]$next.id].conductor_segment
                if ([string]::IsNullOrWhiteSpace($arrivalSeg) -or
                    $arrivalSeg -ne $nextSeg) {
                    return [pscustomobject]@{
                        node_id      = $currentNode
                        node         = $current
                        arrival_edge = $previousEdge
                        reason       = "degree2_distribution_segment_mismatch"
                    }
                }
            }
            $previousEdge = [string]$next.id
            $currentNode = Other-Node $currentNode $next
            continue
        }

        # Current implementation's distribution rule.
        $matches = @(
            $others |
                Where-Object {
                    $sid = [string]$edgeById[[string]$_.id].conductor_segment
                    -not [string]::IsNullOrWhiteSpace($arrivalSeg) -and $sid -eq $arrivalSeg
                }
        )

        if ($matches.Count -eq 0) {
            return [pscustomobject]@{
                node_id      = $currentNode
                node         = $current
                arrival_edge = $previousEdge
                reason       = "no_same_segment"
            }
        }

        if ($matches.Count -ne 1) {
            return $null
        }

        $next = $matches[0]
        $previousEdge = [string]$next.id
        $currentNode = Other-Node $currentNode $next
    }

    return $null
}

# Deduplicate by terminal splice node. Preserve every residual endpoint that reaches it.
$terminalSplices = @{}
foreach ($endpoint in $zeroEndpoints) {
    $stop = Find-Terminal-Splice $endpoint
    if ($null -eq $stop) { continue }
    if ($null -eq $stop.node -or [string]$stop.node.type -ne "splice") { continue }

    $key = [string]$stop.node_id
    if (-not $terminalSplices.ContainsKey($key)) {
        $terminalSplices[$key] = New-Object System.Collections.ArrayList
    }
    [void]$terminalSplices[$key].Add([pscustomobject]@{
        endpoint_id = [string]$endpoint.id
        x           = [double]$endpoint.x
        y           = [double]$endpoint.y
    })
}

$entries = @(
    $terminalSplices.GetEnumerator() |
        Sort-Object {
            "{0:D10}:{1:D10}:{2}" -f
                [int][math]::Round([double]$_.Value[0].y),
                [int][math]::Round([double]$_.Value[0].x),
                $_.Key
        } |
        ForEach-Object -Begin { $i = 0 } -Process {
            $i++
            $node = $nodeById[[string]$_.Key]
            [pscustomobject]@{
                ordinal     = $i
                splice_id   = [string]$_.Key
                x           = [double]$node.x
                y           = [double]$node.y
                zero_endpoints = @(
                    $_.Value | Sort-Object endpoint_id |
                        ForEach-Object {
                            [pscustomobject]@{
                                id = $_.endpoint_id
                                x  = $_.x
                                y  = $_.y
                            }
                        }
                )
            }
        }
)

$sourceFull = [System.IO.Path]::GetFullPath($SourceImagePath)
$outFull = [System.IO.Path]::GetFullPath($OutDir)
$rawDir = Join-Path $outFull "raw"
$annotatedDir = Join-Path $outFull "annotated"

New-Item -ItemType Directory -Force -Path $rawDir, $annotatedDir | Out-Null

$source = New-Object -TypeName System.Drawing.Bitmap -ArgumentList $sourceFull
if ($audit.source.image_width -and $audit.source.image_height) {
    if ($source.Width -ne [int]$audit.source.image_width -or $source.Height -ne [int]$audit.source.image_height) {
        throw "Source image dimensions ($($source.Width)x$($source.Height)) do not match audit dimensions ($($audit.source.image_width)x$($audit.source.image_height))."
    }
}
try {
    $manifestEntries = New-Object System.Collections.ArrayList
    $rawBitmaps = New-Object System.Collections.ArrayList

    foreach ($entry in $entries) {
        $left = [math]::Max(0, [int][math]::Floor($entry.x - $Pad))
        $top = [math]::Max(0, [int][math]::Floor($entry.y - $Pad))
        $right = [math]::Min($source.Width - 1, [int][math]::Ceiling($entry.x + $Pad))
        $bottom = [math]::Min($source.Height - 1, [int][math]::Ceiling($entry.y + $Pad))
        $width = $right - $left + 1
        $height = $bottom - $top + 1

        $rawCrop = New-Object -TypeName System.Drawing.Bitmap -ArgumentList $width, $height
        try {
            $g = [System.Drawing.Graphics]::FromImage($rawCrop)
            try {
                $g.DrawImage($source,
                    (New-Object System.Drawing.Rectangle(0,0,$width,$height)),
                    (New-Object System.Drawing.Rectangle($left,$top,$width,$height)),
                    [System.Drawing.GraphicsUnit]::Pixel)
            }
            finally { $g.Dispose() }

            $stem = "splice-{0:D3}" -f [int]$entry.ordinal
            $rawPath = Join-Path $rawDir ($stem + ".png")
            $annotatedPath = Join-Path $annotatedDir ($stem + ".png")

            $rawCrop.Save($rawPath, [System.Drawing.Imaging.ImageFormat]::Png)

            $annotated = New-Object -TypeName System.Drawing.Bitmap -ArgumentList ([int]($width * $Scale)), ([int]($height * $Scale))
            try {
                $ga = [System.Drawing.Graphics]::FromImage($annotated)
                try {
                    $ga.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
                    $ga.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
                    $ga.DrawImage($rawCrop,
                        (New-Object System.Drawing.Rectangle(0,0,$annotated.Width,$annotated.Height)),
                        (New-Object System.Drawing.Rectangle(0,0,$rawCrop.Width,$rawCrop.Height)),
                        [System.Drawing.GraphicsUnit]::Pixel)

                    $cx = [single](($entry.x - $left) * $Scale)
                    $cy = [single](($entry.y - $top) * $Scale)
                    $r = [single](5 * $Scale)

                    $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::Red, [single][math]::Max(1, $Scale))
                    try {
                        $ga.DrawEllipse($pen, $cx - $r, $cy - $r, 2*$r, 2*$r)
                        $ga.DrawLine($pen, $cx - 8*$Scale, $cy, $cx + 8*$Scale, $cy)
                        $ga.DrawLine($pen, $cx, $cy - 8*$Scale, $cx, $cy + 8*$Scale)
                    }
                    finally { $pen.Dispose() }

                    $font = New-Object System.Drawing.Font("Consolas", [single][math]::Max(8, 9*$Scale), [System.Drawing.FontStyle]::Bold)
                    $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::Red)
                    try {
                        $label = "#{0}  {1}`n({2:0.0},{3:0.0})" -f $entry.ordinal, $entry.splice_id, $entry.x, $entry.y
                        $ga.DrawString($label, $font, $brush, 2*$Scale, 2*$Scale)
                    }
                    finally {
                        $brush.Dispose()
                        $font.Dispose()
                    }
                }
                finally { $ga.Dispose() }

                $annotated.Save($annotatedPath, [System.Drawing.Imaging.ImageFormat]::Png)
            }
            finally { $annotated.Dispose() }

            [void]$manifestEntries.Add([pscustomobject]@{
                ordinal = $entry.ordinal
                splice_id = $entry.splice_id
                x = $entry.x
                y = $entry.y
                crop = [pscustomobject]@{
                    left = $left; top = $top; width = $width; height = $height
                    pad_requested = $Pad
                }
                zero_endpoints = @($entry.zero_endpoints)
                raw_crop = (Resolve-Path $rawPath).Path
                annotated_crop = (Resolve-Path $annotatedPath).Path
            })
        }
        finally { $rawCrop.Dispose() }
    }

    $manifest = [pscustomobject]@{
        schema_version = 1
        source_image = $sourceFull
        audit = [System.IO.Path]::GetFullPath($AuditPath)
        source_dimensions = [pscustomobject]@{ width = $source.Width; height = $source.Height }
        crop_padding = $Pad
        annotation_scale = $Scale
        residual_zero_wire_endpoints = $zeroEndpoints.Count
        terminal_splices = $entries.Count
        entries = @($manifestEntries)
    }
    $manifestPath = Join-Path $outFull "splice_reconciliation_manifest.json"
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

    # Contact sheet from annotated crops, fixed 5 columns for deterministic review order.
    $cols = 5
    $cellW = 0
    $cellH = 0
    $bitmaps = @()
    try {
        foreach ($m in $manifestEntries) {
            $bmp = New-Object -TypeName System.Drawing.Bitmap -ArgumentList ([string]$m.annotated_crop)
            $bitmaps += $bmp
            $cellW = [math]::Max($cellW, $bmp.Width)
            $cellH = [math]::Max($cellH, $bmp.Height)
        }

        $rows = [int][math]::Ceiling($bitmaps.Count / [double]$cols)
        if ($rows -lt 1) { $rows = 1 }

        $sheet = New-Object -TypeName System.Drawing.Bitmap -ArgumentList ([int]($cols*$cellW)), ([int]($rows*$cellH))
        try {
            $gs = [System.Drawing.Graphics]::FromImage($sheet)
            try {
                $gs.Clear([System.Drawing.Color]::White)
                $gs.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
                for ($i = 0; $i -lt $bitmaps.Count; $i++) {
                    $x = ($i % $cols) * $cellW
                    $y = [int]([math]::Floor($i / [double]$cols)) * $cellH
                    $gs.DrawImage($bitmaps[$i], $x, $y)
                }
            }
            finally { $gs.Dispose() }

            $contactPath = Join-Path $outFull "splice_reconciliation_contact_sheet.png"
            $sheet.Save($contactPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally { $sheet.Dispose() }
    }
    finally {
        foreach ($bmp in $bitmaps) { $bmp.Dispose() }
    }

    Write-Host "[DX-SPLICE-AUDIT] Residual zero-wire endpoints : $($zeroEndpoints.Count)" -ForegroundColor Cyan
    Write-Host "[DX-SPLICE-AUDIT] Terminal splice nodes       : $($entries.Count)" -ForegroundColor Cyan
    Write-Host "[DX-SPLICE-AUDIT] Raw crops                  : $rawDir" -ForegroundColor Green
    Write-Host "[DX-SPLICE-AUDIT] Annotated crops            : $annotatedDir" -ForegroundColor Green
    Write-Host "[DX-SPLICE-AUDIT] Manifest                   : $manifestPath" -ForegroundColor Green
    Write-Host "[DX-SPLICE-AUDIT] Contact sheet              : $(Join-Path $outFull 'splice_reconciliation_contact_sheet.png')" -ForegroundColor Green
}
finally {
    $source.Dispose()
}
