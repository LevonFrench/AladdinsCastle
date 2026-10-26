#!/usr/bin/env python3
"""Read-only, resumable disc identity survey. Python 3.12+ standard library; no network.

Paths and filenames in the output are private. Unsupported codecs stay unread.
"""
import argparse
import gzip
import json
import os
from pathlib import Path
import re
import struct
import sys
import time
import zlib

VERSION = 1
MIB = 1024 * 1024
EXTENSIONS = {'.iso', '.bin', '.cue', '.gdi', '.cdi', '.chd', '.cso', '.gz',
              '.pbp', '.m3u', '.gcm', '.wbfs', '.rvz', '.wia', '.gcz', '.wad', '.nkit'}
PROTECTED_STEMS = {'credential', 'credentials', 'license', 'licence', 'activation',
                   'authentication', 'auth', 'token', 'tokens', 'entitlement',
                   'entitlements', 'keychain', 'keychains'}
PROTECTED_DIRS = {'.git', '.ssh', '.aws', '.azure', '.gnupg', '.codex', '.claude',
                  'appdata', 'windows', 'programdata', '$recycle.bin',
                  'system volume information', 'credentials', 'keychains',
                  'licenses', 'licences', 'entitlements', 'auth', 'tokens'}


def protected_name(name):
    return Path(name).stem.casefold() in PROTECTED_STEMS or str(name).casefold() in PROTECTED_DIRS


class Unread(Exception):
    pass


def exact(f, offset, count):
    if offset < 0 or count < 0 or count > 4 * MIB:
        raise Unread('invalid read bounds')
    f.seek(offset)
    data = f.read(count)
    if len(data) != count:
        raise Unread('truncated header')
    return data


def iso_serial(read):
    for stride, payload in [(2048, 0), (2352, 16), (2352, 24), (2448, 16), (2448, 24)]:
        pvd = read(16 * stride + payload, 2048)
        if pvd[1:6] == b'CD001' and pvd[0] == 1:
            break
    else:
        raise Unread('no ISO9660 primary descriptor')

    def logical(sector, count):
        if count > 2 * MIB:
            raise Unread('ISO directory budget')
        return b''.join(read((sector + p // 2048) * stride + payload, min(2048, count - p))
                        for p in range(0, count, 2048))

    sector, count = struct.unpack_from('<I', pvd, 158)[0], struct.unpack_from('<I', pvd, 166)[0]
    directory = logical(sector, count)
    pos = 0
    while pos < len(directory):
        size = directory[pos]
        if not size:
            pos = (pos // 2048 + 1) * 2048
            continue
        if size < 34 or pos + size > len(directory):
            raise Unread('invalid ISO directory')
        row = directory[pos:pos + size]
        if row[33:33 + row[32]].upper().split(b';')[0] == b'SYSTEM.CNF':
            length = struct.unpack_from('<I', row, 10)[0]
            if length > 65536:
                raise Unread('SYSTEM.CNF budget')
            cnf = logical(struct.unpack_from('<I', row, 2)[0], length)
            match = re.search(rb'^\s*(BOOT2?)\s*=\s*[^\r\n]*?([A-Z]{4})[_-](\d{3})\.(\d{2})', cnf, re.I | re.M)
            if match:
                return ('ps2' if match[1].upper() == b'BOOT2' else 'psx',
                        (match[2] + b'-' + match[3] + match[4]).decode().upper())
            raise Unread('no BOOT/BOOT2 serial')
        pos += size
    raise Unread('no SYSTEM.CNF')


def katana(f, deadline):
    tail = b''
    total = 0
    while time.monotonic() < deadline and total < 2 * 1024 * MIB:
        data = f.read(MIB)
        if not data:
            raise Unread('no Dreamcast boot header')
        total += len(data)
        block = tail + data
        at = block.find(b'SEGA SEGAKATANA')
        while at >= 0 and at + 74 <= len(block):
            product = block[at + 64:at + 74].decode('ascii', 'replace').strip()
            if re.fullmatch(r'[A-Z0-9][A-Z0-9 -]{2,9}', product):
                return 'dreamcast', product
            at = block.find(b'SEGA SEGAKATANA', at + 1)
        tail = block[-128:]
    raise Unread('Dreamcast time/2 GiB read budget')


def compressed_reader(f, kind, deadline):
    header = exact(f, 0, 40 if kind == 'pbp' else 32 if kind == 'gcz' else 24)
    size = os.fstat(f.fileno()).st_size
    if kind == 'cso':
        if header[:4] != b'CISO':
            raise Unread('unsupported CSO header')
        base, logical, block = struct.unpack_from('<IQI', header, 4)
        shift = header[21]
        if header[20] > 1 or shift > 16:
            raise Unread('unsupported CSO version/alignment')
        entries = (logical + block - 1) // block if block else 0
    elif kind == 'gcz':
        magic, subtype, packed, logical, block, entries = struct.unpack('<IIQQII', header)
        if magic != 0xB10BC001 or subtype > 1:
            raise Unread('unsupported GCZ header')
        base = 32
    else:
        if header[:4] != b'\0PBP':
            raise Unread('invalid PBP header')
        disc = struct.unpack_from('<I', header, 36)[0]
        if exact(f, disc, 16).startswith(b'PSTITLEIMG000000'):
            relative = struct.unpack('<I', exact(f, disc + 0x200, 4))[0]
            if not relative or relative == 0x44475000:
                raise Unread('encrypted/empty PBP disc table')
            disc += relative
        if not exact(f, disc, 12).startswith(b'PSISOIMG0000'):
            raise Unread('encrypted/unsupported PBP image')
        relative = struct.unpack('<I', exact(f, disc + 0xbfc, 4))[0]
        if relative < 0x100000:
            raise Unread('invalid PBP data offset')
        base, block, entries = disc + 0x4000, 16 * 2352, 32256
        logical, pbp_data = entries * block, disc + relative
    if block < 512 or block > 2 * MIB or not entries or entries > 16 * MIB:
        raise Unread('compressed block dimensions')
    data_base = 32 + entries * 12 if kind == 'gcz' else 0
    cache = {}

    def read(offset, count):
        if offset < 0 or count < 0 or offset + count > logical or count > 2 * MIB:
            raise Unread('compressed logical bounds')
        out = b''
        while count:
            if time.monotonic() >= deadline:
                raise Unread('compressed time budget')
            number, within = divmod(offset, block)
            if number not in cache:
                if kind == 'cso':
                    a, b = struct.unpack('<II', exact(f, base + number * 4, 8))
                    raw = bool(a & 0x80000000)
                    start, end = (a & 0x7fffffff) << shift, (b & 0x7fffffff) << shift
                elif kind == 'gcz':
                    a = struct.unpack('<Q', exact(f, base + number * 8, 8))[0]
                    b = struct.unpack('<Q', exact(f, base + (number + 1) * 8, 8))[0] if number + 1 < entries else packed
                    raw = bool(a & (1 << 63))
                    start, end = data_base + (a & ((1 << 63) - 1)), data_base + (b & ((1 << 63) - 1))
                else:
                    relative, length = struct.unpack('<IH', exact(f, base + number * 32, 6))
                    start, end, raw = pbp_data + relative, pbp_data + relative + length, length == block
                if start > size or end < start or end - start > 4 * MIB:
                    raise Unread('compressed physical bounds')
                packed_block = exact(f, start, block if raw else end - start)
                if kind == 'gcz':
                    checksum = struct.unpack('<I', exact(f, 32 + entries * 8 + number * 4, 4))[0]
                    if zlib.adler32(packed_block) != checksum:
                        raise Unread('GCZ block checksum mismatch')
                decoder = zlib.decompressobj(15 if kind == 'gcz' else -15)
                decoded = packed_block if raw else decoder.decompress(packed_block, block + 1)
                if len(decoded) != min(block, logical - number * block):
                    raise Unread('compressed block length')
                cache.clear()
                cache[number] = decoded
            n = min(count, block - within)
            out += cache[number][within:within + n]
            count -= n
            offset += n
        return out
    return read


def identify(path, system, seconds=20, seen=None, base_offset=0):
    seen = set() if seen is None else seen
    if protected_name(Path(path).name):
        raise Unread('protected media name')
    path = Path(path).resolve()
    if path in seen or len(seen) >= 8:
        raise Unread('playlist/sheet recursion')
    seen.add(path)
    deadline = time.monotonic() + seconds
    ext = path.suffix.lower()
    if ext in {'.cue', '.gdi', '.m3u'}:
        if path.stat().st_size > 65536:
            raise Unread('sheet size budget')
        text = path.read_text(encoding='utf-8-sig', errors='replace')
        if ext == '.m3u':
            tracks = [(line.strip(), 0) for line in text.splitlines() if line.strip() and not line.lstrip().startswith('#')][:1]
        elif ext == '.gdi':
            matches = re.findall(r'^\s*3\s+\d+\s+\d+\s+\d+\s+(?:"([^"]+)"|(\S+))\s+(\d+)', text, re.M)
            tracks = [(a or b, int(offset)) for a, b, offset in matches]
        else:
            tracks, files, current, data, stride, has_track = [], [], '', False, 2352, False
            for line in text.splitlines():
                file = re.fullmatch(r'\s*FILE\s+"([^"]+)"\s+BINARY\s*', line, re.I)
                track = re.fullmatch(r'\s*TRACK\s+\d+\s+(\S+)\s*', line, re.I)
                index = re.fullmatch(r'\s*INDEX\s+01\s+(\d+):(\d+):(\d+)\s*', line, re.I)
                if file:
                    current, data = file[1], False
                    files.append(current)
                elif track:
                    mode, has_track = track[1].upper(), True
                    data = mode.startswith(('MODE1/', 'MODE2/'))
                    stride = 2048 if mode.endswith('/2048') else 2448 if mode.endswith('/2448') else 2352
                elif index and data and current:
                    minute, second, frame = map(int, index.groups())
                    if minute < 1000 and second < 60 and frame < 75:
                        tracks.append((current, ((minute * 60 + second) * 75 + frame) * stride))
                    data = False
            if not has_track:
                tracks = [(name, 0) for name in files]
        for name, offset in tracks:
            if protected_name(Path(name).name):
                raise Unread('protected media name')
            reference = path.parent / name
            if reference.is_symlink() or reference.is_junction():
                raise Unread('linked sheet reference')
            target = (path.parent / name).resolve()
            if target.parent != path.parent:
                raise Unread('sheet reference outside its folder')
            try:
                return identify(target, system, max(1, deadline - time.monotonic()), seen, offset)
            except Unread:
                if ext == '.m3u':
                    raise
        raise Unread('no readable data track')
    if ext == '.chd':
        raise Unread('chd codec')
    with path.open('rb') as f:
        if system == 'dreamcast':
            f.seek(base_offset)
            return katana(f, deadline)
        read = lambda offset, count: exact(f, base_offset + offset, count)
        if ext in {'.cso', '.gcz', '.pbp'}:
            read = compressed_reader(f, ext[1:], deadline)
        if ext == '.gz':
            stream = gzip.GzipFile(fileobj=f)
            window = bytearray()
            position = 0

            def read(offset, count):
                nonlocal stream, window, position
                if offset < 0 or offset + count > 10 * 1024 * MIB:
                    raise Unread('gzip 10 GiB logical budget')
                if offset < position - len(window):
                    stream.close()
                    f.seek(0)
                    stream, window, position = gzip.GzipFile(fileobj=f), bytearray(), 0
                while position < offset + count:
                    if time.monotonic() >= deadline:
                        raise Unread('gzip time budget')
                    piece = stream.read(65536)
                    if not piece:
                        raise Unread('truncated gzip ISO')
                    position += len(piece)
                    window.extend(piece)
                    if len(window) > 8 * MIB:
                        del window[:len(window) - 4 * MIB]
                at = offset - (position - len(window))
                return bytes(window[at:at + count])
        if system in {'ps2', 'psx'}:
            return iso_serial(read)
        start = read(0, 32)
        if ext == '.wad':
            header, cert, crl, ticket = (struct.unpack_from('>I', start, n)[0] for n in (0, 8, 12, 16))
            align = lambda n: (n + 63) & ~63
            ticket_at = align(header) + align(cert) + align(crl)
            if header < 32 or header > 65536 or ticket < 0x1e4:
                raise Unread('invalid WAD sections')
            signature = struct.unpack('>I', read(ticket_at, 4))[0]
            body = {0x10000: 0x240, 0x10001: 0x140, 0x10002: 0x80}.get(signature)
            if body is None or body + 0xa4 > ticket:
                raise Unread('unsupported WAD ticket signature')
            return 'wii', read(ticket_at + body + 0x9c, 8).hex().upper()
        offset = 0
        if start[:4] == b'WBFS':
            if not 9 <= start[8] <= 25:
                raise Unread('invalid WBFS sector size')
            # Table entries point to occupied host sectors; the first need not be 1.
            slots = read(12, min((1 << start[8]) - 12, 65536))
            sector = next((n for n in slots if n), 0)
            if not sector:
                raise Unread('empty WBFS')
            offset = sector << start[8]
        elif start[:4] in {b'RVZ\x01', b'WIA\x01'}:
            offset = 0x58
        header = read(offset, 32)
        game = header[:6]
        if not re.fullmatch(rb'[A-Z0-9]{6}', game):
            raise Unread('invalid Nintendo disc ID')
        if header[0x18:0x1c] == bytes.fromhex('5d1c9ea3'):
            return 'wii', game.decode()
        if header[0x1c:0x20] == bytes.fromhex('c2339f3d'):
            return 'gc', game.decode()
        raise Unread('no Nintendo disc magic')


def self_test():
    """Run authored metadata fixtures only; never visit a survey root."""
    import tempfile
    import unittest
    from unittest import mock

    def put32(data, offset, value, endian='<'):
        struct.pack_into(endian + 'I', data, offset, value)

    def iso(ps2=True):
        data = bytearray(24 * 2048)
        p, d = 16 * 2048, 20 * 2048
        data[p:p + 7] = b'\x01CD001\x01'
        data[p + 156] = 34
        put32(data, p + 158, 20)
        put32(data, p + 166, 2048)
        data[d] = 46
        put32(data, d + 2, 21)
        put32(data, d + 10, 64)
        data[d + 32] = 12
        data[d + 33:d + 45] = b'SYSTEM.CNF;1'
        cnf = (b'BOOT2' if ps2 else b'BOOT') + b' = cdrom0:\\SLUS_999.98;1\r\n'
        data[21 * 2048:21 * 2048 + len(cnf)] = cnf
        return bytes(data)

    def deflate(data):
        encoder = zlib.compressobj(level=1, wbits=-15)
        return encoder.compress(data) + encoder.flush()

    def cso(image, compressed):
        blocks = len(image) // 2048
        data = bytearray(24 + (blocks + 1) * 4)
        struct.pack_into('<4sIQIBB', data, 0, b'CISO', 24, len(image), 2048, 1, 0)
        for n in range(blocks):
            put32(data, 24 + n * 4, len(data) | (0 if compressed else 0x80000000))
            block = image[n * 2048:(n + 1) * 2048]
            data.extend(deflate(block) if compressed else block)
        put32(data, 24 + blocks * 4, len(data))
        return bytes(data)

    def nintendo(wii=True):
        data = bytearray(1024)
        data[:6] = b'RZZE01' if wii else b'GZZE01'
        put32(data, 0x18 if wii else 0x1c, 0x5d1c9ea3 if wii else 0xc2339f3d, '>')
        return bytes(data)

    def gcz(compressed):
        image = nintendo(False)
        payload = zlib.compress(image) if compressed else image
        header = struct.pack('<IIQQIIQI', 0xb10bc001, 0, len(payload), len(image), 1024, 1,
                             0 if compressed else 1 << 63, zlib.adler32(payload))
        return header + payload

    def pbp(compressed, multidisc=False):
        psar = 0x100
        disc = psar + (0x400 if multidisc else 0)
        data = bytearray(disc + 0x100000)
        struct.pack_into('<4sI8I', data, 0, b'\0PBP', 0x10000, *([psar] * 8))
        if multidisc:
            data[psar:psar + 16] = b'PSTITLEIMG000000'
            put32(data, psar + 0x200, disc - psar)
            # A bogus second disc pointer must never replace the first disc.
            put32(data, psar + 0x204, 0xfffffff0)
        data[disc:disc + 12] = b'PSISOIMG0000'
        put32(data, disc + 0xbfc, 0x100000)
        image = iso(False)
        block_size = 16 * 2352
        raw = bytearray(2 * block_size)
        for n in range(24):
            raw[n * 2352 + 24:n * 2352 + 24 + 2048] = image[n * 2048:(n + 1) * 2048]
        offset = 0
        for n in range(2):
            block = bytes(raw[n * block_size:(n + 1) * block_size])
            payload = deflate(block) if compressed else block
            struct.pack_into('<IH', data, disc + 0x4000 + n * 32, offset, len(payload))
            data.extend(payload)
            offset += len(payload)
        return bytes(data)

    class SurveyTests(unittest.TestCase):
        def setUp(self):
            self.directory = tempfile.TemporaryDirectory(prefix='disc-id-fixtures-')
            self.addCleanup(self.directory.cleanup)
            self.root = Path(self.directory.name)

        def file(self, name, data):
            path = self.root / name
            path.write_bytes(data)
            return path

        def test_playstation_formats(self):
            for name, data, expected in [
                ('boot.iso', iso(False), 'psx'), ('boot2.iso', iso(), 'ps2'),
                ('raw.cso', cso(iso(), False), 'ps2'), ('packed.cso', cso(iso(), True), 'ps2'),
                ('disc.gz', gzip.compress(iso(), mtime=0), 'ps2'),
                ('raw.pbp', pbp(False), 'psx'), ('packed.pbp', pbp(True), 'psx'),
                ('first-disc.pbp', pbp(True, True), 'psx')]:
                with self.subTest(format=name):
                    self.assertEqual(identify(self.file(name, data), 'ps2'), (expected, 'SLUS-99998'))

        def test_gzip_distant_multisector_root(self):
            # Small compressed fixture; its sparse logical ISO has a distant root.
            root = 100 * MIB // 2048
            original = iso()
            prefix = bytearray(original)
            put32(prefix, 16 * 2048 + 158, root)
            put32(prefix, 16 * 2048 + 166, 6 * 2048)
            directory = bytearray(6 * 2048)
            directory[5 * 2048:5 * 2048 + 46] = original[20 * 2048:20 * 2048 + 46]
            put32(directory, 5 * 2048 + 2, root + 6)
            path = self.root / 'distant.gz'
            with gzip.open(path, 'wb', compresslevel=1) as stream:
                stream.write(prefix)
                remaining = root * 2048 - len(prefix)
                while remaining:
                    count = min(MIB, remaining)
                    stream.write(bytes(count))
                    remaining -= count
                stream.write(directory)
                stream.write(original[21 * 2048:22 * 2048])
            self.assertEqual(identify(path, 'ps2'), ('ps2', 'SLUS-99998'))
        def test_nintendo_formats(self):
            wbfs = bytearray(512)
            wbfs[:4] = b'WBFS'
            put32(wbfs, 4, 3, '>')
            wbfs[8], wbfs[9], wbfs[12] = 9, 20, 1
            for name, data, expected in [
                ('disc.iso', nintendo(), ('wii', 'RZZE01')),
                ('disc.gcm', nintendo(False), ('gc', 'GZZE01')),
                ('disc.nkit', nintendo(), ('wii', 'RZZE01')),
                ('disc.nkit.iso', nintendo(), ('wii', 'RZZE01')),
                ('disc.wbfs', bytes(wbfs) + nintendo(), ('wii', 'RZZE01')),
                ('disc.rvz', b'RVZ\x01' + bytes(0x54) + nintendo(), ('wii', 'RZZE01')),
                ('disc.wia', b'WIA\x01' + bytes(0x54) + nintendo(), ('wii', 'RZZE01')),
                ('raw.gcz', gcz(False), ('gc', 'GZZE01')),
                ('packed.gcz', gcz(True), ('gc', 'GZZE01'))]:
                with self.subTest(format=name):
                    self.assertEqual(identify(self.file(name, data), expected[0]), expected)

        def test_wad_ticket(self):
            data = bytearray(0x40 + 0x2a4)
            put32(data, 0, 0x20, '>')
            put32(data, 0x10, 0x2a4, '>')
            put32(data, 0x40, 0x10001, '>')
            data[0x40 + 0x1dc:0x40 + 0x1e4] = bytes.fromhex('00010001575a5a45')
            self.assertEqual(identify(self.file('ticket.wad', data), 'wii'), ('wii', '00010001575A5A45'))

        def test_cue_index_and_first_playlist_disc(self):
            image = iso()
            raw = bytearray((75 + 24) * 2352)
            for n in range(24):
                raw[(75 + n) * 2352 + 24:(75 + n) * 2352 + 24 + 2048] = image[n * 2048:(n + 1) * 2048]
            self.file('tracks.bin', raw)
            cue = self.file('disc.cue', b'FILE "tracks.bin" BINARY\n TRACK 01 AUDIO\n INDEX 01 00:00:00\n TRACK 02 MODE2/2352\n INDEX 01 00:01:00\n')
            self.file('second.iso', iso(False))
            playlist = self.file('set.m3u', b'# first disc only\n\ndisc.cue\nsecond.iso\n')
            self.assertEqual(identify(cue, 'ps2'), ('ps2', 'SLUS-99998'))
            self.assertEqual(identify(playlist, 'ps2'), ('ps2', 'SLUS-99998'))
            self.file('unread.iso', bytes(24 * 2048))
            unread_first = self.file('unread-first.m3u', b'unread.iso\nsecond.iso\n')
            with self.assertRaises(Unread):
                identify(unread_first, 'ps2')
            self.file('loop.m3u', b'loop.m3u\n')
            with self.assertRaises(Unread):
                identify(self.root / 'loop.m3u', 'ps2')

        def test_late_cdi_and_deadline(self):
            header = bytearray(b' ' * 256)
            header[:16] = b'SEGA SEGAKATANA  '
            header[64:74] = b'T-99999N  '
            path = self.file('late.cdi', bytes(17 * MIB) + header)
            self.assertEqual(identify(path, 'dreamcast'), ('dreamcast', 'T-99999N'))
            # Deterministic expiry after the first read, before the late header.
            with mock.patch(__name__ + '.time.monotonic', side_effect=[0, 0, 2]):
                with self.assertRaisesRegex(Unread, 'time/2 GiB'):
                    identify(path, 'dreamcast', seconds=1)
            self.file('track03.bin', bytes(4096) + header)
            gdi = self.file('disc.gdi', b'3\n3 45000 4 2352 track03.bin 4096\n')
            self.assertEqual(identify(gdi, 'dreamcast'), ('dreamcast', 'T-99999N'))

        def test_invalid_bounds_and_unproven_names(self):
            malformed = bytearray(cso(iso(), True))
            put32(malformed, 24 + 16 * 4, 0x7fffffff)
            broken_gcz = bytearray(gcz(False))
            struct.pack_into('<Q', broken_gcz, 32, 0x7fffffffffffffff)
            wrong_seed = bytearray(gcz(False))
            put32(wrong_seed, 40, zlib.adler32(wrong_seed[44:], 0))
            with self.assertRaisesRegex(Unread, 'checksum mismatch'):
                identify(self.file('wrong-seed.gcz', wrong_seed), 'gc')
            broken_iso = bytearray(iso())
            put32(broken_iso, 16 * 2048 + 158, 0xffffffff)
            for name, data, system in [('bad.cso', malformed, 'ps2'), ('bad.gcz', broken_gcz, 'gc'),
                                       ('bad.iso', broken_iso, 'ps2'), ('SLUS_999.98.iso', bytes(24 * 2048), 'ps2')]:
                with self.subTest(format=name):
                    with self.assertRaises((Unread, OSError, ValueError, EOFError, zlib.error)):
                        identify(self.file(name, data), system)
            protected = self.file('auth.bin', iso())
            with self.assertRaisesRegex(Unread, 'protected media name'):
                identify(protected, 'ps2')
            cue = self.file('protected.cue', b'FILE "auth.bin" BINARY\n TRACK 01 MODE1/2048\n INDEX 01 00:00:00\n')
            with self.assertRaises(Unread):
                identify(cue, 'ps2')

    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(SurveyTests))
    return 0 if result.wasSuccessful() else 1

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test', action='store_true', help='run generated stdlib-only format fixtures')
    parser.add_argument('--root', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--minutes', type=float, default=55)
    parser.add_argument('--file-seconds', type=float, default=20)
    parser.add_argument('--stop-file', type=Path)
    args = parser.parse_args()
    if args.test:
        return self_test()
    if args.root is None or args.output is None:
        parser.error('--root and --output are required for a private survey')
    try:
        prior = json.loads(args.output.read_text(encoding='utf-8')) if args.output.exists() else {}
        rows = {r['file']: r for r in prior.get('files', [])} if prior.get('version') == VERSION else {}
    except (OSError, ValueError, TypeError, KeyError, AttributeError) as error:
        # An unreadable earlier run only costs the resume: the survey starts again.
        print(f'ignoring unreadable earlier output ({type(error).__name__}); starting again', file=sys.stderr)
        rows = {}
    start = time.monotonic()

    def save(complete=False):
        result = dict(version=VERSION, complete=complete, seconds=time.monotonic() - start, files=list(rows.values()))
        temp = args.output.with_suffix('.tmp')
        temp.write_text(json.dumps(result, indent=2), encoding='utf-8')
        temp.replace(args.output)

    complete = True
    try:
        for system in ('dreamcast', 'ps2', 'psx', 'wii', 'gc'):
            for folder, dirs, names in os.walk(args.root / system, followlinks=False):
                dirs[:] = sorted(d for d in dirs if not protected_name(d) and not (Path(folder) / d).is_symlink() and not (Path(folder) / d).is_junction())
                for name in sorted(names):
                    path = Path(folder) / name
                    if protected_name(name) or path.suffix.lower() not in EXTENSIONS or path.is_symlink():
                        continue
                    if time.monotonic() - start > args.minutes * 60 or (args.stop_file and args.stop_file.exists()):
                        complete = False
                        return
                    try:
                        stat = path.stat()
                    except OSError:
                        continue  # vanished or unreadable since the folder was listed
                    key = str(path)
                    retry_reason = rows.get(key, {}).get('reason', '')
                    retry = retry_reason in {'PBP codec', 'gzip 256 MiB/time budget'} or retry_reason.startswith("'ascii' codec")
                    if key in rows and rows[key]['size'] == stat.st_size and rows[key]['mtime_ns'] == stat.st_mtime_ns and not retry and path.suffix.lower() not in {'.cue', '.gdi', '.m3u'}:
                        continue
                    row = dict(file=key, folder_system=system, size=stat.st_size, mtime_ns=stat.st_mtime_ns)
                    then = time.monotonic()
                    try:
                        actual, identity = identify(path, system, args.file_seconds)
                        row.update(system=actual, identity=identity, status='read')
                    except (Unread, OSError, ValueError, EOFError, zlib.error, struct.error) as error:
                        row.update(system=system, identity='', status='unread', reason=str(error))
                    row['seconds'] = time.monotonic() - then
                    rows[key] = row
                    save()
    finally:
        save(complete)


if __name__ == '__main__':
    raise SystemExit(main())
