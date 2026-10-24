#!/usr/bin/env python3
"""Read-only, resumable disc identity survey. Standard library; no network.

Paths and filenames in the output are private. Unsupported codecs stay unread.
"""
import argparse
import gzip
import json
import os
from pathlib import Path
import re
import struct
import time
import zlib

VERSION = 1
MIB = 1024 * 1024
EXTENSIONS = {'.iso', '.bin', '.cue', '.gdi', '.cdi', '.chd', '.cso', '.gz',
              '.pbp', '.m3u', '.gcm', '.wbfs', '.rvz', '.wia', '.gcz', '.wad', '.nkit'}


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
        if at >= 0 and at + 74 <= len(block):
            product = block[at + 64:at + 74].decode('ascii', 'strict').strip()
            if re.fullmatch(r'[A-Z0-9][A-Z0-9 -]{2,9}', product):
                return 'dreamcast', product
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


def identify(path, system, seconds=20, seen=None):
    seen = set() if seen is None else seen
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
            names = [line.strip() for line in text.splitlines() if line.strip() and not line.lstrip().startswith('#')][:1]
        elif ext == '.gdi':
            names = re.findall(r'^\s*3\s+\d+\s+\d+\s+\d+\s+(?:"([^"]+)"|(\S+))', text, re.M)
            names = [a or b for a, b in names]
        else:
            names = re.findall(r'^\s*FILE\s+"([^"]+)"\s+BINARY', text, re.I | re.M)
        for name in names:
            target = (path.parent / name).resolve()
            if target.parent != path.parent:
                raise Unread('sheet reference outside its folder')
            try:
                return identify(target, system, max(1, deadline - time.monotonic()), seen)
            except Unread:
                if ext == '.m3u':
                    raise
        raise Unread('no readable data track')
    if ext == '.chd':
        raise Unread('chd codec')
    with path.open('rb') as f:
        if system == 'dreamcast':
            return katana(f, deadline)
        read = lambda offset, count: exact(f, offset, count)
        if ext in {'.cso', '.gcz', '.pbp'}:
            read = compressed_reader(f, ext[1:], deadline)
        if ext == '.gz':
            stream = gzip.GzipFile(fileobj=f)
            window = b''
            position = 0

            def read(offset, count):
                nonlocal stream, window, position
                if offset < 0 or offset + count > 10 * 1024 * MIB:
                    raise Unread('gzip 10 GiB logical budget')
                if offset < position - len(window):
                    stream.close()
                    f.seek(0)
                    stream, window, position = gzip.GzipFile(fileobj=f), b'', 0
                while position < offset + count:
                    if time.monotonic() >= deadline:
                        raise Unread('gzip time budget')
                    piece = stream.read(65536)
                    if not piece:
                        raise Unread('truncated gzip ISO')
                    position += len(piece)
                    window = (window + piece)[-4 * MIB:]
                at = offset - (position - len(window))
                return window[at:at + count]
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--minutes', type=float, default=55)
    parser.add_argument('--file-seconds', type=float, default=20)
    parser.add_argument('--stop-file', type=Path)
    args = parser.parse_args()
    prior = json.loads(args.output.read_text(encoding='utf-8')) if args.output.exists() else {}
    rows = {r['file']: r for r in prior.get('files', [])} if prior.get('version') == VERSION else {}
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
                dirs[:] = sorted(d for d in dirs if not (Path(folder) / d).is_symlink() and not (Path(folder) / d).is_junction())
                for name in sorted(names):
                    path = Path(folder) / name
                    if path.suffix.lower() not in EXTENSIONS or path.is_symlink():
                        continue
                    if time.monotonic() - start > args.minutes * 60 or (args.stop_file and args.stop_file.exists()):
                        complete = False
                        return
                    stat = path.stat()
                    key = str(path)
                    if key in rows and rows[key]['size'] == stat.st_size and rows[key]['mtime_ns'] == stat.st_mtime_ns and rows[key].get('reason') not in {'PBP codec', 'gzip 256 MiB/time budget'}:
                        continue
                    row = dict(file=key, folder_system=system, size=stat.st_size, mtime_ns=stat.st_mtime_ns)
                    then = time.monotonic()
                    try:
                        actual, identity = identify(path, system, args.file_seconds)
                        row.update(system=actual, identity=identity, status='read')
                    except (Unread, OSError, ValueError, EOFError, zlib.error) as error:
                        row.update(system=system, identity='', status='unread', reason=str(error))
                    row['seconds'] = time.monotonic() - then
                    rows[key] = row
                    save()
    finally:
        save(complete)


if __name__ == '__main__':
    main()
