# SPDX-License-Identifier: GPL-3.0-only
"""World-fixed fallback pointer plane and actual pose-derived inertial samples."""
import math
from collections import deque
from dataclasses import replace
from dsu import Pad


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def local(matrix, world):
    return tuple(sum(matrix[r*4+c]*world[r] for r in range(3)) for c in range(3))


class MotionLatch:
    """Repeat short sensor events for Dolphin's newest-packet input drain."""

    HOLD_SECONDS = .025

    def __init__(self):
        self.pads = [Pad(), Pad()]
        self.tracked = [False, False]
        self.last_sample = 0.0
        self.accel_pending = [None, None]
        self.gyro_pending = [None, None]

    def publish(self, pads, tracked, dynamic_g, now):
        self.pads = pads[:]
        self.tracked = tracked[:]
        self.last_sample = now
        for slot, pad in enumerate(pads):
            if not tracked[slot]:
                self.accel_pending[slot] = None
                self.gyro_pending[slot] = None
                continue
            if dynamic_g[slot] >= 0.1:
                old = self.accel_pending[slot]
                if old is None or now >= old[2] or dynamic_g[slot] > old[0]:
                    self.accel_pending[slot] = (dynamic_g[slot], pad, now+self.HOLD_SECONDS)
            gyro_speed = math.sqrt(dot(pad.gyro, pad.gyro))
            if gyro_speed >= 5:
                old = self.gyro_pending[slot]
                if old is None or now >= old[2] or gyro_speed > old[0]:
                    self.gyro_pending[slot] = (gyro_speed, pad, now+self.HOLD_SECONDS)

    def consume(self, now):
        if now-self.last_sample >= .15:
            self.accel_pending = [None, None]
            self.gyro_pending = [None, None]
            return [Pad(), Pad()], [False, False]
        result = []
        for slot, pad in enumerate(self.pads):
            if not self.tracked[slot]:
                result.append(Pad())
                continue
            accel_event = self.accel_pending[slot]
            gyro_event = self.gyro_pending[slot]
            if accel_event and now >= accel_event[2]:
                accel_event = self.accel_pending[slot] = None
            if gyro_event and now >= gyro_event[2]:
                gyro_event = self.gyro_pending[slot] = None
            if accel_event:
                pad = replace(pad, accel=accel_event[1].accel,
                              timestamp=accel_event[1].timestamp)
            if gyro_event:
                pad = replace(pad, gyro=gyro_event[1].gyro)
            result.append(pad)
        return result, self.tracked[:]


class Mapper:
    def __init__(self, width=3.2, height=1.8, distance=2.0, offscreen='edge'):
        if offscreen not in ('edge', 'hide'):
            raise ValueError('unknown off-screen pointer policy')
        self.width, self.height, self.distance = width, height, distance
        self.offscreen = offscreen
        self.center = None
        self.velocity = [deque(), deque()]
        self.pads = [Pad(), Pad()]
        self.tracked = [False, False]
        self.on_plane = [False, False]
        self.peak_g = [0.0, 0.0]
        self.speed_peak = [0.0, 0.0]
        self.dynamic_g = [0.0, 0.0]

    def take_peaks(self):
        peaks = tuple(zip(self.peak_g, self.speed_peak))
        self.peak_g = [0.0, 0.0]
        self.speed_peak = [0.0, 0.0]
        return peaks

    def recenter(self, head):
        if len(head) != 12 or not all(math.isfinite(v) for v in head):
            return False
        # Freeze a level plane once; it must never follow the head between calibrations.
        forward = (-head[2], 0, -head[10])
        length = math.sqrt(dot(forward, forward))
        if length < 0.01:
            return False
        self.forward = tuple(v/length for v in forward)
        self.right = (-self.forward[2], 0, self.forward[0])
        self.up = (0, 1, 0)
        self.center = tuple(head[i*4+3]+self.distance*self.forward[i] for i in range(3))
        return True

    def theatre_calibration(self, record):
        if (not isinstance(record, dict) or record.get('schema') != 1 or record.get('head_valid') is not True or
            record.get('width') != self.width or record.get('height') != self.height or
            record.get('distance') != self.distance):
            return False
        return self.recenter(record.get('head', []))

    def update(self, frame):
        t = frame['time']
        # Reader supplies left then right; Dolphin slot0 is right, slot1 left.
        for slot, hand in enumerate(reversed(frame['hands'])):
            if not hand['valid']:
                self.pads[slot] = Pad()
                self.velocity[slot].clear()
                self.tracked[slot] = False
                self.on_plane[slot] = False
                self.dynamic_g[slot] = 0.0
                continue
            m, velocity = hand['pose'], hand['velocity']
            values = [*m, *velocity, *hand['omega'], *hand['stick'], t]
            if not all(math.isfinite(v) for v in values):
                self.pads[slot] = Pad()
                self.velocity[slot].clear()
                self.tracked[slot] = False
                self.on_plane[slot] = False
                self.dynamic_g[slot] = 0.0
                continue
            self.tracked[slot] = True
            acceleration = [0.0, 0.0, 0.0]
            history = self.velocity[slot]
            if history and (t <= history[-1][0] or t-history[-1][0] > 0.05):
                history.clear()
            # Pose reader supplies ~120 Hz. Compare across a short window instead
            # of differencing the last UI tick, which can skip an entire shake.
            while history and t-history[0][0] > 0.025:
                history.popleft()
            if history and t-history[0][0] >= 0.001:
                acceleration = [(v-old)/(t-history[0][0]) for v,old in zip(velocity, history[0][1])]
            history.append((t, tuple(velocity)))
            self.dynamic_g[slot] = math.sqrt(dot(acceleration, acceleration))/9.80665
            self.peak_g[slot] = max(self.peak_g[slot], self.dynamic_g[slot])
            self.speed_peak[slot] = max(self.speed_peak[slot], math.sqrt(dot(velocity, velocity)))
            # OpenVR uses local right/up/back. Dolphin DSU's accelerometer
            # controls are left/down/forward; gyro fields are pitch up,
            # yaw right and roll right (see DualShockUDPClient.cpp inputs).
            acceleration[1] += 9.80665
            specific = local(m, acceleration)
            accel = tuple(max(-16., min(16., -v/9.80665)) for v in specific)
            omega = local(m, hand['omega'])
            gyro = (math.degrees(omega[0]), -math.degrees(omega[1]), -math.degrees(omega[2]))
            pointer = self.pads[slot].pointer
            on_plane = False
            if self.center is not None:
                origin = (m[3], m[7], m[11])
                ray = (-m[2], -m[6], -m[10])
                denominator = dot(ray, self.forward)
                if denominator > 0.001:
                    distance = dot(tuple(c-o for c,o in zip(self.center,origin)),self.forward)/denominator
                    if distance > 0:
                        hit = tuple(o+distance*d-c for o,d,c in zip(origin,ray,self.center))
                        raw = (dot(hit,self.right)*2/self.width,dot(hit,self.up)*2/self.height)
                        on_plane = abs(raw[0]) <= 1 and abs(raw[1]) <= 1
                        pointer = (max(-1., min(1., raw[0])), max(-1., min(1., raw[1])))
            if not on_plane and self.center is not None:
                # A parallel or backward ray has no plane intersection. Move to
                # an edge using its direction, preserving the previous axis.
                if abs(pointer[0]) < 1 and abs(pointer[1]) < 1:
                    pointer = (1. if dot(ray, self.right) >= 0 else -1., pointer[1])
            self.on_plane[slot] = on_plane
            trigger, grip, primary, secondary, menu, click = hand['buttons']
            # Square is reserved for Dolphin Point/Hide when the game profile
            # opts into the hide policy. Trigger is always passed for reloads.
            buttons = ((int(trigger and self.center is not None)<<9) | (int(grip)<<8) |
                       (int(self.offscreen == 'hide' and not on_plane)<<15) |
                       (int(primary)<<13) | (int(secondary)<<14) | (int(menu)<<3))
            self.pads[slot] = Pad(True,pointer,tuple(hand['stick']),buttons,accel,gyro,int(t*1_000_000))
        return self.pads
