#!/usr/bin/env python3
"""Evaluate current EKE-DX-WIRE audit predictions against TRX300-GT-001.

This is an offline, read-only diagnostic. It does not alter the extractor,
its output, or the ground-truth file. It only scores categories for which the
input audit contains a defensible, structured prediction inventory.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any, Callable

REPORT_SCHEMA = "eke-dx-wire-ground-truth-evaluation/v1"


def read_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8-sig") as stream:
            value = json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"Cannot read valid JSON from {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object at {path}")
    return value


def require_bbox(item: dict[str, Any], context: str) -> tuple[float, float, float, float]:
    """Return x, y, width, height from a GT or audit object."""
    raw = item.get("bbox_xywh_px")
    if raw is not None:
        if not isinstance(raw, list) or len(raw) != 4:
            raise ValueError(f"{context}: bbox_xywh_px must contain [x,y,width,height]")
        x, y, width, height = raw
    else:
        names = ("x", "y", "width", "height")
        if not all(name in item for name in names):
            raise ValueError(f"{context}: missing bbox_xywh_px or x/y/width/height")
        x, y, width, height = (item[name] for name in names)
    try:
        box = tuple(float(v) for v in (x, y, width, height))
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{context}: bbox values must be numeric") from exc
    if not all(math.isfinite(v) for v in box) or box[2] <= 0 or box[3] <= 0:
        raise ValueError(f"{context}: bbox has non-finite or non-positive dimensions")
    return box  # type: ignore[return-value]


def point_xy(item: dict[str, Any], context: str) -> tuple[float, float]:
    raw = item.get("point_xy_px") or item.get("center_xy_px")
    if raw is None and "x" in item and "y" in item and "width" not in item and "height" not in item:
        raw = [item["x"], item["y"]]
    if raw is None:
        x, y, width, height = require_bbox(item, context)
        return x + (width - 1.0) / 2.0, y + (height - 1.0) / 2.0
    if not isinstance(raw, list) or len(raw) != 2:
        raise ValueError(f"{context}: point_xy_px/center_xy_px must contain [x,y]")
    try:
        x, y = float(raw[0]), float(raw[1])
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{context}: point coordinates must be numeric") from exc
    if not math.isfinite(x) or not math.isfinite(y):
        raise ValueError(f"{context}: point coordinates must be finite")
    return x, y


def iou_xywh(a: tuple[float, float, float, float], b: tuple[float, float, float, float]) -> float:
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    left, top = max(ax, bx), max(ay, by)
    right, bottom = min(ax + aw, bx + bw), min(ay + ah, by + bh)
    iw, ih = max(0.0, right - left), max(0.0, bottom - top)
    intersection = iw * ih
    union = aw * ah + bw * bh - intersection
    return intersection / union if union > 0 else 0.0


def point_distance(a: tuple[float, float], b: tuple[float, float]) -> float:
    return math.hypot(a[0] - b[0], a[1] - b[1])


def maximum_cardinality_match(
    prediction_count: int,
    truth_count: int,
    score: Callable[[int, int], float],
    eligible: Callable[[float], bool],
    *,
    higher_is_better: bool,
) -> list[tuple[int, int, float]]:
    """Deterministic maximum-cardinality bipartite matching.

    Edges per prediction are ordered by best score first. Augmenting paths
    maximize the number of valid one-to-one matches, avoiding duplicate
    claims on a single ground-truth object.
    """
    adjacency: list[list[tuple[int, float]]] = []
    for p in range(prediction_count):
        edges = []
        for g in range(truth_count):
            value = score(p, g)
            if eligible(value):
                edges.append((g, value))
        edges.sort(key=lambda pair: ((-pair[1] if higher_is_better else pair[1]), pair[0]))
        adjacency.append(edges)

    pred_order = sorted(
        range(prediction_count),
        key=lambda p: (len(adjacency[p]), p),
    )
    matched_truth_to_prediction: dict[int, int] = {}

    def augment(prediction: int, seen_truth: set[int]) -> bool:
        for truth, _ in adjacency[prediction]:
            if truth in seen_truth:
                continue
            seen_truth.add(truth)
            prior = matched_truth_to_prediction.get(truth)
            if prior is None or augment(prior, seen_truth):
                matched_truth_to_prediction[truth] = prediction
                return True
        return False

    for prediction in pred_order:
        augment(prediction, set())

    result = []
    for truth, prediction in matched_truth_to_prediction.items():
        result.append((prediction, truth, score(prediction, truth)))
    result.sort(key=lambda row: (row[1], row[0]))
    return result


def score_summary(prediction_count: int, truth_count: int, matches: list[tuple[int, int, float]]) -> dict[str, Any]:
    true_positive = len(matches)
    false_positive = prediction_count - true_positive
    false_negative = truth_count - true_positive
    precision = true_positive / prediction_count if prediction_count else 0.0
    recall = true_positive / truth_count if truth_count else 0.0
    f1 = (2 * precision * recall / (precision + recall)) if precision + recall else 0.0
    return {
        "prediction_count": prediction_count,
        "ground_truth_count": truth_count,
        "true_positives": true_positive,
        "false_positives": false_positive,
        "false_negatives": false_negative,
        "precision": round(precision, 6),
        "recall": round(recall, 6),
        "f1": round(f1, 6),
    }


def annotation_category(item: dict[str, Any]) -> str:
    category = item.get("category")
    if not isinstance(category, str) or not category:
        raise ValueError(f"Ground-truth annotation has no category: {item.get('id', '<no id>')}")
    return category


def truth_by_category(ground_truth: dict[str, Any]) -> dict[str, list[dict[str, Any]]]:
    raw = ground_truth.get("annotations")
    if not isinstance(raw, list):
        raise ValueError("Ground truth must contain an 'annotations' array")
    result: dict[str, list[dict[str, Any]]] = {}
    seen_ids: set[str] = set()
    for index, item in enumerate(raw):
        if not isinstance(item, dict):
            raise ValueError(f"Ground-truth annotation #{index} is not an object")
        item_id = item.get("id")
        if not isinstance(item_id, str) or not item_id:
            raise ValueError(f"Ground-truth annotation #{index} has no stable id")
        if item_id in seen_ids:
            raise ValueError(f"Duplicate ground-truth annotation id: {item_id}")
        seen_ids.add(item_id)
        result.setdefault(annotation_category(item), []).append(item)
    return result


def audit_objects(audit: dict[str, Any]) -> dict[str, Any]:
    objects = audit.get("objects")
    if not isinstance(objects, dict):
        raise ValueError("Extraction audit must contain an 'objects' object")
    return objects


def get_bbox_predictions(items: Any, prefix: str) -> list[dict[str, Any]]:
    if not isinstance(items, list):
        return []
    result = []
    for index, item in enumerate(items):
        if not isinstance(item, dict):
            continue
        # Only predictions with valid, explicit boxes are scoreable.
        try:
            box = require_bbox(item, f"{prefix}[{index}]")
        except ValueError:
            continue
        result.append({"id": str(item.get("id", f"{prefix}-{index:04d}")), "bbox": box, "raw": item})
    return result


def get_terminal_predictions(objects: dict[str, Any]) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    """Build an explicit terminal inventory; never promote ground connection to terminal."""
    candidates = objects.get("terminal_candidates", [])
    connector_terminals = objects.get("connector_terminals", [])
    predictions: list[dict[str, Any]] = []
    counts = {"component_boundary": 0, "connector_boundary": 0, "ground_connection_excluded": 0, "unknown_excluded": 0, "connector_terminal": 0}

    if isinstance(candidates, list):
        for index, item in enumerate(candidates):
            if not isinstance(item, dict):
                continue
            kind = item.get("kind", "unknown")
            if kind == "ground_connection":
                counts["ground_connection_excluded"] += 1
                continue
            if kind not in ("component_boundary", "connector_boundary"):
                counts["unknown_excluded"] += 1
                continue
            try:
                point = point_xy(item, f"terminal_candidates[{index}]")
            except ValueError:
                continue
            counts[kind] += 1
            predictions.append({"id": str(item.get("id", f"terminal-candidate-{index:04d}")), "point": point, "source": "terminal_candidates", "kind": kind})

    if isinstance(connector_terminals, list):
        for index, item in enumerate(connector_terminals):
            if not isinstance(item, dict):
                continue
            try:
                point = point_xy(item, f"connector_terminals[{index}]")
            except ValueError:
                continue
            counts["connector_terminal"] += 1
            predictions.append({"id": str(item.get("id", f"connector-terminal-{index:04d}")), "point": point, "source": "connector_terminals", "kind": "connector_terminal"})

    inventory = {
        "included_sources": ["objects.terminal_candidates[kind=component_boundary|connector_boundary]", "objects.connector_terminals"],
        "excluded_ground_connection_count": counts["ground_connection_excluded"],
        "excluded_unknown_kind_count": counts["unknown_excluded"],
        "count_by_included_kind": {k: counts[k] for k in ("component_boundary", "connector_boundary", "connector_terminal")},
        "prediction_count": len(predictions),
        "policy": "Ground connections are not scored as terminals. Component-boundary candidates and connector-terminal objects are included as distinct predictions; one-to-one matching prevents a single annotation from being claimed twice.",
    }
    return predictions, inventory


def metric_for_boxes(
    category: str,
    truths: list[dict[str, Any]],
    predictions: list[dict[str, Any]],
    threshold: float,
) -> dict[str, Any]:
    truth_boxes = [require_bbox(t, f"ground_truth.{category}[{t.get('id')}]") for t in truths]
    pred_boxes = [p["bbox"] for p in predictions]
    matches = maximum_cardinality_match(
        len(pred_boxes), len(truth_boxes),
        lambda p, g: iou_xywh(pred_boxes[p], truth_boxes[g]),
        lambda value: value >= threshold,
        higher_is_better=True,
    )
    result = score_summary(len(pred_boxes), len(truth_boxes), matches)
    result.update({
        "status": "scored",
        "criterion": f"IoU >= {threshold:.3f}",
        "matches": [
            {"prediction_id": predictions[p]["id"], "ground_truth_id": truths[g]["id"], "iou": round(score, 6)}
            for p, g, score in matches
        ],
        "unmatched_prediction_ids": [predictions[p]["id"] for p in range(len(predictions)) if p not in {m[0] for m in matches}],
        "unmatched_ground_truth_ids": [truths[g]["id"] for g in range(len(truths)) if g not in {m[1] for m in matches}],
    })
    return result


def metric_for_points(
    truths: list[dict[str, Any]],
    predictions: list[dict[str, Any]],
    radius: float,
) -> dict[str, Any]:
    truth_points = [point_xy(t, f"ground_truth.terminal[{t.get('id')}]") for t in truths]
    pred_points = [p["point"] for p in predictions]
    matches = maximum_cardinality_match(
        len(pred_points), len(truth_points),
        lambda p, g: point_distance(pred_points[p], truth_points[g]),
        lambda value: value <= radius,
        higher_is_better=False,
    )
    result = score_summary(len(pred_points), len(truth_points), matches)
    pred_matched = {m[0] for m in matches}
    gt_matched = {m[1] for m in matches}
    result.update({
        "status": "scored",
        "criterion": f"Euclidean distance <= {radius:.3f} px",
        "matches": [
            {"prediction_id": predictions[p]["id"], "prediction_source": predictions[p]["source"], "ground_truth_id": truths[g]["id"], "distance_px": round(score, 6)}
            for p, g, score in matches
        ],
        "unmatched_prediction_ids": [predictions[p]["id"] for p in range(len(predictions)) if p not in pred_matched],
        "unmatched_ground_truth_ids": [truths[g]["id"] for g in range(len(truths)) if g not in gt_matched],
    })
    return result


def unscored(reason: str, ground_truth_count: int) -> dict[str, Any]:
    return {
        "status": "unscored",
        "ground_truth_count": ground_truth_count,
        "prediction_count": None,
        "reason": reason,
        "precision": None,
        "recall": None,
        "f1": None,
    }


def find_diode_predictions(objects: dict[str, Any]) -> tuple[bool, list[dict[str, Any]], str]:
    # These ground-truth annotations mark the small inline diode-like device
    # glyphs. A component-family classification without an explicit bbox for
    # that glyph is not the same target and must not be scored against it.
    for key in ("diode_detections", "diode_regions"):
        if key in objects and isinstance(objects[key], list):
            return True, get_bbox_predictions(objects[key], key), f"objects.{key}"
    return False, [], ""


def find_wire_color_label_predictions(objects: dict[str, Any]) -> tuple[bool, list[dict[str, Any]], str]:
    # Only explicit wire-color-label region predictions count as class-specific
    # prediction. Generic text regions must not be relabelled as wire-color labels.
    for key in ("wire_color_label_regions", "wire_color_label_detections"):
        if key in objects and isinstance(objects[key], list):
            return True, get_bbox_predictions(objects[key], key), f"objects.{key}"
    return False, [], ""


def evaluate(ground_truth: dict[str, Any], audit: dict[str, Any], connector_iou: float, terminal_radii: list[float]) -> dict[str, Any]:
    gt = truth_by_category(ground_truth)
    objects = audit_objects(audit)
    source = audit.get("source", {}) if isinstance(audit.get("source"), dict) else {}
    gt_source = ground_truth.get("source", {}) if isinstance(ground_truth.get("source"), dict) else {}
    if source.get("image_width") and gt_source.get("width") and source["image_width"] != gt_source["width"]:
        raise ValueError(f"Source width mismatch: audit={source['image_width']} ground_truth={gt_source['width']}")
    if source.get("image_height") and gt_source.get("height") and source["image_height"] != gt_source["height"]:
        raise ValueError(f"Source height mismatch: audit={source['image_height']} ground_truth={gt_source['height']}")

    connector_truth = gt.get("connector", [])
    connector_predictions = get_bbox_predictions(objects.get("connectors", []), "connectors")
    connector_metric = metric_for_boxes("connector", connector_truth, connector_predictions, connector_iou)

    terminal_truth = gt.get("terminal", [])
    terminal_predictions, terminal_inventory = get_terminal_predictions(objects)
    terminal_metrics = {f"within_{radius:g}_px": metric_for_points(terminal_truth, terminal_predictions, radius) for radius in terminal_radii}

    diode_truth = gt.get("diode", [])
    diode_available, diode_predictions, diode_source = find_diode_predictions(objects)
    diode_metric = metric_for_boxes("diode", diode_truth, diode_predictions, connector_iou) if diode_available else unscored(
        "The audit does not export a class-specific diode detection list or symbol-family resolution collection. Coarse component kinds are not treated as diode predictions.", len(diode_truth))

    label_truth = gt.get("wire_color_label", gt.get("wire-color-label", []))
    labels_available, label_predictions, label_source = find_wire_color_label_predictions(objects)
    label_metric = metric_for_boxes("wire_color_label", label_truth, label_predictions, connector_iou) if labels_available else unscored(
        "The audit does not export class-specific wire-color-label bounding boxes. Endpoint wire_color values or generic semantic-association counts are not treated as label-localization predictions.", len(label_truth))

    recognition = {
        "text_regions_exported": isinstance(objects.get("text_regions"), list),
        "text_recognition_evidence_exported": isinstance(objects.get("text_recognition_evidence"), list),
        "text_semantic_evidence_exported": isinstance(objects.get("text_semantic_evidence"), list),
        "symbol_family_resolutions_exported": isinstance(objects.get("symbol_family_resolutions"), list),
        "symbol_family_evidence_exported": isinstance(objects.get("symbol_family_evidence"), list),
    }
    return {
        "schema": REPORT_SCHEMA,
        "benchmark_id": ground_truth.get("benchmark_id", "unspecified"),
        "input": {
            "source_id": source.get("source_id", audit.get("run_identity", {}).get("source_id")),
            "image_width": source.get("image_width"),
            "image_height": source.get("image_height"),
            "ground_truth_annotation_count": len(ground_truth.get("annotations", [])),
        },
        "evaluation_policy": {
            "status": "exploratory_no_release_thresholds_frozen",
            "matching": "Deterministic one-to-one maximum-cardinality matching; connector/diode/label bounding boxes use IoU; terminals use Euclidean distance.",
            "connector_iou_threshold": connector_iou,
            "terminal_radii_px": terminal_radii,
            "terminal_prediction_policy": terminal_inventory["policy"],
            "ground_connections_are_not_terminals": True,
        },
        "prediction_inventory": {
            "connectors": {"source": "objects.connectors", "count": len(connector_predictions)},
            "terminals": terminal_inventory,
            "diodes": {"source": diode_source or "unavailable", "prediction_inventory_available": diode_available, "count": len(diode_predictions) if diode_available else None},
            "wire_color_labels": {"source": label_source or "unavailable", "prediction_inventory_available": labels_available, "count": len(label_predictions) if labels_available else None},
            "recognition_capabilities": recognition,
        },
        "metrics": {
            "connector_localization": connector_metric,
            "terminal_localization": terminal_metrics,
            "diode_localization": diode_metric,
            "wire_color_label_localization": label_metric,
        },
        "semantic_limits": [
            "A location match does not establish correct object identity, terminal function, wire connectivity, or net membership.",
            "A diode or wire-color-label class is unscored when the audit has no dedicated prediction inventory; this is not interpreted as zero model accuracy.",
            "Wire-color label text recognition and label-to-conductor association require separate evidence and metrics.",
        ],
    }


def parse_radii(value: str) -> list[float]:
    try:
        values = [float(part.strip()) for part in value.split(",") if part.strip()]
    except ValueError as exc:
        raise argparse.ArgumentTypeError("radii must be comma-separated numbers") from exc
    if not values or any(not math.isfinite(v) or v < 0 for v in values):
        raise argparse.ArgumentTypeError("radii must be a non-empty list of finite non-negative values")
    if len(set(values)) != len(values):
        raise argparse.ArgumentTypeError("radii must not contain duplicates")
    return values


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ground-truth", required=True, type=Path, help="TRX300-GT-001 ground_truth.json")
    parser.add_argument("--audit", required=True, type=Path, help="extraction_audit.json from the current run")
    parser.add_argument("--manifest", type=Path, help="optional review_manifest.json (reported only for schema/context; not used as detector predictions)")
    parser.add_argument("--output", type=Path, help="write report JSON here; stdout when omitted")
    parser.add_argument("--connector-iou", type=float, default=0.50, help="exploratory bbox IoU threshold (default: 0.50)")
    parser.add_argument("--terminal-radii", type=parse_radii, default=parse_radii("2,5,8"), help="exploratory terminal matching radii in px (default: 2,5,8)")
    args = parser.parse_args(argv)
    if not math.isfinite(args.connector_iou) or not 0.0 <= args.connector_iou <= 1.0:
        parser.error("--connector-iou must be between 0 and 1")

    try:
        ground_truth = read_json(args.ground_truth)
        audit = read_json(args.audit)
        report = evaluate(ground_truth, audit, args.connector_iou, args.terminal_radii)
        if args.manifest:
            manifest = read_json(args.manifest)
            layers = manifest.get("layers", {}) if isinstance(manifest.get("layers"), dict) else {}
            report["manifest_context"] = {
                "path": str(args.manifest),
                "generated_at": manifest.get("generated_at"),
                "source_id": manifest.get("source_id"),
                "layer_counts_not_treated_as_predictions": {
                    "wire_colors": layers.get("wire_colors"),
                    "labels": layers.get("labels"),
                    "terminals": layers.get("terminals"),
                },
                "warning": "Review-manifest layer counters describe renderer/model collections; they are not class-accuracy measurements. See the manifest schema documentation before interpreting them as detections.",
            }
        payload = json.dumps(report, indent=2, sort_keys=True) + "\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(payload, encoding="utf-8", newline="\n")
        else:
            sys.stdout.write(payload)
    except (ValueError, OSError) as exc:
        print(f"dx-gt-evaluate: error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
