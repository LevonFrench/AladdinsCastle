# SPDX-License-Identifier: GPL-3.0-only
import configparser
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import product
import setup


def parsed(data):
    result = configparser.ConfigParser(interpolation=None)
    result.optionxform = str
    result.read_string(data.decode())
    return result


class ProductLabels(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / "data").mkdir()
        self.path = self.root / "data/product.json"
        self.labels = {"schema": 1, "display_name": "Synthetic Gallery", "short_id": "synthetic-gallery",
                       "dsu_server_name": "Gallery Input"}
        self.path.write_text(json.dumps(self.labels))

    def test_new_profiles_use_metadata_and_remain_frozen_after_rename(self):
        user = self.root / ".local/user"
        with patch.object(setup, "ROOT", self.root):
            setup.create(user, "one-gun", self.root)
            config = parsed((user / "Config/WiimoteNew.ini").read_bytes())
            self.assertEqual(config["Wiimote1"]["Device"], "DSUClient/0/Gallery Input")
            self.assertEqual(config["Wiimote1"]["Nunchuk/Buttons/Z"], "`DSUClient/1/Gallery Input:L2`")
            server = parsed((user / "Config/DSUClient.ini").read_bytes())
            self.assertEqual(server["Server"]["Entries"], "Gallery Input:127.0.0.1:26760")
            self.assertTrue((user / "Config/Profiles/Wiimote/synthetic-gallery-one-gun-1.ini").is_file())
            manifest = json.loads((user / setup.MANIFEST).read_text())
            self.assertEqual(manifest["schema"], 1)
            self.assertEqual(manifest["product_labels"], self.labels)
            (user / "calibration.json").write_text('{"schema":1,"synthetic_calibration":"keep"}')
            (user / "receipt.json").write_text('{"schema":1,"synthetic_receipt":"keep"}')
            before = {p.relative_to(user):p.read_bytes() for p in user.rglob("*") if p.is_file()}
            self.labels.update(display_name="Another Gallery", dsu_server_name="Other Input", short_id="other-gallery")
            self.path.write_text(json.dumps(self.labels))
            setup.check(user, self.root)
            setup.create(user, "one-gun", self.root)
            setup.command(user, self.root / "Synthetic Dolphin.exe", root=self.root)
            self.assertEqual(before, {p.relative_to(user):p.read_bytes() for p in user.rglob("*") if p.is_file()})
            setup.create(self.root / ".local/new", "one-gun", self.root)
            new = parsed((self.root / ".local/new/Config/WiimoteNew.ini").read_bytes())
            self.assertEqual(new["Wiimote1"]["Device"], "DSUClient/0/Other Input")

    def test_exact_six_legacy_manifests_admit_without_rewriting(self):
        fixtures = json.loads(Path(__file__).with_name("legacy-manifests.json").read_text())
        with patch.object(setup, "ROOT", self.root):
            for mode, manifests in fixtures.items():
                for index, files in enumerate(setup.legacy_templates(mode)):
                    self.assertEqual(setup.envelope(mode, files), manifests[index])
                    user = self.root / ".local" / (mode + str(index))
                    for name, data in files.items():
                        path = user / name
                        path.parent.mkdir(parents=True, exist_ok=True)
                        path.write_bytes(data)
                    (user / setup.MANIFEST).write_text(json.dumps(manifests[index]))
                    setup.check(user, self.root)
                    before = {p.relative_to(user):p.read_bytes() for p in user.rglob("*") if p.is_file()}
                    setup.create(user, mode, self.root)
                    self.assertEqual(before, {p.relative_to(user):p.read_bytes() for p in user.rglob("*") if p.is_file()})
                    (user / "Config/GFX.ini").write_text("[Synthetic runtime edit]\nKeep=True\n")
                    self.assertEqual(setup.command(user, self.root / "Synthetic Dolphin.exe", root=self.root)[1:3],
                                     ["--user", str(user)])
                    self.assertIn("Keep=True", (user / "Config/GFX.ini").read_text())

    def test_invalid_metadata_fails_before_profile_write(self):
        for key, value in (("schema", True), ("short_id", "../escape"), ("display_name", "bad\nsection"),
                           ("display_name", chr(0xd800)),
                           ("dsu_server_name", "bad:name"), ("dsu_server_name", "bad;name"),
                           ("dsu_server_name", "bad`name"), ("dsu_server_name", " bad"),
                           ("dsu_server_name", "bad/name")):
            malformed = {**self.labels, key:value}
            self.path.write_text(json.dumps(malformed))
            with patch.object(setup, "ROOT", self.root), self.assertRaises(ValueError):
                setup.create(self.root / ".local/user", "one-gun", self.root)
            self.assertFalse((self.root / ".local/user").exists())

    def test_new_manifest_labels_tampering_and_unsafe_paths_reject(self):
        user = self.root / ".local/user"
        with patch.object(setup, "ROOT", self.root):
            setup.create(user, "one-gun", self.root)
            path = user / setup.MANIFEST
            manifest = json.loads(path.read_text())
            for field, value in (("short_id", "../../escape"), ("dsu_server_name", "Other Input")):
                corrupt = json.loads(json.dumps(manifest))
                corrupt["product_labels"][field] = value
                path.write_text(json.dumps(corrupt))
                with self.assertRaises(ValueError):
                    setup.command(user, self.root / "Synthetic Dolphin.exe", root=self.root)

    def test_viewer_title_encodes_quotes_and_unicode_as_utf8_bytes(self):
        self.labels["display_name"] = 'Synthetic "Gallery" ' + chr(0x1f3ae)
        self.path.write_text(json.dumps(self.labels))
        header = product.viewer_header(self.path)
        self.assertNotIn(self.labels["display_name"], header)
        literal = header.split(' = "', 1)[1].split('";', 1)[0]
        decoded = bytes(int(part,16) for part in literal.split("\\x")[1:]).decode("utf-8")
        self.assertEqual(decoded, self.labels["display_name"] + " 3D theatre")


if __name__ == "__main__":
    unittest.main()
