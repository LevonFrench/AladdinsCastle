# SPDX-License-Identifier: GPL-3.0-only
import configparser
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("profile_setup", Path(__file__).with_name("setup.py"))
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


def parse(data):
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read_string(data.decode())
    return config


class Profiles(unittest.TestCase):
    def test_one_gun_uses_other_hand_nunchuk_and_physical_motion(self):
        p = setup.profile(0, 1)
        self.assertEqual(p["Nunchuk/Buttons/Z"], "`DSUClient/1/AladdinsCastle:L2`")
        self.assertEqual(p["Nunchuk/IMUAccelerometer/Up"], "`DSUClient/1/AladdinsCastle:Accel Up`")
        self.assertEqual(p["Buttons/B"], "`R2`")
        self.assertEqual(p["IMUGyroscope/Pitch Up"], "`Gyro Pitch Up`")
        self.assertFalse(any(v for k,v in p.items() if "/Shake/" in "/"+k or "/Swing/" in "/"+k))
        self.assertEqual(p["IMUIR/Enabled"], "False")
        self.assertEqual(p["IR/Up"], "`Right Y+`")
        self.assertEqual(p["Nunchuk/Stick/Up"], "`DSUClient/1/AladdinsCastle:Left Y+`")

    def test_two_remotes_and_game_selectors_are_independent(self):
        files = setup.render("two-remotes")
        w = parse(files["Config/WiimoteNew.ini"])
        self.assertEqual(w["Wiimote2"]["Device"], "DSUClient/1/AladdinsCastle")
        self.assertEqual(w["Wiimote2"]["Nunchuk/Buttons/Z"], "`L2`")
        self.assertEqual(w["Wiimote3"]["Source"], "0")
        for game in setup.GAME_IDS:
            controls = parse(files[f"GameSettings/{game}.ini"])["Controls"]
            for i in (1,2):
                name = controls[f"WiimoteProfile{i}"]
                self.assertIn(f"Config/Profiles/Wiimote/{name}.ini", files)

    def test_legacy_stereo_no_cheats_or_duplicate_units(self):
        files = setup.render("one-gun")
        self.assertEqual(parse(files["Config/GFX.ini"])["Stereoscopy"]["StereoMode"], "1")
        self.assertNotIn(b"StereoPerEyeResolutionFull", files["Config/GFX.ini"])
        self.assertEqual(parse(files["Config/Dolphin.ini"])["Core"]["EnableCheats"], "False")
        self.assertEqual(parse(files["Config/Dolphin.ini"])["Analytics"]["Enabled"], "False")
        self.assertEqual(parse(files["Config/Dolphin.ini"])["Analytics"]["PermissionAsked"], "True")
        self.assertEqual(parse(files["Config/Dolphin.ini"])["AutoUpdate"]["UpdateTrack"], "")
        self.assertEqual(parse(files["Config/DSUClient.ini"])["Server"]["Entries"], "AladdinsCastle:127.0.0.1:26760")

    def test_isolation_idempotence_and_modified_config_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            user = root/".local"/"user"
            setup.create(user, "one-gun", root)
            self.assertEqual(setup.create(user, "one-gun", root), user)
            config = user/"Config/GFX.ini"
            config.write_text("[user changes]\n")
            with self.assertRaises(ValueError): setup.create(user, "one-gun", root)
            self.assertEqual(config.read_text(), "[user changes]\n")

    def test_unowned_and_outside_paths_untouched(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            user = root/".local"/"existing"
            user.mkdir(parents=True)
            (user/"marker").write_text("keep")
            with self.assertRaises(OSError): setup.create(user, "one-gun", root)
            self.assertEqual((user/"marker").read_text(), "keep")
            for target in (root/"User",root/".local",root/".local/../escape"):
                with self.assertRaises(ValueError): setup.safe_user(target, root)

    def test_atomic_write_failure_and_manifest_tampering(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            user = root/".local"/"user"
            with patch.object(Path, "write_bytes", side_effect=OSError("disk full")):
                with self.assertRaises(OSError): setup.create(user,"one-gun",root)
            self.assertFalse(user.exists())
            self.assertEqual(list(user.parent.iterdir()), [])
            setup.create(user,"one-gun",root)
            manifest = user/setup.MANIFEST
            data = json.loads(manifest.read_text())
            data["files"]["../../escape"] = "fake"
            manifest.write_text(json.dumps(data))
            with self.assertRaises(ValueError): setup.check(user,root)

    def test_linked_ancestor_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/".local").mkdir()
            link = root/".local/link"
            try: link.symlink_to(root, target_is_directory=True)
            except OSError: self.skipTest("This OS does not permit symlink creation")
            with self.assertRaises(ValueError): setup.safe_user(link/"new-user",root)

    def test_commands_are_argument_lists_not_launches(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            user = root/".local/user space"
            setup.create(user,"one-gun",root)
            args = setup.command(user,root/"Synthetic Dolphin.exe",root/"Synthetic game.iso",root)
            self.assertEqual(args[1:3], ["--user",str(user)])
            self.assertEqual(args[3:5], ["-b","-e"])
            self.assertEqual(len(setup.command(user,root/"Synthetic Dolphin.exe",root=root)),3)


if __name__ == "__main__": unittest.main()
