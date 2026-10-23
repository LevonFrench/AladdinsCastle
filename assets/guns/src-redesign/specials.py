# SPDX-License-Identifier: GPL-3.0-only
"""Original 2026-10-21 special weapon/tool silhouettes.

All shapes are authored here from dimensions. No old model, outline, or builder
is imported. FHL dimensions are millimetres; custom mesh coordinates are metres.
"""
import math
import bpy
from fresh import *


def _loft(name, sections, mat='body', parent=None, segments=16, exponent=3.0,
          bevel=.45):
    """Closed changing rounded cross section along forward axis.

    section = forward, height centre, height radius, lateral radius.
    This is new ring topology, not an extrusion of a flat receiver outline.
    """
    verts = []
    for forward, height, rh, rw in sections:
        for i in range(segments):
            a = 2 * math.pi * i / segments
            c, s = math.cos(a), math.sin(a)
            lateral = rw * math.copysign(abs(c) ** (2 / exponent), c)
            z = height + rh * math.copysign(abs(s) ** (2 / exponent), s)
            verts.append((lateral / 1000, forward / 1000, z / 1000))
    faces = [tuple(range(segments))]
    for ring_idx in range(len(sections) - 1):
        for j in range(segments):
            k = (j + 1) % segments
            a, b = ring_idx * segments + j, ring_idx * segments + k
            faces.append((a, a + segments, b + segments, b))
    faces.append(tuple((len(sections) - 1) * segments + j
                       for j in reversed(range(segments))))
    mesh = bpy.data.meshes.new(name + '_new_mesh')
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    for polygon in mesh.polygons:
        polygon.use_smooth = len(polygon.vertices) == 4
    return finish(obj, mat, bevel, parent)


def _ellipse(cx, cy, rx, ry, n=16):
    return [(cx + rx * math.cos(2 * math.pi * i / n),
             cy + ry * math.sin(2 * math.pi * i / n)) for i in range(n)]


def _rounded(points, steps=3):
    """Periodic Catmull-Rom silhouette; avoids bevelled straight octagons."""
    result = []
    for i in range(len(points)):
        p0, p1 = points[(i - 1) % len(points)], points[i]
        p2, p3 = points[(i + 1) % len(points)], points[(i + 2) % len(points)]
        for j in range(steps):
            t = j / steps
            result.append(tuple(.5 * (2 * p1[k] + (-p0[k] + p2[k]) * t +
                   (2*p0[k] - 5*p1[k] + 4*p2[k] - p3[k]) * t*t +
                   (-p0[k] + 3*p1[k] - 3*p2[k] + p3[k]) * t*t*t)
                   for k in range(2)))
    return result


def _skin(name, sections, stations, angles, side=1, exponent=3.0,
          offset=.9, mat='accent', parent=None):
    """A closed, thin plate on the same radial loft as its supporting shell.

    Angular grid aligns to the shell's ring vertices. This prevents the
    clipped planar-plate artifacts seen on the first renders.
    """
    verts = []
    for lift in (-.25, offset):
        for f in stations:
            a, b = sections[0], sections[-1]
            for first, second in zip(sections[:-1], sections[1:]):
                if first[0] <= f <= second[0]:
                    a, b = first, second
                    break
            u = (f - a[0]) / (b[0] - a[0])
            h, rh, rw = [a[k] + (b[k] - a[k]) * u for k in (1, 2, 3)]
            for degrees in angles:
                theta = math.radians(degrees)
                c, s = math.cos(theta), math.sin(theta)
                l = rw * math.copysign(abs(c) ** (2 / exponent), c) + c * lift
                z = h + rh * math.copysign(abs(s) ** (2 / exponent), s) + s * lift
                verts.append((side*l/1000, f/1000, z/1000))
    nf, na = len(stations), len(angles)
    count = nf * na
    faces = []
    for i in range(nf - 1):
        for j in range(na - 1):
            a = i * na + j
            faces.append((a, a + 1, a + na + 1, a + na))
            faces.append((count+a, count+a+na, count+a+na+1, count+a+1))
    boundary = (list(range(na)) + [i*na + na-1 for i in range(1, nf)] +
                [(nf-1)*na+j for j in reversed(range(na-1))] +
                [i*na for i in reversed(range(1, nf-1))])
    for a, b in zip(boundary, boundary[1:] + boundary[:1]):
        faces.append((a, count+a, count+b, b))
    mesh = bpy.data.meshes.new(name + '_curved_plate_mesh')
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return finish(obj, mat, .1, parent)


def _palm_volume(name, sections, mat='body', detail=0):
    """Broad oval hand volume following a curved spine in the vertical axis."""
    n = 12 if detail else 24
    verts = []
    for height, forward, rf, rl in sections:
        for j in range(n):
            a = math.tau*j/n
            verts.append((rl*math.cos(a)/1000,
                          (forward+rf*math.sin(a))/1000, height/1000))
    faces = [tuple(reversed(range(n)))]
    for i in range(len(sections)-1):
        for j in range(n):
            a,b=i*n+j,i*n+(j+1)%n
            faces.append((a,b,b+n,a+n))
    faces.append(tuple((len(sections)-1)*n+j for j in range(n)))
    mesh=bpy.data.meshes.new(name+'_palm_mesh')
    mesh.from_pydata(verts,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj)
    return finish(obj,mat,.4)


def _trigger(pivot, points, width=5, parent=None):
    p = node('pivot_trigger', pivot, parent=parent)
    profile('curved_trigger', points, width, 'accent', .65, parent=p)
    return p


def _fluted_drum(parent, detail):
    """Actual shallow six-fold concavity along the cylinder, with solid rims."""
    n = 48 if not detail else 24
    sections = [(-10, 23.0, 0), (-7, 24.5, 0), (-2, 24.5, 2.4),
                (32, 24.5, 2.4), (38, 24.5, 0), (41, 23.3, 0)]
    verts = []
    for f, radius, flute in sections:
        for j in range(n):
            a = 2 * math.pi * j / n
            r = radius - flute * max(0, math.cos(6 * a)) ** 3
            verts.append((r * math.cos(a) / 1000, f / 1000,
                          (21 + r * math.sin(a)) / 1000))
    faces = [tuple(range(n))]
    for i in range(len(sections) - 1):
        for j in range(n):
            a, b = i * n + j, i * n + (j + 1) % n
            faces.append((a, a + n, b + n, b))
    faces.append(tuple((len(sections) - 1) * n + j for j in reversed(range(n))))
    mesh = bpy.data.meshes.new('fluted_cylinder_new_mesh')
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new('six_flute_cylinder', mesh)
    bpy.context.collection.objects.link(obj)
    for face in mesh.polygons:
        face.use_smooth = len(face.vertices) == 4
    finish(obj, 'accent', .2, parent)


def _heavy_revolver(detail):
    n = 12 if detail else 24
    # Solid metal frame has a true cylinder window, a raised strap and a
    # restrained rear shoulder. The cylinder is not the receiver silhouette.
    profile('curved_forged_rear_frame', _rounded([(-27, 0), (-34, 15),
            (-33, 34), (-26, 45), (-17, 48), (-13, 43), (-20, 32),
            (-21, 15), (-14, 4), (-8, -5), (-16, -14), (-25, -9)],
            2 if detail else 3), 25, 'body', 1.7)
    cyl('thin_round_recoil_shield', (-13.5, 21, 0), 25, 6,
        'body', 'f', n, 1.1)
    profile('upper_cylinder_strap', [(-30, 49), (51, 49), (61, 44),
            (59, 39), (45, 40), (-28, 42)], 24, 'body', 1.2)
    profile('cylinder_front_yoke', [(40, 5), (49, 4), (59, 10), (65, 35),
            (56, 43), (45, 39), (49, 20), (43, 11)], 28, 'body', 1.4)
    profile('lower_frame_spine', [(-16, 2), (43, 2), (52, 8), (51, -2),
            (36, -8), (7, -8), (-13, -6)], 23, 'body', 1.2)
    cylinder = node('cylinder_spin', (15.5, 21, 0))
    _fluted_drum(cylinder, detail)
    cyl('cylinder_front_axis', (44, 21, 0), 5, 7, 'dark', 'f', n, .4)
    # Tapered barrel, separate bore, and sculpted underlug. Barrel sits at the
    # chamber axis, above the central cylinder spindle.
    _loft('tapered_barrel_jacket', [(55, 37, 13.5, 14), (68, 37, 13, 14),
          (173, 37, 9, 10.5), (187, 37, 9, 10.5)],
          segments=12 if detail else 20, exponent=3.4)
    tube('recessed_muzzle_bore', (188, 37, 0), 7.7, 5.4, 6,
         'dark', 'f', n, .35)
    cyl('deep_bore_shadow', (187.3, 37, 0), 5.35, .6,
        'dark', 'f', n, .1)
    profile('tapered_ejector_underlug', [(58, 27), (166, 27), (179, 30),
            (175, 22), (70, 17), (55, 20)], 20, 'accent', 1.0)
    cyl('ejector_rod', (99, 21, 0), 2.8, 88, 'body', 'f', 12, .2)
    # R11 adopts the v2 R07 fix: a fixed centre-line web fills gaps around the spinning
    # cylinder, so the frame reads as solid without changing the drum motion.
    profile('cylinder_window_web', [(-23, 5), (48, 5), (59, 13),
            (56, 39), (46, 44), (-25, 43)], 12, 'body', 1.0)
    profile('ejector_underlug_web', [(61, 18), (168, 18), (181, 28),
            (166, 33), (61, 29)], 10, 'body', 1.0, lateral=-4)
    # A revolver's broad curved palm volume, not an extruded pistol-grip panel.
    _palm_volume('rounded_wraparound_revolver_grip',
                 [(-99,-31,13,13),(-95,-31,18,17),(-83,-31,21,19),
                  (-62,-27,20,19),(-40,-23,17,17),(-23,-20,15,14),
                  (-10,-23,14,12),(-4,-27,10,10)], 'dark', detail)
    _palm_volume('grip_butt_cap', [(-101,-31,12,12),(-99,-31,15,15),
                 (-96,-31,18,17)], 'body', detail)
    reload_button = node('button_reload', (-25, 30, -14.5))
    cyl('cylinder_release_seat', (-25, 30, -13.0), 5.1, 1.8,
        'dark', 'l', n, .5)
    cyl('cylinder_release_button', (-25, 30, -14.5), 4.1, 2.6,
        'accent', 'l', n, .7, parent=reload_button)
    # v2 detail budget: remove sub-2 mm fasteners and spend those triangles on
    # the broad silhouette features above and the smoother trigger guard below.
    ring('oval_trigger_guard', _ellipse(18, -27, 25, 21, 20 if detail else 40),
         _ellipse(18, -27, 19.2, 15, 20 if detail else 40), 11,
         'body', .9)
    _trigger((13, -8, 0), [(12, -7), (18, -9), (17, -17), (12, -24),
             (15, -30), (12, -33), (7, -27), (8, -19)])
    hammer = node('hammer_cock', (-33, 38, 0))
    profile('exposed_hammer', [(-34, 34), (-29, 41), (-34, 53),
            (-43, 57), (-48, 54), (-44, 49)], 10, 'accent', .8,
            parent=hammer)
    # Shared wrapper geometry supplies the tier-1 notch and blade at the level
    # anchors returned below, replacing these model-specific sight fragments.
    if not detail:
        for f in (-39, -36, -33):
            box('hammer_thumb_serration_' + str(f), (f, 54.7, 0),
                (1.2, .65, 8), 'dark', .15, parent=hammer)
    return {'muzzle': (191, 37, 0), 'sight_front': (176, 56, 0),
            'sight_rear': (-21, 56, 0), 'functional_sights': True,
            'pivot_trigger': (13, -8, 0),
            'length_mm': 240}


def _energy_sidearm(detail):
    n = 12 if detail else 24
    # Sweeping, changing-section shell surrounding a visible round induction
    # core. The rear narrows into a curved hand web instead of a block receiver.
    shell = [(-45, 31, 9, 11), (-31, 39, 20, 19),
          (4, 44, 24, 24), (61, 44, 21, 23), (106, 37, 16, 19),
          (144, 30, 12, 15)]
    _loft('flowing_energy_carapace', shell, segments=12 if detail else 24, exponent=2.6,
          bevel=.4)
    _loft('underslung_power_spine', [(-21, 15, 8, 13), (35, 18, 8, 15),
          (93, 22, 7, 14), (145, 28, 9, 14)], mat='accent',
          segments=8 if detail else 12, exponent=3.2)
    recoil = node('slide_recoil', (120, 31, 0))
    core = node('energy_core_cycle', (134, 31, 0), parent=recoil)
    cyl('induction_core', (141, 31, 0), 13.5, 58, 'accent', 'f', n, .7,
        parent=core)
    for f in (122, 150):
        tube('core_retaining_band_' + str(f), (f, 31, 0), 16, 13.0,
             6, 'body', 'f', n, .45, parent=core)
    tube('lens_hood', (171, 31, 0), 15, 11.5, 13,
         'body', 'f', n, .7, parent=recoil)
    cyl('deep_optical_lens', (174.8, 31, 0), 10.5, 1.8, 'glass', 'f', n, .2,
        parent=recoil)
    tube('lens_retainer', (177, 31, 0), 12, 10.4, 2.0,
         'dark', 'f', n, .2, parent=recoil)
    # A genuine thumb-web curve and palm swell, with no lower forward prong.
    profile('energy_grip_web', _rounded([(-25, 21), (7, 18), (15, 9), (6, -3),
            (7, -21), (8, -44), (7, -74), (3, -88), (-10, -96),
            (-29, -92), (-35, -81), (-32, -61), (-23, -30),
            (-20, -11), (-33, 8)], 2 if detail else 4), 32, 'body', 4.0)
    for side in (-1, 1):
        profile('swept_palm_insert_' + str(side), _rounded([(-19, -10), (-4, -6),
                (-11, -25), (-10, -49), (0, -76), (-5, -84),
                (-21, -84), (-25, -76), (-21, -51), (-15, -29)],
                2 if detail else 4),
                3.4, 'dark', 2.7, lateral=side * 16.2)
        _skin('curved_carapace_service_strip_' + str(side), shell,
              [4, 35, 61, 85], [-30,0,30] if detail else [-18,0,18],
              side=side, exponent=2.6, offset=.8)
    # v2 detail budget: keep the palm panels broad and clear, remove their tiny
    # screws, then spend the recovered triangles on the shell and grip outline.
    ring('energy_guard', _ellipse(27,-14,29,22,20 if detail else 40),
         _ellipse(27,-14,22.5,15.5,20 if detail else 40), 11, 'accent', 1.3)
    _trigger((20, 2, 0), [(17, 2), (24, 2), (24, -8), (18, -18),
             (18, -22), (13, -22), (11, -17), (17, -6)])
    reload_button = node('button_reload', (-10, 7, -18))
    cyl('energy_cell_release_seat', (-10, 7, -17), 5.3, 1.6,
        'dark', 'l', n, .5)
    cyl('energy_cell_release_button', (-10, 7, -18.5), 4.2, 2.5,
        'accent', 'l', n, .6, parent=reload_button)
    # The build wrapper adds the shared tier-1 notch/blade sight pair at these
    # level anchors; retire the round optic and its stalk to avoid duplicate sights.
    profile('emitter_index_ridge', [(120, 44), (151, 44), (154, 46),
            (131, 48)], 6, 'accent', .7)
    return {'muzzle': (178, 31, 0), 'sight_front': (149, 76, 0),
            'sight_rear': (-13, 76, 0), 'functional_sights': True,
            'pivot_trigger': (20, 2, 0),
            'limit_recoil': (112, 31, 0),
            'length_mm': 223}


def _mounted_heavy(detail):
    n = 12 if detail else 24
    yaw = node('pivot_yaw', (70, -156, 0))
    # This pedestal has a real cross-member and two separated bearing cheeks.
    cyl('pedestal_foot', (70, -181, 0), 65, 22, 'dark', 'h', n, 3)
    cyl('pedestal_rotation_bearing', (70, -165, 0), 47, 14,
        'accent', 'h', n, 1.5)
    box('yoke_crossmember', (70, -141, 0), (57, 26, 161), 'body', 4,
        parent=yaw)
    for side in (-1, 1):
        profile('yoke_cheek_' + str(side), [(43, -145), (97, -145),
                (97, 27), (91, 47), (80, 58), (60, 58),
                (46, 48), (42, 25)], 17, 'body', 3,
                lateral=side * 71, parent=yaw)
        cyl('elevation_bearing_' + str(side), (70, 35, side * 73), 23,
            23, 'accent', 'l', n, 1.1, parent=yaw)
        cyl('bearing_cap_' + str(side), (70, 35, side * 86.3), 14,
            5, 'dark', 'l', n, .8, parent=yaw)
        if not detail:
            for h in (-118, -73):
                profile('yoke_recess_%s_%s' % (side, h),
                        [(56, h), (83, h), (83, h + 29), (56, h + 29)],
                        1, 'accent', 3, lateral=side * 79.6, parent=yaw)
    pitch = node('pivot_pitch', (70, 35, 0), parent=yaw)
    cyl('receiver_elevation_axle', (70, 35, 0), 12, 145,
        'accent', 'l', n, .7, parent=pitch)
    receiver = [(-61, 57, 26, 30), (-44, 59, 35, 41),
          (95, 59, 36, 43), (157, 61, 29, 37), (176, 61, 23, 29)]
    _loft('heavy_receiver', receiver,
          parent=pitch, segments=12 if detail else 20, exponent=4.0, bevel=1.3)
    _skin('conforming_receiver_top_cover', receiver, [-44,20,95,132],
          [60,90,120] if detail else [54,72,90,108,126],
          exponent=4.0, offset=1.3, parent=pitch)
    # Barrel changes mass at the chamber, shroud, stiffening collar and muzzle.
    cyl('chamber_barrel_socket', (180, 61, 0), 28, 37,
        'dark', 'f', n, 1.2, parent=pitch)
    tube('heavy_barrel', (306, 61, 0), 14, 8, 228,
         'accent', 'f', n, .7, parent=pitch)
    tube('ventilated_shroud_rear', (216, 61, 0), 26, 21, 24,
         'body', 'f', n, .8, parent=pitch)
    tube('ventilated_shroud_front', (334, 61, 0), 24, 20, 18,
         'body', 'f', n, .8, parent=pitch)
    # Six longitudinal rails leave actual open cooling gaps around the barrel.
    for k in range(6):
        a = math.pi / 6 + k * math.pi / 3
        h, lat = 61 + 23 * math.sin(a), 23 * math.cos(a)
        rod('barrel_shroud_rail_' + str(k), (225, h, lat),
            (329, h, lat), 4.2, 'body', 6 if detail else 10, parent=pitch)
    # v2 (R09, summary item 10): a dark core inside the open rail cage so the
    # background no longer shows through the shroud; the rails stay as drawn.
    cyl('shroud_dark_core', (277, 61, 0), 17.5, 104, 'dark', 'f', n, .5,
        parent=pitch)
    tube('forward_recoil_collar', (368, 61, 0), 19, 13, 17,
         'dark', 'f', n, .8, parent=pitch)
    tube('muzzle_brake', (420, 61, 0), 23, 8, 33,
         'body', 'f', n, 1.2, parent=pitch)
    if not detail:
        for f in (411, 422):
            for side in (-1, 1):
                box('brake_side_port_%s_%s' % (side, f),
                    (f, 61, side * 22.9), (5, 13, .7), 'dark', .7,
                    parent=pitch)
    # Heavy rear spade grips attach through a proper bridge in two planes.
    box('rear_handle_bridge', (-74, 60, 0), (18, 22, 139), 'accent', 3,
        parent=pitch)
    for side in (-1, 1):
        grab = node('grab_left' if side < 0 else 'grab_right',
                    (-86, 59, side * 67), parent=pitch)
        rod('upper_handle_stay_' + str(side), (-53, 82, side * 33),
            (-86, 86, side * 67), 6, 'body', 10, parent=pitch)
        rod('lower_handle_stay_' + str(side), (-53, 34, side * 33),
            (-86, 29, side * 67), 6, 'body', 10, parent=pitch)
        rod('spade_grip_' + str(side), (-86, 29, side * 67),
            (-86, 86, side * 67), 10, 'dark', 10 if detail else 16,
            parent=grab)
        thumb_parent = (node('button_special', (-86, 89, side * 67),
                             parent=grab) if side > 0 else grab)
        cyl('thumb_fire_button_' + str(side), (-86, 89, side * 67),
            6, 6, 'accent', 'h', 12, .7, parent=thumb_parent)
        if side > 0:
            cyl('special_button_status_lens', (-86, 92.2, side * 67),
                3.4, .8, 'glass', 'h', 12, .15, parent=thumb_parent)
        if not detail:
            # v2 fix 6: receiver fasteners removed.
            _skin('conforming_receiver_side_cover_' + str(side), receiver,
                  [-29,16,48], [-18,0,18], side=side, exponent=4,
                  offset=.9, parent=pitch)
    box('rear_sight_bridge', (9, 100, 0), (23, 9, 25), 'dark', 1,
        parent=pitch)
    profile('front_sight_base', [(325, 83), (347, 83), (342, 105),
            (333, 105)], 9, 'accent', .7, parent=pitch)
    # v2 fix 3/4: tier-1 notch-blade sights at the round-1 heights (front blade
    # top 114 mm); the rear aperture ring becomes a 20 mm notch block on the
    # existing bridge.
    sights = functional_sights((338, 105), (9, 104.5), 114, pitch, pitch,
                               rear_width=20)
    # v2: the trigger hinge sits on its blade at the centre bridge (round 1
    # placed the node 67 mm to the left of its geometry).
    _trigger((-80, 61, 0), [(-79, 65), (-73, 65), (-71, 52),
             (-76, 47), (-80, 49)], 5, parent=pitch)
    # v2 fix 8 (R09): assets/guns/README.md and assets/hands/assembly.json put
    # the right hand on `grip` and the left on `grip_two`, each at the web of a
    # rear handle. The model is re-originated on the right spade grip's hand
    # web (8 mm under the grip top, just below the upper stay) and grip_two is the
    # same point on the left grip. v2 fix 1: scaled 1.2 to the 650 mm toml.
    return {'_deform': {'origin': (-86, 78, 67), 'scale': 1.2},
            'muzzle': (437, 61, 0), **sights,
            'pivot_trigger': (-80, 61, 0),
            'grip_two': (-86, 78, -67), 'length_mm': 533}


def _industrial_cutter(detail):
    n = 12 if detail else 24
    # A rising motor shell and triangular collet-like head. The three jaws
    # surround an open throat; none is the rejected horizontal fork shape.
    motor = [(-40, 29, 13, 17), (-27, 35, 26, 28),
          (34, 43, 31, 31), (89, 53, 27, 27), (120, 57, 19, 22)]
    _loft('rising_tool_motor_housing', motor,
          segments=12 if detail else 20, exponent=3.0, bevel=.8)
    _loft('motor_lower_cradle', [(-27, 12, 8, 20), (40, 16, 9, 25),
          (98, 38, 10, 21), (124, 51, 8, 20)], mat='accent',
          segments=8 if detail else 12, exponent=3.5, bevel=.7)
    recoil = node('slide_recoil', (124, 57, 0))
    cyl('head_motor_collar', (124, 57, 0), 26, 19,
        'dark', 'f', n, .8, parent=recoil)
    tube('tool_head_lock_ring', (138, 57, 0), 29, 19, 14,
         'accent', 'f', n, 1, parent=recoil)
    tube('open_emitter_throat', (148, 57, 0), 21, 16, 16,
         'body', 'f', n, .6, parent=recoil)
    cyl('recessed_power_lens', (145, 57, 0), 14.5, 2,
        'glass', 'f', n, .2, parent=recoil)
    # Two lateral jaw cartridges with swept/chamfered noses. Their three-
    # dimensional separation leaves a central aperture and real silhouette gaps.
    for side in (-1, 1):
        jaw = node('jaw_' + ('left' if side < 0 else 'right'),
                   (143, 57, side * 23), parent=recoil)
        profile('jaw_cartridge_' + str(side), [(133, 40), (147, 34),
                (182, 39), (205, 52), (207, 60), (195, 73),
                (171, 79), (145, 75), (135, 67)], 20,
                'body', 2.0, lateral=side * 24, parent=jaw)
        profile('jaw_thermal_insert_' + str(side), [(155, 43), (181, 48),
                (195, 55), (196, 60), (183, 66), (156, 68)],
                2.2, 'dark', .8, lateral=side * 34.3, parent=jaw)
        cyl('jaw_front_roller_' + str(side), (195, 57, side * 24),
            5.8, 17, 'accent', 'l', 12 if detail else 20, .5, parent=jaw)
        # The blue working faces are inset on the interior of the jaws.
        profile('jaw_ceramic_contact_' + str(side), [(166, 50), (195, 54),
                (197, 59), (186, 64), (166, 65)], 1.7,
                'glass', .4, lateral=side * 13.4, parent=jaw)
    top_jaw = node('jaw_upper', (143, 80, 0), parent=recoil)
    profile('upper_thermal_cartridge', [(133, 76), (173, 83), (192, 77),
            (200, 82), (191, 93), (168, 97), (143, 91)],
            15, 'accent', 1.6, parent=top_jaw)
    profile('upper_ceramic_contact', [(160, 81), (185, 79), (193, 81),
            (187, 85), (162, 86)], 11, 'glass', .5, parent=top_jaw)
    # Tapered power-tool grip curves into a removable battery shoe.
    profile('tool_grip_chassis', _rounded([(-24, 19), (12, 20), (23, 8), (19, -12),
            (6, -35), (4, -63), (16, -88), (9, -98), (-18, -98),
            (-29, -89), (-31, -75), (-22, -50), (-15, -24),
            (-19, -5), (-32, 7)], 2 if detail else 4), 36, 'body', 3.6)
    for side in (-1, 1):
        profile('tool_rubber_palm_' + str(side), _rounded([(-14, -7), (8, -4),
                (7, -18), (-3, -40), (-6, -65), (4, -84),
                (-15, -86), (-21, -76), (-14, -50), (-7, -24)],
                2 if detail else 4),
                3.2, 'dark', 2.5, lateral=side * 18)
        _skin('curved_motor_service_plate_' + str(side), motor,
              [-10,12,34,65,80], [-30,0,30] if detail else [-36,-18,0,18,36],
              side=side, exponent=3, offset=1.0)
        if not detail:
            for k in range(4):
                _skin('seated_motor_vent_%s_%s' % (side,k), motor,
                      [4+k*9,7+k*9], [-18,0,18], side=side,
                      exponent=3, offset=1.8, mat='dark')
    box('battery_mount', (-3, -101, 0), (49, 8, 40), 'accent', 1.6)
    _loft('removable_battery', [(-39, -111, 11, 21), (-31, -114, 15, 25),
          (24, -114, 15, 25), (31, -110, 11, 20)],
          mat='dark', segments=8 if detail else 12, exponent=4, bevel=1.2)
    reload_button = node('button_reload', (11, -100, -22))
    box('battery_release', (11, -100, -22), (13, 7, 4), 'accent', 1,
        parent=reload_button)
    ring('tool_safety_guard', _ellipse(36,-11,29,26,16 if detail else 32),
         _ellipse(36,-11,22,19,16 if detail else 32),
         13, 'body', 1.2)
    _trigger((30, 9, 0), [(26, 10), (34, 11), (34, 1), (28, -14),
             (26, -22), (20, -22), (20, -15), (26, 0)], width=7)
    profile('top_status_seated_wedge', [(32.5,72.7),(59.5,75.7),
            (59.5,81.5),(32.5,81.5)], 12, 'dark', .7)
    box('status_optical_insert', (46, 82, 0), (18, 1, 6), 'glass', .7)
    return {'muzzle': (208, 57, 0), 'sight_front': (178, 97, 0),
            'sight_rear': (46, 97, 0), 'functional_sights': True,
            'pivot_trigger': (30, 9, 0),
            'limit_recoil': (116, 57, 0),
            'length_mm': 248}


def build(model_id, detail=0):
    builders = {'mnt-mg-heavy': _mounted_heavy,
                'game-energy-gun': _energy_sidearm,
                'game-revolver-heavy': _heavy_revolver,
                'game-tool-cutter': _industrial_cutter}
    if model_id not in builders:
        raise ValueError('Unsupported special model: ' + model_id)
    return builders[model_id](detail)
