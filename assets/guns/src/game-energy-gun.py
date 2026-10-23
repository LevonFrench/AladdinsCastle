# SPDX-License-Identifier: GPL-3.0-only
"""R10 source-plan adapter; geometry lives in src-redesign/specials.py.

This file keeps source tools and hand assembly on the common semantic-plan
interface. It is not a Blender build script; build.py intentionally excludes
the redesigned special models.
"""
from common import Design

PARAMETERS = dict(length_mm=295)


def _xyz(forward_height_lateral):
    forward, height, lateral = forward_height_lateral
    return lateral, height, -forward


def make_design():
    d = Design(PARAMETERS['length_mm'])
    muzzle = (250, 31, 0)
    front = (205.87, 76, 0)
    rear = (-13, 76, 0)
    for name, at in (('muzzle', muzzle), ('fx_muzzle', muzzle),
                     ('fx_laser', muzzle), ('sight_front', front),
                     ('sight_rear', rear), ('pivot_trigger', (20, 2, 0)),
                     ('button_reload', (-10, 7, -18)),
                     ('slide_recoil', (161.739126, 31, 0)),
                     ('limit_recoil', (146.739125, 31, 0)),
                     ('energy_core_cycle', (183.043465, 31, 0))):
        parent = 'slide_recoil' if name == 'energy_core_cycle' else None
        d.node(name, _xyz(at), parent)
    d.sight_setup = dict(kind='notch-blade', notch_width_m=.006,
                         notch_depth_m=.003, front_width_m=.003)
    return d
