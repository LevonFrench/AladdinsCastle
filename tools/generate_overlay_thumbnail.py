# SPDX-License-Identifier: GPL-3.0-only
"""Reproduce the original synthetic dashboard icon without external artwork."""
import pathlib
import struct
import zlib

size = 128
pixels = bytearray()
for y in range(size):
    pixels.append(0)  # PNG filter: none
    for x in range(size):
        border = x < 4 or y < 4 or x >= 124 or y >= 124
        tower = (24 <= x < 44 or 84 <= x < 104) and 42 <= y < 104
        battlement = 34 <= y < 48 and (24 <= x < 31 or 37 <= x < 44 or 84 <= x < 91 or 97 <= x < 104)
        wall = 42 <= x < 86 and 62 <= y < 104
        door = 56 <= x < 72 and 82 <= y < 104
        color = (238, 137, 42, 255) if border or ((tower or battlement or wall) and not door) else (16, 19, 28, 255)
        pixels.extend(color)

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b"")
path = pathlib.Path(__file__).resolve().parent.parent / "hub/resources/overlay-thumbnail.png"
path.write_bytes(png)
print("Generated original synthetic overlay thumbnail")
