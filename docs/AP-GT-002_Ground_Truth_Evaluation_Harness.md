# AP-GT-002 — TRX300 Ground-Truth Evaluation Harness

## Status

Diagnostic/tooling-only proposal. It introduces a deterministic evaluator for the independent AP-GT-001 annotation fixture and the current `extraction_audit.json`. It does not modify the production extractor, detector thresholds, topology, terminals, OCR, or review images.

## Purpose

Provide a repeatable answer to three different questions without conflating them:

1. How well do existing structured predictions localize annotated connector and terminal locations?
2. Which annotation classes cannot yet be evaluated because the current audit does not export suitable predictions?
3. Which review-manifest counters are model/render inventory sizes rather than recognition measurements?

The evaluator deliberately reports a class as `unscored` when its category-specific prediction inventory is absent. A present but empty prediction array is scored as zero detections, not mislabeled as unavailable.

## Inputs

- `tests/fixtures/trx300_ground_truth/ground_truth.json` — user-marked location/region annotations.
- `artifacts/audit/extraction_audit.json` — audit from the extraction run being evaluated.
- Optional `artifacts/extraction_review/review_manifest.json` — emitted only as contextual inventory in the report, never treated as model predictions.

The tool checks known source dimensions when they are supplied. A mismatch fails closed instead of comparing coordinates from different images.

## Matching contract

- Connector bounding boxes use a configurable IoU threshold; the default 0.50 is exploratory, not a release criterion.
- Terminal points use configurable Euclidean-distance radii; defaults are 2, 5, and 8 pixels, all exploratory.
- Assignment is one-to-one maximum-cardinality bipartite matching. One predicted object cannot claim multiple ground-truth annotations, and one annotation cannot satisfy multiple predictions.
- Terminal predictions include `TerminalCandidate` objects of kind `component_boundary` or `connector_boundary`, plus explicit `connector_terminals` objects. `ground_connection` candidates are excluded from terminal scoring. This preserves the distinction between grounded components and physical terminals.
- The annotated diode regions mark small inline diode glyphs. A generic `circular_symbol`, an enclosure labelled “RECTIFIER”, or component-family text alone is not accepted as a location prediction for those inline glyphs. The evaluator only scores explicit `diode_detections`/`diode_regions` boxes.
- Wire-color-label location is scored only when the audit explicitly exports `wire_color_label_regions` or `wire_color_label_detections`. Endpoint `wire_color` semantics or total semantic-association count are not text-location predictions.

## Baseline reproduced from the 2026-10-18 extraction audit

With provisional connector IoU 0.50 and terminal radii 2/5/8 pixels:

| Category / threshold | Predictions | Ground truth | Matches | Precision | Recall | F1 |
|---|---:|---:|---:|---:|---:|---:|
| Connector boxes, IoU ≥ 0.50 | 2 | 13 | 1 | 0.5000 | 0.0769 | 0.1333 |
| Terminal points, ≤ 2 px | 18 | 127 | 9 | 0.5000 | 0.0709 | 0.1241 |
| Terminal points, ≤ 5 px | 18 | 127 | 12 | 0.6667 | 0.0945 | 0.1655 |
| Terminal points, ≤ 8 px | 18 | 127 | 13 | 0.7222 | 0.1024 | 0.1793 |
| Inline diode regions | — | 10 | — | unscored | unscored | unscored |
| Wire-color-label regions | — | 95 | — | unscored | unscored | unscored |

The focus connector has a high-IoU match, but whole-page connector recall is low. Terminal matches are a location-only signal; they do not establish correct component ownership, terminal function, or electrical connectivity. Diode and wire-color-label classes are unscored because this audit format does not include suitable class-specific prediction lists.

## Manifest counters: do not interpret as class accuracy

The current review writer populates `layers.wire_colors` from conductor-segment count, `layers.labels` from semantic-association count, and `layers.terminals` from the terminal-candidate collection. Those counts are not equivalent to localized wire-color labels, recognized label text, or confirmed physical terminals. The report includes the counters and a warning when a manifest is supplied, but makes no claim that they are accuracy scores.

Changing the manifest schema and labels is a separate artifact-contract change; AP-GT-002 does not mutate production manifest output.

## Usage (Python 3.10+, standard library only)

From the repository root:

```powershell
py -3 tools\dx-gt-evaluate.py `
  --ground-truth tests\fixtures\trx300_ground_truth\ground_truth.json `
  --audit artifacts\audit\extraction_audit.json `
  --manifest artifacts\extraction_review\review_manifest.json `
  --output artifacts\audit\TRX300-GT-001_evaluation.json

py -3 tools\tests\test_dx_gt_evaluate.py
```

To adjust provisional thresholds, use `--connector-iou 0.50` and `--terminal-radii 2,5,8`. Do not select acceptance thresholds by optimizing to one diagram; freeze them only after annotation review and a broader benchmark set.

## Validation and scope

The evaluator's 7 standard-library unit tests passed in the development environment against synthetic cases; an additional direct run against the uploaded 2026-10-18 audit reproduces the table above. The full C++/CMake build was not rerun because this change does not touch C++ production code or CMake targets. Windows validation is still required before treating the tool as accepted repository infrastructure.

This AP intentionally does not “fix” terminal recall. It makes the gap reproducible and records missing prediction interfaces so the follow-up production APs can be bounded and measured rather than changing multiple extraction stages at once.
