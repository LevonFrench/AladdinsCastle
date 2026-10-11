# SPDX-License-Identifier: GPL-3.0-only
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import desktop_capture as capture
import setup
from test_disc_header import synthetic_header, synthetic_wbfs


class SelectedDiscPreparation(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.local = self.root / ".local"
        self.local.mkdir()
        self.disc = self.local / "synthetic-disc.bin"
        self.disc.write_bytes(synthetic_header())
        self.generated = self.local / "generated"
        self.profile = self.generated / "games/synthetic-game/scripted-dsu-right.ini"
        self.profile.parent.mkdir(parents=True)
        self.profile.write_bytes(b"[Wiimote1]\nSource = 1\nDevice = DSUClient/0/AladdinsCastle\nButtons/A = `Cross`\n")
        self.profile_manifest = self.generated / "generated-files.json"
        self.profile_manifest.write_text(json.dumps({"version":"shell-stick/1",
            "files":{"games/synthetic-game/scripted-dsu-right.ini":capture.sha256(self.profile)}}))
        self.state = {"disc":str(self.disc), "disc_header_sha256":capture.sha256(self.disc, True),
                      "game_id":"DEMO01", "game_slug":"synthetic-game", "dsu_profile":str(self.profile),
                      "dsu_profile_sha256":capture.sha256(self.profile)}
        for key, name in (("dolphin", "Dolphin.exe"), ("capture", "acvr-dolphin-theatre.exe")):
            path = self.local / name
            path.write_bytes(b"synthetic binary, not executable")
            self.state[key] = str(path)
            self.state[key+"_sha256"] = capture.sha256(path)
        self.config = self.local / "inputs.json"
        self.save()
        self.run = self.local / "run"

    def save(self):
        self.config.write_text(json.dumps(self.state))

    def test_iso_and_wbfs_prepare_only_selected_game_without_launch(self):
        for suffix, disc in (("iso", synthetic_header()), ("wbfs", synthetic_wbfs(shift=12))):
            self.disc.write_bytes(disc)
            self.state["disc_header_sha256"] = capture.sha256(self.disc, True)
            self.save()
            with patch.object(capture.subprocess, "Popen", side_effect=AssertionError("no launch")):
                plan = capture.prepare(self.config, self.local / suffix, self.root)
            self.assertEqual(plan["game_id"], "DEMO01")
            self.assertEqual(plan["profile_key"], "games/synthetic-game/scripted-dsu-right.ini")
            self.assertEqual(plan["disc_header"]["format"], suffix)
            user = Path(plan["user"])
            self.assertEqual([p.name for p in (user / "GameSettings").iterdir()], ["DEMO01.ini"])
            self.assertEqual((user / "Config/WiimoteNew.ini").read_bytes(), self.profile.read_bytes())
            self.assertNotIn("WiimoteProfile", (user / "GameSettings/DEMO01.ini").read_text())
            self.assertEqual(json.loads((user / setup.MANIFEST).read_text())["games"], ["DEMO01"])
            capture.verify_prepared(plan, self.root)

    def test_id_slug_hash_and_native_role_mismatches_fail_before_writes(self):
        for key, value in (("game_id", "DEMO02"), ("game_id", "demo01"), ("game_slug", "other-game"),
                           ("game_slug", "../bad"), ("dsu_profile_sha256", "0"*64)):
            changed = {**self.state, key:value}
            self.config.write_text(json.dumps(changed))
            with self.assertRaises(ValueError):
                capture.prepare(self.config, self.run, self.root)
            self.assertFalse(self.run.exists())
        self.profile.write_bytes(b"[Wiimote1]\nSource = 3\nDevice = DSUClient/0/AladdinsCastle\n")
        digest = capture.sha256(self.profile)
        self.profile_manifest.write_text(json.dumps({"version":"shell-stick/1",
            "files":{"games/synthetic-game/scripted-dsu-right.ini":digest}}))
        self.state["dsu_profile_sha256"] = digest
        self.save()
        with self.assertRaisesRegex(ValueError, "Source 1"):
            capture.prepare(self.config, self.run, self.root)
        self.assertFalse(self.run.exists())

    def test_same_size_changed_header_refuses_and_original_recipe_is_untouched(self):
        before = self.config.read_bytes()
        modified = bytearray(self.disc.read_bytes())
        modified[10] = 42  # ID and Wii magic still match, whole header hash changed.
        self.disc.write_bytes(modified)
        with self.assertRaisesRegex(ValueError, "identity changed"):
            capture.prepare(self.config, self.run, self.root)
        self.assertFalse(self.run.exists())
        self.assertEqual(self.config.read_bytes(), before)

    def test_changed_config_after_preparation_including_whitespace_is_refused(self):
        plan = capture.prepare(self.config, self.run, self.root)
        original = self.config.read_bytes()
        for data in (original+b"\n", json.dumps({**self.state,"game_slug":"other-game"}).encode()):
            self.config.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "changed after preparation"):
                capture.verify_prepared(plan, self.root)
        self.config.write_bytes(original)
        capture.verify_prepared(plan, self.root)

    def test_generic_requires_explicit_identity_and_legacy_pair_stays_default(self):
        self.state.pop("game_id")
        self.state.pop("game_slug")
        self.state.pop("dsu_profile")
        self.state.pop("dsu_profile_sha256")
        self.save()
        with self.assertRaisesRegex(ValueError, "explicit game_id"):
            capture.prepare(self.config, self.run, self.root)
        self.assertFalse(self.run.exists())
        for legacy_id in setup.GAME_IDS:
            self.disc.write_bytes(synthetic_header(legacy_id))
            self.state["disc_header_sha256"] = capture.sha256(self.disc, True)
            self.save()
            plan = capture.prepare(self.config, self.local / legacy_id, self.root)
            manifest = json.loads((Path(plan["user"]) / setup.MANIFEST).read_text())
            self.assertEqual(manifest["games"], list(setup.GAME_IDS))
            self.assertEqual(plan["game_id"], legacy_id)

    def test_selected_game_manifest_cannot_be_reused_for_other_game(self):
        user = self.local / "selected"
        setup.create(user, "one-gun", self.root, game_id="DEMO01")
        before = {p.relative_to(user):p.read_bytes() for p in user.rglob("*") if p.is_file()}
        with self.assertRaises(ValueError):
            setup.create(user, "one-gun", self.root, game_id="DEMO02")
        self.assertEqual(before, {p.relative_to(user):p.read_bytes() for p in user.rglob("*") if p.is_file()})
        manifest_path = user / setup.MANIFEST
        manifest = json.loads(manifest_path.read_text())
        manifest["games"] = ["DEMO02"]
        manifest_path.write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):
            setup.command(user, self.local / "Dolphin.exe", root=self.root)

    def test_other_supported_dsu_leaf_and_malformed_profile_syntax(self):
        alternate = self.profile.with_name("flat-dolphin-right.ini")
        alternate.write_bytes(self.profile.read_bytes())
        key = "games/synthetic-game/flat-dolphin-right.ini"
        self.profile_manifest.write_text(json.dumps({"version":"shell-stick/1",
            "files":{key:capture.sha256(alternate)}}))
        self.state.update(dsu_profile=str(alternate), dsu_profile_sha256=capture.sha256(alternate))
        self.save()
        self.assertEqual(capture.admit(self.config, self.root), self.state)
        alternate.write_bytes(b"invalid generated INI")
        self.profile_manifest.write_text(json.dumps({"version":"shell-stick/1",
            "files":{key:capture.sha256(alternate)}}))
        self.state["dsu_profile_sha256"] = capture.sha256(alternate)
        self.save()
        with self.assertRaisesRegex(ValueError, "profile syntax"):
            capture.prepare(self.config, self.run, self.root)
        self.assertFalse(self.run.exists())

    def test_powershell_style_uppercase_hashes_are_admitted_without_rewriting(self):
        key = "games/synthetic-game/scripted-dsu-right.ini"
        self.profile_manifest.write_text(json.dumps({"version":"shell-stick/1",
            "files":{key:capture.sha256(self.profile).upper()}}))
        for field in ("dolphin_sha256", "capture_sha256", "disc_header_sha256", "dsu_profile_sha256"):
            self.state[field] = self.state[field].upper()
        self.save()
        before = self.config.read_bytes()
        self.assertEqual(capture.admit(self.config, self.root), self.state)
        self.assertEqual(self.config.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
