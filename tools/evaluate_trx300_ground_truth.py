#!/usr/bin/env python3
"""Evaluate available TRX300 production location predictions against GT-001.

Standard-library only. The current audit schema supports connector boxes and
typed terminal locations. Diode and wire-color-label scores remain explicitly
unscored until dedicated prediction collections are exported.
"""
from __future__ import annotations

import argparse
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class Prediction:
    prediction_id: str
    values: tuple[float, ...]
    source_kind: str


def load_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8-sig") as stream:
            value = json.load(stream)
    except OSError as exc:
        raise ValueError(f"Cannot read {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise ValueError(f"Invalid JSON in {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object in {path}")
    return value


def bbox_iou(a: tuple[float, ...], b: tuple[float, ...]) -> float:
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    left, top = max(ax, bx), max(ay, by)
    right, bottom = min(ax + aw, bx + bw), min(ay + ah, by + bh)
    intersection = max(0.0, right - left) * max(0.0, bottom - top)
    union = aw * ah + bw * bh - intersection
    return intersection / union if union > 0.0 else 0.0


def point_distance(a: tuple[float, ...], b: tuple[float, ...]) -> float:
    return math.hypot(a[0] - b[0], a[1] - b[1])


def maximum_cardinality_matching(
    ground_truth_count: int,
    prediction_count: int,
    candidates: list[list[tuple[int, float]]],
) -> list[tuple[int, int, float]]:
    """Maximum-cardinality bipartite matching; quality breaks traversal ties."""
    prediction_to_gt: dict[int, tuple[int, float]] = {}

    def augment(gt_index: int, seen: set[int]) -> bool:
        for pred_index, quality in candidates[gt_index]:
            if pred_index in seen:
                continue
            seen.add(pred_index)
            current = prediction_to_gt.get(pred_index)
            if current is None or augment(current[0], seen):
                prediction_to_gt[pred_index] = (gt_index, quality)
                return True
        return False

    for gt_index in range(ground_truth_count):
        augment(gt_index, set())

    matches = [
        (gt_index, pred_index, quality)
        for pred_index, (gt_index, quality) in prediction_to_gt.items()
    ]
    return sorted(matches, key=lambda item: (item[0], item[1]))


def metric_summary(
    gt_count: int, predictions: list[Prediction],
    matches: list[tuple[int, int, float]],
) -> dict[str, Any]:
    tp = len(matches)
    fp = len(predictions) - tp
    fn = gt_count - tp
    precision = tp / len(predictions) if predictions else 0.0
    recall = tp / gt_count if gt_count else 0.0
    f1 = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
    return {
        "ground_truth_count": gt_count,
        "prediction_count": len(predictions),
        "true_positive": tp,
        "false_positive": fp,
        "false_negative": fn,
        "precision": round(precision, 6),
        "recall": round(recall, 6),
        "f1": round(f1, 6),
    }


def gt_records(gt: dict[str, Any], category: str) -> list[dict[str, Any]]:
    return sorted(
        (row for row in gt.get("annotations", []) if row.get("category") == category),
        key=lambda row: row.get("id", ""),
    )


def get_connector_predictions(audit: dict[str, Any]) -> list[Prediction]:
    rows = audit.get("objects", {}).get("connectors")
    if not isinstance(rows, list):
        raise ValueError("Audit does not contain objects.connectors")
    result = []
    for row in rows:
        try:
            box = tuple(float(row[key]) for key in ("x", "y", "width", "height"))
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError(f"Malformed connector candidate: {row!r}") from exc
        if box[2] <= 0 or box[3] <= 0:
            raise ValueError(f"Non-positive connector bounds: {row!r}")
        result.append(Prediction(str(row.get("id", "")), box, "connector_candidate"))
    return sorted(result, key=lambda p: p.prediction_id)


def get_terminal_predictions(
    audit: dict[str, Any],
) -> tuple[list[Prediction], dict[str, int]]:
    objects = audit.get("objects", {})
    candidates = objects.get("terminal_candidates")
    connector_terminals = objects.get("connector_terminals")
    if not isinstance(candidates, list) or not isinstance(connector_terminals, list):
        raise ValueError(
            "Audit must contain objects.terminal_candidates and objects.connector_terminals"
        )

    predictions: list[Prediction] = []
    excluded_ground = 0
    included_typed = 0
    for row in candidates:
        kind = str(row.get("kind", "unknown"))
        if kind == "ground_connection":
            excluded_ground += 1
            continue
        if kind not in {"component_boundary", "connector_boundary"}:
            continue
        try:
            point = (float(row["x"]), float(row["y"]))
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError(f"Malformed terminal candidate: {row!r}") from exc
        predictions.append(Prediction(str(row.get("id", "")), point, kind))
        included_typed += 1

    for row in connector_terminals:
        try:
            point = (float(row["x"]), float(row["y"]))
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError(f"Malformed connector terminal: {row!r}") from exc
        predictions.append(
            Prediction(str(row.get("id", "")), point, "connector_terminal")
        )

    predictions.sort(key=lambda p: (p.prediction_id, p.source_kind))
    return predictions, {
        "terminal_candidates_component_or_connector_boundary": included_typed,
        "terminal_candidates_excluded_ground_connection": excluded_ground,
        "connector_terminal_objects": len(connector_terminals),
    }


def score_connectors(
    gt_rows: list[dict[str, Any]], predictions: list[Prediction], threshold: float
) -> dict[str, Any]:
    gt_boxes = [tuple(float(v) for v in row["bbox_xywh_px"]) for row in gt_rows]
    candidates: list[list[tuple[int, float]]] = []
    all_ious: list[list[float]] = []
    for box in gt_boxes:
        ious = [bbox_iou(box, pred.values) for pred in predictions]
        all_ious.append(ious)
        row = [(j, value) for j, value in enumerate(ious) if value >= threshold]
        row.sort(key=lambda pair: (-pair[1], predictions[pair[0]].prediction_id))
        candidates.append(row)

    matches = maximum_cardinality_matching(len(gt_rows), len(predictions), candidates)
    summary = metric_summary(len(gt_rows), predictions, matches)
    summary["matching_rule"] = {
        "type": "bbox_iou",
        "minimum_iou": threshold,
        "threshold_status": "exploratory_not_frozen",
    }
    summary["matches"] = [
        {
            "ground_truth_id": gt_rows[gi]["id"],
            "prediction_id": predictions[pi].prediction_id,
            "iou": round(score, 6),
        }
        for gi, pi, score in matches
    ]
    matched_gt = {gi for gi, _, _ in matches}
    matched_pred = {pi for _, pi, _ in matches}
    summary["unmatched_ground_truth_ids"] = [
        gt_rows[i]["id"] for i in range(len(gt_rows)) if i not in matched_gt
    ]
    summary["unmatched_prediction_ids"] = [
        predictions[i].prediction_id for i in range(len(predictions)) if i not in matched_pred
    ]
    summary["best_iou_per_prediction"] = [
        {
            "prediction_id": pred.prediction_id,
            "best_iou": round(max(all_ious[gi][pi] for gi in range(len(gt_rows))), 6)
            if gt_rows else 0.0,
        }
        for pi, pred in enumerate(predictions)
    ]
    return summary


def score_terminals(
    gt_rows: list[dict[str, Any]], predictions: list[Prediction], radius: float
) -> dict[str, Any]:
    gt_points = [
        tuple(float(v) for v in row.get("point_xy_px", row["center_xy_px"]))
        for row in gt_rows
    ]
    candidates: list[list[tuple[int, float]]] = []
    distances_all: list[list[float]] = []
    for point in gt_points:
        distances = [point_distance(point, pred.values) for pred in predictions]
        distances_all.append(distances)
        row = [(j, value) for j, value in enumerate(distances) if value <= radius]
        row.sort(key=lambda pair: (
            pair[1], predictions[pair[0]].prediction_id,
            predictions[pair[0]].source_kind,
        ))
        candidates.append(row)

    matches = maximum_cardinality_matching(len(gt_rows), len(predictions), candidates)
    summary = metric_summary(len(gt_rows), predictions, matches)
    summary["matching_rule"] = {
        "type": "point_distance_px",
        "maximum_distance_px": radius,
        "threshold_status": "exploratory_not_frozen",
    }
    summary["matches"] = [
        {
            "ground_truth_id": gt_rows[gi]["id"],
            "prediction_id": predictions[pi].prediction_id,
            "prediction_source_kind": predictions[pi].source_kind,
            "distance_px": round(distance, 4),
        }
        for gi, pi, distance in matches
    ]
    matched_gt = {gi for gi, _, _ in matches}
    matched_pred = {pi for _, pi, _ in matches}
    summary["unmatched_ground_truth_ids"] = [
        gt_rows[i]["id"] for i in range(len(gt_rows)) if i not in matched_gt
    ]
    summary["unmatched_prediction_ids"] = [
        predictions[i].prediction_id for i in range(len(predictions)) if i not in matched_pred
    ]
    summary["best_distance_per_prediction"] = [
        {
            "prediction_id": pred.prediction_id,
            "best_distance_px": round(min(distances_all[gi][pi] for gi in range(len(gt_rows))), 4)
            if gt_rows else None,
        }
        for pi, pred in enumerate(predictions)
    ]
    return summary


def evaluate(
    gt: dict[str, Any], audit: dict[str, Any],
    connector_iou: float = 0.5, terminal_radius: float = 5.0,
) -> dict[str, Any]:
    if not 0.0 <= connector_iou <= 1.0:
        raise ValueError("connector_iou must be between 0 and 1")
    if terminal_radius < 0:
        raise ValueError("terminal_radius cannot be negative")

    source = gt.get("source", {})
    audit_source = audit.get("source", {})
    width, height = int(source.get("width", 0)), int(source.get("height", 0))
    audit_width = int(audit_source.get("image_width", 0))
    audit_height = int(audit_source.get("image_height", 0))
    if width <= 0 or height <= 0:
        raise ValueError("Ground-truth source dimensions are missing or invalid")
    if (audit_width, audit_height) != (width, height):
        raise ValueError(
            f"Source dimensions mismatch: ground truth {width}x{height}, "
            f"audit {audit_width}x{audit_height}"
        )

    source_id = str(audit_source.get("source_id", ""))
    source_name = re.split(r"[/\\]", source_id)[-1].lower()
    expected_name = Path(str(source.get("repository_path", ""))).name.lower()
    if source_name and expected_name and source_name != expected_name:
        raise ValueError(
            f"Audit source filename {source_name!r} does not match expected "
            f"ground-truth source {expected_name!r}"
        )

    connector_gt = gt_records(gt, "connector")
    terminal_gt = gt_records(gt, "terminal")
    connectors = get_connector_predictions(audit)
    terminals, terminal_population = get_terminal_predictions(audit)
    objects = audit.get("objects", {})

    return {
        "schema": "eke-dx-wire-ground-truth-evaluation/v1",
        "benchmark_id": gt.get("benchmark_id", "unknown"),
        "status": "exploratory_baseline_not_release_gate",
        "source": {
            "ground_truth_repository_path": source.get("repository_path", ""),
            "ground_truth_repository_blob_sha": source.get("repository_blob_sha"),
            "audit_source_id": source_id,
            "image_width": width,
            "image_height": height,
            "note": (
                "Dimensions and source filename were validated. The input audit "
                "does not carry the image blob hash, so provenance still depends "
                "on supplying the canonical source during extraction."
            ),
        },
        "connector": score_connectors(connector_gt, connectors, connector_iou),
        "terminal": {
            **score_terminals(terminal_gt, terminals, terminal_radius),
            "prediction_population_breakdown": terminal_population,
            "terminal_semantics": (
                "GroundConnection candidates are excluded. ComponentBoundary/"
                "ConnectorBoundary TerminalCandidate records and ConnectorTerminal "
                "records are scored as location predictions only; terminal function "
                "and net correctness are not inferred."
            ),
        },
        "not_scored": {
            "diode": {
                "ground_truth_count": len(gt_records(gt, "diode")),
                "reason": (
                    "The current extraction audit does not export a dedicated "
                    "diode-location prediction collection. Generic component bounds "
                    "or primitive geometry are not treated as diode predictions."
                ),
            },
            "wire_color_label": {
                "ground_truth_count": len(gt_records(gt, "wire_color_label")),
                "reason": (
                    "The current extraction audit does not export localized text "
                    "region boxes or OCR observations; endpoint wire_color fields "
                    "are not label-location predictions."
                ),
            },
        },
        "available_audit_object_collections": sorted(objects.keys()) if isinstance(objects, dict) else [],
        "next_required_exports": [
            "text_regions with bbox and kind",
            "text_recognition_evidence with region ID and recognized text",
            "symbol-family/diode candidates with bbox and confidence",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ground-truth", required=True, type=Path)
    parser.add_argument("--audit", required=True, type=Path)
    parser.add_argument(
        "--connector-iou", type=float, default=0.5,
        help="Exploratory IoU threshold (default 0.5; not a release criterion)",
    )
    parser.add_argument(
        "--terminal-radius", type=float, default=5.0,
        help="Exploratory point radius in pixels (default 5.0; not a release criterion)",
    )
    parser.add_argument("--output", type=Path, help="Output JSON path; otherwise prints to stdout")
    args = parser.parse_args()

    try:
        result = evaluate(
            load_json(args.ground_truth), load_json(args.audit),
            args.connector_iou, args.terminal_radius,
        )
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    content = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(content, encoding="utf-8")
        print(f"Wrote evaluation: {args.output}")
    else:
        sys.stdout.write(content)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
