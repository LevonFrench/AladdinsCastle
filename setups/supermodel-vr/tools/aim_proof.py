# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline proof of head-tracked pointing in Star Wars Trilogy (Supermodel with the ACVR patch).

Drives made-up controller and head poses (ACVR_FAKE_CONTROLLERS with ACVR_VR_FAKE), saves the two eye pictures
the patched emulator draws, finds the game's own crosshair in them, and compares it with where the controller's
ray says it should be. No headset, no OpenXR runtime: the run is hidden, muted and quits by itself.

    python aim_proof.py run     --exe Supermodel.exe --game swtrilgy.zip --state yavin.st0 --work <folder>
                                --out <folder> --scenarios scenarios.json [--env NAME=VALUE ...]
    python aim_proof.py measure --out <folder> --scenarios scenarios.json --template reticle.png

With --screen (both commands) the same check is made for the Hub's Screen view: the game drawn in stereo for a
flat screen 2.5 m ahead, the controller pointing at it, and the eye pictures read as that screen (head_yaw 0).

scenarios.json: {"segment": 60, "points": [{"name": "...", "head": [x, y, z], "head_yaw": deg, "hand": [x, y, z],
"aim": [yaw, pitch], "grip": 0}, ...]}. Metres and degrees, left and up positive, in the frame the made-up head
was recentred in (the origin, facing -z). Each point is held for one segment and its last frame is saved.

What "right" means. The game draws its crosshair in its 3D scene, far away (measure prints the disparity
between the eyes, and the depth it implies). A crosshair that far is on the controller's ray when it lies in the
ray's direction, from any head or hand position: that direction, through the game's own field of view, is the
expected place. Errors are given in the game's 496x384 pixels and in degrees.

The reticle template is a mask of the game's crosshair cut from a capture of your own (not part of this repo):
    python aim_proof.py template <png at 496x384 scale> <x> <y> <out.png>
Needs numpy and Pillow.
"""
import argparse
import glob
import json
import math
import os
import shutil
import subprocess
import sys
import time

GAME_W, GAME_H = 496, 384
REACH_X, REACH_Y = (34.0, 461.0), (36.0, 357.0)     # the crosshair never leaves these (Yavin, measured)
DEFAULT_TAN = (-0.414, 0.414, -0.314, 0.314)          # Star Wars Trilogy's arcade view, as tangents


def load_scenarios(path):
    data = json.load(open(path, encoding='utf-8'))
    seg = int(data.get('segment', 60))
    pts = data['points']
    for i, p in enumerate(pts):
        p.setdefault('head', [0.0, 0.0, 0.0]); p.setdefault('head_yaw', 0.0)
        p.setdefault('hand', [0.2, -0.3, -0.3]); p.setdefault('grip', 0)
        p['first'], p['last'] = i * seg + 1, (i + 1) * seg
    extra = data.get('extra_captures', [])
    return seg, pts, extra


def fake_script(pts):
    out = []
    for p in pts:
        r = '%d-%d' % (p['first'], p['last'])
        for k, v in (('head.x', p['head'][0]), ('head.y', p['head'][1]), ('head.z', p['head'][2]),
                     ('head.yaw', p['head_yaw']), ('hand.x', p['hand'][0]), ('hand.y', p['hand'][1]),
                     ('hand.z', p['hand'][2]), ('aim.yaw', p['aim'][0]), ('aim.pitch', p['aim'][1])):
            out.append('%s:%s:%g' % (r, k, v))
        if p['grip']:
            out.append('%s:grip.r:1' % r)
    return ';'.join(out)


def cmd_run(a):
    seg, pts, extra = load_scenarios(a.scenarios)
    os.makedirs(a.out, exist_ok=True)
    for f in glob.glob(os.path.join(a.out, '*.bmp')) + glob.glob(os.path.join(a.out, 'eye-*.png')):
        os.remove(f)
    pristine = os.path.join(a.work, 'NVRAM.pristine')
    if os.path.isdir(pristine):
        for f in os.listdir(pristine):
            shutil.copyfile(os.path.join(pristine, f), os.path.join(a.work, 'NVRAM', f))
    env = {k: v for k, v in os.environ.items() if not k.startswith('ACVR_') and not k.startswith('XR_')}
    frames = sorted(set([p['last'] for p in pts] + list(extra)))
    env.update({'XR_RUNTIME_JSON': os.path.join(a.work, 'no-openxr-runtime.json'),   # does not exist: no runtime
                'ACVR_HIDDEN': '1', 'ACVR_WORLD_SCALE': '1',
                'ACVR_HUD_DISTANCE': '1.6', 'ACVR_HUD_WIDTH': '1.6',
                'ACVR_CAPTURE_FRAME': ','.join(str(f) for f in frames), 'ACVR_CAPTURE_DIR': os.path.abspath(a.out),
                'ACVR_FAKE_CONTROLLERS': fake_script(pts)})
    if a.screen:   # the Hub's screen view, without a headset: the convergence the presenter would set
        env.update({'ACVR_STEREO': '1', 'ACVR_EYE_SEP': '0.065', 'ACVR_CONVERGENCE': '2.5'})
    else:
        env['ACVR_VR_FAKE'] = '1'
    for kv in a.env or []:
        k, v = kv.split('=', 1)
        if k in ('ACVR_XR', 'ACVR_VR'):
            sys.exit('ACVR_XR and ACVR_VR are not allowed in offline runs')
        env[k] = v
    rw, rh = (a.res, a.res * 3 // 4) if a.screen else (a.res, a.res)
    args = [a.exe, a.game, '-window', '-res=%d,%d' % (rw, rh), '-no-sound', '-no-dsb', '-input-system=sdl',
            '-no-vsync', '-no-throttle', '-stretch', '-ss=2', '-load-state=' + a.state]
    with open(os.path.join(a.out, 'run.json'), 'w', encoding='utf-8') as f:
        json.dump({'args': args[1:], 'env': {k: v for k, v in env.items() if k.startswith(('ACVR_', 'XR_'))}}, f, indent=1)
    flags = 0x00004000 | 0x08000000 if os.name == 'nt' else 0      # below normal priority, no console window
    t0 = time.time()
    with open(os.path.join(a.out, 'stdout.txt'), 'w') as so, open(os.path.join(a.out, 'stderr.txt'), 'w') as se:
        p = subprocess.Popen(args, cwd=a.work, env=env, stdout=so, stderr=se, creationflags=flags)
        try:
            rc = p.wait(timeout=a.timeout)
        except subprocess.TimeoutExpired:
            p.kill(); p.wait(); rc = 'timeout'
    print('emulator exit %s after %.0f s' % (rc, time.time() - t0))
    from PIL import Image
    for b in sorted(glob.glob(os.path.join(a.out, '*.bmp'))):
        Image.open(b).convert('RGB').save(b[:-4] + '.png'); os.remove(b)
    print('pictures:', len(glob.glob(os.path.join(a.out, 'eye-left-f*.png'))), 'of', len(frames))


# ---------------------------------------------------------------- measuring
def ray_dir(yaw_deg, pitch_deg):
    y, p = math.radians(yaw_deg), math.radians(pitch_deg)
    return (-math.sin(y) * math.cos(p), math.sin(p), -math.cos(y) * math.cos(p))


def to_game(tx, ty, tan):
    tl, tr, tb, tt = tan
    return (tx - tl) / (tr - tl) * GAME_W, (tt - ty) / (tt - tb) * GAME_H


def expected(p, tan, depth):
    """Where the crosshair belongs: the ray's direction (depth None), or the ray's point at that depth seen from
    the head. Game pixels."""
    d = ray_dir(*p['aim'])
    if depth is None:
        return to_game(d[0] / -d[2], d[1] / -d[2], tan)
    h, e = p['hand'], p['head']
    t = (depth + h[2]) / -d[2]
    q = [h[k] + t * d[k] - e[k] for k in range(3)]
    return to_game(q[0] / -q[2], q[1] / -q[2], tan)


def redmask(img):
    import numpy as np
    a = np.asarray(img.convert('RGB')).astype(int)
    r, g, b = a[:, :, 0], a[:, :, 1], a[:, :, 2]
    # red, or orange while the game shows a lock-on: strong red, little blue, green well below red
    return ((r > 100) & (b < 80) & (g < 0.8 * r) & (r - b > 80)).astype(float)


def find_reticle(png, template, k):
    """The crosshair's centre in eye-picture pixels: the red mask scaled to game pixels (k eye pixels each),
    correlated with the template, peak refined to a fraction of a pixel."""
    import numpy as np
    from PIL import Image
    im = Image.open(png)
    W, H = im.size
    sw, sh = round(W / k), round(H / k)
    kx, ky = W / sw, H / sh                                  # exact eye pixels per scaled pixel
    m = Image.fromarray((redmask(im) * 255).astype(np.uint8)).resize((sw, sh), Image.BOX)
    m = np.asarray(m).astype(float) / 255.0
    T = template
    h, w = m.shape; P = T.shape[0]
    big = np.zeros((h + 2 * P, w + 2 * P)); big[P:P + h, P:P + w] = m
    kern = np.zeros_like(big); kern[:T.shape[0], :T.shape[1]] = T
    s = np.real(np.fft.ifft2(np.fft.fft2(big) * np.conj(np.fft.fft2(kern))))
    iy, ix = np.unravel_index(np.argmax(s), s.shape)

    def sub(c, l, r):
        den = l - 2 * c + r
        return 0.0 if abs(den) < 1e-9 else 0.5 * (l - r) / den
    dx = sub(s[iy, ix], s[iy, ix - 1], s[iy, (ix + 1) % s.shape[1]])
    dy = sub(s[iy, ix], s[iy - 1, ix], s[(iy + 1) % s.shape[0], ix])
    cx, cy = ix + dx + T.shape[1] / 2.0 - P, iy + dy + T.shape[0] / 2.0 - P     # in scaled pixels (edges)
    return cx * kx, cy * ky, float(s[iy, ix] / T.sum()), W, H


def find_dot(png, base=None):
    """The yellow aim dot's centre in eye-picture pixels, or None. base: the same frame from a run with the laser
    off (runs are repeatable), so only what the laser drew counts; the game has yellow things of its own."""
    import numpy as np
    from PIL import Image
    a = np.asarray(Image.open(png).convert('RGB')).astype(int)
    m = (a[:, :, 0] > 150) & (a[:, :, 1] > 130) & (a[:, :, 2] < 120) & (a[:, :, 1] > 0.75 * a[:, :, 0])
    if base and os.path.exists(base):
        b = np.asarray(Image.open(base).convert('RGB')).astype(int)
        m &= np.abs(a - b).sum(axis=2) > 30
    ys, xs = np.nonzero(m)
    if len(xs) < 4:
        return None
    return float(xs.mean()) + 0.5, float(ys.mean()) + 0.5


def eye_to_tan(x, y, W, H, yaw_deg):
    ex, ey = x / W * 2.0 - 1.0, 1.0 - y / H * 2.0          # the made-up headset's eyes see tangents -1..1
    yw = math.radians(yaw_deg)
    X = ex * math.cos(yw) - math.sin(yw)                   # (ex, ey, -1) turned left by the head's yaw
    Z = -ex * math.sin(yw) - math.cos(yw)
    return X / -Z, ey / -Z


def screen_to_dir(x, y, W, H, eye, tan, dist, height):
    """Screen mode: a pixel of one eye's picture is a point on the screen (the whole picture, dist metres ahead of
    the recentred head, sized to the game's field of view); the direction from that eye to it, as tangents."""
    w, h = dist * (tan[1] - tan[0]), dist * (tan[3] - tan[2])
    px, py = -w / 2 + x / W * w, height + h / 2 - y / H * h
    dx, dy, dz = px - eye[0], py - eye[1], -dist - eye[2]
    return dx / -dz, dy / -dz


def cmd_measure(a):
    import numpy as np
    from PIL import Image
    seg, pts, _ = load_scenarios(a.scenarios)
    tan = tuple(float(x) for x in a.tangents.split(',')) if a.tangents else None
    if tan is None:   # the patch prints the arcade view with its aim log
        for line in open(os.path.join(a.out, 'stdout.txt'), errors='replace'):
            if 'arcade view' in line:
                tan = tuple(float(x) for x in line.split('arcade view')[1].strip(' ()\n').split())
        tan = tan or DEFAULT_TAN
    T = np.asarray(Image.open(a.template).convert('L')).astype(float) / 255.0
    rows, disp = [], []
    for p in pts:
        f = p['last']
        L, R = (os.path.join(a.out, 'eye-%s-f%d.png' % (e, f)) for e in ('left', 'right'))
        if not os.path.exists(L):
            rows.append({'name': p['name'], 'missing': True}); continue
        W = Image.open(L).size[0]
        k = W / GAME_W if a.screen else (W / 2.0) * (tan[1] - tan[0]) / GAME_W   # eye pixels per game pixel
        got, row = [], {'name': p['name'], 'frame': f}
        for eye, png in (('left', L), ('right', R)):
            x, y, score, W, H = find_reticle(png, T, k)
            if a.screen:   # the eyes the screen pictures are drawn for: half the separation each side of the head
                e = [p['head'][0] + (-a.half_sep if eye == 'left' else a.half_sep), p['head'][1], p['head'][2]]
                got.append(screen_to_dir(x, y, W, H, e, tan, a.screen_distance, a.screen_height) + (score,))
            else:
                got.append(eye_to_tan(x, y, W, H, p['head_yaw']) + (score,))
            dot = find_dot(png, os.path.join(a.base, os.path.basename(png)) if a.base else None) if a.dot else None
            if dot:
                row['dot_' + eye] = to_game(*eye_to_tan(dot[0], dot[1], W, H, p['head_yaw']), tan)
                row['cross_' + eye] = to_game(got[-1][0], got[-1][1], tan)
        tx, ty = (got[0][0] + got[1][0]) / 2, (got[0][1] + got[1][1]) / 2
        disp.append(got[0][0] - got[1][0])
        mx, my = to_game(tx, ty, tan)
        ex, ey = expected(p, tan, a.depth)
        ppd = GAME_W / math.degrees(2 * math.atan(tan[1]))    # game pixels per degree near the centre
        reach = REACH_X[0] <= ex <= REACH_X[1] and REACH_Y[0] <= ey <= REACH_Y[1]
        row.update({'pose': {k2: p[k2] for k2 in ('head', 'head_yaw', 'hand', 'aim', 'grip')},
                    'expected_px': [round(ex, 1), round(ey, 1)], 'crosshair_px': [round(mx, 1), round(my, 1)],
                    'error_px': [round(mx - ex, 1), round(my - ey, 1)],
                    'error_deg': round(math.hypot(mx - ex, my - ey) / ppd, 2),
                    'in_reach': reach, 'match': [round(got[0][2], 2), round(got[1][2], 2)],
                    'disparity_tan': round(got[0][0] - got[1][0], 5)})
        if a.dot and 'dot_left' in row and 'dot_right' in row:
            gaps = [math.hypot(row['dot_' + e][0] - row['cross_' + e][0], row['dot_' + e][1] - row['cross_' + e][1]) for e in ('left', 'right')]
            row['dot_gap_px'] = [round(g, 1) for g in gaps]
            for e in ('left', 'right', ):
                row['dot_' + e] = [round(v, 1) for v in row['dot_' + e]]; row['cross_' + e] = [round(v, 1) for v in row['cross_' + e]]
        rows.append(row)
        print('%-22s expected %6.1f %6.1f  crosshair %6.1f %6.1f  error %+6.1f %+6.1f px (%.2f deg)%s%s' % (
            p['name'], ex, ey, mx, my, mx - ex, my - ey, row['error_deg'], '' if reach else '  [out of reach]',
            ('  dot gap L %.1f R %.1f px' % tuple(row['dot_gap_px'])) if 'dot_gap_px' in row else ''))
    good = [r for r in rows if 'error_px' in r and r['in_reach']]
    errs = [math.hypot(*r['error_px']) for r in good]
    md = float(np.median(disp)) if disp else 0.0
    summary = {'points': len(rows), 'in_reach': len(good),
               'max_error_px': round(max(errs), 1) if errs else None,
               'mean_error_px': round(sum(errs) / len(errs), 1) if errs else None,
               'max_error_deg': max(r['error_deg'] for r in good) if good else None,
               'median_disparity_tan': round(md, 5),
               'crosshair_depth_m_from_disparity': (round(0.064 / md, 1) if md > 1e-5 else 'too far to measure'),
               'arcade_tangents': tan, 'expected_depth_m': a.depth or 'infinity'}
    gaps = [g for r in rows for g in r.get('dot_gap_px', [])]
    if gaps:
        summary['max_dot_gap_px'] = round(max(gaps), 1)
    print(json.dumps(summary))
    json.dump({'summary': summary, 'points': rows}, open(os.path.join(a.out, a.result), 'w'), indent=1)


def cmd_template(a):
    import numpy as np
    from PIL import Image
    m = redmask(Image.open(a.png)); x, y = a.x, a.y
    win = m[y - 40:y + 40, x - 40:x + 40]; ys, xs = np.nonzero(win)
    cx, cy = x - 40 + (xs.min() + xs.max()) // 2, y - 40 + (ys.min() + ys.max()) // 2
    Image.fromarray((m[cy - 36:cy + 36, cx - 36:cx + 36] * 255).astype(np.uint8)).save(a.out_png)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest='cmd', required=True)
    r = sp.add_parser('run')
    for n in ('exe', 'game', 'state', 'work', 'out', 'scenarios'):
        r.add_argument('--' + n, required=True)
    r.add_argument('--env', action='append'); r.add_argument('--res', type=int, default=900)
    r.add_argument('--timeout', type=int, default=240)
    r.add_argument('--screen', action='store_true', help="screen mode (the Hub's Screen view) instead of head-tracked")
    m = sp.add_parser('measure')
    m.add_argument('--out', required=True); m.add_argument('--scenarios', required=True)
    m.add_argument('--template', required=True); m.add_argument('--tangents', default='')
    m.add_argument('--depth', type=float, default=None, help='crosshair depth in metres (default: infinity)')
    m.add_argument('--dot', action='store_true', help='also find the yellow aim dot (runs with the laser on)')
    m.add_argument('--screen', action='store_true', help='screen mode: the eye pictures are the screen itself')
    m.add_argument('--screen-distance', type=float, default=2.5); m.add_argument('--screen-height', type=float, default=0.0)
    m.add_argument('--half-sep', type=float, default=0.0325, help='screen mode: half the eye separation, metres')
    m.add_argument('--base', default='', help='with --dot: a run of the same points with the laser off')
    m.add_argument('--result', default='aim-result.json')
    t = sp.add_parser('template')
    t.add_argument('png'); t.add_argument('x', type=int); t.add_argument('y', type=int); t.add_argument('out_png')
    a = ap.parse_args()
    {'run': cmd_run, 'measure': cmd_measure, 'template': cmd_template}[a.cmd](a)


if __name__ == '__main__':
    main()
