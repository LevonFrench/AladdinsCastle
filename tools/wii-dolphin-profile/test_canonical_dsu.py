# SPDX-License-Identifier: GPL-3.0-only
import configparser
import hashlib
import unittest
from unittest.mock import patch

import canonical_dsu


class CanonicalContract(unittest.TestCase):
    def test_current_and_legacy_published_producer_hashes_and_role_slots(self):
        current = canonical_dsu.render("AladdinsCastle")
        legacy = canonical_dsu.render("AladdinsCastle", legacy=True)
        self.assertEqual(hashlib.sha256(current).hexdigest(),
                         "4fce5331e9ddda67365f5908a02f31fece9275c60279598446cd1af1153bc139")
        self.assertEqual(hashlib.sha256(legacy).hexdigest(),
                         "6e3ab876ee5f38d2560e169b5f725bcec67e64759928622a31d7ea7fec5efbde")
        parsed = configparser.ConfigParser(interpolation=None)
        parsed.read_string(current.decode())
        self.assertEqual(parsed["Wiimote1"]["Buttons/Home"], "")
        self.assertEqual(parsed["Wiimote1"]["Device"], "DSUClient/0/AladdinsCastle")
        self.assertEqual(parsed["Wiimote1"]["Nunchuk/Buttons/Z"], "`DSUClient/1/AladdinsCastle:L2`")
        canonical_dsu.validate(current, "AladdinsCastle")
        with self.assertRaises(ValueError):
            canonical_dsu.validate(legacy, "AladdinsCastle")
        canonical_dsu.validate(legacy, "AladdinsCastle", allow_legacy=True)
        canonical_dsu.validate(current, "AladdinsCastle", allow_legacy=True)

    def test_product_label_substitution_and_trusted_template_corruption(self):
        rendered = canonical_dsu.render("SyntheticInput")
        self.assertIn(b"Device = DSUClient/0/SyntheticInput", rendered)
        self.assertIn(b"DSUClient/1/SyntheticInput:L2", rendered)
        self.assertNotIn(b"AladdinsCastle", rendered)
        canonical_dsu.validate(rendered, "SyntheticInput")
        for server in ("bad:name", "bad`name", "bad\nname", "bad/name", "", "name with spaces"):
            with self.assertRaises(ValueError):
                canonical_dsu.render(server)
        with patch.object(canonical_dsu.Path, "read_bytes", return_value=b"altered trusted template"), self.assertRaisesRegex(ValueError, "reviewed update"):
            canonical_dsu.render("SyntheticInput")


if __name__ == "__main__":
    unittest.main()
