"""Guarded local overlay, never modifies the read-only upstream reference.

Technique reference: DR-89/time-crisis-vr tools/patch_upstream.py. This patch
captures raw XYZ instead of recovering it from quantized raster coordinates.
Native geometry, sorting, textures, shading and rasterization remain upstream.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

PIN = 'a9f1a0a1f9de62387a7138006fa36933fd477123'

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    if '.local' not in output.parts or output.exists() or source == output:
        raise RuntimeError('Output must be fresh under .local')
    head = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if head != PIN:
        raise RuntimeError('Wrong upstream pin')
    manifest = json.loads(Path(__file__).with_name('source-hashes.json').read_text())
    contents = {}
    for name, digest in manifest.items():
        raw = (source / name).read_bytes().replace(b'\r\n', b'\n')
        if hashlib.sha256(raw).hexdigest() != digest:
            raise RuntimeError('Source drift: ' + name)
        contents[name] = raw.decode('utf-8')
    def edit(name, old, new):
        if contents[name].count(old) != 1:
            raise RuntimeError('Anchor drift: ' + name + ': ' + old[:64])
        contents[name] = contents[name].replace(old, new)
    edit('engine/geo_hw.h', '    int32_t  uf, vf, bf;',
         '    int32_t  uf, vf, bf;\n    double acn_view[3]; /* ACN22 raw board view XYZ, before projection */')
    edit('engine/geo_hw.c', '        if (vz[i] <= 0) { q.v[i].valid = 0;',
         '        q.v[i].acn_view[0] = vx[i]; q.v[i].acn_view[1] = vy[i]; q.v[i].acn_view[2] = vz[i];\n        if (vz[i] <= 0) { q.v[i].valid = 0;')
    edit('engine/geo_hw.c', '            q.rv[i].z    = (int32_t)cz_[i];',
         '            q.rv[i].z    = (int32_t)cz_[i];\n            q.rv[i].acn_view[0] = cx_[i]; q.rv[i].acn_view[1] = cy_[i]; q.rv[i].acn_view[2] = cz_[i];')
    edit('engine/geo_hw.c', '                    d->z  = (int32_t)llround(Z);',
         '                    d->z  = (int32_t)llround(Z);\n                    d->acn_view[0] = P[i][0]; d->acn_view[1] = P[i][1]; d->acn_view[2] = Z;')
    edit('engine/ss22_gl.c', '#include "hud_edges.h"', '#include "hud_edges.h"\n#include "eye.h"')
    edit('engine/ss22_gl.c', '    fog_load_regs(r->mixer, r->czattr, r->czram);',
         '    acn22_latch_regs(r);\n    fog_load_regs(r->mixer, r->czattr, r->czram);')
    edit('engine/ss22_gl.c', '    qn = 0; qorder = 0;', '    acn22_end_eye();\n    qn = 0; qorder = 0;')
    edit('engine/ss22_gl.c', 'int ss22_quads(void)   { return qn; }',
         'int ss22_quads(void)   { return qn; }\nconst geo_quad *acn22_prepared(int *count) { *count = qn; return qbuf; }')
    edit('engine/ss22_gl.c', '    glViewport(0, 0, vw, vh);',
         '    glViewport(acn22_eye ? acn22_eye->rect_x : 0, acn22_eye ? acn22_eye->rect_y : 0, vw, vh);')
    edit('engine/ss22_gl.c', '    glClear(GL_COLOR_BUFFER_BIT);',
         '    if (acn22_eye) { glEnable(GL_SCISSOR_TEST); glScissor(acn22_eye->rect_x, acn22_eye->rect_y, vw, vh); }\n    glClear(GL_COLOR_BUFFER_BIT);\n    glDisable(GL_SCISSOR_TEST);')
    edit('engine/ss22_gl.c', '    cfg.wide_backdrop = 1;', '    cfg.wide_backdrop = acn22_eye ? 0 : 1;')
    edit('engine/ss22_gl.c', '        else eng_draw_quad(&qbuf[qi++], &cfg);',
         '        else acn22_draw_quad(&qbuf[qi++], &cfg);')
    edit('engine/ss22_gl.c', '    draw_text();', '    if (!acn22_eye) draw_text(); /* fixture world-only gate */')
    edit('engine/ss22_gl.c', '    if (g_fog_valid && g_fog.have_gamma) eng_post_lut(g_fog.gamma, vw, vh);',
         '    if (!acn22_eye && g_fog_valid && g_fog.have_gamma) eng_post_lut(g_fog.gamma, vw, vh);')
    edit('engine/ss22_gl.c', 'static SDL_Window   *hl_win;',
         '#ifndef ACN22_SYNTHETIC_SLICE /* fixture creates its real SDL context */\nstatic SDL_Window   *hl_win;')
    edit('engine/ss22_gl.c', 'bool eng_gl_write_ppm(const char *path, int vw, int vh)',
         '#endif /* upstream headless helper excluded only in synthetic slice */\n\nbool eng_gl_write_ppm(const char *path, int vw, int vh)')
    for name, text in contents.items():
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8', newline='\n')
    print('Verified pinned generic sources; wrote fresh guarded overlay')

if __name__ == '__main__':
    main()
