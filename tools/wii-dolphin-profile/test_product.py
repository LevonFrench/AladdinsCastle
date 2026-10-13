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
            self.assertEqual(server["Server"]["Entries"], "Gallery Input:127.0.0.1:26760;")
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

    def test_dsu_entries_use_product_name_and_canonical_terminator(self):
        for name in ("Gallery Input", "Other Input"):
            with self.subTest(name=name), patch.object(setup, "ROOT", self.root):
                self.labels["dsu_server_name"] = name
                self.path.write_text(json.dumps(self.labels))
                entries = parsed(setup.render("one-gun")["Config/DSUClient.ini"])["Server"]["Entries"]
                self.assertEqual(entries, name + ":127.0.0.1:26760;")
                parts = [entry.split(":") for entry in entries.split(";") if entry]
                rebuilt = "".join(f"{p[0]}:{p[1]}:{int(p[2])};" for p in parts)
                self.assertEqual(entries, rebuilt)

    def test_old_product_dsu_manifest_is_admitted_without_rewriting(self):
        games = ["SYN001"]
        with patch.object(setup, "ROOT", self.root):
            for mode in ("one-gun", "two-remotes"):
                with self.subTest(mode=mode):
                    files = setup.render(mode, self.labels, games=games)
                    # Independently spell the exact pre-terminator DSU file.
                    files["Config/DSUClient.ini"] = setup.ini({"Server": {
                        "Enabled": "True", "Entries": "Gallery Input:127.0.0.1:26760"}})
                    user = self.root / ".local" / mode
                    for name, data in files.items():
                        path = user / name
                        path.parent.mkdir(parents=True, exist_ok=True)
                        path.write_bytes(data)
                    manifest_path = user / setup.MANIFEST
                    manifest_path.write_text(json.dumps(setup.envelope(mode, files, self.labels, games)))
                    before = {p.relative_to(user): p.read_bytes() for p in user.rglob("*") if p.is_file()}
                    setup.check(user, self.root)
                    setup.create(user, mode, self.root, game_id=games[0])
                    setup.command(user, self.root / "Synthetic Dolphin.exe", root=self.root)
                    self.assertEqual(before, {p.relative_to(user): p.read_bytes() for p in user.rglob("*") if p.is_file()})
                    # Compatibility accepts only the former exact template,
                    # never arbitrary re-hashed endpoint changes.
                    files["Config/DSUClient.ini"] = setup.ini({"Server": {
                        "Enabled": "True", "Entries": "Gallery Input:127.0.0.1:26761"}})
                    (user / "Config/DSUClient.ini").write_bytes(files["Config/DSUClient.ini"])
                    manifest_path.write_text(json.dumps(setup.envelope(mode, files, self.labels, games)))
                    with self.assertRaises(ValueError):
                        setup.check(user, self.root)

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


    def test_ephemeral_viewer_key_uses_short_id_and_preserves_default(self):
        for short_id in ("aladdinscastle", "synthetic-gallery", "other-gallery"):
            with self.subTest(short_id=short_id):
                self.labels["short_id"] = short_id
                self.path.write_text(json.dumps(self.labels))
                header = product.viewer_header(self.path)
                literal = header.split('productOverlayKeyPrefix = "', 1)[1].split('";', 1)[0]
                prefix = bytes(int(part, 16) for part in literal.split("\\x")[1:]).decode("utf-8")
                self.assertEqual(prefix, short_id + ".dolphin.theatre.")
                # Key namespace is independent of the visible Unicode title.
                self.labels["display_name"] = 'Other "Gallery" ' + chr(0x1f3ae)
                self.path.write_text(json.dumps(self.labels))
                self.assertIn('productOverlayKeyPrefix = "' + literal + '";',
                              product.viewer_header(self.path))


if __name__ == "__main__":
    unittest.main()
