# SPDX-License-Identifier: GPL-3.0-only
"""Bounded ISO/single-disc WBFS Wii header identity, not payload verification."""
import hashlib
import os
from pathlib import Path
import re

MAGIC = b"\x5d\x1c\x9e\xa3"


def valid_id(value):
    return isinstance(value, str) and re.fullmatch(r"[A-Z0-9]{6}", value) is not None


def inspect(path, expected_id=None):
    if expected_id is not None and not valid_id(expected_id):
        raise ValueError("Expected six-character Wii game_id")
    with Path(path).open("rb") as stream:
        header = stream.read(32)
        if len(header) != 32:
            raise ValueError("Disc header is truncated")
        offset, kind = 0, "iso"
        if header[:4] == b"WBFS":
            host_shift, wbfs_shift = header[8:10]
            if not 9 <= host_shift <= 12 or not 15 <= wbfs_shift <= 31 or wbfs_shift < host_shift:
                raise ValueError("Invalid WBFS sector shifts")
            size = 1 << host_shift  # Bounded before allocation: at most 4096 bytes.
            sectors = int.from_bytes(header[4:8], "big")
            if not sectors or sectors * size < size + 32 or os.fstat(stream.fileno()).st_size < size + 32:
                raise ValueError("Truncated WBFS disc information")
            stream.seek(0)
            container = stream.read(size)
            if len(container) != size or not container[12]:
                raise ValueError("WBFS has no first disc slot")
            if any(container[13:]):
                raise ValueError("Multiple WBFS disc slots are unsupported")
            offset, kind = size, "wbfs"
            header = stream.read(32)
            if len(header) != 32:
                raise ValueError("Copied Wii header is truncated")
    game_id = header[:6].decode("ascii", errors="replace")
    if not valid_id(game_id) or header[24:28] != MAGIC:
        raise ValueError("Invalid Wii disc ID or magic")
    if expected_id is not None and game_id != expected_id:
        raise ValueError("Disc header differs from selected game_id")
    return {"game_id": game_id, "format": kind, "offset": offset,
            "sha256": hashlib.sha256(header).hexdigest()}
