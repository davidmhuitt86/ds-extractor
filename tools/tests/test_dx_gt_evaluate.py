#!/usr/bin/env python3
"""Unit tests for tools/dx-gt-evaluate.py; stdlib unittest only."""
from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent.parent / "dx-gt-evaluate.py"
SPEC = importlib.util.spec_from_file_location("dx_gt_evaluate", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Cannot import evaluator at {SCRIPT}")
EVAL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EVAL)


class GroundTruthEvaluatorTests(unittest.TestCase):
    def test_iou_exact_and_disjoint(self) -> None:
        self.assertEqual(EVAL.iou_xywh((10, 10, 20, 10), (10, 10, 20, 10)), 1.0)
        self.assertEqual(EVAL.iou_xywh((0, 0, 1, 1), (3, 3, 1, 1)), 0.0)

    def test_matching_maximizes_cardinality_not_just_greedy_first_edge(self) -> None:
        matrix = [[1.0, 0.9], [0.8, 0.0]]
        matches = EVAL.maximum_cardinality_match(
            2, 2,
            lambda p, g: matrix[p][g],
            lambda value: value >= 0.5,
            higher_is_better=True,
        )
        self.assertEqual(len(matches), 2)
        self.assertEqual({(p, g) for p, g, _ in matches}, {(0, 1), (1, 0)})

    def test_ground_connections_are_not_counted_as_terminals(self) -> None:
        objects = {
            "terminal_candidates": [
                {"id": "terminal-a", "kind": "component_boundary", "x": 10, "y": 12},
                {"id": "ground-a", "kind": "ground_connection", "x": 11, "y": 12},
                {"id": "unknown-a", "kind": "unknown", "x": 12, "y": 12},
            ],
            "connector_terminals": [
                {"id": "connector-terminal-a", "x": 20, "y": 22}
            ],
        }
        predictions, inventory = EVAL.get_terminal_predictions(objects)
        self.assertEqual(len(predictions), 2)
        self.assertEqual(inventory["excluded_ground_connection_count"], 1)
        self.assertEqual(inventory["excluded_unknown_kind_count"], 1)

    def test_audit_without_specific_diode_and_label_outputs_is_unscored(self) -> None:
        gt = {
            "benchmark_id": "unit-test",
            "source": {"width": 100, "height": 100},
            "annotations": [
                {"id": "c1", "category": "connector", "bbox_xywh_px": [10, 10, 10, 10]},
                {"id": "d1", "category": "diode", "bbox_xywh_px": [30, 10, 5, 3]},
                {"id": "t1", "category": "terminal", "point_xy_px": [10, 10], "bbox_xywh_px": [9, 9, 3, 3]},
                {"id": "l1", "category": "wire_color_label", "bbox_xywh_px": [50, 10, 8, 4]},
            ],
        }
        audit = {
            "source": {"source_id": "unit.png", "image_width": 100, "image_height": 100},
            "objects": {
                "connectors": [{"id": "c-pred", "x": 10, "y": 10, "width": 10, "height": 10}],
                "terminal_candidates": [{"id": "t-pred", "kind": "component_boundary", "x": 10, "y": 10}],
                "connector_terminals": [],
                "components": [{"id": "shape1", "kind": "circular_symbol", "x": 30, "y": 10, "width": 5, "height": 3}],
            },
        }
        report = EVAL.evaluate(gt, audit, 0.5, [2.0, 5.0])
        self.assertEqual(report["metrics"]["connector_localization"]["true_positives"], 1)
        self.assertEqual(report["metrics"]["terminal_localization"]["within_2_px"]["true_positives"], 1)
        self.assertEqual(report["metrics"]["diode_localization"]["status"], "unscored")
        self.assertEqual(report["metrics"]["wire_color_label_localization"]["status"], "unscored")
        self.assertIsNone(report["metrics"]["diode_localization"]["recall"])

    def test_resolved_diode_regions_can_be_scored_when_explicit(self) -> None:
        gt = [{"id": "d1", "category": "diode", "bbox_xywh_px": [10, 10, 10, 5]}]
        preds = [{"id": "d-pred", "x": 10, "y": 10, "width": 10, "height": 5}]
        out = EVAL.metric_for_boxes("diode", gt, EVAL.get_bbox_predictions(preds, "diodes"), 0.5)
        self.assertEqual(out["true_positives"], 1)
        self.assertEqual(out["precision"], 1.0)
        self.assertEqual(out["recall"], 1.0)


    def test_explicit_empty_detector_inventory_scores_zero_recall_not_unscored(self) -> None:
        gt = [
            {"id": "d1", "category": "diode", "bbox_xywh_px": [10, 10, 10, 5]}
        ]
        available, predictions, source = EVAL.find_diode_predictions({"diode_detections": []})
        self.assertTrue(available)
        self.assertEqual(predictions, [])
        self.assertEqual(source, "objects.diode_detections")
        scored = EVAL.metric_for_boxes("diode", gt, predictions, 0.5)
        self.assertEqual(scored["true_positives"], 0)
        self.assertEqual(scored["recall"], 0.0)

    def test_source_dimension_mismatch_fails_closed(self) -> None:
        gt = {"source": {"width": 100, "height": 100}, "annotations": []}
        audit = {"source": {"image_width": 101, "image_height": 100}, "objects": {}}
        with self.assertRaisesRegex(ValueError, "Source width mismatch"):
            EVAL.evaluate(gt, audit, 0.5, [5.0])


if __name__ == "__main__":
    unittest.main(verbosity=2)
