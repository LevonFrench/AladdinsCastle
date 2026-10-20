"""Mesh preparation for arc-light-gun (GPL-3.0-only code; the mesh it reads and writes is CC BY 4.0).

Input: hotd2-vr's core/rend/vr/gun_model.h, the baked form of "Namco Arcade Gun" by Martoscar
(https://sketchfab.com/3d-models/namco-arcade-gun-15fbd5b9add94a34b5c21746e3dd32be, CC BY 4.0),
already re-oriented by hotd2-vr into a controller aim space (metres, -z forward, +y up).

Steps (numpy only, so Blender's bundled Python can run it):
1. parse_header: positions (int16, 10 um units), normals (int8/127), triangles, named parts.
2. remove_lettering: the maker's name is engraved 0.9 mm deep into the right side of the upper
   housing. Each letter is a hole in that flat side face; the side face of the left side has
   only the screw holes. Holes present on the right and absent (mirrored) on the left are the
   letters: their walls and floors are deleted and each hole is refilled flat (ear clipping).
3. split_slide: everything above the housing's bottom plane (y = 61.05 mm) becomes the blowback
   slide; triangles crossing that plane are split and the cut sections are capped on both sides.
4. assemble: named parts in the asset frame (origin at the web of the hand, -z bore), with
   our functional notch/blade sights in place of the original sights and a static cover plate
   under the front of the housing that is uncovered when the slide is back.
"""
from __future__ import annotations

import re
from collections import defaultdict

import numpy as np

UNIT = 1e-5                # metres per header position unit
CUT_Y = 6105               # bottom plane of the upper housing, header units
ORIGIN = (0.0, 0.041, 0.075)   # web of the hand in hotd2-vr gun space (metres)
LENS_CENTRE = (0.0, 0.0471, -0.1401)   # hotd2-vr gunmodel::Muzzle


def parse_header(text):
    def arr(name, dt):
        body = re.search(name + r'\[\] = \{(.*?)\};', text, re.S).group(1)
        return np.array([int(x) for x in body.replace('\n', '').split(',') if x.strip()], dtype=dt)
    parts = [(n, int(a) // 3, int(c) // 3) for n, a, c in re.findall(r'\{ "(\w+)", (\d+)u, (\d+)u \}', text)]
    return dict(pos=arr('Positions', np.int64).reshape(-1, 3), nrm=arr('Normals', np.int64).reshape(-1, 3),
                tri=arr('Indices', np.int64).reshape(-1, 3), parts=parts)


def weld(pos):
    """Welded vertex id per vertex (vertices are split per normal in the header)."""
    _, inv = np.unique(pos, axis=0, return_inverse=True)
    return inv.reshape(-1)


def components(tri_welded):
    """Connected components over shared welded vertices: label per triangle."""
    parent = list(range(int(tri_welded.max()) + 1))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for a, b, c in tri_welded.tolist():
        ra, rb, rc = find(a), find(b), find(c)
        parent[rb] = ra
        parent[find(rc)] = ra
    roots = np.array([find(a) for a in tri_welded[:, 0].tolist()])
    _, lab = np.unique(roots, return_inverse=True)
    return lab


def _edge_map(tw):
    em = defaultdict(list)
    for t, (a, b, c) in enumerate(tw.tolist()):
        for e in ((a, b), (b, c), (c, a)):
            em[(min(e), max(e))].append(t)
    return em


def _flood(seed, allowed, tw, em):
    seen = {seed}
    stack = [seed]
    while stack:
        t = stack.pop()
        a, b, c = tw[t]
        for e in ((a, b), (b, c), (c, a)):
            for u in em[(min(e), max(e))]:
                if u not in seen and allowed[u]:
                    seen.add(u)
                    stack.append(u)
    return np.array(sorted(seen))


def _boundary_loops(face, tw):
    """Directed boundary loops (welded ids) of a set of triangles."""
    count = defaultdict(int)
    directed = {}
    for t in face:
        a, b, c = tw[t]
        for e in ((a, b), (b, c), (c, a)):
            k = (min(e), max(e))
            count[k] += 1
            directed[k] = e
    nxt = {}
    for k, n in count.items():
        if n == 1:
            a, b = directed[k]
            nxt[a] = b
    loops, seen = [], set()
    for s in nxt:
        if s in seen:
            continue
        lp, x = [s], nxt[s]
        seen.add(s)
        while x != s:
            lp.append(x)
            seen.add(x)
            x = nxt[x]
        loops.append(lp)
    return loops


def earclip(pts):
    """Triangulate a simple polygon (list of 2D points). Returns index triples (any winding)."""
    n = len(pts)
    area = 0.5 * sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    order = list(range(n)) if area > 0 else list(range(n))[::-1]

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    tris = []
    while len(order) > 3:
        for i in range(len(order)):
            a, b, c = order[i - 1], order[i], order[(i + 1) % len(order)]
            if cross(pts[a], pts[b], pts[c]) <= 1e-12:
                continue
            same = (tuple(pts[a]), tuple(pts[b]), tuple(pts[c]))
            if any(cross(pts[a], pts[b], pts[o]) >= 0 and cross(pts[b], pts[c], pts[o]) >= 0 and cross(pts[c], pts[a], pts[o]) >= 0
                   for o in order if o not in (a, b, c) and tuple(pts[o]) not in same):
                continue
            tris.append((a, b, c))
            order.pop(i)
            break
        else:   # only collinear remainders: drop the flattest vertex
            k = min(range(len(order)), key=lambda i: abs(cross(pts[order[i - 1]], pts[order[i]], pts[order[(i + 1) % len(order)]])))
            order.pop(k)
    tris.append(tuple(order))
    return tris


def remove_lettering(mesh, side_x=1577):
    """Delete the engraved letters on the +x side face of the upper housing and refill the face.

    Returns (mesh, report). The face is the flat region at x = side_x in 'hardsurfaces'; the
    letters are the boundary loops of that face that have no mirror image in the -x face.
    """
    P, tri = mesh['pos'], mesh['tri']
    w = weld(P)
    tw = w[tri]
    key = np.zeros((tw.max() + 1, 3), np.int64)
    key[w] = P
    em = _edge_map(tw)
    v = P[tri]
    fn = np.cross((v[:, 1] - v[:, 0]).astype(float), (v[:, 2] - v[:, 0]).astype(float))
    fn /= np.linalg.norm(fn, axis=1, keepdims=True) + 1e-15
    hard = np.zeros(len(tri), bool)
    for name, first, count in mesh['parts']:
        if name == 'hardsurfaces':
            hard[first:first + count] = True
    centre = v.mean(1)
    report = {}
    faces = {}
    for sign in (1, -1):
        plane = hard & np.all(v[:, :, 0] == sign * side_x, axis=1) & (fn[:, 0] * sign > 0.99)
        target = np.array([sign * side_x, 6800, -7500.0])
        cand = np.where(plane)[0]
        seed = cand[np.argmin(np.linalg.norm(centre[cand] - target, axis=1))]
        faces[sign] = _flood(seed, plane, tw, em)
    mirror = {tuple(key[x] * [-1, 1, 1]) for lp in _boundary_loops(faces[-1], tw) for x in lp}
    loops = _boundary_loops(faces[1], tw)
    letter_loops = [lp for lp in loops if not any(tuple(key[x]) in mirror for x in lp)]
    in_face = np.zeros(len(tri), bool)
    in_face[faces[1]] = True
    letter_vertices = {x for lp in letter_loops for x in lp}
    seeds = set()
    for lp in letter_loops:
        for a, b in zip(lp, lp[1:] + lp[:1]):
            seeds.update(u for u in em[(min(a, b), max(a, b))] if not in_face[u])
    letters = set()
    allowed = ~in_face & hard
    for s in seeds:
        if s not in letters:
            letters.update(_flood(s, allowed, tw, em).tolist())
    letters = np.array(sorted(letters))
    # one original vertex (with the face's +x normal) per welded id on the face
    rep = {}
    for t in faces[1]:
        for k in range(3):
            rep.setdefault(tw[t][k], tri[t][k])
    fill = []
    for lp in letter_loops:
        pts = [(float(key[x][2]), float(key[x][1])) for x in lp]
        for a, b, c in earclip(pts):
            t = [rep[lp[a]], rep[lp[b]], rep[lp[c]]]
            pa, pb, pc = (P[i].astype(float) for i in t)
            if np.cross(pb - pa, pc - pa)[0] < 0:
                t = [t[0], t[2], t[1]]
            fill.append(t)
    remove = np.zeros(len(tri), bool)
    remove[letters] = True
    out_tris, parts = [], []
    for name, first, count in mesh['parts']:
        keep = [tri[i].tolist() for i in range(first, first + count) if not remove[i]]
        if name == 'hardsurfaces':
            keep += fill
        parts.append((name, len(out_tris), len(keep)))
        out_tris += keep
    out_tris = np.array(out_tris)
    used = np.unique(out_tris)
    remap = -np.ones(len(P), np.int64)
    remap[used] = np.arange(len(used))
    lv = key[sorted(letter_vertices)]
    report.update(letter_loops=len(letter_loops), letter_triangles=int(len(letters)), fill_triangles=len(fill),
                  depth_mm=float((side_x - P[tri[letters]][:, :, 0].min()) * UNIT * 1000),
                  box_m=[(lv.min(0) * UNIT).round(4).tolist(), (lv.max(0) * UNIT).round(4).tolist()],
                  face_triangles_right=int(len(faces[1])), face_triangles_left=int(len(faces[-1])))
    return dict(pos=P[used], nrm=mesh['nrm'][used], tri=remap[out_tris], parts=parts), report


def open_edge_count(mesh):
    tw = weld(mesh['pos'])[mesh['tri']]
    count = defaultdict(int)
    for a, b, c in tw.tolist():
        for e in ((a, b), (b, c), (c, a)):
            count[(min(e), max(e))] += 1
    return sum(1 for n in count.values() if n == 1)


def split_slide(mesh, cut=CUT_Y):
    """Split at y = cut. Returns float positions/normals (header units), triangles with a side
    flag (1 = slide), their part names, and cap polygons for the cut sections."""
    P = mesh['pos'].astype(np.float64)
    N = mesh['nrm'].astype(np.float64)
    tri = mesh['tri']
    comp = components(weld(mesh['pos'])[tri])
    part_of = np.empty(len(tri), object)
    for name, first, count in mesh['parts']:
        part_of[first:first + count] = name
    pos, nrm = list(P), list(N)
    cache = {}

    def at_plane(i, j):
        k = (min(i, j), max(i, j))
        if k not in cache:
            a, b = P[k[0]], P[k[1]]
            t = (cut - a[1]) / (b[1] - a[1])
            p = a + t * (b - a)
            p[1] = cut
            pos.append(p)
            nrm.append(N[k[0]] + t * (N[k[1]] - N[k[0]]))
            cache[k] = len(pos) - 1
        return cache[k]
    out = []   # (triangle, slide?, part, component)
    # screws stay whole: a screw goes with the side holding most of its height
    screw_up = {}
    for c in np.unique(comp[part_of == 'screws']):
        ys = P[tri[(comp == c) & (part_of == 'screws')]][:, :, 1]
        screw_up[c] = bool(ys.mean() > cut)
    for t, tr in enumerate(tri.tolist()):
        ys = P[tr][:, 1]
        part, c = part_of[t], comp[t]
        above, below = ys > cut, ys < cut
        if part == 'screws' or not (above.any() and below.any()):
            if part == 'screws':
                up = screw_up[c]
            elif above.any() or below.any():
                up = bool(above.any())
            else:   # lying in the plane: faces looking down are the housing's underside
                a, b, cc = (P[i] for i in tr)
                up = np.cross(b - a, cc - a)[1] < 0
            out.append((tr, up, part, c))
            continue
        sgn = np.sign(ys - cut)
        for r in range(3):
            if sgn[r] != 0 and sgn[(r + 1) % 3] != sgn[r] and sgn[(r + 2) % 3] != sgn[r]:
                break
        a, b, cc = tr[r], tr[(r + 1) % 3], tr[(r + 2) % 3]
        sb, sc = sgn[(r + 1) % 3], sgn[(r + 2) % 3]
        lone = bool(sgn[r] > 0)
        if sb == 0:
            q = at_plane(a, cc)
            out += [([a, b, q], lone, part, c), ([q, b, cc], not lone, part, c)]
        elif sc == 0:
            q = at_plane(a, b)
            out += [([a, q, cc], lone, part, c), ([q, b, cc], not lone, part, c)]
        else:
            qb, qc = at_plane(a, b), at_plane(a, cc)
            out += [([a, qb, qc], lone, part, c), ([qb, b, cc], not lone, part, c), ([qb, cc, qc], not lone, part, c)]
    pos, nrm = np.array(pos), np.array(nrm)
    # two frame screws poke 0.8 mm through the housing floor (hidden inside it): flatten their
    # tips onto the plane so nothing crosses the moving housing
    for tr, up, part, c in out:
        if part == 'screws' and not up:
            for i in tr:
                pos[i][1] = min(pos[i][1], cut)
    # cut edges: in-plane edges shared by a slide and a static triangle of one component
    pkey = lambda i: (round(pos[i][0], 3), round(pos[i][2], 3))
    use = defaultdict(lambda: [0, 0])
    for tr, up, part, c in out:
        for e in ((tr[0], tr[1]), (tr[1], tr[2]), (tr[2], tr[0])):
            if pos[e[0]][1] == cut and pos[e[1]][1] == cut:
                k = (c,) + tuple(sorted((pkey(e[0]), pkey(e[1]))))
                use[k][0 if up else 1] += 1
    adj = defaultdict(lambda: defaultdict(set))
    for k, (nu, nd) in use.items():
        if nu % 2 == 1 and nd % 2 == 1 and k[1] != k[2]:
            adj[k[0]][k[1]].add(k[2])
            adj[k[0]][k[2]].add(k[1])
    caps = []
    for c, g in adj.items():
        seen = set()
        for s in g:
            if s in seen or len(g[s]) != 2:
                continue
            lp, prev, cur, closed = [s], None, s, False
            seen.add(s)
            while True:
                nx = [k for k in g[cur] if k != prev]
                if not nx or len(g[cur]) != 2:
                    break
                k = nx[0]
                if k == s:
                    closed = True
                    break
                if k in seen:
                    break
                lp.append(k)
                seen.add(k)
                prev, cur = cur, k
            if closed and len(lp) >= 3:
                caps.append((c, lp))
    return dict(pos=pos, nrm=nrm, out=out, caps=caps, comp=comp)


def _flat(verts, faces):
    """Flat-shaded triangle soup from polygons."""
    P, Nn, T = [], [], []
    for f in faces:
        pts = [np.array(verts[i], float) for i in f]
        n = np.cross(pts[1] - pts[0], pts[2] - pts[0])
        n /= np.linalg.norm(n)
        base = len(P)
        P += pts
        Nn += [n] * len(pts)
        T += [(base, base + i, base + i + 1) for i in range(1, len(pts) - 1)]
    return np.array(P), np.array(Nn), np.array(T)


def box(lo, hi):
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    v = [(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x1, y0, z0), (x1, y0, z1), (x1, y1, z1), (x1, y1, z0)]
    return _flat(v, [(0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1), (3, 2, 6, 7), (0, 3, 7, 4), (1, 5, 6, 2)])


def notched(centre, size, notch_w, notch_d):
    """Rear sight: a block with an open U notch on top, as common.geometry('notched_sight')."""
    cx, cy, cz = centre
    x, y, z = (s / 2 for s in size)
    g = notch_w / 2
    floor = y - notch_d
    prof = [(-x, -y), (x, -y), (x, y), (g, y), (g, floor), (-g, floor), (-g, y), (-x, y)]
    verts = [(cx + a, cy + b, cz + c) for c in (z, -z) for a, b in prof]
    n = len(prof)
    # caps as triangles over the concave profile, then the side quads
    faces = []
    for a, b, c in ((0, 1, 2), (0, 2, 4), (0, 4, 5), (2, 3, 4), (0, 5, 7), (5, 6, 7)):
        faces.append((a, b, c))
        faces.append((n + c, n + b, n + a))
    faces += [(i, i + n, (i + 1) % n + n, (i + 1) % n) for i in range(n)]
    return _flat(verts, faces)


def assemble(mesh, cut=CUT_Y):
    """Named parts in the asset frame: metres, origin at the web of the hand, -z bore, +y up.

    Returns dict name -> dict(pos, nrm, tri, material, parent) and anchors (metres)."""
    origin = np.array(ORIGIN)
    tri = mesh['tri']
    tw = weld(mesh['pos'])[tri]
    comp = components(tw)
    P = mesh['pos']
    # original sights: separate shells on top of the housing (replaced by functional sights)
    sight_tris = np.zeros(len(tri), bool)
    for c in np.unique(comp):
        sel = comp == c
        v = P[tri[sel]].reshape(-1, 3)
        lo, hi = v.min(0), v.max(0)
        rear = lo[1] >= 8740 and hi[1] >= 9400 and lo[2] >= 4500 and hi[2] <= 6400
        front = lo[1] >= 8900 and hi[1] >= 9380 and hi[2] <= -12400 and np.abs(v[:, 0]).max() <= 200
        if rear or front:
            sight_tris |= sel
    trigger = np.zeros(len(tri), bool)
    for c in np.unique(comp):
        sel = comp == c
        v = P[tri[sel]].reshape(-1, 3)
        if np.abs(v[:, 0]).max() <= 300 and v[:, 1].min() > 1000 and v[:, 1].max() < 4000 and v[:, 2].min() > 0:
            trigger |= sel
    keep = ~sight_tris
    base = dict(pos=P, nrm=mesh['nrm'], tri=tri[keep], parts=[])
    names = np.empty(len(tri), object)
    for name, first, count in mesh['parts']:
        names[first:first + count] = name
    base_names = names[keep]
    is_trigger = trigger[keep]
    # rebuild part ranges for split_slide (it reads part names per triangle)
    order = np.argsort([['hardsurfaces', 'softsurfaces', 'screws', 'lens'].index(n) for n in base_names], kind='stable')
    base['tri'] = base['tri'][order]
    base_names = base_names[order]
    is_trigger = is_trigger[order]
    first = 0
    for name in ('hardsurfaces', 'softsurfaces', 'screws', 'lens'):
        count = int((base_names == name).sum())
        base['parts'].append((name, first, count))
        first += count
    s = split_slide(base, cut)
    pos = s['pos'] * UNIT - origin
    nrm = s['nrm'] / np.linalg.norm(s['nrm'], axis=1, keepdims=True)
    groups = defaultdict(list)
    material = {'hardsurfaces': 'body', 'softsurfaces': 'accent', 'screws': 'dark', 'lens': 'glass'}
    # triangles keep the order of base triangles; split pieces map back to their source index
    trig_vertices = set(base['tri'][is_trigger].reshape(-1).tolist())
    for tr, up, part, c in s['out']:
        if not up and all(i in trig_vertices for i in tr):
            groups[('trigger', material[part])].append(tr)
        else:
            groups[('slide' if up else 'frame', material[part])].append(tr)
    parts = {}

    def pack(name, tris, mat, parent):
        tris = np.array(tris)
        used = np.unique(tris)
        remap = -np.ones(len(pos), np.int64)
        remap[used] = np.arange(len(used))
        parts[name] = dict(pos=pos[used], nrm=nrm[used], tri=remap[tris], material=mat, parent=parent)
    nice = {'body': 'shell', 'accent': 'panels', 'dark': 'screws', 'glass': 'lens'}
    for (where, mat), tris in sorted(groups.items()):
        parent = {'slide': 'slide_recoil', 'trigger': 'pivot_trigger', 'frame': None}[where]
        pack(f'{where}_{nice[mat]}', tris, mat, parent)
    # caps for the cut sections: slide side looks down, frame side looks up
    cap_up, cap_down = [], []
    capP = []
    for c, lp in s['caps']:
        pts = [(k[0], k[1]) for k in lp]
        for a, b, cc in earclip(pts):
            q = [np.array([pts[i][0], cut, pts[i][1]]) * UNIT - origin for i in (a, b, cc)]
            n = np.cross(q[1] - q[0], q[2] - q[0])
            if np.linalg.norm(n) < 1e-14:
                continue
            down = q if n[1] < 0 else [q[0], q[2], q[1]]
            cap_up.append(down)
            cap_down.append([down[0], down[2], down[1]])
    def soup(tris):
        Pq = np.array([p for t in tris for p in t])
        n = np.cross(Pq[1::3] - Pq[0::3], Pq[2::3] - Pq[0::3])
        n /= np.linalg.norm(n, axis=1, keepdims=True)
        return Pq, np.repeat(n, 3, axis=0), np.arange(len(Pq)).reshape(-1, 3)
    if cap_up:
        p, n, t = soup(cap_up)
        parts['slide_cut_cap'] = dict(pos=p, nrm=n, tri=t, material='body', parent='slide_recoil')
        p, n, t = soup(cap_down)
        parts['frame_cut_cap'] = dict(pos=p, nrm=n, tri=t, material='body', parent=None)
    # cover plate on the frame under the front of the housing (uncovered at full recoil)
    y = cut * UNIT - origin[1] - 0.0002
    p, n, t = box((-0.0130, y - 0.0010, -0.1380 - origin[2]), (0.0130, y, -0.1180 - origin[2]))
    parts['frame_top_cover'] = dict(pos=p, nrm=n, tri=t, material='body', parent=None)
    # functional sights (dark), seated where the original sights were; equal tops
    top = 0.0942 - origin[1]
    p, n, t = box((-0.0015, 0.0893 - origin[1], -0.1401 - origin[2]), (0.0015, top, -0.1250 - origin[2]))
    parts['front_sight_geometry'] = dict(pos=p, nrm=n, tri=t, material='dark', parent='slide_recoil')
    rear_lo, rear_hi = 0.08748 - origin[1], top
    p, n, t = notched((0.0, (rear_lo + rear_hi) / 2, 0.05445 - origin[2]), (0.018, rear_hi - rear_lo, 0.0173), 0.006, 0.003)
    parts['rear_sight_geometry'] = dict(pos=p, nrm=n, tri=t, material='dark', parent='slide_recoil')
    tp = np.vstack([parts[k]['pos'] for k in parts if k.startswith('trigger_')])
    blade = parts['trigger_panels']['pos']
    pivot = np.array([0.0, blade[:, 1].max() - 0.002, (blade[:, 2].min() + blade[:, 2].max()) / 2])
    anchors = dict(
        muzzle=np.array(LENS_CENTRE) - origin,
        sight_front=np.array([0.0, top, -0.1325 - origin[2]]),
        sight_rear=np.array([0.0, top, 0.05445 - origin[2]]),
        pivot_trigger=pivot,
        slide_recoil=np.array([0.0, 0.0755 - origin[1], -0.0300 - origin[2]]),
    )
    anchors['limit_recoil'] = anchors['slide_recoil'] + [0, 0, 0.015]
    return parts, anchors
