// SPDX-License-Identifier: MIT
#pragma once
// Draws the U4 hold ring (hold_ring.hpp) into the framebuffer that is bound for drawing, with no shaders, no
// textures and no loader: each bead is a scissored glClear. Only OpenGL 1.x entry points are used (glClear,
// glScissor, glClearColor, glColorMask and the state queries), which every host already links, so the same call
// works in a core-profile eye renderer, the Model 2 quad-layer copy and the MAME gun viewer.
//
// Include it AFTER your own GL declarations (windows.h with GL/gl.h, a loader header, ...). It uses the names
// GLint, GLfloat, GLboolean and GL_SCISSOR_TEST etc. as they come from there.
//
// Colours are display values: the helper switches GL_FRAMEBUFFER_SRGB off while it clears, so the numbers land in the
// image as written whether the target is an sRGB or a plain RGBA8 image (an sRGB image stores them as encoded values).
// The caller's scissor, clear colour, colour mask and sRGB state are put back.
// windows.h defines min and max as macros unless NOMINMAX is set, which breaks std::min and std::max; emulator
// sources often include it first, so they are set aside for this header and put back at its end.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "hold_ring.hpp"

#ifndef GL_FRAMEBUFFER_SRGB
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#endif

namespace acvr {

// cx, cy, radius in pixels of the bound drawing target (y up). fb_w, fb_h: its size, so nothing is cleared outside it.
inline void draw_hold_ring_gl(const HoldRingView &view, float cx, float cy, float radius, int fb_w, int fb_h) {
    if (!view.visible || fb_w <= 0 || fb_h <= 0) return;
    const HoldRingRects rects = hold_ring_rects(view, cx, cy, radius);
    if (!rects.n) return;
    const GLboolean was_scissor = glIsEnabled(GL_SCISSOR_TEST), was_srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    GLint box[4]{};
    GLfloat clear[4]{};
    GLboolean mask[4]{};
    glGetIntegerv(GL_SCISSOR_BOX, box);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    glColorMask(1, 1, 1, 1);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_SCISSOR_TEST);
    for (int i = 0; i < rects.n; ++i) {
        const HoldRingRect &q = rects.r[i];
        int x0 = int(std::floor(q.x + .5f)), y0 = int(std::floor(q.y + .5f));
        int x1 = int(std::floor(q.x + q.w + .5f)), y1 = int(std::floor(q.y + q.h + .5f));
        x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(fb_w, x1); y1 = std::min(fb_h, y1);
        if (x1 <= x0 || y1 <= y0) continue;
        glScissor(x0, y0, x1 - x0, y1 - y0);
        glClearColor(q.rgb[0], q.rgb[1], q.rgb[2], 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glScissor(box[0], box[1], box[2], box[3]);
    glClearColor(clear[0], clear[1], clear[2], clear[3]);
    glColorMask(mask[0], mask[1], mask[2], mask[3]);
    if (!was_scissor) glDisable(GL_SCISSOR_TEST);
    if (was_srgb) glEnable(GL_FRAMEBUFFER_SRGB);
}

}  // namespace acvr

#pragma pop_macro("max")
#pragma pop_macro("min")
