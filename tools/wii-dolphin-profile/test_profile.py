# SPDX-License-Identifier: GPL-3.0-only
import configparser
import importlib.util
import json
import io
from contextlib import redirect_stdout
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

    def test_runtime_edits_admitted_for_command_but_not_inspect_or_create(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            user = root/".local/user"
            setup.create(user,"one-gun",root)
            config = user/"Config/GFX.ini"
            config.write_text("[Stereoscopy]\nStereoDepth = 25\n")
            before = {p.relative_to(user): p.read_bytes() for p in user.rglob("*") if p.is_file()}
            output = io.StringIO()
            with redirect_stdout(output):
                self.assertEqual(setup.main(["command","--user",str(user),"--dolphin",str(root/"Synthetic Dolphin.exe")],root),0)
            self.assertEqual(json.loads(output.getvalue().splitlines()[0])[1:],["--user",str(user)])
            with self.assertRaises(ValueError): setup.check(user,root)
            with self.assertRaises(ValueError): setup.create(user,"one-gun",root)
            self.assertEqual(before,{p.relative_to(user):p.read_bytes() for p in user.rglob("*") if p.is_file()})

    def test_command_rejects_unowned_tampered_missing_and_linked_profiles(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            original = root/".local/SyntheticOriginalUser"
            original.mkdir(parents=True)
            (original/"marker").write_text("preserve")
            with self.assertRaises(OSError): setup.command(original,root/"Synthetic Dolphin.exe",root=root)
            user = root/".local/user"
            setup.create(user,"one-gun",root)
            manifest_path=user/setup.MANIFEST
            manifest=json.loads(manifest_path.read_text())
            for mutation in (dict(manifest,source_pin="invalid"),dict(manifest,files={}),dict(manifest,unexpected=True),[]):
                manifest_path.write_text(json.dumps(mutation))
                with self.assertRaises(ValueError): setup.command(user,root/"Synthetic Dolphin.exe",root=root)
            manifest_path.write_text(json.dumps(manifest))
            (user/"Config/GFX.ini").unlink()
            with self.assertRaises(ValueError): setup.command(user,root/"Synthetic Dolphin.exe",root=root)
            self.assertEqual((original/"marker").read_text(),"preserve")

    def test_command_rejects_linked_manifest_config_and_ancestor(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            user=root/".local/user"
            setup.create(user,"one-gun",root)
            link=root/".local/user-alias"
            try: link.symlink_to(user,target_is_directory=True)
            except OSError: self.skipTest("This OS does not permit symlink creation")
            with self.assertRaises(ValueError): setup.command(link,root/"Synthetic Dolphin.exe",root=root)
            for relative in (setup.MANIFEST,"Config/GFX.ini"):
                path=user/relative
                external=root/("synthetic-"+path.name)
                external.write_bytes(path.read_bytes())
                path.unlink()
                path.symlink_to(external)
                with self.assertRaises(ValueError): setup.command(user,root/"Synthetic Dolphin.exe",root=root)
                path.unlink()
                path.write_bytes(external.read_bytes())

    def test_initial_template_manifest_survives_runtime_edits(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            user=root/".local/user"
            setup.create(user,"one-gun",root)
            initial=setup.render("one-gun")
            initial["Config/Dolphin.ini"]=setup.ini({"Core":{"EnableCheats":"False"}})
            (user/"Config/Dolphin.ini").write_bytes(initial["Config/Dolphin.ini"])
            (user/setup.MANIFEST).write_text(json.dumps(setup.envelope("one-gun",initial)))
            setup.check(user,root)
            (user/"Config/Dolphin.ini").write_text("[Core]\nEnableCheats = False\n[General]\nSyntheticRuntime = True\n")
            self.assertEqual(setup.command(user,root/"Synthetic Dolphin.exe",root=root)[1:],["--user",str(user)])
            with self.assertRaises(ValueError): setup.check(user,root)

    def test_cli_menu_wording_declares_unavailable_touch_mapping(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            user=root/".local/user"
            output=io.StringIO()
            with redirect_stdout(output): setup.main(["create","--user",str(user)],root)
            self.assertNotIn("Start menu",output.getvalue())
            self.assertIn("Touch application-menu input is unavailable",output.getvalue())
            self.assertIn("mapping is pending",output.getvalue())


if __name__ == "__main__": unittest.main()
