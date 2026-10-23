# SPDX-License-Identifier: GPL-3.0-only
"""R12 source-plan adapter; geometry lives in src-redesign/specials.py.

This file keeps source tools and hand assembly on the common semantic-plan
interface. It is not a Blender build script; build.py intentionally excludes
the redesigned special models.
"""
from common import Design

PARAMETERS = dict(length_mm=285)


def _xyz(forward_height_lateral):
    forward, height, lateral = forward_height_lateral
    return lateral, height, -forward


def make_design():
    d = Design(PARAMETERS['length_mm'])
    muzzle = (246.862993, 57, 0)
    front = (208.71, 103, 0)
    rear = (46, 103, 0)
    for name, at in (('muzzle', muzzle), ('fx_muzzle', muzzle),
                     ('fx_laser', muzzle), ('sight_front', front),
                     ('sight_rear', rear), ('pivot_trigger', (30, 9, 0)),
                     ('button_reload', (11, -100, -22)),
                     ('slide_recoil', (140.034303, 57, 0)),
                     ('limit_recoil', (125.034302, 57, 0)),
                     ('jaw_left', (164.197877, 57, -23)),
                     ('jaw_right', (164.197877, 57, 23)),
                     ('jaw_upper', (164.197877, 79.999998, 0))):
        d.node(name, _xyz(at))
    d.sight_setup = dict(kind='notch-blade', notch_width_m=.006,
                         notch_depth_m=.003, front_width_m=.003)
    return d
