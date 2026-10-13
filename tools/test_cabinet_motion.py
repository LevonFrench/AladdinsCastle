# SPDX-License-Identifier: GPL-3.0-only
import math
import unittest
from cabinet_motion import wheel_angle,steering_count,h_gate,pedal_count

class MotionTests(unittest.TestCase):
    def test_clockwise_and_unwrap(self):
        self.assertEqual(wheel_angle((-.1,0,0),(.1,0,0)),0)
        self.assertEqual(wheel_angle((0,.1,0),(0,-.1,0)),90)
        a=math.radians(-179)
        self.assertAlmostEqual(wheel_angle((0,0,0),(math.cos(a),-math.sin(a),0),179),181)
    def test_bad_hand_span_keeps_angle(self):
        self.assertEqual(wheel_angle((0,0,0),(.001,0,0),27),27)
    def test_asymmetric_counts(self):
        self.assertEqual([steering_count(a) for a in (-200,-135,0,135,200)],[0,0,128,255,255])
        with self.assertRaises(ValueError):steering_count(1,0)
    def test_gate_neutral_and_all_gears(self):
        self.assertEqual(h_gate(0,0),0)
        self.assertEqual([h_gate(x,z) for x,z in [(-.037,-.054),(-.037,.054),(.037,-.054),(.037,.054)]],[1,2,3,4])
    def test_gate_cannot_cross_plate(self):
        self.assertEqual(h_gate(.037,-.054,1),1)
        self.assertEqual(h_gate(.037,0,1),0)
        self.assertEqual(h_gate(.037,-.054,0),3)
    def test_pedal_deadzone_and_endpoints(self):
        self.assertEqual([pedal_count(v) for v in (-1,0,.04,1,2)],[0,0,0,255,255])
        self.assertLess(pedal_count(.5),128)

if __name__=='__main__':unittest.main()
