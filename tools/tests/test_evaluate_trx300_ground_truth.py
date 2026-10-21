import unittest

from evaluate_trx300_ground_truth import (
    bbox_iou,
    evaluate,
    maximum_cardinality_matching,
)


class EvaluationTests(unittest.TestCase):
    def test_iou_identical_boxes(self):
        self.assertEqual(bbox_iou((1, 2, 10, 8), (1, 2, 10, 8)), 1.0)

    def test_iou_disjoint_boxes(self):
        self.assertEqual(bbox_iou((0, 0, 2, 2), (3, 3, 2, 2)), 0.0)

    def test_matching_is_one_to_one_and_maximizes_cardinality(self):
        # GT-0 can match predictions 0 or 1; GT-1 can only match prediction 0.
        edges = [[(0, 0.9), (1, 0.8)], [(0, 0.7)]]
        matches = maximum_cardinality_matching(2, 2, edges)
        self.assertEqual(len(matches), 2)
        self.assertEqual({match[1] for match in matches}, {0, 1})

    def test_evaluation_excludes_ground_connection_and_marks_missing_classes(self):
        gt = {
            "benchmark_id": "test",
            "source": {
                "width": 100, "height": 100,
                "repository_blob_sha": "x", "repository_path": "test.png",
            },
            "annotations": [
                {"id": "c1", "category": "connector", "bbox_xywh_px": [10, 10, 10, 10]},
                {"id": "t1", "category": "terminal", "point_xy_px": [20, 20], "center_xy_px": [20, 20]},
                {"id": "d1", "category": "diode", "bbox_xywh_px": [30, 30, 5, 5]},
                {"id": "l1", "category": "wire_color_label", "bbox_xywh_px": [40, 40, 5, 5]},
            ],
        }
        audit = {
            "source": {"source_id": r"C:\test.png", "image_width": 100, "image_height": 100},
            "objects": {
                "connectors": [{"id": "pc1", "x": 10, "y": 10, "width": 10, "height": 10}],
                "terminal_candidates": [
                    {"id": "ground", "kind": "ground_connection", "x": 20, "y": 20},
                    {"id": "term", "kind": "component_boundary", "x": 20, "y": 20},
                ],
                "connector_terminals": [],
            },
        }
        result = evaluate(gt, audit)
        self.assertEqual(result["connector"]["true_positive"], 1)
        self.assertEqual(result["terminal"]["true_positive"], 1)
        self.assertEqual(result["terminal"]["prediction_count"], 1)
        self.assertEqual(
            result["terminal"]["prediction_population_breakdown"]["terminal_candidates_excluded_ground_connection"],
            1,
        )
        self.assertIn("reason", result["not_scored"]["diode"])
        self.assertIn("reason", result["not_scored"]["wire_color_label"])

    def test_dimension_mismatch_fails(self):
        gt = {
            "source": {"width": 100, "height": 100, "repository_path": "test.png"},
            "annotations": [],
        }
        audit = {
            "source": {"source_id": "test.png", "image_width": 100, "image_height": 99},
            "objects": {"connectors": [], "terminal_candidates": [], "connector_terminals": []},
        }
        with self.assertRaises(ValueError):
            evaluate(gt, audit)

    def test_source_filename_mismatch_fails(self):
        gt = {
            "source": {"width": 100, "height": 100, "repository_path": "test.png"},
            "annotations": [],
        }
        audit = {
            "source": {"source_id": "other.png", "image_width": 100, "image_height": 100},
            "objects": {"connectors": [], "terminal_candidates": [], "connector_terminals": []},
        }
        with self.assertRaises(ValueError):
            evaluate(gt, audit)


if __name__ == "__main__":
    unittest.main()
