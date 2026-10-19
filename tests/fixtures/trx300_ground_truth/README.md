# TRX300-GT-001 — Ground-truth location fixture

## Purpose
Machine-readable locations marked by the user on four annotations of the canonical 898 × 549 source image `samples/trx300ODG.png`.

## Counts
| Class | Count | Markup |
|---|---:|---|
| Connectors | 13 | Red rectangles |
| Diodes | 10 | Red rectangles |
| Electrical terminals | 127 | Small red-square markers |
| Wire-color label locations | 95 | Red rectangles |
| **Total** | **245** | |

## Coordinate convention
- Coordinates are zero-based raster pixels, origin upper-left, x rightwards and y downwards.
- Boxes use `[x, y, width, height]`.
- For connectors, diodes and wire-color labels, the box stores the drawn annotation extent, including the outline stroke; it is the user's marked target region, not a pixel-perfect object segmentation.
- For terminals, the box describes the small marker and `point_xy_px` is its center.
- IDs are stable within this fixture, enumerated top-to-bottom and left-to-right within a class.

## Interpretation limits
The data records location/region only. It does not assign diode polarity/type, transcribe wire-color labels, associate labels to conductors, decide net membership, or infer terminal functions. Do not invent unmarked semantics.

The JSON preserves the supplied annotation image filenames and SHA-256 hashes for provenance. Original marked-up PNGs are available in the companion package; this repository fixture commits coordinates and SVG overlays reconstructed from those coordinates over the version-controlled source image.

## Overlays
Open the SVG files locally from a repository checkout to inspect annotations against `samples/trx300ODG.png`: `connectors.svg`, `diodes.svg`, `terminals.svg`, and `wire_color_labels.svg`. Every SVG mark has a `<title>` with its fixture ID.

## Repeatable baseline evaluation

The standard-library evaluator `tools/evaluate_trx300_ground_truth.py` scores the classes for which the extraction audit currently exports appropriate locations: connector boxes and typed terminal points. It uses maximum-cardinality one-to-one matching. Ground-connection candidates are explicitly excluded from terminal scoring.

Run it from the repository root after a production extraction:

```powershell
python tools/evaluate_trx300_ground_truth.py `
  --ground-truth tests/fixtures/trx300_ground_truth/ground_truth.json `
  --audit artifacts/audit/extraction_audit.json `
  --output artifacts/audit/TRX300-GT-001_evaluation.json
```

The defaults, IoU >= 0.50 for connector boxes and a 5 px terminal radius, are exploratory and **not frozen acceptance thresholds**. Change them with `--connector-iou` and `--terminal-radius` for sensitivity studies. The JSON report includes matches, false positives, misses, and unmatched IDs.

Diode and wire-color-label classes are intentionally reported as `not_scored` until production output exports dedicated diode-location predictions and localized text/OCR observations. Do not substitute generic component boxes or endpoint wire-color fields as proxy predictions.

Run evaluator unit tests from the repository root:

```powershell
python -m unittest discover -s tools/tests -p "test_evaluate_trx300_ground_truth.py" -v
```

## Scope
This fixture/evaluation helper does not change production detection logic. Detector thresholds should remain unfrozen until the annotations are reviewed and more than one representative source diagram is evaluated.
