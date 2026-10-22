"""Four original controller/peripheral designs, authored for the owner redo.

No source meshes or previous outlines are loaded.  The primary shells use
explicit, changing cross sections rather than scaled boxes.  Dimensions are mm
in forward/height/lateral order; custom mesh vertices use Blender metres.
"""
import math
import bpy
from fresh import *


# v2 fix 6: models with LOD0 headroom after the detail cuts spend it on rounder
# main shells (24 -> 32 sections); set per model in build().
LOFT_SIDES = 24


def _sgn_power(value, exponent):
    return math.copysign(abs(value) ** exponent, value)


def _loft(name, sections, axis='f', mat='body', detail=0, power=.78,
          parent=None):
    """Sections: (axial position, other centre, other radius, lateral radius).

    A closed rounded superellipse loft with individual cross section placement.
    The curved sides are smooth; the planar end caps retain flat shading.
    """
    sides = 12 if detail else LOFT_SIDES
    vertices = []
    for along, centre, radius, width in sections:
        for j in range(sides):
            angle = j * 2 * math.pi / sides
            lateral = width * _sgn_power(math.cos(angle), power)
            other = centre + radius * _sgn_power(math.sin(angle), power)
            f, h = (along, other) if axis == 'f' else (other, along)
            vertices.append((lateral / 1000, f / 1000, h / 1000))
    faces = []
    for i in range(len(sections) - 1):
        for j in range(sides):
            a, an = i*sides+j, i*sides+(j+1)%sides
            b, bn = a+sides, an+sides
            faces.append((a,b,bn,an) if axis == 'f' else (a,an,bn,b))
    start = tuple(range(sides))
    end = tuple((len(sections)-1)*sides+j for j in range(sides))
    faces.extend((start, tuple(reversed(end))) if axis == 'f'
                 else (tuple(reversed(start)), end))
    mesh = bpy.data.meshes.new(name + '_mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    finish(obj, mat, 0, parent)
    for poly in mesh.polygons:
        poly.use_smooth = len(poly.vertices) == 4
    return obj


def _button(name, loc, radius, detail, mat='dark'):
    """A real separate key, seated in a recessed-looking contrasting collar."""
    f,h,l = loc
    n = 12 if detail else 24
    cyl(name+'_socket', (f,h-1.5,l), radius+1.4, 1.8, 'accent',
        axis='h', vertices=n, bevel=.35)
    pivot = node(name, loc)
    cyl(name+'_cap', loc, radius, 2.7, mat, axis='h', vertices=n,
        bevel=.65, parent=pivot)
    return pivot


def _cross_key(name, loc, length, arm, height, parent):
    """One watertight cross: no overlapping coplanar button faces."""
    f,h,l=loc
    a=length/2;b=arm/2
    outline=[(-a,-b),(-b,-b),(-b,-a),(b,-a),(b,-b),(a,-b),
             (a,b),(b,b),(b,a),(-b,a),(-b,b),(-a,b)]
    verts=[((l+y)/1000,(f+x)/1000,(h+z*height/2)/1000)
           for z in [-1,1] for x,y in outline]
    count=len(outline)
    faces=[tuple(range(count-1,-1,-1)),tuple(range(count,2*count))]
    faces += [(i,(i+1)%count,(i+1)%count+count,i+count)
              for i in range(count)]
    data=bpy.data.meshes.new(name+'_mesh')
    data.from_pydata(verts,[],faces);data.update()
    obj=bpy.data.objects.new(name,data)
    bpy.context.collection.objects.link(obj)
    return finish(obj,'dark',.65,parent)


def _grip_patch(name, sections, side, detail=0, power=.9, mat='dark',
                axis='h', angle_center=0):
    """A thin closed insert following the grip's real curved cross sections.

    Each row is (height, forward centre, forward radius, side radius,
    half-angle in degrees). The variable angular extent rounds the perimeter
    without planar polygon corners cutting through the curved grip surface.
    """
    steps=4 if detail else 8
    verts=[]
    for offset in [-.3,.55]:
        for along,centre,r,w,extent in sections:
            for j in range(steps+1):
                a=angle_center+math.radians(extent)*(-1+2*j/steps)
                lat=side*(w+offset)*_sgn_power(math.cos(a),power)
                across=centre+(r+offset)*_sgn_power(math.sin(a),power)
                f,h=(along,across) if axis=='f' else (across,along)
                verts.append((lat/1000,f/1000,h/1000))
    cols=steps+1
    rows=len(sections)
    layer=rows*cols
    faces=[]
    for i in range(rows-1):
        for j in range(steps):
            a=i*cols+j;b=a+cols
            faces.extend([(a,a+1,b+1,b),
                          (a+layer,b+layer,b+1+layer,a+1+layer)])
    perimeter=list(range(cols))
    perimeter += [i*cols+steps for i in range(1,rows)]
    perimeter += [(rows-1)*cols+j for j in range(steps-1,-1,-1)]
    perimeter += [i*cols for i in range(rows-2,0,-1)]
    for i,a in enumerate(perimeter):
        b=perimeter[(i+1)%len(perimeter)]
        faces.append((a,b,b+layer,a+layer))
    data=bpy.data.meshes.new(name+'_mesh')
    data.from_pydata(verts,[],faces);data.update()
    obj=bpy.data.objects.new(name,data)
    bpy.context.collection.objects.link(obj)
    finish(obj,mat,0)
    for poly in data.polygons:
        poly.use_smooth=True
    return obj


# v2 fix 1 (R05): the real Wii Remote body is 148 mm (toml). Round 1 drew it
# 195.7 mm from the rear cap to the infrared window. Every forward position is
# multiplied by POINTER_F about the grip anchor; cross-section radii, button
# sizes and heights are unchanged, so the controls stay round and the shell
# keeps its section. Shell plus window: 148 mm.
POINTER_F = 148/195.7

def _pf(f):
    return f*POINTER_F


def _pointer(detail):
    F=_pf
    # Palm side is curved in both length and section.  The parting line is a
    # separate compressed gasket between the bottom and face shell.
    bottom = [(F(-78),-2,2.5,5),(F(-74),-2,7,13),(F(-61),-3,10,16),
              (F(-33),-4,10.5,17.4),(F(4),-4,10,18),(F(40),-3,9.5,18.4),
              (F(76),-2,9,18.4),(F(101),-1,8,17.3),(F(111),0,5,14.7),
              (F(114),0,2.5,12)]
    _loft('remote_lower_sculpted_shell', bottom, mat='body', detail=detail)
    _loft('remote_continuous_shell_seal',
          [(f,h+5,r*.35,w+.35) for f,h,r,w in bottom],
          mat='dark', detail=detail)
    face = [(F(-77),3,1.5,4.8),(F(-73),4.4,5,13),(F(-60),4.5,7.4,16.5),
            (F(-33),4.8,7.8,17.8),(F(4),5,8.1,18.5),(F(40),5,8.3,18.9),
            (F(76),5,8,18.9),(F(101),4.8,7,17.8),(F(111),4,4.8,15),
            (F(114),3.8,2,12.2)]
    _loft('remote_contoured_face_shell', face, detail=detail)
    # Optical end assembly is tucked into the nose rather than a black block.
    box('remote_optical_bezel',(F(112.5),2.2,0),(8,20.2,33),
        'dark',bevel=2.8)
    box('remote_optical_window',(F(117.1),3,0),(1.2,13.5,25),
        'glass',bevel=4)
    # D-pad is a moving compound cross with a concealed circular central seat.
    cyl('dpad_socket',(F(76),13.3,0),11.6,2.1,'accent',axis='h',
        vertices=12 if detail else 20,bevel=.5)
    pad = node('button_dpad',(F(76),15,0))
    _cross_key('remote_dpad_cross',(F(76),15,0),19,6.2,3.5,pad)
    _button('button_a',(F(43),14.2,0),8.6,detail)
    _button('button_minus',(F(17),13.2,-10),3.2,detail)
    _button('button_plus',(F(17),13.2,10),3.2,detail)
    _button('button_home',(F(17),14,0),3.3,detail,mat='body')
    _button('button_one',(F(-32),13,0),5,detail)
    _button('button_two',(F(-52),12.7,0),5,detail)
    # v2 fix 4: the power key is not in the toml's button list, so it is a
    # static key (no button_ node) and the checker's button set matches.
    _button('key_power',(F(99),12.9,-9),2.7,detail,mat='accent')
    if not detail:
        # v2 fix 6: the nine 2 mm speaker holes become three flat grille slots
        # (one per former row) and the player lights are flat strips; the rear
        # screws are removed.
        for f in [-3,-8,-13]:
            box('speaker_grille_slot',(F(f),12.9,0),(1.3,.5,13),'dark',bevel=0)
        for l in [-9,-3,3,9]:
            box('player_indicator',(F(-65),11.8,l),(3.1,.55,1.5),
                'glass',bevel=0)
    lower_key = node('button_b',(F(44),-10,0))
    _loft('remote_underfinger_key',[(F(26),-9.5,1.1,5),(F(30),-12,2,7),
          (F(40),-15,3.3,7.5),(F(50),-13.8,3,6),(F(54),-11,1,3)],
          mat='dark',detail=detail,parent=lower_key)
    # Lower battery cover sits within the palm envelope and carries a latch.
    _loft('remote_battery_hatch',[(F(-64),-9,2,10),(F(-59),-11.8,2.4,12.5),
          (F(-25),-13.5,2.2,13.5),(F(1),-13,2,13),(F(8),-11.2,1.5,9)],
          mat='accent',detail=detail)
    box('battery_latch',(F(-61),-12.2,0),(6,1.5,9),'dark',bevel=.8)
    tube('wrist_strap_eye',(F(-78),-2,0),5.5,3,4,'accent',axis='l',
         vertices=12 if detail else 20,bevel=.5)
    node('strap_anchor',(F(-78),-2,0))
    return {'asset_kind':'controller','optical_forward':(F(117.7),3,0),
            'grip_primary':(F(-18),-3,0),'button_a':(F(43),15,0),
            'button_dpad':(F(76),15,0),'button_b':(F(44),-10,0),
            'length_mm':148}


# v2 fix 1/7 (R06): the remote insert is lengthened from 120 mm to 148 mm so it
# matches the R05 remote; its rear moves back over the restored rear handle.
def _ins(f):
    return -18+(f+4)*148/120


def _rounded_loop(points, steps=3):
    """Centripetal Catmull-Rom loop: no cusps where point spacing is uneven."""
    def lerp(a,b,ta,tb,t):
        if tb-ta<1e-9:return a
        return tuple(((tb-t)*a[k]+(t-ta)*b[k])/(tb-ta) for k in range(2))
    out=[]
    n=len(points)
    for i in range(n):
        p0,p1,p2,p3=points[i-1],points[i],points[(i+1)%n],points[(i+2)%n]
        d=lambda a,b:max(1e-6,((a[0]-b[0])**2+(a[1]-b[1])**2)**.25)
        t0=0;t1=t0+d(p0,p1);t2=t1+d(p1,p2);t3=t2+d(p2,p3)
        for j in range(steps):
            t=t1+(t2-t1)*j/steps
            a1=lerp(p0,p1,t0,t1,t);a2=lerp(p1,p2,t1,t2,t);a3=lerp(p2,p3,t2,t3,t)
            b1=lerp(a1,a2,t0,t2,t);b2=lerp(a2,a3,t1,t3,t)
            out.append(lerp(b1,b2,t1,t2,t))
    return out


def _shell(detail):
    # A sculptural complete dock shell: no skeletal triangular stock or old
    # remote-on-a-box gun.  Cross sections taper continuously into the nose.
    _loft('dock_main_sculpted_housing',[
        (-35,22,6,8),(-31,23,14,17),(-17,24,21,21),
        (5,25,23,22),(31,26,22,23),(67,27,20,22),
        (103,28,17,19),(132,29,14.5,17),(140,29,12.8,12.8),
        (148,29,12,12),
        (153,29,9,10)],detail=detail)
    _loft('shell_grip_core',[
        (-107,-33,6,7),(-103,-33,14,13),(-91,-29,18.5,15),
        (-71,-21,20,16.3),(-47,-12,19,17),(-22,-5,17,17),
        (-3,-5,18,18),(9,-8,18,18.5),(17,-11,16,17)],
        axis='h',detail=detail,power=.91)
    # Swept rubber rear spine and thumb contact pads define the grip as a
    # manufactured ergonomic assembly, not a rectangular pistol handle.
    _grip_patch('shell_seated_backstrap',[
        (-101,-32.3,14.75,13.3,14),(-91,-29,18.5,15,38),
        (-71,-21,20,16.3,43),(-47,-12,19,17,43),
        (-22,-5,17,17,35),(-12,-5,17.5,17.5,13)],
        1,detail,power=.91,angle_center=-math.pi/2)
    for side in [-1,1]:
        _grip_patch('shell_contoured_grip_insert',[
            (-98,-31.3,15.9,13.9,15),(-91,-29,18.5,15,40),
            (-71,-21,20,16.3,48),(-47,-12,19,17,48),
            (-30,-7.2,17.6,17,40),(-23,-5.3,17.1,17,14)],
            side,detail,power=.91)
        _grip_patch('shell_thumb_contact',[
            (-17,-5,17.3,17.3,12),(-8,-5,17.8,17.8,30),
            (1,-6,18,18.2,30),(6,-7.3,18,18.4,12)],
            side,detail,power=.91)
    # Guard is genuinely open with a rolled, asymmetric perimeter.
    outer = [(-1,6),(18,10),(42,6),(56,-5),(53,-22),(42,-34),
             (22,-40),(3,-34),(-4,-17)]
    inner = [(5,0),(18,3),(37,-1),(46,-9),(44,-19),(37,-26),
             (23,-31),(10,-26),(3,-15)]
    ring('shell_open_trigger_guard',smooth_loop(outer),smooth_loop(inner),15,'body',bevel=1.6)  # v2: rounded guard
    trig = node('pivot_trigger',(21,1,0))
    profile('shell_curved_trigger',[(17,3),(24,3),(24,-4),(19,-13),
            (20,-20),(16,-23),(12,-17),(13,-9)],7,'dark',
            bevel=1.2,parent=trig)
    # The inset dock bed has exposed clearance and two rounded retaining
    # rails.  It is clearly a shell with a real removable controller cradle.
    I=_ins
    box('dock_recess_floor',((I(-8.5)+I(120.5))/2,43.5,0),(I(120.5)-I(-8.5),4.5,31),'dark',bevel=4)
    for side in [-1,1]:
        rail = _loft('dock_retention_rail',[
            (I(-11),44,2,1.8),(I(-4),47,4,2.8),(I(26),48,4.4,2.8),
            (I(81),46,3.8,2.8),(I(114),44,3,2.5),(I(123),42,1.5,1.5)],
            detail=detail,mat='accent')
        # Offset the actual object returned by the loft, independent of names
        # or existing LOD geometry in the shared Blender scene.
        rail.location.x += side*16.8/1000
    # The rear lock sits at the back of the longer insert and on the restored
    # rear handle. v2 fix 4: the release key is not in the toml's button list,
    # so it is a static key on the lock's rear face (no button_ node).
    box('dock_rear_lock',(I(-12),45,0),(10,14,34),'body',bevel=3)
    lock = node('key_dock_release',(I(-12)-5.8,46,0))
    box('dock_release_key',(I(-12)-5.8,46,0),(2.4,7,14),'dark',bevel=.8,parent=lock)
    # A complete slim controller insert is seated inside the cradle.  Its
    # live controls remain independently animated in both LODs.
    _loft('dock_insert_face',[
        (I(-4),47,1.7,7),(I(2),48,3.5,12),(I(27),48.5,4,12.4),
        (I(66),48.5,4,12.4),(I(104),47,3.5,12),(I(116),45,2.5,8)],
        detail=detail)
    cyl('dock_dpad_socket',(I(87),52,0),8.4,1.7,'accent',axis='h',
        vertices=12 if detail else 20,bevel=.4)
    pad=node('button_dpad',(I(87),53.5,0))
    _cross_key('dock_dpad_cross',(I(87),53.5,0),14,4.7,2.8,pad)
    _button('button_a',(I(62),53.5,0),6.2,detail)
    _button('button_home',(I(39),53.4,0),2.9,detail,mat='body')
    _button('button_minus',(I(39),52.5,-8),2.5,detail)
    _button('button_plus',(I(39),52.5,8),2.5,detail)
    _button('button_one',(I(15),52.5,-5),3.5,detail)
    _button('button_two',(I(15),52.5,5),3.5,detail)
    # v2 fix 7: the rear handle the toml describes ("open rear handle dropping
    # down and back") is restored as a rounded open loop behind the trigger
    # grip, bringing the shell to its 300 mm length. grip_two (the rear dock
    # hold) sits under the middle of its upper bar, outside the housing.
    outer=[(-14,40),(-42,40),(-92,1),(-138,-36),(-145,-54),(-136,-69),(-80,-64),(-22,-58),(-8,-30)]
    inner=[(-32,20),(-50,12),(-84,-14),(-110,-35),(-117,-45),(-110,-51),(-76,-46),(-42,-40),(-30,-18)]
    steps=2 if detail else 3
    ring('shell_rear_handle',_rounded_loop(outer,steps),_rounded_loop(inner,steps),28,'body',bevel=4)
    # Distinct muzzle shroud with true open optical aperture.
    tube('shell_front_shroud',(150,29,0),13.8,9.5,15,'accent',
         axis='f',vertices=12 if detail else 24,bevel=1)
    cyl('shell_optical_lens',(156,29,0),8.8,1.4,'glass',axis='f',
        vertices=12 if detail else 24,bevel=.3)
    for side in [-1,1]:
        _grip_patch('shell_seated_service_panel',[
            (42,26.31,21.39,22.69,8),(48,26.47,21.06,22.53,22),
            (67,27,20,22,25),(88,27.58,18.25,20.25,23),
            (100,27.92,17.25,19.25,8)],side,detail,power=.78,
            mat='accent',axis='f')
        # v2 fix 6: panel fasteners and the eight 1.8 mm cooling-slot inserts removed.
    node('grip_primary',(-19,-57,0))
    # v2 fix 3/4: the remote's keys reach 54.9 mm, so the level sight line runs at
    # 57.5 mm: a notch on the rear lock and a blade on a riser at the remote's nose.
    sights=functional_sights((125,48.5),(I(-12),52),57.5,rear_sink=2,
                             front_ramp=6,riser_front=(125,48.5,53.5))
    return {'asset_kind':'gun','muzzle':(157.8,29,0),
            'grip_primary':(-19,-57,0),'pivot_trigger':(21,1,0),
            'grip_two':(-85,-22,0),**sights,
            'dock_attach':(55,47,0),'length_mm':300}


def _stick(detail):
    # A diagonally biased palm volume, broad beneath the thumb and narrowed
    # behind the fingertips.  Individual axial centres create a gentle S.
    _loft('stick_hand_contour',[
        (-104,-15,4,5),(-100,-16,10,10),(-91,-18,15.5,15),
        (-76,-18,18.5,18),(-56,-14,20.5,20),(-34,-8,21,21),
        (-15,-2,22,21.5),(1,4,22,21),(7,7,21,20)],
        axis='h',detail=detail,power=.94)
    _grip_patch('stick_seated_palm_cushion',[
        (-98,-16.44,11.22,11.11,12),(-91,-18,15.5,15,36),
        (-76,-18,18.5,18,55),(-56,-14,20.5,20,60),
        (-34,-8,21,21,58),(-15,-2,22,21.5,45),
        (-6,1.375,22,21.21875,14)],1,detail,power=.94,
        angle_center=-math.pi/2)
    # Sculptural top cap is its own manufactured assembly with a visible
    # shadow joint.  Two nested lofts keep the cap off a flat hockey puck.
    _loft('stick_top_gasket',[
        (6.8,7,21.1,20.1),(8.8,7.5,20.7,19.8)],
        axis='h',mat='dark',detail=detail,power=.94)
    _loft('stick_top_cap',[
        (8.5,7.5,20.7,19.8),(13,9.5,19,18),
        (18,11,15,14),(21.8,12,13,13)],
        axis='h',mat='accent',detail=detail,power=.94)
    n=12 if detail else 24
    # The boot, shaft and actual thumb cap share a functional motion node.
    cyl('stick_socket',(12,23,0),13.2,3,'dark',axis='h',vertices=n,bevel=1)
    tilt_x=node('pivot_stick_x',(12,25,0))
    tilt_y=node('pivot_stick_y',(12,25,0),parent=tilt_x)
    sphere('stick_flexible_boot',(12,25,0),(9.5,4.8,9.5),'dark',
           segments=n,rings=6 if detail else 8,parent=tilt_x)
    cyl('stick_shaft',(12,31,0),4.7,11,'accent',axis='h',
        vertices=n,bevel=.4,parent=tilt_y)
    click=node('button_stick',(12,37,0),parent=tilt_y)
    cyl('stick_thumbcap_base',(12,37,0),12.2,5,'dark',axis='h',
        vertices=n,bevel=1.7,parent=click)
    tube('stick_thumbcap_rim',(12,39.4,0),12.1,9.8,2.1,'glass',
         axis='h',vertices=n,bevel=.5,parent=click)
    cyl('stick_thumb_contact',(12,39.3,0),9.7,1.4,'dark',axis='h',
        vertices=n,bevel=.5,parent=click)
    # Two nested finger keys on the sloping front face, not disks glued to
    # the side.  They move as distinct controller buttons.
    for name,loc,size in [('button_c',(25,-9,0),(7,14,22)),
                          ('button_z',(19,-28,0),(7,18,24))]:
        f,h,l=loc
        box(name+'_recess',(f-1,h,l),(size[0],size[1]+3,size[2]+3),
            'dark',bevel=4)
        key=node(name,loc)
        box(name+'_key',loc,size,'accent',bevel=4,parent=key)
    for side in [-1,1]:
        _grip_patch('stick_seated_side_pad',[
            (-89,-18,15.9,15.4,12),(-80,-18,17.7,17.2,39),
            (-76,-18,18.5,18,48),(-56,-14,20.5,20,51),
            (-34,-8,21,21,45),(-25,-5.2,21.5,21.2,29),
            (-21,-3.9,21.7,21.3,12)],side,detail,power=.94)
        # v2 fix 6: the four slotted case screws (28% of round-1 LOD0) removed.
    # v2 fix 1/4 (R08): the toml and assets/guns/README.md say no cable. The cable
    # ferrule and strain relief are removed; the palm loft closes the base.
    node('grip_primary',(-12,-55,0))
    return {'asset_kind':'controller','grip_primary':(-12,-55,0),
            'pivot_stick_x':(12,25,0),'pivot_stick_y':(12,25,0),
            'button_stick':(12,37,0),'button_c':(25,-9,0),
            'button_z':(19,-28,0),
            # v2 fix 1: 144.5 mm tall without the cable; scaled 0.85 to the
            # 120 mm toml height (real Nunchuk about 115 mm).
            '_deform':{'scale':.85},'length_mm':158}


def _chunky(detail):
    # Compact molded optical revolver with an intentionally abbreviated
    # barrel, open cylinder silhouette and a rolled compound palm grip.
    n=12 if detail else 24
    _loft('revolver_palm_frame',[
        (-102,-29,6,7),(-98,-29,15,13.5),(-85,-26,21,16),
        (-64,-21,22,17.2),(-42,-14,20,17),(-22,-7,17,16.2),
        (-6,-5,15,16),(8,-10,15,17),(20,-12,14,17.2)],
        axis='h',detail=detail,power=.85)
    _grip_patch('revolver_seated_backstrap',[
        (-96,-28.54,15.92,13.88,14),(-85,-26,21,16,38),
        (-64,-21,22,17.2,43),(-42,-14,20,17,43),
        (-22,-7,17,16.2,35),(-10,-5.5,15.5,16.05,13)],
        1,detail,power=.85,angle_center=-math.pi/2)
    # Side frame curves around the actual cylinder instead of hiding it in
    # an oversized slab.  Long calm panels surround clustered small details.
    profile('revolver_rear_frame',[
        (-25,9),(-29,26),(-26,43),(-17,52),(-4,51),(7,43),
        (6,34),(1,24),(12,12),(6,2),(-7,-7),(-19,0)],
        35,'body',bevel=3.2)
    profile('revolver_top_strap',[
        (-19,48),(-8,58),(17,60),(53,60),(80,56),(105,51),
        (107,44),(81,47),(55,51),(18,51),(-3,49)],
        23,'body',bevel=2)
    profile('revolver_lower_cradle',[
        (1,12),(22,8),(59,8),(74,15),(91,17),(91,26),
        (69,24),(56,19),(20,19),(7,23)],
        24,'body',bevel=2.5)
    cylinder=node('cylinder_spin',(33,35,0))
    cyl('cylinder_rear_bearing',(12,35,0),17.5,5,'dark',axis='f',
        vertices=n,bevel=.6,parent=cylinder)
    tube('revolver_visible_cylinder',(35,35,0),23,6.5,39,'accent',
         axis='f',vertices=18 if detail else 36,bevel=1.4,parent=cylinder)
    cyl('cylinder_front_web',(55.4,35,0),21.5,2.5,'body',axis='f',
        vertices=n,bevel=.7,parent=cylinder)
    # Six inset chamber mouths with separate lip and recessed dark interior.
    for i in range(6):
        ang=2*math.pi*i/6
        h=35+14.7*math.sin(ang)
        l=14.7*math.cos(ang)
        tube('cylinder_chamber_rim',(57.3,h,l),5.2,3.5,2.8,
             'accent',axis='f',vertices=8 if detail else 12,
             bevel=.35,parent=cylinder)
        cyl('cylinder_chamber_recess',(57.1,h,l),3.45,.4,
            'dark',axis='f',vertices=8 if detail else 12,
            bevel=0,parent=cylinder)
        # v2 fix 6: the six 1.5 mm flute rods are removed.
    cyl('cylinder_central_axle',(57.6,35,0),4.5,2,'dark',axis='f',
        vertices=n,bevel=.5,parent=cylinder)
    _loft('compact_barrel_shroud',[
        (60,43,9,10),(65,43,13,13),(86,43,13,13),
        (112,43,11.5,11.5),(116,43,9.8,9.8),(120,43,9.5,9.5)],
        detail=detail,power=1)
    tube('short_muzzle_crown',(120,43,0),10.5,6.7,8,'accent',
         axis='f',vertices=n,bevel=.7)
    cyl('revolver_optical_lens',(121.5,43,0),6.3,1,'glass',
        axis='f',vertices=n,bevel=.3)
    # Muzzle is short and lower ejector housing remains physically integrated.
    rod('ejector_rod',(61,23,-8),(109,23,-8),3.3,'accent',vertices=n)
    # v2 fix 9 (R07): close the open cylinder window so it does not read as
    # hollow. A solid web on the centre line fills the slits between frame,
    # cylinder, barrel and cradle (it is hidden inside the cylinder itself),
    # and a web joins the ejector rod to the barrel.
    profile('cylinder_window_web',[(2,14),(68,14),(68,50),(2,50)],12,'body',bevel=1)
    profile('ejector_underlug_web',[(61,21),(108,21),(108,34),(61,37)],10,'body',
            bevel=1,lateral=-4)
    cyl('ejector_tip',(110,23,-8),4,5,'dark',axis='f',vertices=n,bevel=.5)
    outer=[(2,5),(17,9),(37,7),(49,-3),(48,-20),(36,-32),
           (19,-36),(5,-29),(-1,-14)]
    inner=[(8,-1),(19,2),(33,1),(41,-7),(40,-18),(32,-25),
           (20,-28),(11,-22),(6,-12)]
    ring('revolver_rolled_trigger_guard',smooth_loop(outer),smooth_loop(inner),13,'body',bevel=1.5)  # v2: rounded guard
    trigger=node('pivot_trigger',(21,2,0))
    profile('revolver_hooked_trigger',[(17,3),(24,3),(25,-4),
            (18,-13),(18,-19),(14,-22),(11,-17),(12,-9)],
            7,'dark',bevel=1.1,parent=trigger)
    hammer=node('pivot_hammer',(-24,47,0))
    profile('revolver_cocking_spur',[(-29,43),(-22,48),(-26,57),
            (-38,63),(-42,59),(-32,52)],12,'accent',
            bevel=1.5,parent=hammer)
    for side in [-1,1]:
        _grip_patch('revolver_seated_grip_cheek',[
            (-95,-28.3,16.4,14.1,14),(-89,-26.9,19.2,15.2,37),
            (-85,-26,21,16,49),(-64,-21,22,17.2,54),
            (-42,-14,20,17,50),(-29,-9.5,18.1,16.5,35),
            (-24,-7.7,17.3,16.3,13)],side,detail,power=.85)
        profile('revolver_cylinder_access_plate',[
            (-23,20),(-25,33),(-20,44),(-11,48),(-2,43),
            (1,34),(-6,22),(-13,15)],1.1,'accent',bevel=1.1,
            lateral=side*17.7)
        # v2 fix 4: no reload buttons in the toml, so the paddles are static
        # (release_ nodes, not button_ nodes) and the checker's button set matches.
        release=node('release_'+('left' if side<0 else 'right'),
                     (-15,34,side*18.2))
        profile('cylinder_release_paddle',[(-24,32),(-21,40),
                (-9,41),(-6,36),(-10,30)],2.2,'dark',bevel=1.4,
                lateral=side*18.2,parent=release)
        # v2 fix 6: cheek and frame fasteners removed.
    box('front_sight_base',(108,55,0),(13,3.2,8),'accent',bevel=1)
    box('rear_sight_base',(-4,59,0),(12,3,15),'accent',bevel=.8)
    # v2 fix 3/4: level notch-blade sights on the existing bases; the top strap
    # crests at 60 mm, so the line runs at 62.5 mm.
    sights=functional_sights((108,56.6),(-5,60.5),62.5)
    _button('button_start',(-17,58,0),3.2,detail)
    node('grip_primary',(-20,-59,0))
    # v2 fix 1: 171.5 mm against the 230 mm toml: scaled 1.12 (grip perimeter
    # stays near R01's) and the barrel forward of the cylinder lengthened 1.32x.
    return {'_deform':{'scale':1.12,'stretch':(67,1.32)},
            'asset_kind':'gun','muzzle':(124,43,0),**sights,
            'pivot_trigger':(21,2,0),'cylinder_spin':(33,35,0),
            'grip_primary':(-20,-59,0),'button_start':(-17,58,0),
            'length_mm':174}


def build(model_id, detail=0):
    global LOFT_SIDES
    LOFT_SIDES = 32 if model_id in ('con-pointer-remote','con-stick-grip','con-revolver-chunky') else 24
    builders={'con-pointer-remote':_pointer, 'con-remote-shell':_shell,
              'con-stick-grip':_stick, 'con-revolver-chunky':_chunky}
    return builders[model_id](detail)
