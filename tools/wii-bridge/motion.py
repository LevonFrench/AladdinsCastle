# SPDX-License-Identifier: GPL-3.0-only
"""World-fixed fallback pointer plane and actual pose-derived inertial samples."""
import math
from dsu import Pad


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def local(matrix, world):
    return tuple(sum(matrix[r*4+c]*world[r] for r in range(3)) for c in range(3))


class Mapper:
    def __init__(self, width=3.2, height=1.8, distance=2.0):
        self.width, self.height, self.distance = width, height, distance
        self.center = None
        self.previous = [None, None]
        self.pads = [Pad(), Pad()]

    def recenter(self, head):
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

    def update(self, frame):
        t = frame['time']
        # Reader supplies left then right; Dolphin slot0 is right, slot1 left.
        for slot, hand in enumerate(reversed(frame['hands'])):
            if not hand['valid']:
                self.pads[slot] = Pad()
                self.previous[slot] = None
                continue
            m, velocity = hand['pose'], hand['velocity']
            values = [*m, *velocity, *hand['omega'], *hand['stick'], t]
            if not all(math.isfinite(v) for v in values):
                self.pads[slot] = Pad()
                self.previous[slot] = None
                continue
            acceleration = [0.0, 0.0, 0.0]
            previous = self.previous[slot]
            if previous and 0.001 <= t-previous[0] <= 0.05:
                acceleration = [(v-old)/(t-previous[0]) for v,old in zip(velocity, previous[1])]
            self.previous[slot] = (t, tuple(velocity))
            # Specific force a-g in the aim/body frame, then DSU's signed axes.
            acceleration[1] += 9.80665
            specific = local(m, acceleration)
            accel = tuple(max(-16., min(16., -v/9.80665)) for v in specific)
            omega = local(m, hand['omega'])
            gyro = (math.degrees(omega[0]), -math.degrees(omega[1]), -math.degrees(omega[2]))
            pointer = (0., 0.)
            on_plane = False
            if self.center is not None:
                origin = (m[3], m[7], m[11])
                ray = (-m[2], -m[6], -m[10])
                denominator = dot(ray, self.forward)
                if denominator > 0.001:
                    distance = dot(tuple(c-o for c,o in zip(self.center,origin)),self.forward)/denominator
                    if distance > 0:
                        hit = tuple(o+distance*d-c for o,d,c in zip(origin,ray,self.center))
                        pointer = (dot(hit,self.right)*2/self.width,dot(hit,self.up)*2/self.height)
                        on_plane = abs(pointer[0]) <= 1 and abs(pointer[1]) <= 1
            trigger, grip, primary, secondary, menu, click = hand['buttons']
            buttons = ((int(trigger and on_plane)<<9) | (int(grip)<<8) |
                       (int(primary)<<13) | (int(secondary)<<14) | (int(menu)<<3))
            self.pads[slot] = Pad(True,pointer,tuple(hand['stick']),buttons,accel,gyro,int(t*1_000_000))
        return self.pads
