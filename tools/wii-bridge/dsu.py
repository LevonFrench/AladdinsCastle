#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Loopback-only cemuhook DSU v1001 server. No game data or external dependencies."""
import math
import secrets
import socket
import struct
import time
import zlib
from dataclasses import dataclass

VERSION, PORTS, DATA = 0x100000, 0x100001, 0x100002


def packet(kind, payload, sender=1, magic=b'DSUS'):
    result = bytearray(struct.pack('<4sHHIII', magic, 1001, len(payload)+4, 0, sender, kind)+payload)
    struct.pack_into('<I', result, 8, zlib.crc32(result))
    return bytes(result)


def parse(data):
    if len(data) < 20:
        return None
    magic, version, size, crc, sender, kind = struct.unpack_from('<4sHHIII', data)
    if magic != b'DSUC' or version != 1001 or size < 4 or len(data) < 16+size:
        return None
    checked = bytearray(data[:16+size])
    checked[8:12] = bytes(4)
    if zlib.crc32(checked) != crc:
        return None
    return sender, kind, data[20:16+size]


def axis(value):
    value = max(-1.0, min(1.0, value)) if math.isfinite(value) else 0.0
    return round(128 + value*(127 if value >= 0 else 128))


@dataclass
class Pad:
    connected: bool = False
    pointer: tuple = (0.0, 0.0)  # right/up
    stick: tuple = (0.0, 0.0)
    buttons: int = 0  # two DSU bitmask bytes, little endian
    accel: tuple = (0.0, -1.0, 0.0)  # DSU signed g, stationary upright controller
    gyro: tuple = (0.0, 0.0, 0.0)
    timestamp: int = 0


def mac(slot):
    return bytes([2, 0xAC, 0x57, 0, 0, slot+1])


def info(slot, connected):
    return struct.pack('<BBBB6sB', slot, 2 if connected else 0, 2 if connected else 0,
                       1 if connected else 0, mac(slot), 5 if connected else 0)


def payload(slot, pad, sequence):
    result = bytearray(80)
    result[:11] = info(slot, pad.connected)
    struct.pack_into('<BIHBB4B', result, 11, int(pad.connected), sequence & 0xffffffff,
                     pad.buttons if pad.connected else 0, 0, 0,
                     axis(pad.stick[0]), axis(pad.stick[1]),
                     axis(pad.pointer[0]), axis(pad.pointer[1]))
    # Analog mirrors are what Dolphin reads for the face/shoulder buttons.
    for i, bit in enumerate([7, 6, 5, 4, 15, 14, 13, 12, 11, 10, 9, 8]):
        result[24+i] = 255 if pad.connected and pad.buttons & (1 << bit) else 0
    struct.pack_into('<Q6f', result, 48, pad.timestamp, *pad.accel, *pad.gyro)
    return bytes(result)


class Server:
    def __init__(self, port=26760):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(('127.0.0.1', port))
        self.socket.setblocking(False)
        self.sender = secrets.randbits(32)
        self.subscriptions = {}
        self.sequence = [0, 0]
        self.sent = 0

    def close(self):
        self.socket.close()

    def send(self, address, kind, body):
        try:
            self.socket.sendto(packet(kind, body, self.sender), address)
            self.sent += 1
        except OSError:
            pass  # A closed localhost client must not stop other clients.

    def tick(self, pads, now=None):
        now = time.monotonic() if now is None else now
        # Bound inbound work so a noisy client cannot starve tracking samples.
        for _ in range(64):
            try:
                raw, address = self.socket.recvfrom(2048)
            except (BlockingIOError, ConnectionResetError):
                break
            msg = parse(raw)
            if msg is None:
                continue
            sender, kind, body = msg
            if kind == VERSION and not body:
                self.send(address, kind, struct.pack('<H', 1001))
            elif kind == PORTS and len(body) >= 4:
                count, = struct.unpack_from('<i', body)
                if not 0 <= count <= 4 or len(body) < 4+count:
                    continue
                for slot in body[4:4+count]:
                    if slot < 4:
                        connected = slot < 2 and pads[slot].connected
                        self.send(address, kind, info(slot, connected)+b'\0')
            elif kind == DATA and len(body) == 8:
                flags, slot = body[:2]
                if flags & ~3:
                    continue
                selected = [i for i in range(2) if
                            (flags == 0 or (flags & 1 and slot == i) or
                             (flags & 2 and body[2:] == mac(i)))]
                # One subscription per client endpoint AND slot: never broadcast
                # another hand into Dolphin's per-device socket.
                for i in selected:
                    self.subscriptions[(address, sender, i)] = now+5.0
        for i in range(2):
            self.sequence[i] = (self.sequence[i]+1) & 0xffffffff
        for key, deadline in list(self.subscriptions.items()):
            if deadline <= now:
                del self.subscriptions[key]
                continue
            address, _, slot = key
            self.send(address, DATA, payload(slot, pads[slot], self.sequence[slot]))
