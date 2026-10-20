// SPDX-License-Identifier: GPL-3.0-or-later
// Stereo for a prebuilt libretro hardware-OpenGL core, without its source: the host owns the core's OpenGL
// entry points (get_proc_address, and the core's import table for the OpenGL 1.1 calls it links directly),
// so it can see and steer what the core draws. See GlStereo.cpp.
#pragma once
#include "WinGL.h"
#include "retro_abi.h"
#include <string>

namespace glstereo {
// Reads ACVR_GL_TRACE / ACVR_GL_STEREO* from the environment; logs through `log`.
void Configure(void (*log)(const char *fmt, ...));
bool Tracing();
bool StereoOn();
// Patches the core module's imports of opengl32.dll that this module wraps. Call after LoadLibrary.
void InstallImports(HMODULE core);
// The proc address the core gets for `sym`: a wrapper where one exists, else the real entry point.
retro_proc_address_t Wrap(const char *sym, retro_proc_address_t real);
// The host's framebuffer that the core draws its final picture into (left eye), and its size.
void SetHostFramebuffer(unsigned fbo, unsigned w, unsigned h, bool depth, bool stencil);
// Called once per emulated frame after retro_run.
void EndFrame(unsigned frame);
// The right eye's picture (same size as the host framebuffer), or 0 when stereo is off.
unsigned RightFramebuffer();
// The w now on the screen plane (automatic or set).
float ZeroPlane();
}
