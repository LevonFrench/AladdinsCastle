"""Four original arcade/console light peripherals, authored in millimetres.

All surfaces are new geometry. Shared helpers are construction tools, never
saved meshes or silhouettes from the rejected series. Runtime/export belongs
to the coordinator; this module only constructs the requested current LOD.
"""
from fresh import *
import bpy
import math


def _mesh(name, vertices_fhl, faces, mat, bevel=0.0, parent=None):
    mesh = bpy.data.meshes.new(name + "_mesh")
    # fresh primitives take mm; raw Blender data is metres in L/F/H order.
    mesh.from_pydata([(p[2] / 1000, p[0] / 1000, p[1] / 1000)
                      for p in vertices_fhl], [], faces)
    mesh.validate()
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return finish(obj, mat, bevel, parent)


def _loft(name, stations, mat='body', count=16, power=0.6,
          bevel=0.5, parent=None):
    """Longitudinal superellipse sections: (forward,height,halfheight,width)."""
    verts = []
    for f, h, rh, w in stations:
        for i in range(count):
            a = i * math.tau / count
            c, s = math.cos(a), math.sin(a)
            verts.append((f, h + rh * math.copysign(abs(s) ** power, s),
                          w * math.copysign(abs(c) ** power, c)))
    faces = [tuple(range(count))]
    for j in range(len(stations) - 1):
        for i in range(count):
            a = j * count + i
            b = j * count + (i + 1) % count
            faces.append((a, a + count, b + count, b))
    faces.append(tuple(reversed(range((len(stations)-1)*count,
                                     len(stations)*count))))
    return _mesh(name, verts, faces, mat, bevel, parent)


def _grip(name, stations, mat='body', count=16, power=0.7):
    """Ergonomic horizontal sections: (height,forward,halflength,halfwidth)."""
    verts = []
    for h, f, rf, w in stations:
        for i in range(count):
            a = i * math.tau / count
            c, s = math.cos(a), math.sin(a)
            verts.append((f + rf * math.copysign(abs(s)**power, s), h,
                          w * math.copysign(abs(c)**power, c)))
    faces = [tuple(reversed(range(count)))]
    for j in range(len(stations)-1):
        for i in range(count):
            a = j * count + i
            b = j * count + (i+1) % count
            faces.append((a, b, b+count, a+count))
    faces.append(tuple(range((len(stations)-1)*count, len(stations)*count)))
    return _mesh(name, verts, faces, mat, 0.6)


def _optic(name, front, h, radius, detail, parent=None):
    """Annular end faces and a real open bore with a recessed sensor lens."""
    n = 12 if detail else 28
    tube(name + '_outer_bezel', (front-4, h, 0), radius, radius*.72,
         8, 'accent', vertices=n, bevel=.45, parent=parent)
    tube(name + '_optical_well', (front-11, h, 0), radius*.75,
         radius*.63, 18, 'dark', vertices=n, bevel=.2, parent=parent)
    cyl(name + '_recessed_lens', (front-17, h, 0), radius*.62, .8,
        'glass', vertices=n, bevel=.1, parent=parent)


def _panels(name, points, lateral, depth=1.8, mat='accent', bevel=1.2,
            parent=None):
    for side in (-1, 1):
        profile(name + ('_left' if side < 0 else '_right'), points, depth,
                mat, bevel, lateral=side*lateral, parent=parent)


def _curve(points, detail=0):
    """Periodic Catmull-Rom contour for manufactured radiused outlines."""
    subdivisions = 2 if detail else 4
    out = []
    for i, p1 in enumerate(points):
        p0, p2, p3 = points[i-1], points[(i+1)%len(points)], points[(i+2)%len(points)]
        for k in range(subdivisions):
            t = k / subdivisions
            out.append(tuple(.5*((2*p1[a])+(-p0[a]+p2[a])*t+
                          (2*p0[a]-5*p1[a]+4*p2[a]-p3[a])*t*t+
                          (-p0[a]+3*p1[a]-3*p2[a]+p3[a])*t*t*t)
                          for a in range(2)))
    return out


def _grip_surface(stations, h, f, count, power=.7):
    """Exact positive-lateral intersection with the authored loft polygon.

    Using the actual polygon, including LOD1's coarse section, prevents the
    former constant-depth patches from disappearing inside the curved grip.
    """
    low, high = stations[0], stations[-1]
    for a,b in zip(stations,stations[1:]):
        if a[0] <= h <= b[0]:
            low, high = a,b
            break
    t = min(1,max(0,(h-low[0])/(high[0]-low[0])))
    fc,rf,w = [low[k]+(high[k]-low[k])*t for k in (1,2,3)]
    section=[]
    for i in range(count):
        a=i*math.tau/count
        s,c=math.sin(a),math.cos(a)
        section.append((fc+rf*math.copysign(abs(s)**power,s),
                        w*math.copysign(abs(c)**power,c)))
    hits=[]
    for p,q in zip(section,section[1:]+section[:1]):
        if abs(q[0]-p[0])<1e-8:
            continue
        u=(f-p[0])/(q[0]-p[0])
        if -.00001 <= u <= 1.00001:
            hits.append(p[1]+u*(q[1]-p[1]))
    if not hits:
        raise ValueError('Grip panel extends beyond its supporting casting')
    return max(hits)


def _conforming_grip_panel(name, stations, rows, count, detail):
    """Closed panel skins sampled directly over each LOD's actual grip.

    rows are (height, forward centre, halfspan). Both backs penetrate the
    casting; every visible point stays above its surface. This produces one
    continuous assembled panel instead of overlapping convex islands.
    """
    cols=5 if detail else 13
    if detail:  # v2 fix 6: LOD1 samples every other row of the panel
        rows=rows[::2] if (len(rows)-1)%2==0 else rows[::2]+[rows[-1]]
    rcount=len(rows)
    for side in (-1,1):
        verts=[]
        for front in (False,True):
            for ri,(h,fc,span) in enumerate(rows):
                along=math.sin(math.pi*ri/(rcount-1))
                for ci in range(cols):
                    across=-1+2*ci/(cols-1)
                    f=fc+span*across
                    surface=_grip_surface(stations,h,f,count)
                    relief=(1.25+.6*(1-across*across)*along) if front else -1.25
                    verts.append((f,h,side*(surface+relief)))
        plane=rcount*cols
        faces=[]
        for layer in (0,plane):
            for r in range(rcount-1):
                for c in range(cols-1):
                    a=layer+r*cols+c
                    faces.append((a,a+1,a+1+cols,a+cols))
        boundary=list(range(cols))
        boundary += [r*cols+cols-1 for r in range(1,rcount)]
        boundary += list(range((rcount-1)*cols+cols-2,(rcount-1)*cols-1,-1))
        boundary += [r*cols for r in range(rcount-2,0,-1)]
        for a,b in zip(boundary,boundary[1:]+boundary[:1]):
            faces.append((a,b,b+plane,a+plane))
        _mesh(name+('_left' if side<0 else '_right'),verts,faces,'accent',.12)


def _slide(detail):
    # v2 fix 6: freed LOD0 triangles go to rounder slide and grip sections (20 -> 28).
    n = 10 if detail else 28
    # Long upper assembly, sculpted separate heel, narrow under-frame.
    motion = node('slide_recoil', (32, 42, 0))
    _loft('slide_casting', [(-43,42,10,13),(-34,43,15,17),
          (2,43,15.5,18),(119,42,14.5,17),(153,41,12,15.5),
          (158,41,10,13.5)], count=n, power=.53, parent=motion)
    _panels('slide_face', [(-33,47),(-25,52),(120,51),(151,47),
            (155,36),(143,32),(-29,32)], 17.2, 1.4, 'body', .65,
            parent=motion)
    profile('receiver_lower', [(-44,23),(-27,28),(142,26),(154,20),
            (148,13),(64,10),(53,5),(10,6),(-1,-2),(-16,-5),
            (-21,4),(-40,8)], 29, 'body', 2)
    _panels('receiver_split_seam', [(-34,26),(141,26),(147,23),
            (33,22),(-31,23)], 14.6, .7, 'dark', .25)
    grip_stations=[(-112,-43,14,13),(-106,-43,19,15),
          (-89,-39,21,17),(-65,-30,20,18),(-38,-20,18,17.5),
          (-17,-16,17,16.5),(3,-21,21,17),(13,-26,18,16)]
    _grip('ergonomic_frame',grip_stations,count=n)
    profile('swept_beavertail',_curve([(-26,17),(-47,15),(-55,8),
            (-49,4),(-38,5),(-28,-3),(-16,-15),(-5,-13),
            (-8,-1),(-11,12)],detail),32,'body',.7)
    _conforming_grip_panel('slide_grip_panel',grip_stations,
            [(-19,-17,2),(-22,-18,9),(-29,-20,13.5),(-44,-24,14),
             (-62,-30,14.5),(-83,-37,14.8),(-99,-43,12.5),
             (-105,-44,8),(-107,-44,2)],n,detail)
    profile('grip_heel', [(-58,-106),(-23,-106),(-22,-114),
            (-53,-116),(-60,-111)], 31, 'body', 1.5)
    ring('oval_trigger_guard', _curve([(7,5),(40,5),(60,0),(65,-18),
         (57,-33),(29,-37),(9,-32),(3,-13)],detail),
         _curve([(13,-1),(38,-1),(54,-5),(58,-17),(52,-27),
          (29,-30),(14,-26),(10,-12)],detail), 12, 'body', .9)
    trigger = node('pivot_trigger', (24, 2, 0))
    profile('curved_trigger', [(21,3),(29,2),(26,-6),(24,-13),
            (29,-21),(26,-24),(20,-20),(17,-12),(18,-4)],
            7, 'accent', .7, parent=trigger)
    _optic('arcade_muzzle', 176, 41, 10.2, detail)
    _loft('optical_sleeve', [(132,41,11.8,12.8),(160,41,10.6,11)],
          mat='body', count=n, power=1, bevel=.4)
    # Narrow recoil rail ends below the sensor; not a second fake barrel.
    cyl('under_rail', (121,16,0), 4.8, 62, 'accent', vertices=n)
    cyl('under_rail_end', (154,16,0), 5.2, 4, 'dark', vertices=n)
    _panels('recoil_rail_shoulder',_curve([(38,18),(76,18),(90,14),
            (84,10),(48,10),(37,12)],detail),14.8,2.5,'accent',.6)
    _panels('takedown_paddle',_curve([(-3,16),(14,16),(17,12),
            (13,9),(-4,10),(-6,13)],detail),16.8,2.5,'body',.5)
    # v2 fix 3/4: level notch-blade sights seated on the slide roof (rear 58,
    # front 54.4); they ride the slide like the repo's blowback convention.
    sights=functional_sights((143,54.4),(-28,58),61,motion,motion)
    if not detail:
        for side in (-1,1):
            # v2 fix 6: four serrations a side (was seven), no slide fasteners.
            for i in range(4):
                f = -20 + i*8.5
                # Seated 0.6 mm into the slide side (round 1 floated 0.15 mm off it).
                profile('slide_serration', [(f,35),(f+1.5,35),
                        (f+6,50),(f+4.4,50)], 1.2, 'accent', .25,
                        lateral=side*18.0, parent=motion)
            cyl('release_axle', (1,13,side*15.3), 4.4, 1.4,
                'accent', axis='l', vertices=16, bevel=.3)
    return {'muzzle':(176,41,0), **sights, 'pivot_trigger':(24,2,0),
            'limit_recoil':(20,42,0),
            'grip_primary':(-28,-54,0), 'length_mm':236}


def _twin(detail):
    n = 10 if detail else 20
    # A short bulbous rear receiver meets two independently seated optical
    # tubes. The stepped lower shroud and angled handle are unique to this unit.
    kick = node('visual_kick', (0, 24, 0))
    _loft('twin_rear_shell', [(-46,36,10,13),(-38,36,19,21),
          (-12,38,21,23),(34,37,20,22),(77,33,17,20),
          (93,29,14,18)], count=n, power=.78, bevel=.75, parent=kick)
    profile('twin_lower_spine', [(-40,22),(18,20),(61,14),(139,7),
            (151,1),(145,-5),(57,-3),(45,-6),(8,-5),
            (-5,-16),(-22,-11),(-31,5)], 30, 'body', 2.1)
    _panels('twin_side_inlay', [(-34,42),(-21,49),(17,48),(62,37),
            (73,27),(43,22),(5,25),(-27,29)], 22.4, 1.8, 'body', 2,
            parent=kick)
    # Real annular bodies, not solid cylinders with painted black ends.
    for h,r,f in [(38,12.5,163),(12,10.7,158)]:
        tube('paired_shroud', (120,h,0), r, r*.7, 67,
             'body', vertices=n, bevel=.6, parent=kick)
        tube('paired_shroud_band', (100,h,0), r+.8, r-.6, 5,
             'accent', vertices=n, bevel=.35, parent=kick)
        _optic('twin_sensor_' + str(h), f, h, r+.3, detail, parent=kick)
    # Solid bridge joins the tubes behind their collars, leaving their round
    # section legible across the forward half of the design.
    profile('sensor_yoke', _curve([(84,51),(94,49),(102,41),(102,10),
            (95,0),(85,1),(78,18),(79,40)],detail),33,'body',.75,parent=kick)
    grip_stations=[(-108,-53,13,13),(-104,-54,19,16),
          (-87,-47,19,18),(-62,-35,18.5,18.5),(-39,-23,17,18),
          (-17,-11,17,17),(-1,-11,19,17.5),(12,-23,17,17)]
    _grip('twin_palm_casting',grip_stations,count=n)
    _conforming_grip_panel('twin_grip_panel',grip_stations,
            [(-22,-12,2),(-26,-16,9),(-38,-22,12),(-55,-31,12),
             (-74,-42,13),(-90,-49,13),(-100,-53,9),(-103,-54,2)],
            n,detail)
    profile('twin_thumb_shelf',_curve([(-38,14),(-21,15),(-4,3),
            (2,-9),(-7,-18),(-20,-11),(-28,0),(-41,7)],detail),
            36,'body',.8)
    ring('twin_swept_guard',_curve([(7,-1),(37,-2),(57,-11),(53,-29),
         (37,-43),(15,-42),(-3,-32),(-3,-14)],detail),
         _curve([(13,-7),(35,-8),(50,-14),(47,-26),(35,-36),
          (16,-35),(4,-28),(4,-16)],detail), 12, 'body', .9)
    trigger = node('pivot_trigger',(20,-4,0))
    profile('twin_trigger', [(16,-2),(25,-4),(22,-13),(18,-20),
            (23,-27),(20,-30),(14,-25),(12,-18),(13,-9)],
            8, 'accent', .8, parent=trigger)
    # v2 fix 3/4: the rear shell crests at 59 mm, so a level line needs a 61 mm
    # top; the front blade stands on a ramped riser on the upper shroud.
    sights=functional_sights((132,50.5),(-24,57.2),61,kick,kick,front_ramp=10,
                             riser_front=(132,50.5,57),riser_mat='body')
    for side in (-1,1):
        rod('twin_shroud_rib',(103,48.7,side*6),(147,48.7,side*6),
            1.3,'body',vertices=8,parent=kick)
    cyl('twin_cable_socket',(-54,-110,0),7,5,'dark',axis='h',vertices=n)
    if not detail:
        for side in (-1,1):
            # v2 fix 6: fasteners removed. Two recessed-looking channels follow the sweep; quiet midsection.
            for offset in (0,6):
                profile('twin_molded_channel', [(-15,43-offset),(20,42-offset),
                        (49,34-offset),(47,32-offset),(20,40-offset),
                        (-15,41-offset)], .55, 'accent', .3,
                        lateral=side*23.4, parent=kick)
    return {'muzzle':(163,38,0), 'muzzle_secondary':(158,12,0),
            **sights,
            'pivot_trigger':(20,-4,0),'grip_primary':(-36,-57,0),
            'length_mm':237}


def _slim(detail):
    # v2 fix 6: freed LOD0 triangles go to rounder shell and grip sections (24 -> 32).
    n = 10 if detail else 32
    # Long, thin molded console peripheral. Continuous upper curve and an
    # openly rounded guard replace the rejected flat rectangular receiver.
    _loft('slim_upper_shell', [(-53,33,8,10),(-45,35,14,15),
          (-18,36,15,17),(34,35,13.5,16.5),(119,32,11.5,14.5),
          (161,31,10,12.5),(174,31,8.5,10.5)], count=n, power=.78,
          bevel=.7)
    profile('slim_lower_casting',[(-47,23),(-27,24),(135,20),(159,16),
            (153,10),(66,7),(49,1),(13,0),(0,-12),(-12,-11),
            (-28,4),(-40,8)],27,'body',2)
    # Existing upper/lower shell separation is the seam; a planar dark strip
    # cannot stay seated along this compound taper and is intentionally absent.
    grip_stations=[(-111,-70,9,11),(-106,-70,15,14),
          (-90,-64,17,15.5),(-69,-51,18,16.5),(-46,-36,17,17),
          (-24,-22,16,16.5),(-6,-18,16,15.5),(7,-25,18,15)]
    _grip('slim_curved_grip',grip_stations,count=n)
    _conforming_grip_panel('slim_grip_panel',grip_stations,
            [(-28,-24,1.5),(-32,-27,7.5),(-44,-35,11),(-62,-46,12),
             (-80,-58,12),(-97,-67,10),(-103,-70,6),(-105,-70,2)],
            n,detail)
    profile('slim_web_saddle',_curve([(-42,11),(-24,13),(-9,5),
            (0,-11),(-6,-20),(-19,-15),(-31,-2),(-44,3)],detail),
            30,'body',.8)
    ring('slim_oval_guard',_curve([(8,1),(43,0),(62,-7),(68,-19),
         (60,-31),(40,-38),(16,-37),(0,-24)],detail),
         _curve([(14,-5),(42,-6),(56,-12),(61,-19),(55,-26),
          (40,-32),(17,-31),(7,-23)],detail),11,'body',.8)
    trigger=node('pivot_trigger',(22,-1,0))
    profile('slim_trigger',[(18,0),(27,-1),(25,-8),(21,-15),
            (26,-23),(22,-25),(16,-20),(14,-11)],7,'accent',.7,
            parent=trigger)
    _optic('slim_optical_tip',187,31,10.3,detail)
    # The shallow crown carries fine sight references without pretending this
    # molded console unit has a moving firearm slide.
    # v2 fix 3/4: the sight rib is now level at 52 mm (the shell crests at 51),
    # its lower edge seated 2.5 mm into the shell; notch and blade sit on it.
    profile('slim_sight_rib',[(-37,46.5),(-18,48.5),(34,46),(119,41),
            (158,38.5),(158,52),(-37,52)],7,'body',.6)
    sights=functional_sights((154,52),(-35,52),55)
    for side in (-1,1):
        cyl('slim_reload_socket',(7,13,side*14),6.4,1.6,'dark',
            axis='l',vertices=12 if detail else 24,bevel=.4)
        control=node('button_a' if side<0 else 'button_b',
                     (7,13,side*15.2))
        cyl('slim_reload_button',(7,13,side*15.2),4.9,2,'accent',
            axis='l',vertices=12 if detail else 24,bevel=.5,parent=control)
    cyl('slim_cable_relief',(-70,-112,0),5.7,9,'dark',axis='h',vertices=n)
    # v2 fix 6: shell fasteners and cable-relief ribs removed.
    # v2 fix 1: 272 mm against the 240 mm toml; the barrel ahead of the guard
    # (forward of 40 mm) is shortened to 0.85, grip and guard unchanged.
    return {'_deform':{'stretch':(40,.85)},'muzzle':(187,31,0),**sights,
            'pivot_trigger':(22,-1,0),
            'grip_primary':(-49,-65,0),'button_reload':(7,13,-15.2),
            'button_a':(7,13,-15.2),'button_b':(7,13,15.2),
            'length_mm':274}


def _dpad(detail):
    n=10 if detail else 24
    # Compact round-shouldered electronics housing, broad thumb-control area,
    # short sensor snout and strongly relieved finger web.
    _loft('dpad_upper_clamshell',[(-48,33,10,13),(-41,38,20,21),
          (-25,39,24,25),(5,37,24,25),(42,34,21,23),
          (87,31,16,19),(117,30,13,16),(126,30,10.5,13)],
          count=n,power=.86,bevel=.8)
    profile('dpad_lower_frame',[(-40,23),(-21,22),(72,13),(111,14),
            (125,8),(119,2),(71,0),(50,-5),(16,-7),(7,-19),
            (-9,-17),(-16,-2),(-32,6)],32,'body',2.2)
    # The actual separated clamshell edges already describe the joint. Do not
    # add flat seam strips across a tapering compound surface.
    grip_stations=[(-112,-26,12,14),(-107,-27,18.5,17.5),
          (-91,-22,21,20),(-66,-15,23,22),(-43,-10,21,22),
          (-21,-8,18.5,21),(-4,-13,20,20),(11,-24,19,19)]
    _grip('dpad_palm_body',grip_stations,count=n)
    _conforming_grip_panel('dpad_grip_panel',grip_stations,
            [(-31,-10,1.5),(-34,-11,10),(-45,-11,16),(-63,-15,17),
             (-83,-20,15),(-99,-25,12),(-105,-27,7),(-108,-27,2)],
            n,detail)
    profile('dpad_web_flare',_curve([(-42,16),(-28,19),(-9,10),
            (4,-4),(1,-19),(-9,-24),(-22,-13),(-32,1),
            (-44,8)],detail),38,'body',.9)
    profile('dpad_heel',_curve([(-43,-106),(-14,-106),(-7,-102),
            (-7,-109),(-16,-115),(-35,-115),(-45,-111)],detail),
            35,'body',.8)
    ring('dpad_round_guard',_curve([(14,-2),(43,-3),(60,-12),(65,-28),
         (56,-43),(36,-50),(17,-45),(6,-27)],detail),
         _curve([(21,-10),(41,-11),(52,-17),(56,-27),(50,-35),
          (35,-41),(23,-37),(14,-26)],detail),15,'body',1)
    trigger=node('pivot_trigger',(27,-7,0))
    profile('dpad_wide_trigger',[(22,-5),(31,-7),(29,-16),(25,-24),
            (30,-31),(26,-34),(20,-29),(18,-20),(19,-12)],
            9,'accent',.8,parent=trigger)
    _optic('dpad_optic',141,30,12,detail)
    _loft('dpad_nose_collar',[(110,30,13.5,16.5),(124,30,12,14)],
          count=n,power=.9,mat='body',bevel=.6)
    # Directional control is seated into the broad rear shell, with individual
    # arms and a centre pivot, instead of a sticker or exposed floating cross.
    for side in (-1,1):
        cyl('dpad_control_recess',(-21,39,side*25),13.2,2,'accent',
            axis='l',vertices=12 if detail else 28,bevel=.7)
        dnode=node('button_dpad',(-21,39,-26.5))
        cross=[(-25,50),(-17,50),(-17,43),(-10,43),(-10,35),
               (-17,35),(-17,28),(-25,28),(-25,35),(-32,35),
               (-32,43),(-25,43)]
        # v2 fix 9: one moving d-pad (left, thumb side, where its node is);
        # the right-hand cross is the same moulding but static, so a single
        # inward press axis (+X) is valid. Round 1 hung both on one node.
        pad_parent=dnode if side<0 else None
        profile('direction_pad',cross,3.5,'dark',.9,
                lateral=side*26.5,parent=pad_parent)
        cyl('direction_pad_centre',(-21,39,side*28.5),3.3,.8,'accent',
            axis='l',vertices=12 if detail else 16,bevel=.2,parent=pad_parent)
        controls = [(37,34,'a'),(58,31,'b')] if side<0 else [
                    (37,34,'c'),(58,31,'select')]
        for f,h,label in controls:
            l=23.4 if f==37 else 21.9
            cyl('function_button_seat',(f,h,side*l),5.6,1.2,'dark',
                axis='l',vertices=12 if detail else 20,bevel=.4)
            bnode=node('button_' + label,
                       (f,h,side*(l+1)))
            cyl('function_' + label,(f,h,side*(l+1)),4.2,1.9,
                'accent',axis='l',vertices=12 if detail else 20,
                bevel=.6,parent=bnode)
    cyl('start_control_seat',(23,57.7,0),6.2,1.6,'dark',
        axis='h',vertices=12 if detail else 24,bevel=.4)
    start = node('button_start',(23,59.4,0))
    cyl('start_control_key',(23,59.4,0),4.8,2.1,'accent',
        axis='h',vertices=12 if detail else 24,bevel=.6,parent=start)
    # v2 fix 3/4: the clamshell crests at 63 mm and falls 20 mm to the nose, so
    # the level line runs at 65.5 mm; the front blade stands on a ramped riser
    # at the front of the crest instead of on the low nose.
    sights=functional_sights((45,54.5),(-30,61.6),65.5,front_ramp=12,
                             riser_front=(45,54.5,61.5),riser_mat='body')
    cyl('dpad_cable_socket',(-27,-115,0),7,6,'dark',axis='h',vertices=n)
    # v2 fix 6: shell fasteners removed.
    # v2 fix 1: 189 mm against the 225 mm toml. Everything forward of the grip
    # web (15 mm) is lengthened 1.167x; the 44 mm grip is not enlarged.
    return {'_deform':{'stretch':(15,1.167)},'muzzle':(141,30,0),**sights,
            'pivot_trigger':(27,-7,0),
            'grip_primary':(-15,-61,0),'button_dpad':(-21,39,-26.5),
            'button_a':(37,34,-24.4),'button_b':(58,31,-22.9),
            'button_c':(37,34,24.4),'button_select':(58,31,22.9),
            'button_start':(23,59.4,0),'button_reload':(58,31,-22.9),
            'length_mm':197}


def build(model_id, detail=0):
    builders={'arc-pistol-slide':_slide, 'arc-pistol-twin':_twin,
              'con-pistol-slim':_slim, 'con-pistol-dpad':_dpad}
    if model_id not in builders:
        raise ValueError('arcade.py does not own model: ' + model_id)
    return builders[model_id](detail)
