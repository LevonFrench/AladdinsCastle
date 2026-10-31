#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Report black bars (letterbox and pillarbox) in PNG frames. Standard library only.

    python tools/letterbox_check.py frame.png more.png some_folder/
    python tools/letterbox_check.py --eyes 2 --threshold 6 --min-thickness 8 dump/
    python tools/letterbox_check.py --test

A row or column counts as black when no more than --tolerance of its pixels have a
colour channel above --threshold (0-255). Bars are measured from each edge inward:
rows from the top and bottom, columns from the left and right. A bar is reported when
it is at least --min-thickness pixels thick. --eyes 2 treats a side-by-side stereo
frame as two pictures and measures each half on its own, so the centre gap between
the eyes is not mistaken for a pillarbox.

--bands 0.9 also reports black bands inside the picture (rows that are at least 90% black),
for frames where the bars float inside a larger eye buffer instead of touching the edge.

Exit code: 0 no bars, 1 bars found in at least one frame, 2 a file could not be read.
A frame that is entirely black is reported as BLANK (a fade or a loading screen is not
a bar); --fail-on-blank makes it count as bars. Reads 8-bit non-interlaced PNGs of any
colour type, which is what emulator frame dumps and screenshots are.
"""
import argparse
import struct
import sys
import zlib
from operator import add
from pathlib import Path

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}


def read_png(data):
    """Return (width, height, rows) where each row is bytes of RGB triples (palette and
    grey are expanded, alpha is dropped)."""
    if data[:8] != PNG_SIGNATURE:
        raise ValueError("not a PNG file")
    pos, idat, palette = 8, [], None
    width = height = depth = ctype = interlace = None
    while pos + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, ctype, _comp, _filt, interlace = struct.unpack(">IIBBBBB", body)
        elif kind == b"PLTE":
            palette = body
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"IEND":
            break
    if width is None or not idat:
        raise ValueError("truncated PNG")
    if depth != 8 or interlace != 0 or ctype not in CHANNELS:
        raise ValueError("only 8-bit non-interlaced PNGs are supported")
    if ctype == 3 and palette is None:
        raise ValueError("palette PNG without a palette")
    bpp = CHANNELS[ctype]
    stride = width * bpp
    raw = zlib.decompress(b"".join(idat))
    if len(raw) < (stride + 1) * height:
        raise ValueError("truncated PNG pixel data")
    rows, prev = [], bytes(stride)
    for y in range(height):
        base = y * (stride + 1)
        ftype = raw[base]
        cur = raw[base + 1:base + 1 + stride]
        if ftype == 0:
            pass
        elif ftype == 1:
            out = bytearray(cur)
            for i in range(bpp, stride):
                out[i] = (out[i] + out[i - bpp]) & 255
            cur = bytes(out)
        elif ftype == 2:
            cur = bytes([(a + b) & 255 for a, b in zip(cur, prev)])
        elif ftype == 3:
            out = bytearray(cur)
            for i in range(stride):
                left = out[i - bpp] if i >= bpp else 0
                out[i] = (out[i] + ((left + prev[i]) >> 1)) & 255
            cur = bytes(out)
        elif ftype == 4:
            out = bytearray(cur)
            for i in range(stride):
                a = out[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                out[i] = (out[i] + pred) & 255
            cur = bytes(out)
        else:
            raise ValueError("bad PNG filter type %d" % ftype)
        prev = cur
        if ctype == 2:
            rows.append(cur)
        elif ctype == 6:
            rows.append(b"".join(cur[i:i + 3] for i in range(0, stride, 4)))
        elif ctype == 0:
            rows.append(b"".join(bytes((v, v, v)) for v in cur))
        elif ctype == 4:
            rows.append(b"".join(bytes((v, v, v)) for v in cur[0::2]))
        else:
            rows.append(b"".join(palette[3 * v:3 * v + 3] for v in cur))
    return width, height, rows


def edge_runs(flags, tolerance_ok):
    """Count leading and trailing entries for which tolerance_ok is true. Returns
    (lead, trail); if every entry passes, lead == len(flags) and trail == 0."""
    n = len(flags)
    lead = 0
    while lead < n and tolerance_ok(flags[lead]):
        lead += 1
    if lead == n:
        return n, 0
    trail = 0
    while trail < n and tolerance_ok(flags[n - 1 - trail]):
        trail += 1
    return lead, trail


def band_runs(row_bad, w, coverage, min_rows):
    """Runs of at least min_rows consecutive rows whose black share is at least coverage.
    Returns [(first_row, row_count)]. Unlike the edge bars this finds bands anywhere, such
    as black bars drawn inside a frame that is itself larger than the picture."""
    runs, start = [], None
    for y, bad in enumerate(row_bad + [w]):             # the sentinel closes a run at the end
        black = y < len(row_bad) and (w - bad) >= coverage * w
        if black and start is None:
            start = y
        elif not black and start is not None:
            if y - start >= min_rows:
                runs.append((start, y - start))
            start = None
    return runs


def measure(width, rows, threshold, tolerance, x0=0, x1=None, band_coverage=None, min_rows=4):
    """Measure one picture (columns x0..x1 of every row). Returns a dict with the number
    of black rows at the top and bottom and black columns at the left and right."""
    x1 = width if x1 is None else x1
    w = x1 - x0
    height = len(rows)
    table = bytes(1 if v > threshold else 0 for v in range(256))
    row_bad = []                    # non-black pixel count per row
    col_total = [0] * w             # non-black pixel count per column
    acc, acc_rows = 0, 0
    for row in rows:
        seg = row[3 * x0:3 * x1]
        lanes = [int.from_bytes(seg[c::3].translate(table), "little") for c in range(3)]
        mask = lanes[0] | lanes[1] | lanes[2]      # one byte (0 or 1) per pixel
        row_bad.append(bin(mask).count("1"))
        acc += mask
        acc_rows += 1
        if acc_rows == 255:                        # byte lanes cannot overflow before this
            col_total = list(map(add, col_total, acc.to_bytes(w, "little")))
            acc, acc_rows = 0, 0
    if acc_rows:
        col_total = list(map(add, col_total, acc.to_bytes(w, "little")))
    row_limit = tolerance * w
    col_limit = tolerance * height
    top, bottom = edge_runs(row_bad, lambda n: n <= row_limit)
    left, right = edge_runs(col_total, lambda n: n <= col_limit)
    result = {"width": w, "height": height, "top": top, "bottom": bottom,
              "left": left, "right": right,
              "blank": top == height or left == w, "bands": []}
    if band_coverage is not None and not result["blank"]:
        # interior bands only; the rows touching the top or bottom edge are the edge bars
        result["bands"] = [(y, n) for y, n in band_runs(row_bad, w, band_coverage, min_rows)
                           if y > 0 and y + n < height]
    return result


def analyse(data, threshold, tolerance, eyes, band_coverage=None, min_rows=4):
    width, height, rows = read_png(data)
    pictures = []
    for e in range(eyes):
        x0, x1 = width * e // eyes, width * (e + 1) // eyes
        pictures.append(measure(width, rows, threshold, tolerance, x0, x1, band_coverage, min_rows))
    return width, height, pictures


def verdict(pictures, min_thickness, fail_on_blank):
    """Return (has_bars, text) for a list of per-picture measurements."""
    parts, bars = [], False
    for i, m in enumerate(pictures):
        label = "" if len(pictures) == 1 else "eye%d " % (i + 1)
        if m["blank"]:
            parts.append(label + "BLANK (entirely black)")
            bars = bars or fail_on_blank
            continue
        found = []
        for name, px, total in (("top", m["top"], m["height"]), ("bottom", m["bottom"], m["height"]),
                                ("left", m["left"], m["width"]), ("right", m["right"], m["width"])):
            if px >= min_thickness:
                found.append("%s %d px (%.1f%%)" % (name, px, 100.0 * px / total))
        for y, n in m["bands"]:
            found.append("band rows %d-%d (%d px)" % (y, y + n - 1, n))
        if found:
            bars = True
            parts.append(label + "BARS " + ", ".join(found))
        else:
            parts.append(label + "ok")
    return bars, "; ".join(parts)


def gather(paths):
    files = []
    for p in paths:
        path = Path(p)
        if path.is_dir():
            files.extend(sorted(path.rglob("*.png")))
        else:
            files.append(path)
    return files


# ---------------------------------------------------------------- self-test

def _encode_png(width, height, pixel, ctype=2, filter_type=0):
    """Tiny PNG writer for the self-test. pixel(x, y) returns an (r, g, b) tuple."""
    bpp = CHANNELS[ctype]
    prev = bytes(width * bpp)
    out = bytearray()
    for y in range(height):
        cur = bytearray()
        for x in range(width):
            r, g, b = pixel(x, y)
            cur += bytes((r, g, b)) if ctype == 2 else bytes((r, g, b, 255))
        filtered = bytearray()
        for i in range(len(cur)):
            a = cur[i - bpp] if i >= bpp else 0
            up = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if filter_type == 0:
                pred = 0
            elif filter_type == 1:
                pred = a
            elif filter_type == 2:
                pred = up
            elif filter_type == 3:
                pred = (a + up) >> 1
            else:
                p = a + up - c
                pa, pb, pc = abs(p - a), abs(p - up), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (up if pb <= pc else c)
            filtered.append((cur[i] - pred) & 255)
        out += bytes([filter_type]) + bytes(filtered)
        prev = bytes(cur)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    return (PNG_SIGNATURE + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, ctype, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(out))) + chunk(b"IEND", b""))


def self_test():
    def scene(x, y):                # busy, never black
        return (40 + (x * 7) % 200, 30 + (y * 11) % 200, 90 + (x + y) % 100)
    W, H = 48, 32
    cases = [
        ("clean", lambda x, y: scene(x, y), 0, 0, 0, 0),
        ("letterbox", lambda x, y: (0, 0, 0) if y < 5 or y >= H - 6 else scene(x, y), 5, 6, 0, 0),
        ("pillarbox", lambda x, y: (0, 0, 0) if x < 7 or x >= W - 3 else scene(x, y), 0, 0, 7, 3),
        ("window", lambda x, y: (0, 0, 0) if x < 4 or x >= W - 4 or y < 4 or y >= H - 4 else scene(x, y), 4, 4, 4, 4),
        ("dark-not-black", lambda x, y: (30, 30, 30) if y < 5 else scene(x, y), 0, 0, 0, 0),
        ("near-black-noise", lambda x, y: (3, 2, 1) if y < 5 else scene(x, y), 5, 0, 0, 0),
    ]
    for name, fn, top, bottom, left, right in cases:
        for ctype in (2, 6):
            for ftype in (0, 1, 2, 3, 4):
                m = analyse(_encode_png(W, H, fn, ctype, ftype), 8, 0.0, 1)[2][0]
                got = (m["top"], m["bottom"], m["left"], m["right"])
                assert got == (top, bottom, left, right), (name, ctype, ftype, got)
    # tolerance: a bright speck inside the top bar is ignored only when tolerated
    def speck(x, y):
        return (255, 255, 255) if (x, y) == (10, 2) else ((0, 0, 0) if y < 5 else scene(x, y))
    png = _encode_png(W, H, speck)
    assert analyse(png, 8, 0.0, 1)[2][0]["top"] == 2
    assert analyse(png, 8, 0.03, 1)[2][0]["top"] == 5
    # side-by-side eyes: black columns at the inner edge of the left eye are a bar for that eye only
    def sbs(x, y):
        return (0, 0, 0) if (y < 4 or y >= H - 4 or W // 2 - 6 <= x < W // 2) else scene(x, y)
    png = _encode_png(W, H, sbs)
    assert analyse(png, 8, 0.0, 1)[2][0]["right"] == 0
    pics = analyse(png, 8, 0.0, 2)[2]
    assert pics[0]["right"] == 6 and pics[1]["right"] == 0 and all(p["top"] == 4 and p["bottom"] == 4 for p in pics)
    # interior bands: a black stripe inside the picture, found only with band coverage on
    def striped(x, y):
        return (0, 0, 0) if 10 <= y < 14 and 2 <= x < W - 2 else scene(x, y)
    png = _encode_png(W, H, striped)
    assert analyse(png, 8, 0.0, 1)[2][0]["bands"] == []
    assert analyse(png, 8, 0.0, 1, 0.9, 4)[2][0]["bands"] == [(10, 4)]
    assert analyse(png, 8, 0.0, 1, 0.99, 4)[2][0]["bands"] == []
    assert verdict(analyse(png, 8, 0.0, 1, 0.9, 4)[2], 4, False)[0] is True
    # blank frame, and the verdict text / thresholds
    blank = analyse(_encode_png(W, H, lambda x, y: (0, 0, 0)), 8, 0.0, 1)[2]
    assert blank[0]["blank"] and verdict(blank, 4, False)[0] is False and verdict(blank, 4, True)[0] is True
    bars = analyse(_encode_png(W, H, cases[1][1]), 8, 0.0, 1)[2]
    assert verdict(bars, 4, False)[0] is True and verdict(bars, 7, False)[0] is False
    # the command line: exit codes for a clean file, a barred file and a bad file
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        good, bad, junk = Path(tmp, "good.png"), Path(tmp, "bad.png"), Path(tmp, "junk.png")
        good.write_bytes(_encode_png(W, H, cases[0][1]))
        bad.write_bytes(_encode_png(W, H, cases[1][1]))
        junk.write_bytes(b"not a png")
        quiet = open(Path(tmp, "out.txt"), "w")
        old = sys.stdout, sys.stderr
        sys.stdout = sys.stderr = quiet
        try:
            codes = (main([str(good)]), main([str(bad)]), main([str(good), str(junk)]), main([tmp + "/nothing"]))
        finally:
            sys.stdout, sys.stderr = old
            quiet.close()
        assert codes == (0, 1, 2, 2), codes
    print("letterbox_check self-test passed")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="*", help="PNG files or folders (searched recursively)")
    ap.add_argument("--threshold", type=int, default=8,
                    help="a channel value above this is not black (0-255, default 8)")
    ap.add_argument("--tolerance", type=float, default=0.0,
                    help="fraction of pixels in a row or column allowed above the threshold (default 0)")
    ap.add_argument("--min-thickness", type=int, default=4,
                    help="a bar must be at least this many pixels thick to count (default 4)")
    ap.add_argument("--bands", type=float, default=None, metavar="COVERAGE",
                    help="also report black bands anywhere in the picture: runs of at least "
                         "--min-thickness rows that are at least this share black (for example 0.9)")
    ap.add_argument("--eyes", type=int, default=1, choices=(1, 2),
                    help="2 = side-by-side stereo frame, measure each half on its own")
    ap.add_argument("--fail-on-blank", action="store_true", help="count an entirely black frame as bars")
    ap.add_argument("--test", action="store_true", help="run the self-test on synthetic images and exit")
    args = ap.parse_args(argv)
    if args.test:
        return self_test()
    if not 0 <= args.threshold <= 255 or not 0 <= args.tolerance < 1 or args.min_thickness < 1:
        ap.error("threshold 0-255, tolerance 0 to below 1, min-thickness at least 1")
    if args.bands is not None and not 0 < args.bands <= 1:
        ap.error("--bands takes a share above 0 and up to 1")
    files = gather(args.paths)
    if not files:
        print("no PNG files found", file=sys.stderr)
        return 2
    found = failed = False
    for f in files:
        try:
            width, height, pictures = analyse(f.read_bytes(), args.threshold, args.tolerance, args.eyes, args.bands, args.min_thickness)
        except (OSError, ValueError, zlib.error, struct.error) as error:
            print("%s: ERROR %s" % (f, error))
            failed = True
            continue
        bars, text = verdict(pictures, args.min_thickness, args.fail_on_blank)
        found = found or bars
        print("%s: %dx%d %s" % (f, width, height, text))
    return 2 if failed else (1 if found else 0)


if __name__ == "__main__":
    sys.exit(main())
