"""Original geometry toolkit for the owner's complete redesign. No old mesh data."""
import bpy, bmesh, math
from mathutils import Vector, Matrix
DETAIL = 0
LOD = None
NODES = {}
MATERIALS = {}

def xyz(v):
    f,h,l=v
    return Vector((l*.001,f*.001,h*.001))

def setup(lod, detail):
    global LOD, DETAIL, NODES
    LOD,DETAIL,NODES=lod,detail,{}

def materials():
    # v2 (studio fix 2): the repo convention, assets/guns/src/common.py MATERIAL_RGBA,
    # body/accent base factor 1.0 (the tint is the colour), roughness .58 (.32 glass) and no metal, as common.blender_run() writes them.
    values={'body':((1,1,1,1),0,.58),'accent':((1,1,1,1),0,.58),
            'dark':((.012,.012,.018,1),0,.58),'glass':((.025,.10,.16,1),0,.32)}
    for name,(color,metal,rough) in values.items():
        m=bpy.data.materials.get(name) or bpy.data.materials.new(name)
        bs=m.node_tree.nodes.get('Principled BSDF') if m.node_tree else None
        if bs is None:
            m.use_nodes=True
            bs=m.node_tree.nodes.get('Principled BSDF')
        bs.inputs['Base Color'].default_value=color
        bs.inputs['Metallic'].default_value=metal
        bs.inputs['Roughness'].default_value=rough
        m.diffuse_color=color
        MATERIALS[name]=m

def parent_to(obj,parent):
    par=NODES[parent] if isinstance(parent,str) else parent
    if par is None: par=LOD
    if par:
        bpy.context.view_layer.update()
        world=obj.matrix_world.copy()
        obj.parent=par
        obj.matrix_world=world

def node(name,loc=(0,0,0),parent=None):
    if name in NODES: return NODES[name]
    o=bpy.data.objects.new(name+('_lod1' if DETAIL else ''),None)
    bpy.context.scene.collection.objects.link(o)
    o.location=xyz(loc)
    o['semantic_node']=name
    parent_to(o,parent)
    NODES[name]=o
    return o

# v2 (studio fix 6): bevels on parts smaller than this add triangles that cannot be
# seen at 0.3-0.6 m; LOD1 keeps bevels only on large silhouette parts.
SMALL_PART_MM = 12
LOD1_BEVEL_MIN_MM = 60

def _extent_mm(obj):
    co=[v.co for v in obj.data.vertices]
    if not co: return 0
    return max(max(c[i] for c in co)-min(c[i] for c in co) for i in range(3))*1000

def finish(obj,mat='body',bevel=1,parent=None,exact_name=None,protect=False):
    if not obj.users_collection: bpy.context.scene.collection.objects.link(obj)
    if exact_name:
        obj.name=exact_name
    else:
        if not obj.name.startswith('part_'): obj.name='part_'+obj.name
        if DETAIL and not obj.name.endswith('_lod1'): obj.name += '_lod1'
    obj['authored_redesign']='2026-10-21'
    if protect: obj['protect_from_decimation']=True
    obj.data.materials.clear(); obj.data.materials.append(MATERIALS[mat])
    bm=bmesh.new(); bm.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(bm,faces=bm.faces)
    bm.to_mesh(obj.data); bm.free()
    for p in obj.data.polygons: p.use_smooth=True
    size=_extent_mm(obj)
    if size<SMALL_PART_MM: obj['protect_from_decimation']=True
    # Sub-0.3 mm bevels are below a headset pixel; two bevel segments only on
    # parts large enough to show a rounded edge.
    if bevel>=.3 and size>=SMALL_PART_MM and (not DETAIL or size>=LOD1_BEVEL_MIN_MM):
        mod=obj.modifiers.new('manufactured_edge','BEVEL')
        mod.width=bevel*.001;mod.segments=2 if (not DETAIL and size>=40) else 1
        mod.limit_method='ANGLE';mod.angle_limit=.50;mod.harden_normals=False
        mod.use_clamp_overlap=True
    # Normals are rebuilt for every exported mesh after LOD reduction (build_render.normals).
    parent_to(obj,parent)
    return obj

def mesh(name,verts,faces,mat='body',bevel=1,parent=None,exact_name=None,protect=False):
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    obj=bpy.data.objects.new(exact_name or name,data)
    return finish(obj,mat,bevel,parent,exact_name,protect)

def _clean_outline(points):
    """v2: drop repeated and collinear outline points; collinear runs in a cap
    n-gon triangulate into zero-area triangles (round 1 R04 LOD1 web saddle)."""
    pts=[tuple(p) for p in points]
    changed=True
    while changed and len(pts)>3:
        changed=False
        for i in range(len(pts)):
            a,b,c=pts[i-1],pts[i],pts[(i+1)%len(pts)]
            v1=(b[0]-a[0],b[1]-a[1]);v2=(c[0]-b[0],c[1]-b[1])
            l1=(v1[0]**2+v1[1]**2)**.5;l2=(v2[0]**2+v2[1]**2)**.5
            if l1<1e-6 or abs(v1[0]*v2[1]-v1[1]*v2[0])<1e-4*l1*l2:
                del pts[i];changed=True;break
    return pts

def profile(name,points,depth,mat='body',bevel=1,lateral=0,parent=None):
    points=_clean_outline(points)
    n=len(points)
    verts=[xyz((f,h,lateral+s*depth/2)) for s in (-1,1) for f,h in points]
    faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    return mesh(name,verts,faces,mat,bevel,parent)

def box(name,loc,size,mat='body',bevel=1,parent=None):
    f,h,l=loc; a,b,c=[v/2 for v in size]
    pts=[(f-a,h-b),(f+a,h-b),(f+a,h+b),(f-a,h+b)]
    return profile(name,pts,2*c,mat,bevel,l,parent)

def ring(name,outer,inner,depth,mat='body',bevel=1,lateral=0,parent=None):
    assert len(outer)==len(inner),(name,len(outer),len(inner))
    n=len(outer)
    verts=[xyz((f,h,lateral+s*depth/2)) for s in (-1,1) for loop in (outer,inner) for f,h in loop]
    faces=[]
    for i in range(n):
        j=(i+1)%n
        faces += [(i,j,j+n,i+n),(2*n+i,3*n+i,3*n+j,2*n+j),
                  (i,2*n+i,2*n+j,j),(n+i,n+j,3*n+j,3*n+i)]
    return mesh(name,verts,faces,mat,bevel,parent)

def radialpoint(loc,a,r,d,axis):
    f,h,l=loc;u=math.cos(a)*r;v=math.sin(a)*r
    return xyz((f+d,h+u,l+v) if axis=='f' else ((f+u,h+d,l+v) if axis=='h' else (f+u,h+v,l+d)))

def cyl(name,loc,radius,depth,mat='accent',axis='f',vertices=24,bevel=.5,parent=None):
    n=max(8,vertices//2) if DETAIL else vertices
    verts=[radialpoint(loc,2*math.pi*i/n,radius,s*depth/2,axis) for s in (-1,1) for i in range(n)]
    faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    return mesh(name,verts,faces,mat,bevel,parent)

def tube(name,loc,outer,inner,depth,mat='accent',axis='f',vertices=24,bevel=.3,parent=None):
    n=max(8,vertices//2) if DETAIL else vertices
    verts=[radialpoint(loc,2*math.pi*i/n,r,s*depth/2,axis) for s in (-1,1) for r in (outer,inner) for i in range(n)]
    faces=[]
    for i in range(n):
        j=(i+1)%n
        faces += [(i,j,2*n+j,2*n+i),(n+i,3*n+i,3*n+j,n+j),
                  (i,n+i,n+j,j),(2*n+i,2*n+j,3*n+j,3*n+i)]
    return mesh(name,verts,faces,mat,bevel,parent)

def sphere(name,loc,scale,mat='body',segments=24,rings=12,parent=None):
    n=max(8,segments//2) if DETAIL else segments
    r=max(5,rings//2) if DETAIL else rings
    verts=[]
    for j in range(1,r):
        phi=math.pi*j/r
        for i in range(n):
            a=math.tau*i/n
            verts.append(xyz((loc[0]+scale[0]*math.sin(phi)*math.cos(a),loc[1]+scale[1]*math.cos(phi),loc[2]+scale[2]*math.sin(phi)*math.sin(a))))
    verts += [xyz((loc[0],loc[1]+scale[1],loc[2])),xyz((loc[0],loc[1]-scale[1],loc[2]))]
    top,bottom=len(verts)-2,len(verts)-1
    faces=[]
    for j in range(r-2):
        for i in range(n):
            k=j*n+i;t=j*n+(i+1)%n
            faces.append((k,t,t+n,k+n))
    for i in range(n):
        faces += [(top,(i+1)%n,i),(bottom,(r-2)*n+i,(r-2)*n+(i+1)%n)]
    return mesh(name,verts,faces,mat,0,parent)

def rod(name,start,end,radius,mat='accent',vertices=12,parent=None):
    a,b=xyz(start),xyz(end);d=b-a
    q=d.to_track_quat('Z','Y')
    n=max(6,vertices//2) if DETAIL else vertices
    verts=[(a if side==0 else b)+q@Vector((radius*.001*math.cos(math.tau*i/n),radius*.001*math.sin(math.tau*i/n),0)) for side in (0,1) for i in range(n)]
    faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    return mesh(name,verts,faces,mat,.2,parent)

def screw(name,loc,radius=2.2,axis='l',mat='accent',parent=None):
    ob=cyl(name,loc,radius,.9,mat,axis,12,.15,parent)
    f,h,l=loc
    if axis=='l':box(name+'_slot',(f,h,l+(.42 if l>=0 else -.42)),(radius*1.3,.55,.15),'dark',.05,parent)
    elif axis=='h':box(name+'_slot',(f,h+.42,l),(radius*1.3,.15,.55),'dark',.05,parent)
    else:box(name+'_slot',(f+.42,h,l),(.15,.55,radius*1.3),'dark',.05,parent)
    return ob


def functional_sights(front,rear,top,parent_front=None,parent_rear=None,front_sink=3,
                      rear_sink=3,rear_width=18,front_ramp=0,riser_front=None,
                      riser_rear=None,riser_mat='accent'):
    """v2 (studio fixes 3 and 4): the repo's tier-1 notch-blade sights.

    front/rear are (forward, mounting height) on the authored roof. Both tops sit
    at `top`, so the sight line is parallel to the -Z bore. Geometry follows
    assets/guns/src/common.py functional_sights(): a 3 mm wide, 8 mm long front
    blade and an 8 mm long rear block with a 6 mm wide, 3 mm deep open notch,
    named front_/rear_sight_geometry_lod0/1 as tools/check_gun_assets.py expects.
    A taller sight is carried by an optional riser (forward, mounting height,
    riser top) seated on the roof instead of lifting the sight off it.
    """
    lod='_lod%d'%DETAIL
    ff,fh=front; rf,rh=rear
    if riser_front:
        f0,h0,h1=riser_front
        profile('front_sight_riser',[(f0-4-front_ramp,h0-front_sink),(f0+4,h0-front_sink),
                (f0+4,h1),(f0-4,h1)],7,riser_mat,.6,parent=parent_front)
        fb=h1-2
    else:
        fb=fh-front_sink
    pts=[(ff-4-(front_ramp if not riser_front else 0),fb),(ff+4,fb),(ff+4,top),(ff-4,top)]
    verts=[xyz((f,h,s*1.5)) for s in (-1,1) for f,h in pts]
    n=len(pts)
    faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh('front_sight_geometry',verts,faces,'dark',0,parent_front,exact_name='front_sight_geometry'+lod,protect=True)
    if riser_rear:
        f0,h0,h1=riser_rear
        profile('rear_sight_riser',[(f0-5,h0-rear_sink),(f0+5,h0-rear_sink),(f0+5,h1),(f0-5,h1)],
                rear_width-2,riser_mat,.8,parent=parent_rear)
        rb=h1-2
    else:
        rb=rh-rear_sink
    w=rear_width/2
    outline=[(-w,rb),(w,rb),(w,top),(3,top),(3,top-3),(-3,top-3),(-3,top),(-w,top)]
    verts=[xyz((rf+s*4,h,l)) for s in (-1,1) for l,h in outline]
    n=len(outline)
    faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh('rear_sight_geometry',verts,faces,'dark',0,parent_rear,exact_name='rear_sight_geometry'+lod,protect=True)
    return {'sight_front':(ff,top,0),'sight_rear':(rf,top,0),'functional_sights':True}


def smooth_loop(points,steps=None):
    """v2 fix 6: periodic Catmull-Rom rounding for closed outlines (trigger guards),
    spending freed triangles on silhouette curves instead of flat octagons."""
    steps=steps or (2 if DETAIL else 4)
    out=[];n=len(points)
    for i in range(n):
        p0,p1,p2,p3=points[i-1],points[i],points[(i+1)%n],points[(i+2)%n]
        for j in range(steps):
            t=j/steps
            out.append(tuple(.5*(2*p1[k]+(-p0[k]+p2[k])*t+(2*p0[k]-5*p1[k]+4*p2[k]-p3[k])*t*t+
                                 (-p0[k]+3*p1[k]-3*p2[k]+p3[k])*t*t*t) for k in range(2)))
    return out
