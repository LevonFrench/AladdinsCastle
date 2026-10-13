# SPDX-License-Identifier: GPL-3.0-only
import hashlib
from pathlib import Path
import tempfile
import unittest

import disc_header


def synthetic_header(game_id="DEMO01"):
    header = bytearray(32)
    header[:6] = game_id.encode("ascii")
    header[24:28] = disc_header.MAGIC
    return bytes(header)


def synthetic_wbfs(game_id="DEMO01", shift=9):
    size = 1 << shift
    data = bytearray(size * 2)
    data[:4] = b"WBFS"
    data[4:8] = (2).to_bytes(4, "big")
    data[8:10] = bytes((shift, 21))
    data[12] = 1
    data[size:size+32] = synthetic_header(game_id)
    return bytes(data)


class DiscHeaders(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / "synthetic-disc.bin"

    def test_iso_exact_selected_id_magic_and_only_header_hash(self):
        header = synthetic_header()
        self.path.write_bytes(header + b"synthetic payload A")
        result = disc_header.inspect(self.path, "DEMO01")
        self.assertEqual(result, {"game_id":"DEMO01", "format":"iso", "offset":0,
                                  "sha256":hashlib.sha256(header).hexdigest()})
        self.path.write_bytes(header + b"different payload B")
        self.assertEqual(disc_header.inspect(self.path), result)  # Not payload integrity.
        with self.assertRaisesRegex(ValueError, "selected game_id"):
            disc_header.inspect(self.path, "DEMO02")

    def test_wbfs_host_sector_offsets_and_copied_header_hash(self):
        for shift in (9, 10, 11, 12):
            self.path.write_bytes(synthetic_wbfs(shift=shift))
            result = disc_header.inspect(self.path, "DEMO01")
            self.assertEqual(result["format"], "wbfs")
            self.assertEqual(result["offset"], 1 << shift)
            self.assertEqual(result["sha256"], hashlib.sha256(synthetic_header()).hexdigest())
            self.assertNotEqual(result["sha256"], hashlib.sha256(self.path.read_bytes()[:32]).hexdigest())

    def test_invalid_ids_magic_and_short_headers_reject(self):
        for data in (b"", bytes(31), synthetic_header("demo01"), synthetic_header("DEMO01")[:24]+bytes(8)):
            self.path.write_bytes(data)
            with self.assertRaises(ValueError):
                disc_header.inspect(self.path)
        self.path.write_bytes(synthetic_header())
        for value in ("../bad", "DEMO0", "demo01", None, 123):
            if value is None:
                continue
            with self.assertRaises(ValueError):
                disc_header.inspect(self.path, value)

    def test_invalid_wbfs_shifts_declared_size_and_slots_reject(self):
        for index, value in ((8, 8), (8, 13), (8, 255), (9, 14), (9, 32), (12, 0), (13, 1), (511, 1)):
            data = bytearray(synthetic_wbfs())
            data[index] = value
            self.path.write_bytes(data)
            with self.assertRaises(ValueError):
                disc_header.inspect(self.path)
        for sectors in (0, 1):
            data = bytearray(synthetic_wbfs())
            data[4:8] = sectors.to_bytes(4, "big")
            self.path.write_bytes(data)
            with self.assertRaises(ValueError):
                disc_header.inspect(self.path)

    def test_truncated_wbfs_copied_header_and_wrong_magic_reject(self):
        for shift in (9, 12):
            data = synthetic_wbfs(shift=shift)
            size = 1 << shift
            for length in (32, size-1, size+31):
                self.path.write_bytes(data[:length])
                with self.assertRaises(ValueError):
                    disc_header.inspect(self.path)
            corrupt = bytearray(data)
            corrupt[size+24] = 0
            self.path.write_bytes(corrupt)
            with self.assertRaises(ValueError):
                disc_header.inspect(self.path)


if __name__ == "__main__":
    unittest.main()
