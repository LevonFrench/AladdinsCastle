"""CPU-only checks with synthetic manifests/receipts; no game/runtime access."""
import contextlib
import copy
import io
import json
from pathlib import Path
import tempfile
import unittest

import scorecard as sc


class ScorecardTests(unittest.TestCase):
    def setUp(self):
        # Windows sandbox's redirected OS temp can be unwritable. Keep fixtures
        # under the worktree's ignored .local rather than changing permissions.
        test_root = Path(__file__).resolve().parents[3] / ".local" / "scorecard-tests"
        test_root.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=test_root)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.manifest = self.root / "games.json"
        self.manifest.write_text(json.dumps({"synthetic": {
            "title": "Synthetic | example", "status": "Historical E2 claim",
            "views": {"3d": {"disc": "F:/private/example.wbfs", "user": "J:/private/User",
                              "config": ["Graphics.Settings.MSAA=4"],
                              "gameIni": {"path": "J:/private/GameSettings/ABCE01.ini", "content": []},
                              "notes": ["Historical stereo claim"]}}}}), encoding="utf-8")
        self.card = sc.initialize(self.manifest)
        self.receipt = {"schema_version": 1, "game_key": "synthetic", "view": "3d",
                        "phase": "before", "stage": "gameplay", "evidence_level": "E2",
                        "evidence_kind": "offline", "evidence_paths": ["J:/private/run/frames.json"],
                        "metrics": {}}

    def import_run(self, receipt=None, filename="receipt.json"):
        path = self.root / filename
        path.write_text(json.dumps(receipt or self.receipt), encoding="utf-8")
        return sc.import_receipt(self.card, path)

    def test_init_preserves_baseline_without_reading_referenced_files(self):
        game = self.card["games"]["synthetic"]
        view = game["views"]["3d"]
        self.assertEqual(view["game_id_from_ini_filename"], "ABCE01")
        self.assertEqual(game["evidence_level"], "E0")
        self.assertEqual(view["metrics"], sc.METRICS)
        self.assertIn("Historical E2 claim", game["historical_claims_unverified"]["status"])
        self.assertNotIn("notes", view["baseline"])
        self.assertEqual(self.card["source"]["sha256"], sc.digest(self.manifest))
        self.assertIn("Missing: stereo", sc.markdown(self.card))

    def test_offline_normalization_and_delivery_scope(self):
        self.receipt["metrics"] = {
            "stereo": {"eye_width_px": 960, "near_px": 10, "far_px": -2, "distinct_eyes": True},
            "frames": {"72": {"wall_duration_s": 60, "present_count": 4000,
                               "layers_min": 2, "layers_max": 2,
                               "eligible_stereo_frame_count": 3990, "emulated_duration_s": 55}}}
        self.import_run()
        metrics = self.card["games"]["synthetic"]["views"]["3d"]["runs"][0]["metrics"]
        self.assertEqual(metrics["stereo"]["near_px_1920"], 20)
        self.assertEqual(metrics["stereo"]["far_px_1920"], -4)
        self.assertEqual(metrics["frames"]["72"]["expected_wall_frames"], 4320)
        self.assertEqual(metrics["frames"]["72"]["eligible_stereo_frame_count"], 3990)
        self.assertIsNone(metrics["frames"]["72"]["fresh_content_frames"])
        self.assertIsNone(metrics["frames"]["90"]["expected_wall_frames"])
        self.assertEqual(metrics["frames"]["real_compositor"], sc.METRICS["frames"]["real_compositor"])

    def test_distinct_before_after_and_deduplication(self):
        self.assertTrue(self.import_run())
        self.assertFalse(self.import_run())
        after = copy.deepcopy(self.receipt)
        after["phase"] = "after"
        self.assertTrue(self.import_run(after, "after.json"))
        runs = self.card["games"]["synthetic"]["views"]["3d"]["runs"]
        self.assertEqual([r["phase"] for r in runs], ["before", "after"])
        self.assertEqual(self.card["games"]["synthetic"]["views"]["3d"]["metrics"], sc.METRICS)

    def test_reject_ambiguous_or_false_provenance(self):
        cases = [
            {"view": "absent"}, {"evidence_level": "E3", "evidence_kind": "owner"},
            {"evidence_paths": ["relative.json"]}, {"evidence_level": "E3"},
            {"metrics": {"frames": {"real_compositor": {"present_count": 10}}}},
            {"metrics": {"stereo": {"eye_width_px": 0}}},
            {"metrics": {"stereo": {"near_px": 2, "eye_width_px": 960, "near_px_1920": 8}}},
            {"metrics": {"frames": {"72": {"target_hz": 90}}}},
            {"metrics": {"frames": {"72": {"stereo_layer_count": 100}}}},
            {"metrics": {"frames": {"72": {"layers_min": 2, "layers_max": 1}}}},
            {"metrics": {"frames": {"72": {"wall_duration_s": 60, "expected_wall_frames": 1}}}},
            {"metrics": {"launch": {"print_only": False}}},
            {"evidence_level": "E0", "evidence_kind": "source", "metrics": {"controls": {"aim_error_percent": 1}}},
        ]
        original = copy.deepcopy(self.card)
        for patch in cases:
            with self.subTest(patch=patch):
                bad = {**self.receipt, **patch}
                with self.assertRaises(ValueError):
                    self.import_run(bad)
                self.assertEqual(original, self.card)

    def test_explicit_owner_confirmation_is_preserved(self):
        self.receipt.update(evidence_level="E3", evidence_kind="owner",
                            owner_confirmation={"by": "owner", "at": "2026-10-23T00:00:00Z", "statement": "synthetic example only"})
        self.import_run()
        self.assertIn("synthetic example only", sc.markdown(self.card))
        self.assertEqual(self.card["games"]["synthetic"]["evidence_level"], "E0")

    def test_cli_private_output_refuses_overwrite_and_batch_failure(self):
        out = self.root / ".local" / "out"
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(sc.main(["init", "--manifest", str(self.manifest), "--out", str(out)]), 0)
            prior = (out / "scorecard.json").read_bytes()
            self.assertEqual(sc.main(["init", "--manifest", str(self.manifest), "--out", str(out)]), 2)
            self.assertEqual(sc.main(["import", "--out", str(out), "--receipt", str(self.root / "absent.json")]), 2)
            self.assertEqual((out / "scorecard.json").read_bytes(), prior)
            public = Path(__file__).resolve().parents[3] / "scorecard-forbidden-test-output"
            self.assertEqual(sc.main(["init", "--manifest", str(self.manifest), "--out", str(public)]), 2)
        self.assertIn("Synthetic \\| example", (out / "SCORECARD.md").read_text(encoding="utf-8"))

    def test_nan_rejected(self):
        self.receipt["metrics"] = {"stereo": {"near_px": float("nan")}}
        with self.assertRaises(ValueError):
            self.import_run()


if __name__ == "__main__":
    unittest.main()
