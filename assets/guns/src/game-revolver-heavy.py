# SPDX-License-Identifier: GPL-3.0-only
"""R11 source-plan adapter; geometry lives in src-redesign/specials.py.

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
    muzzle = (233, 37, 0)
    front = (213.191003, 56, 0)
    rear = (-21, 56, 0)
    for name, at in (('muzzle', muzzle), ('fx_muzzle', muzzle),
                     ('fx_laser', muzzle), ('sight_front', front),
                     ('sight_rear', rear), ('pivot_trigger', (13, -8, 0)),
                     ('button_reload', (-25, 30, -14.5)),
                     ('cylinder_spin', (15.5, 21, 0)),
                     ('hammer_cock', (-33, 38, 0))):
        d.node(name, _xyz(at))
    d.sight_setup = dict(kind='notch-blade', notch_width_m=.006,
                         notch_depth_m=.003, front_width_m=.003)
    return d
