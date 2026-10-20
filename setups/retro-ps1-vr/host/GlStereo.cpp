// SPDX-License-Identifier: GPL-3.0-or-later
// Layers for a prebuilt libretro hardware-OpenGL core (first user: mupen64plus-next with GLideN64), with no
// change to the core. The host hands the core every OpenGL entry point: newer ones through get_proc_address,
// the OpenGL 1.1 ones through the core's import table, which this module patches in the loaded module.
//
// Extra pictures ("eyes") beside the core's own (the left eye):
//   right eye (ACVR_GL_STEREO=1)  the 3D world seen from the right eye
//   HUD layer (ACVR_GL_HUD=1)     the game's 2D overlay alone (HUD, text, menus) on transparent black, for its
//                                 own steady panel; the world pictures then carry no HUD
// Every framebuffer the core draws into gets a twin per extra picture (same size and format). Each draw, clear,
// blit and framebuffer-to-texture copy runs again for each extra picture, into the twins, with every bound texture
// that is itself a render target swapped for its twin, so the core's own final copy to the host makes each
// picture. Readbacks to the emulated console stay on the left eye.
//
// Vertex shaders get a short tail (GLideN64's shaders: aPosition with clip-space w and aModify, or aRectPosition):
// - 3D (perspective) vertices: wide view (ACVR_GL_WIDE=kx,ky: the projection squeezed about the picture's centre,
//   so the game's geometry past the old edges comes into the picture; the presenter stretches it back on a wider
//   surface), then the stereo rule of our SwanStation patch: x += eye*shift*clamp(1-conv/w,-popout,1)*w.
// - 2D vertices (rectangles, screen-space, orthographic w = 1): drawn into the 3D picture before its first
//   depth-tested draw of the frame they are a backdrop (sky, background: to infinity, part of the world);
//   otherwise they are HUD: on the screen plane, or with the HUD layer on, only in the HUD layer.
// Fragment shaders get a guard that discards HUD fragments from world pictures and world fragments from the HUD
// layer. The HUD layer is drawn over black and over white, which gives its alpha whatever way the core blends.
// The shaders also count clip w into a 64-bin log histogram; the zero plane can follow a percentile of it.
//
// Trace mode (ACVR_GL_TRACE=<first frame>,<frames>) logs what the core draws in those frames and saves every
// vertex shader it compiles (ACVR_GL_TRACE_DIR).
#include "GlStereo.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_SHADER_STORAGE_BUFFER_BINDING
#define GL_SHADER_STORAGE_BUFFER_BINDING 0x90D3
#endif
#ifndef GL_SRC1_COLOR
#define GL_SRC1_COLOR 0x88F9
#define GL_ONE_MINUS_SRC1_COLOR 0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA 0x88FB
#endif
#ifndef GL_SRC1_ALPHA
#define GL_SRC1_ALPHA 0x8589
#endif

namespace glstereo {
namespace {
void (*s_log)(const char *, ...) = nullptr;
bool s_trace = false, s_stereo = false, s_hud = false, s_wide = false;
unsigned s_traceFirst = 0, s_traceCount = 0, s_frame = 0;
std::string s_traceDir;
bool TraceNow() { return s_trace && s_frame >= s_traceFirst && s_frame < s_traceFirst + s_traceCount; }
bool Active() { return s_stereo || s_hud || s_wide; }
float EnvF(const char *n, float d) { const char *v = std::getenv(n); return v && *v ? (float)std::atof(v) : d; }

// The HUD layer is drawn twice, over black and over white: whatever way the core blends (GL blending or in its
// shaders), alpha = 1 - (over white - over black) and the colour over black is premultiplied.
constexpr int kRight = 1, kHud = 2, kHudWhite = 3, kEyes = 4;
bool HudEye(int e) { return e == kHud || e == kHudWhite; }
float s_shift = .0267f, s_conv = -1, s_auto = .3f, s_pop = 2, s_convNow = 0, s_wideX = 1, s_wideY = 1;
bool s_convAuto = true, s_stats = false;
bool s_squeeze = true;   // ACVR_GL_WIDE_SQUEEZE=0: the game itself draws the wide view (a widescreen code); only the presenter stretches
constexpr int kBins = 64, kHistBinding = 13;
float BinW(int bin) { return std::exp2(bin / 2.f - 8.f); }   // lower edge of a bin: log2(w) = bin/2 - 8

// ---- real entry points
#define REAL_PROCS(X) \
  X(PFNGLSHADERSOURCEPROC, ShaderSource) X(PFNGLCREATESHADERPROC, CreateShader) X(PFNGLLINKPROGRAMPROC, LinkProgram) \
  X(PFNGLUSEPROGRAMPROC, UseProgram) X(PFNGLBINDFRAMEBUFFERPROC, BindFramebuffer) X(PFNGLBLITFRAMEBUFFERPROC, BlitFramebuffer) \
  X(PFNGLFRAMEBUFFERTEXTURE2DPROC, FramebufferTexture2D) X(PFNGLFRAMEBUFFERTEXTUREPROC, FramebufferTexture) \
  X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, FramebufferRenderbuffer) X(PFNGLACTIVETEXTUREPROC, ActiveTexture) \
  X(PFNGLDRAWELEMENTSBASEVERTEXPROC, DrawElementsBaseVertex) X(PFNGLDRAWRANGEELEMENTSPROC, DrawRangeElements) \
  X(PFNGLDRAWARRAYSINSTANCEDPROC, DrawArraysInstanced) \
  X(PFNGLDELETEFRAMEBUFFERSPROC, DeleteFramebuffers) X(PFNGLBLENDFUNCSEPARATEPROC, BlendFuncSeparate)
#define X(type, name) type r_##name = nullptr;
REAL_PROCS(X)
#undef X
void (WINAPI *r_DrawArrays)(GLenum, GLint, GLsizei) = nullptr;
void (WINAPI *r_DrawElements)(GLenum, GLsizei, GLenum, const void *) = nullptr;
void (WINAPI *r_Clear)(GLbitfield) = nullptr;
void (WINAPI *r_Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
void (WINAPI *r_Scissor)(GLint, GLint, GLsizei, GLsizei) = nullptr;
void (WINAPI *r_ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *) = nullptr;
void (WINAPI *r_BindTexture)(GLenum, GLuint) = nullptr;
void (WINAPI *r_DeleteTextures)(GLsizei, const GLuint *) = nullptr;
void (WINAPI *r_CopyTexSubImage2D)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei) = nullptr;
void (WINAPI *r_Enable)(GLenum) = nullptr;
void (WINAPI *r_Disable)(GLenum) = nullptr;
void (WINAPI *r_BlendFunc)(GLenum, GLenum) = nullptr;

// Our own helpers (resolved on first use, with the core's context current).
#define OWN_PROCS(X) \
  X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) X(PFNGLUNIFORM1FPROC, Uniform1f) X(PFNGLUNIFORM2FPROC, Uniform2f) \
  X(PFNGLUNIFORM4FPROC, Uniform4f) \
  X(PFNGLCREATEFRAMEBUFFERSPROC, CreateFramebuffers) X(PFNGLNAMEDFRAMEBUFFERTEXTUREPROC, NamedFramebufferTexture) \
  X(PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC, NamedFramebufferRenderbuffer) X(PFNGLCREATETEXTURESPROC, CreateTextures) \
  X(PFNGLTEXTURESTORAGE2DPROC, TextureStorage2D) X(PFNGLGETTEXTURELEVELPARAMETERIVPROC, GetTextureLevelParameteriv) \
  X(PFNGLGETTEXTUREPARAMETERIVPROC, GetTextureParameteriv) X(PFNGLTEXTUREPARAMETERIPROC, TextureParameteri) \
  X(PFNGLCREATERENDERBUFFERSPROC, CreateRenderbuffers) X(PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC, NamedRenderbufferStorageMultisample) \
  X(PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC, GetNamedRenderbufferParameteriv) X(PFNGLCREATEBUFFERSPROC, CreateBuffers) \
  X(PFNGLNAMEDBUFFERDATAPROC, NamedBufferData) X(PFNGLGETNAMEDBUFFERSUBDATAPROC, GetNamedBufferSubData) \
  X(PFNGLCLEARNAMEDBUFFERDATAPROC, ClearNamedBufferData) X(PFNGLBINDBUFFERBASEPROC, BindBufferBase) \
  X(PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC, CheckNamedFramebufferStatus) X(PFNGLGETPROGRAMIVPROC, GetProgramiv) \
  X(PFNGLCLEARNAMEDFRAMEBUFFERFVPROC, ClearNamedFramebufferfv) X(PFNGLBLENDFUNCSEPARATEPROC, BlendFuncSeparate)   X(PFNGLCLEARTEXIMAGEPROC, ClearTexImage)
#define X(type, name) type o_##name = nullptr;
OWN_PROCS(X)
#undef X
bool s_ownLoaded = false, s_ownOk = false;
bool LoadOwn()
{
  if (s_ownLoaded) return s_ownOk;
  s_ownLoaded = true; s_ownOk = true;
#define X(type, name) o_##name = reinterpret_cast<type>(wglGetProcAddress("gl" #name)); if (!o_##name) { s_log("GL layers: no gl%s", #name); s_ownOk = false; }
  OWN_PROCS(X)
#undef X
  return s_ownOk;
}

// ---- tracked core state
GLuint s_drawFbo = 0, s_readFbo = 0, s_program = 0, s_activeUnit = 0;
GLint s_vp[4] = { 0, 0, 0, 0 };
std::map<GLuint, GLenum> s_shaderType;
std::map<GLuint, std::map<GLuint, GLuint>> s_unitTex;            // unit -> target -> texture
struct Attach { bool renderbuffer = false; GLuint name = 0; GLint level = 0; };
std::map<GLuint, std::map<GLenum, Attach>> s_fboAttach;          // fbo -> attachment point -> object
unsigned s_hostFbo = 0, s_hostW = 0, s_hostH = 0;
bool s_depthTest = false, s_blend = false, s_scissor = false;
GLenum s_blendSrcRgb = GL_ONE, s_blendDstRgb = GL_ZERO, s_blendSrcA = GL_ONE, s_blendDstA = GL_ZERO;

// ---- layer objects
struct Program { GLint shift = -1, conv = -1, mode = -1, wide = -1, centre = -1; };
std::map<GLuint, Program> s_programs;
std::map<GLuint, GLuint> s_texTwin[kEyes], s_rbTwin[kEyes];      // core object -> twin, per extra picture
struct TexSize { GLint w = 0, h = 0, format = 0; };
std::map<GLuint, TexSize> s_texSize;
std::set<GLuint> s_renderTargets;                                // core textures used as attachments
struct TwinFbo { GLuint fbo = 0; std::map<GLenum, Attach> attached; };
std::map<GLuint, TwinFbo> s_fboTwin[kEyes];
std::set<GLuint> s_hudCleared;                                   // 3D framebuffers whose HUD twin is cleared this frame
GLuint s_hist = 0;
unsigned s_dupDraws = 0, s_dupClears = 0, s_dupBlits = 0, s_twinFails = 0, s_bgDraws = 0, s_hudDraws = 0;
std::vector<unsigned> s_histo(kBins, 0);

struct FrameStats { std::map<GLuint, unsigned> draws, clears; unsigned blits = 0, reads = 0; std::set<std::string> viewports; } s_tstats;
unsigned s_vertexShaders = 0, s_patchedShaders = 0, s_patchedFragment = 0;

std::vector<int> Extra()
{
  std::vector<int> e;
  if (s_stereo) e.push_back(kRight);
  if (s_hud) { e.push_back(kHud); e.push_back(kHudWhite); }
  return e;
}

const TexSize &SizeOf(GLuint tex)
{
  auto it = s_texSize.find(tex); if (it != s_texSize.end()) return it->second;
  TexSize s;
  o_GetTextureLevelParameteriv(tex, 0, GL_TEXTURE_WIDTH, &s.w); o_GetTextureLevelParameteriv(tex, 0, GL_TEXTURE_HEIGHT, &s.h);
  o_GetTextureLevelParameteriv(tex, 0, GL_TEXTURE_INTERNAL_FORMAT, &s.format);
  return s_texSize[tex] = s;
}

// ---- twins
GLuint TwinTexture(GLuint tex, int eye)
{
  auto &map = s_texTwin[eye];
  auto it = map.find(tex); if (it != map.end()) return it->second;
  const TexSize &s = SizeOf(tex);
  GLuint t = 0;
  if (s.w > 0 && s.h > 0 && s.format) {
    o_CreateTextures(GL_TEXTURE_2D, 1, &t); o_TextureStorage2D(t, 1, (GLenum)s.format, s.w, s.h);
    for (GLenum p : { GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER, GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T }) { GLint v = 0; o_GetTextureParameteriv(tex, p, &v); o_TextureParameteri(t, p, v); }
    // A HUD layer starts empty everywhere, also where the core never clears (borders outside its scissor).
    if (eye == kHud || eye == kHudWhite) { const float c[4] = { eye == kHudWhite ? 1.f : 0.f, eye == kHudWhite ? 1.f : 0.f, eye == kHudWhite ? 1.f : 0.f, eye == kHudWhite ? 1.f : 0.f }; o_ClearTexImage(t, 0, GL_RGBA, GL_FLOAT, c); }
    if (map.size() < 32) s_log("GL layers: %s twin of texture %u: %dx%d format 0x%X", HudEye(eye) ? "HUD" : "right-eye", tex, s.w, s.h, s.format);
  } else { ++s_twinFails; s_log("GL layers: texture %u has no size (%dx%d, 0x%X): no twin", tex, s.w, s.h, s.format); }
  map[tex] = t; return t;
}
GLuint TwinRenderbuffer(GLuint rb, int eye)
{
  auto &map = s_rbTwin[eye];
  auto it = map.find(rb); if (it != map.end()) return it->second;
  GLint w = 0, h = 0, fmt = 0, samples = 0;
  o_GetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_WIDTH, &w); o_GetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_HEIGHT, &h);
  o_GetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_INTERNAL_FORMAT, &fmt); o_GetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_SAMPLES, &samples);
  GLuint r = 0;
  if (w > 0 && h > 0) { o_CreateRenderbuffers(1, &r); o_NamedRenderbufferStorageMultisample(r, samples, (GLenum)fmt, w, h); }
  map[rb] = r; return r;
}
bool ColourPoint(GLenum a) { return a >= GL_COLOR_ATTACHMENT0 && a <= GL_COLOR_ATTACHMENT0 + 15; }
// The twin of a core framebuffer, its attachments kept in step with the original's. The HUD layer has no depth:
// its draws always pass the depth test.
GLuint TwinFramebuffer(GLuint fbo, int eye)
{
  if (!fbo) return 0;
  auto &twin = s_fboTwin[eye][fbo];
  const auto &want = s_fboAttach[fbo];
  if (!twin.fbo) o_CreateFramebuffers(1, &twin.fbo);
  bool changed = false;
  for (const auto &a : want) {
    if (HudEye(eye) && !ColourPoint(a.first)) continue;
    auto have = twin.attached.find(a.first);
    if (have != twin.attached.end() && have->second.name == a.second.name && have->second.renderbuffer == a.second.renderbuffer && have->second.level == a.second.level) continue;
    if (!a.second.name) o_NamedFramebufferTexture(twin.fbo, a.first, 0, 0);
    else if (a.second.renderbuffer) o_NamedFramebufferRenderbuffer(twin.fbo, a.first, GL_RENDERBUFFER, TwinRenderbuffer(a.second.name, eye));
    else o_NamedFramebufferTexture(twin.fbo, a.first, TwinTexture(a.second.name, eye), a.second.level);
    twin.attached[a.first] = a.second; changed = true;
  }
  if (changed) {
    const GLenum status = o_CheckNamedFramebufferStatus(twin.fbo, GL_DRAW_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) { ++s_twinFails; s_log("GL layers: twin of framebuffer %u incomplete (0x%X)", fbo, status); }
  }
  return twin.fbo;
}
bool Known(GLuint fbo) { return Active() && s_ownOk && fbo && s_fboAttach.count(fbo); }

// Swap every bound render-target texture for its twin, and back.
struct Swap { GLuint unit, target, tex; };
std::vector<Swap> s_swapped;
void SwapTextures(int eye)
{
  s_swapped.clear();
  for (const auto &u : s_unitTex) for (const auto &t : u.second) {
    if (!t.second || !s_renderTargets.count(t.second)) continue;
    const GLuint twin = TwinTexture(t.second, eye); if (!twin) continue;
    r_ActiveTexture(GL_TEXTURE0 + u.first); r_BindTexture(t.first, twin);
    s_swapped.push_back({ u.first, t.first, t.second });
  }
  if (!s_swapped.empty()) r_ActiveTexture(GL_TEXTURE0 + s_activeUnit);
}
void RestoreTextures()
{
  if (s_swapped.empty()) return;
  for (const auto &s : s_swapped) { r_ActiveTexture(GL_TEXTURE0 + s.unit); r_BindTexture(s.target, s.tex); }
  r_ActiveTexture(GL_TEXTURE0 + s_activeUnit); s_swapped.clear();
}

// Backdrop tracking: per framebuffer, whether a depth-tested draw happened this frame. Only framebuffers that do
// get depth-tested draws (the game's 3D picture) have backdrops and HUD; the copies that make the final picture
// never do.
bool s_backdrop = true;
std::set<GLuint> s_fboHas3D, s_fbo3DEver;
bool Is3D() { return s_fbo3DEver.count(s_drawFbo) != 0; }
bool Backdrop() { return Is3D() && !s_fboHas3D.count(s_drawFbo); }

// The picture's colour texture size and where it sits in clip space under the current viewport (GLideN64 draws
// its frame buffer with a viewport larger than the texture).
void PictureInClip(float &perPictureX, float &centreX, float &centreY)
{
  perPictureX = 1; centreX = 0; centreY = 0;
  const auto a = s_fboAttach.find(s_drawFbo);
  if (a == s_fboAttach.end() || s_vp[2] <= 0 || s_vp[3] <= 0) return;
  const auto c = a->second.find(GL_COLOR_ATTACHMENT0);
  if (c == a->second.end() || c->second.renderbuffer || !c->second.name) return;
  const TexSize &s = SizeOf(c->second.name);
  if (s.w <= 0 || s.h <= 0) return;
  perPictureX = (float)s.w / (float)s_vp[2];
  centreX = (float)(s.w - 2 * s_vp[0]) / (float)s_vp[2] - 1.f;
  centreY = (float)(s.h - 2 * s_vp[1]) / (float)s_vp[3] - 1.f;
}

// Uniforms for one picture: 0 left eye (or the only one), kRight, kHud.
void SetPicture(int eye)
{
  auto it = s_programs.find(s_program); if (it == s_programs.end()) return;
  const Program &p = it->second;
  float perPicture, cx, cy; PictureInClip(perPicture, cx, cy);
  const float side = !s_stereo || HudEye(eye) ? 0.f : eye == kRight ? 1.f : -1.f;
  const float shift = side * s_shift * perPicture;
  const bool is3D = Is3D(), backdrop = s_backdrop && Backdrop();
  if (p.shift >= 0) o_Uniform1f(p.shift, shift);
  if (p.conv >= 0) o_Uniform4f(p.conv, s_convNow, s_pop, eye == 0 && s_stereo ? 1.f : 0.f, s_depthTest ? 1.f : 0.f);
  // x: backdrop now, y: split HUD from world in this framebuffer, z: this pass is the HUD layer
  if (p.mode >= 0) o_Uniform4f(p.mode, backdrop ? 1.f : 0.f, s_hud && is3D ? 1.f : 0.f, HudEye(eye) ? 1.f : 0.f, 0.f);
  if (p.wide >= 0) o_Uniform2f(p.wide, is3D && s_squeeze ? s_wideX : 1.f, is3D && s_squeeze ? s_wideY : 1.f);
  if (p.centre >= 0) o_Uniform2f(p.centre, cx, cy);
  if (backdrop && eye == 0) ++s_bgDraws;
}

const float kHudClear[2][4] = { { 0, 0, 0, 0 }, { 1, 1, 1, 1 } };
void ClearHudTwins(GLuint fbo)
{
  if (s_scissor) r_Disable(GL_SCISSOR_TEST);
  for (int e : { kHud, kHudWhite }) if (const GLuint twin = TwinFramebuffer(fbo, e)) o_ClearNamedFramebufferfv(twin, GL_COLOR, 0, kHudClear[e == kHudWhite]);
  if (s_scissor) r_Enable(GL_SCISSOR_TEST);
}

// Runs `draw` as asked (left eye), then once per extra picture into the twins.
template <class F> void Layers(F draw, bool isClear = false, GLbitfield clearMask = 0)
{
  if (!Known(s_drawFbo)) { draw(); return; }
  for (int e : Extra()) TwinFramebuffer(s_drawFbo, e);   // learns sizes, keeps twins in step
  if (s_hud && Is3D() && !s_hudCleared.count(s_drawFbo)) { ClearHudTwins(s_drawFbo); s_hudCleared.insert(s_drawFbo); }
  if (!isClear) SetPicture(0);
  draw();
  for (int e : Extra()) {
    if (HudEye(e) && !isClear && Backdrop()) continue;   // backdrops are never HUD
    const GLuint twin = TwinFramebuffer(s_drawFbo, e); if (!twin) continue;
    r_BindFramebuffer(GL_DRAW_FRAMEBUFFER, twin);
    if (isClear) {
      if (HudEye(e)) {   // the HUD layers clear to their black or white, and have no depth
        if (clearMask & GL_COLOR_BUFFER_BIT) o_ClearNamedFramebufferfv(twin, GL_COLOR, 0, kHudClear[e == kHudWhite]);
      } else r_Clear(clearMask);
    } else {
      SwapTextures(e); SetPicture(e);
      draw();
      if (e == kHud && Is3D()) ++s_hudDraws;
      RestoreTextures();
    }
  }
  if (!Extra().empty()) { r_BindFramebuffer(GL_DRAW_FRAMEBUFFER, s_drawFbo); if (!isClear) SetPicture(0); }
}

// ---- shader patches
const char *kTriangleTail = R"glsl(
uniform float acvrShift;   // eye * half the eyes' separation on the screen, in NDC
uniform vec4 acvrConv;     // w on the screen plane, pop-out limit, 1 when this pass counts w, 1 when depth-tested
uniform vec4 acvrMode;     // backdrop, split HUD from world, HUD pass, HUD alpha mode
uniform vec2 acvrWide;     // projection squeeze for the wide view
uniform vec2 acvrCentre;   // the picture's centre in NDC
layout(std430, binding = 13) buffer AcvrHist { uint acvrHist[64]; };
flat out float acvrHud;
void main()
{
  acvr_main();
  acvrHud = 0.0;
  // Perspective vertices only: an orthographic projection (HUD, sprites through the 3D pipeline) has w exactly 1.
  if (aModify[0] == 0.0 && gl_Position.w > 0.0 && abs(gl_Position.w - 1.0) > 1e-5) {
    if (acvrConv.z > 0.5) atomicAdd(acvrHist[clamp(int((log2(gl_Position.w) + 8.0) * 2.0), 0, 63)], 1u);
    vec2 c = acvrCentre * gl_Position.w;
    gl_Position.xy = c + (gl_Position.xy - c) * acvrWide;
    gl_Position.x += acvrShift * clamp(1.0 - acvrConv.x / gl_Position.w, -acvrConv.y, 1.0) * gl_Position.w;
  // Backdrops, at infinity: 2D before the frame's first 3D draw, and screen-space vertices drawn with the depth
  // test (GoldenEye's sky). Orthographic HUD (w = 1) and later 2D are HUD.
  } else if (acvrMode.x > 0.5 || (aModify[0] != 0.0 && acvrConv.w > 0.5)) gl_Position.x += acvrShift * gl_Position.w;
  else acvrHud = 1.0;
}
)glsl";
// GLideN64's rectangles (backgrounds, fills, text, copies): a backdrop or HUD.
const char *kRectTail = R"glsl(
uniform float acvrShift;
uniform vec4 acvrMode;
flat out float acvrHud;
void main()
{
  acvr_main();
  acvrHud = 1.0 - acvrMode.x;
  gl_Position.x += acvrShift * acvrMode.x * gl_Position.w;
}
)glsl";
const char *kOtherTail = R"glsl(
flat out float acvrHud;
void main() { acvr_main(); acvrHud = 0.0; }
)glsl";
const char *kFragmentHead = R"glsl(
flat in float acvrHud;
uniform vec4 acvrMode;
void main()
{
  if (acvrMode.y > 0.5 && ((acvrHud > 0.5) != (acvrMode.z > 0.5))) discard;
  acvr_fmain();
}
)glsl";

bool Desktop(const std::string &src) { return src.find("#version 3") != std::string::npos || src.find("#version 4") != std::string::npos; }
int PatchVertexShader(std::string &src)   // 0 none, 1 triangles, 2 rectangles, 3 other
{
  if (!Desktop(src)) return 0;
  const bool tri = src.find("aModify") != std::string::npos && src.find("aPosition") != std::string::npos;
  const bool rect = !tri && src.find("aRectPosition") != std::string::npos;
  if (!s_hud && !tri && !rect) return 0;   // without the HUD layer only GLideN64's own shaders need a change
  const auto at = src.find("void main()"); if (at == std::string::npos) return 0;
  src.replace(at, 11, "void acvr_main()");
  src += tri ? kTriangleTail : rect ? kRectTail : kOtherTail;
  return tri ? 1 : rect ? 2 : 3;
}
bool PatchFragmentShader(std::string &src)
{
  if (!s_hud || !Desktop(src) || src.find("fragColor") == std::string::npos) return false;
  const auto at = src.find("void main()"); if (at == std::string::npos) return false;
  src.replace(at, 11, "void acvr_fmain()");
  src += kFragmentHead;
  return true;
}

// ---- wrappers
void WINAPI W_ShaderSource(GLuint shader, GLsizei count, const GLchar *const *strings, const GLint *lengths)
{
  const GLenum type = s_shaderType[shader];
  if ((!Active() && s_traceDir.empty()) || (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER)) { r_ShaderSource(shader, count, strings, lengths); return; }
  std::string src;
  for (GLsizei i = 0; i < count; ++i) src += lengths && lengths[i] >= 0 ? std::string(strings[i], lengths[i]) : std::string(strings[i]);
  bool patched = false;
  if (Active()) {
    if (type == GL_VERTEX_SHADER) { patched = PatchVertexShader(src) != 0; if (patched) ++s_patchedShaders; }
    else { patched = PatchFragmentShader(src); if (patched) ++s_patchedFragment; }
  }
  if (!s_traceDir.empty()) {
    char path[512]; std::snprintf(path, sizeof path, "%s\\%s-%03u%s.glsl", s_traceDir.c_str(), type == GL_VERTEX_SHADER ? "vs" : "fs", s_vertexShaders, patched ? "-patched" : "");
    if (FILE *f = std::fopen(path, "wb")) { std::fwrite(src.data(), 1, src.size(), f); std::fclose(f); }
  }
  ++s_vertexShaders;
  const GLchar *one = src.c_str(); const GLint len = (GLint)src.size();
  r_ShaderSource(shader, 1, &one, &len);
}
GLuint WINAPI W_CreateShader(GLenum type) { const GLuint s = r_CreateShader(type); s_shaderType[s] = type; return s; }
void WINAPI W_LinkProgram(GLuint program)
{
  r_LinkProgram(program);
  if (!Active() || !LoadOwn()) return;
  GLint ok = 0; o_GetProgramiv(program, GL_LINK_STATUS, &ok);
  Program p;
  if (ok) {
    p.shift = o_GetUniformLocation(program, "acvrShift"); p.conv = o_GetUniformLocation(program, "acvrConv");
    p.mode = o_GetUniformLocation(program, "acvrMode"); p.wide = o_GetUniformLocation(program, "acvrWide");
    p.centre = o_GetUniformLocation(program, "acvrCentre");
  } else s_log("GL layers: a program of the core failed to link");
  if (p.shift >= 0 || p.mode >= 0 || p.wide >= 0) s_programs[program] = p; else s_programs.erase(program);
}
void WINAPI W_UseProgram(GLuint p) { s_program = p; r_UseProgram(p); }
void WINAPI W_BindFramebuffer(GLenum target, GLuint fbo)
{
  if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) s_drawFbo = fbo;
  if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER) s_readFbo = fbo;
  r_BindFramebuffer(target, fbo);
}
void Attached(GLenum target, GLenum attachment, bool rb, GLuint name, GLint level)
{
  const GLuint fbo = target == GL_READ_FRAMEBUFFER ? s_readFbo : s_drawFbo;
  if (!fbo) return;
  s_fboAttach[fbo][attachment] = { rb, name, level };
  if (!rb && name) s_renderTargets.insert(name);
}
void WINAPI W_FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{ Attached(target, attachment, false, texture, level); r_FramebufferTexture2D(target, attachment, textarget, texture, level); }
void WINAPI W_FramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{ Attached(target, attachment, false, texture, level); r_FramebufferTexture(target, attachment, texture, level); }
void WINAPI W_FramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbtarget, GLuint rb)
{ Attached(target, attachment, true, rb, 0); r_FramebufferRenderbuffer(target, attachment, rbtarget, rb); }
void WINAPI W_ActiveTexture(GLenum unit) { s_activeUnit = unit - GL_TEXTURE0; r_ActiveTexture(unit); }
void WINAPI W_BindTexture(GLenum target, GLuint tex) { s_unitTex[s_activeUnit][target] = tex; r_BindTexture(target, tex); }
void WINAPI W_DeleteTextures(GLsizei n, const GLuint *t)
{
  for (GLsizei i = 0; i < n; ++i) {
    for (auto &map : s_texTwin) { auto it = map.find(t[i]); if (it != map.end()) { if (it->second) r_DeleteTextures(1, &it->second); map.erase(it); } }
    s_texSize.erase(t[i]); s_renderTargets.erase(t[i]);
    for (auto &u : s_unitTex) for (auto &b : u.second) if (b.second == t[i]) b.second = 0;
  }
  r_DeleteTextures(n, t);
}
void WINAPI W_DeleteFramebuffers(GLsizei n, const GLuint *f)
{
  for (GLsizei i = 0; i < n; ++i) {
    for (auto &map : s_fboTwin) { auto it = map.find(f[i]); if (it != map.end()) { if (it->second.fbo) r_DeleteFramebuffers(1, &it->second.fbo); map.erase(it); } }
    s_fboAttach.erase(f[i]); s_fbo3DEver.erase(f[i]);
  }
  r_DeleteFramebuffers(n, f);
}
void WINAPI W_BlitFramebuffer(GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLint h, GLbitfield mask, GLenum filter)
{
  if (TraceNow()) { ++s_tstats.blits; s_log("[gl] blit fbo %u (%d,%d)-(%d,%d) -> fbo %u (%d,%d)-(%d,%d) mask 0x%X", s_readFbo, a, b, c, d, s_drawFbo, e, f, g, h, mask); }
  r_BlitFramebuffer(a, b, c, d, e, f, g, h, mask, filter);
  // Into a known framebuffer: again from the source's twin (or the source itself) into each twin.
  if (Known(s_drawFbo)) {
    for (int eye : Extra()) {
      const GLbitfield m = HudEye(eye) ? (mask & GL_COLOR_BUFFER_BIT) : mask; if (!m) continue;
      const GLuint dst = TwinFramebuffer(s_drawFbo, eye), src = Known(s_readFbo) ? TwinFramebuffer(s_readFbo, eye) : s_readFbo;
      r_BindFramebuffer(GL_READ_FRAMEBUFFER, src); r_BindFramebuffer(GL_DRAW_FRAMEBUFFER, dst);
      r_BlitFramebuffer(a, b, c, d, e, f, g, h, m, filter);
      ++s_dupBlits;
    }
    r_BindFramebuffer(GL_READ_FRAMEBUFFER, s_readFbo); r_BindFramebuffer(GL_DRAW_FRAMEBUFFER, s_drawFbo);
  }
}
void WINAPI W_CopyTexSubImage2D(GLenum target, GLint level, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h)
{
  r_CopyTexSubImage2D(target, level, xo, yo, x, y, w, h);
  // Into a render-target texture with twins: each twin gets the copy from the matching framebuffer twin.
  const GLuint tex = s_unitTex[s_activeUnit][target];
  if (!Active() || !s_ownOk || !tex || !Known(s_readFbo)) return;
  for (int eye : Extra()) {
    auto it = s_texTwin[eye].find(tex); if (it == s_texTwin[eye].end() || !it->second) continue;
    r_BindFramebuffer(GL_READ_FRAMEBUFFER, TwinFramebuffer(s_readFbo, eye)); r_BindTexture(target, it->second);
    r_CopyTexSubImage2D(target, level, xo, yo, x, y, w, h);
  }
  r_BindTexture(target, tex); r_BindFramebuffer(GL_READ_FRAMEBUFFER, s_readFbo);
}
void Seen(bool clear)
{
  // The game's 3D picture: a framebuffer that gets depth-tested draws of the triangle shaders. (GLideN64 also
  // fills its depth image with depth-tested rectangles; that one has no HUD or backdrop.)
  // ACVR_GL_3D_NO_DEPTH=1: games that draw their 3D world without the depth test (Cruis'n Exotica) count it all the same.
  static const bool noDepth = EnvF("ACVR_GL_3D_NO_DEPTH", 0) != 0;
  if (!clear && (s_depthTest || noDepth)) {
    const auto it = s_programs.find(s_program);
    if (it != s_programs.end() && it->second.wide >= 0) { s_fboHas3D.insert(s_drawFbo); s_fbo3DEver.insert(s_drawFbo); }
  }
  if (TraceNow()) {
    if (clear) ++s_tstats.clears[s_drawFbo];
    else {
      static const unsigned limit = (unsigned)EnvF("ACVR_GL_TRACE_DRAWS", 60);
      if (s_tstats.draws[s_drawFbo] < limit) {
        auto it = s_programs.find(s_program);
        const char *kind = it == s_programs.end() ? "other" : it->second.wide >= 0 ? "triangles" : "rectangles";
        s_log("[gl]   draw %u into fbo %u: program %u (%s), depth test %s, blend %s (0x%X,0x%X), backdrop %s", s_tstats.draws[s_drawFbo], s_drawFbo, s_program, kind,
              s_depthTest ? "on" : "off", s_blend ? "on" : "off", s_blendSrcRgb, s_blendDstRgb, Backdrop() ? "yes" : "no");
      }
      ++s_tstats.draws[s_drawFbo]; char v[64]; std::snprintf(v, sizeof v, "fbo %u vp %d,%d %dx%d", s_drawFbo, s_vp[0], s_vp[1], s_vp[2], s_vp[3]); s_tstats.viewports.insert(v);
    }
  }
  if (Known(s_drawFbo) && !Extra().empty()) { if (clear) ++s_dupClears; else ++s_dupDraws; }
}
void WINAPI W_DrawArrays(GLenum mode, GLint first, GLsizei count) { Seen(false); Layers([&] { r_DrawArrays(mode, first, count); }); }
void WINAPI W_DrawElements(GLenum mode, GLsizei count, GLenum type, const void *idx) { Seen(false); Layers([&] { r_DrawElements(mode, count, type, idx); }); }
void WINAPI W_DrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *idx, GLint base) { Seen(false); Layers([&] { r_DrawElementsBaseVertex(mode, count, type, idx, base); }); }
void WINAPI W_DrawRangeElements(GLenum mode, GLuint s, GLuint e, GLsizei count, GLenum type, const void *idx) { Seen(false); Layers([&] { r_DrawRangeElements(mode, s, e, count, type, idx); }); }
void WINAPI W_DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei n) { Seen(false); Layers([&] { r_DrawArraysInstanced(mode, first, count, n); }); }
void WINAPI W_Clear(GLbitfield mask) { Seen(true); Layers([&] { r_Clear(mask); }, true, mask); }
void WINAPI W_Enable(GLenum cap)
{
  if (cap == GL_DEPTH_TEST) s_depthTest = true; else if (cap == GL_BLEND) s_blend = true; else if (cap == GL_SCISSOR_TEST) s_scissor = true;
  r_Enable(cap);
}
void WINAPI W_Disable(GLenum cap)
{
  if (cap == GL_DEPTH_TEST) s_depthTest = false; else if (cap == GL_BLEND) s_blend = false; else if (cap == GL_SCISSOR_TEST) s_scissor = false;
  r_Disable(cap);
}
void WINAPI W_BlendFunc(GLenum s, GLenum d) { s_blendSrcRgb = s_blendSrcA = s; s_blendDstRgb = s_blendDstA = d; r_BlendFunc(s, d); }
void WINAPI W_BlendFuncSeparate(GLenum s, GLenum d, GLenum sa, GLenum da) { s_blendSrcRgb = s; s_blendDstRgb = d; s_blendSrcA = sa; s_blendDstA = da; r_BlendFuncSeparate(s, d, sa, da); }
void WINAPI W_Viewport(GLint x, GLint y, GLsizei w, GLsizei h) { s_vp[0] = x; s_vp[1] = y; s_vp[2] = w; s_vp[3] = h; r_Viewport(x, y, w, h); }
void WINAPI W_Scissor(GLint x, GLint y, GLsizei w, GLsizei h) { r_Scissor(x, y, w, h); }
void WINAPI W_ReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void *p) { if (TraceNow()) { ++s_tstats.reads; s_log("[gl] readpixels fbo %u %dx%d", s_readFbo, w, h); } r_ReadPixels(x, y, w, h, f, t, p); }

struct Entry { const char *name; void *wrapper; void **real; };
#define E(n) { "gl" #n, (void *)&W_##n, (void **)&r_##n }
const Entry kProcs[] = { E(ShaderSource), E(CreateShader), E(LinkProgram), E(UseProgram), E(BindFramebuffer), E(BlitFramebuffer),
                         E(FramebufferTexture2D), E(FramebufferTexture), E(FramebufferRenderbuffer), E(ActiveTexture),
                         E(DrawElementsBaseVertex), E(DrawRangeElements), E(DrawArraysInstanced), E(CopyTexSubImage2D),
                         E(DeleteFramebuffers), E(BlendFuncSeparate),
                         // OpenGL 1.1 entry points the core may also ask for by name
                         E(DrawArrays), E(DrawElements), E(Clear), E(Viewport), E(Scissor), E(ReadPixels), E(BindTexture), E(DeleteTextures),
                         E(Enable), E(Disable), E(BlendFunc) };
const Entry kImports[] = { E(DrawArrays), E(DrawElements), E(Clear), E(Viewport), E(Scissor), E(ReadPixels), E(BindTexture), E(DeleteTextures),
                           E(CopyTexSubImage2D), E(Enable), E(Disable), E(BlendFunc) };
#undef E
}  // namespace

void Configure(void (*log)(const char *, ...))
{
  s_log = log;
  if (const char *t = std::getenv("ACVR_GL_TRACE")) {
    if (std::sscanf(t, "%u,%u", &s_traceFirst, &s_traceCount) == 2 && s_traceCount) s_trace = true;
  }
  if (const char *d = std::getenv("ACVR_GL_TRACE_DIR")) s_traceDir = d;
  s_stereo = EnvF("ACVR_GL_STEREO", 0) != 0;
  s_hud = EnvF("ACVR_GL_HUD", 0) != 0;
  if (const char *w = std::getenv("ACVR_GL_WIDE")) {
    float x = 1, y = 1; const int n = std::sscanf(w, "%f,%f", &x, &y);
    if (n >= 1 && x >= .1f && x <= 1.f) { s_wideX = x; s_wideY = n == 2 && y >= .1f && y <= 1.f ? y : 1.f; s_wide = s_wideX < 1.f || s_wideY < 1.f; }
  }
  if (s_trace) s_log("GL trace: frames %u to %u", s_traceFirst, s_traceFirst + s_traceCount - 1);
  if (s_hud) s_log("GL HUD layer on: the game's 2D overlay goes to its own picture; the world pictures carry none");
  s_squeeze = EnvF("ACVR_GL_WIDE_SQUEEZE", 1) != 0;
  if (s_wide && !s_squeeze) { s_log("GL wide view: the game draws %.2fx the width, %.2fx the height itself; the presenter stretches it", 1 / s_wideX, 1 / s_wideY); }
  else if (s_wide) s_log("GL wide view: projection squeezed to %.3f x %.3f about the picture centre (shows %.2fx the width, %.2fx the height)", s_wideX, s_wideY, 1 / s_wideX, 1 / s_wideY);
  s_backdrop = EnvF("ACVR_GL_STEREO_BACKDROP", 1) != 0;
  s_stats = EnvF("ACVR_GL_STEREO_STATS", 0) != 0;
  if (!s_stereo) return;
  const float ipd = EnvF("ACVR_PS1_IPD", .064f), screen = EnvF("ACVR_PS1_SCREEN_W", 2.4f), depth = EnvF("ACVR_GL_STEREO_DEPTH", 1.f);
  s_shift = depth * ipd / std::max(screen, .2f);   // per eye, in NDC across a picture 2 wide: half of (ipd / screen) of its width
  s_pop = EnvF("ACVR_GL_STEREO_POPOUT", 2.f); s_auto = std::clamp(EnvF("ACVR_GL_STEREO_AUTO", .3f), .01f, .9f);
  const char *conv = std::getenv("ACVR_GL_STEREO_CONV");
  s_convAuto = !conv || !*conv || !std::strcmp(conv, "auto");
  if (!s_convAuto) s_convNow = s_conv = std::max((float)std::atof(conv), 1e-4f);
  s_log("GL stereo on: shift %.4f NDC per eye (IPD %.3f m, screen %.2f m, depth x%.2f), zero plane %s, pop-out %.1f",
        s_shift, ipd, screen, depth, s_convAuto ? ("auto at the " + std::to_string((int)std::lround(s_auto * 100)) + "th percentile of w").c_str() : conv, s_pop);
}
bool Tracing() { return s_trace; }
bool StereoOn() { return s_stereo; }
bool HudOn() { return s_hud; }
void WideView(float &x, float &y) { x = s_wideX; y = s_wideY; }
bool WideSqueezed() { return s_squeeze; }

void InstallImports(HMODULE core)
{
  if (!s_trace && !Active()) return;
  auto *base = reinterpret_cast<unsigned char *>(core);
  auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
  auto *nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
  const auto &dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (!dir.VirtualAddress) { s_log("GL hooks: the core has no import table"); return; }
  unsigned patched = 0;
  for (auto *imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base + dir.VirtualAddress); imp->Name; ++imp) {
    if (_stricmp(reinterpret_cast<const char *>(base + imp->Name), "opengl32.dll")) continue;
    auto *names = reinterpret_cast<IMAGE_THUNK_DATA *>(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
    auto *slots = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imp->FirstThunk);
    for (; names->u1.AddressOfData; ++names, ++slots) {
      if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
      const char *name = reinterpret_cast<const char *>(reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(base + names->u1.AddressOfData)->Name);
      for (const auto &e : kImports) if (!std::strcmp(name, e.name)) {
        DWORD old; VirtualProtect(&slots->u1.Function, sizeof(void *), PAGE_READWRITE, &old);
        *e.real = reinterpret_cast<void *>(slots->u1.Function);
        slots->u1.Function = reinterpret_cast<ULONG_PTR>(e.wrapper);
        VirtualProtect(&slots->u1.Function, sizeof(void *), old, &old); ++patched;
      }
    }
  }
  // The same entry points from opengl32 itself, for anything the core asks for by name.
  if (HMODULE gl = GetModuleHandleA("opengl32.dll")) for (const auto &e : kImports) if (!*e.real) *e.real = reinterpret_cast<void *>(GetProcAddress(gl, e.name));
  s_log("GL hooks: %u OpenGL 1.1 imports of the core wrapped", patched);
}

retro_proc_address_t Wrap(const char *sym, retro_proc_address_t real)
{
  if ((!s_trace && !Active()) || !real) return real;
  for (const auto &e : kProcs) if (!std::strcmp(sym, e.name)) {
    // OpenGL 1.1 names keep the import's real pointer when it was patched already.
    if (!*e.real) *e.real = reinterpret_cast<void *>(real);
    return reinterpret_cast<retro_proc_address_t>(e.wrapper);
  }
  return real;
}

void SetHostFramebuffer(unsigned fbo, unsigned w, unsigned h, bool depth, bool stencil)
{
  s_hostFbo = fbo; s_hostW = w; s_hostH = h;
  if (!Active()) return;
  if (!LoadOwn()) { s_log("GL layers off: this OpenGL lacks direct state access (4.5)"); s_stereo = s_hud = s_wide = false; return; }
  if (!r_BlendFuncSeparate) r_BlendFuncSeparate = o_BlendFuncSeparate;
  // The host's own framebuffer: its colour texture and depth renderbuffer, as attached by the host.
  GLint colour = 0, type = 0, rb = 0;
  auto getAtt = reinterpret_cast<PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC>(wglGetProcAddress("glGetNamedFramebufferAttachmentParameteriv"));
  getAtt(fbo, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &colour);
  s_fboAttach[fbo][GL_COLOR_ATTACHMENT0] = { false, (GLuint)colour, 0 }; s_renderTargets.insert((GLuint)colour);
  if (depth) {
    const GLenum point = stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
    getAtt(fbo, point, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type); getAtt(fbo, point, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &rb);
    if (type == GL_RENDERBUFFER) s_fboAttach[fbo][point] = { true, (GLuint)rb, 0 };
  }
  o_CreateBuffers(1, &s_hist); o_NamedBufferData(s_hist, kBins * sizeof(unsigned), nullptr, GL_DYNAMIC_READ);
  const unsigned zero = 0; o_ClearNamedBufferData(s_hist, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
  GLint generic = 0; glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &generic);
  o_BindBufferBase(GL_SHADER_STORAGE_BUFFER, kHistBinding, s_hist);
  reinterpret_cast<PFNGLBINDBUFFERPROC>(wglGetProcAddress("glBindBuffer"))(GL_SHADER_STORAGE_BUFFER, (GLuint)generic);
  s_log("GL layers: host framebuffer %u (%ux%u)%s%s", fbo, w, h, s_stereo ? ", right eye in its twin" : "", s_hud ? ", HUD layer in its own twin" : "");
}

void EndFrame(unsigned frame)
{
  if (TraceNow()) {
    std::string d, c;
    for (const auto &kv : s_tstats.draws) d += " fbo" + std::to_string(kv.first) + ":" + std::to_string(kv.second);
    for (const auto &kv : s_tstats.clears) c += " fbo" + std::to_string(kv.first) + ":" + std::to_string(kv.second);
    s_log("[gl] frame %u: draws%s; clears%s; blits %u; readpixels %u (host fbo %u)", s_frame, d.c_str(), c.c_str(), s_tstats.blits, s_tstats.reads, s_hostFbo);
    for (const auto &v : s_tstats.viewports) s_log("[gl]   %s", v.c_str());
  }
  s_tstats = FrameStats();
  s_fboHas3D.clear();   // N64 games often start a frame with a fill instead of a clear
  s_hudCleared.clear();
  if (Active() && s_ownOk && s_hist) {
    // The w histogram of this frame's 3D vertices (left eye only), then cleared for the next.
    std::vector<unsigned> h(kBins, 0);
    if (s_stereo) {
      o_GetNamedBufferSubData(s_hist, 0, kBins * sizeof(unsigned), h.data());
      const unsigned zero = 0; o_ClearNamedBufferData(s_hist, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    }
    unsigned long long total = 0; for (unsigned v : h) total += v;
    if (total > 50) {
      for (int i = 0; i < kBins; ++i) s_histo[i] += h[i];
      if (s_convAuto) {
        unsigned long long run = 0; int bin = 0;
        for (; bin < kBins; ++bin) { run += h[bin]; if (run >= total * s_auto) break; }
        const float target = BinW(std::min(bin, kBins - 1)) * 1.41f;   // bin centre (half a log2 step)
        s_convNow = s_convNow > 0 ? std::exp2(std::log2(s_convNow) + (std::log2(target) - std::log2(s_convNow)) * .1f) : target;
      }
    }
    if (s_stats && frame % 120 == 0) {
      std::string line; unsigned long long all = 0; for (unsigned v : s_histo) all += v;
      for (int i = 0; i < kBins; ++i) if (s_histo[i]) { char b[48]; std::snprintf(b, sizeof b, " w>=%.3g:%.1f%%", BinW(i), 100.0 * s_histo[i] / std::max(all, 1ull)); line += b; }
      s_log("GL layers frame %u: zero plane w %.4g; replayed %u draws (%u backdrops, %u into the HUD layer), %u clears, %u blits; twins %zu+%zu textures (%u failures); patched shaders %u vertex, %u fragment; w:%s",
            frame, s_convNow, s_dupDraws, s_bgDraws, s_hudDraws, s_dupClears, s_dupBlits, s_texTwin[kRight].size(), s_texTwin[kHud].size(), s_twinFails,
            s_patchedShaders, s_patchedFragment, line.c_str());
      std::fill(s_histo.begin(), s_histo.end(), 0u);
    }
  }
  s_frame = frame;
}

unsigned RightFramebuffer() { return s_stereo && s_ownOk && s_hostFbo ? TwinFramebuffer(s_hostFbo, kRight) : 0; }
unsigned HudFramebuffer(bool overWhite) { return s_hud && s_ownOk && s_hostFbo ? TwinFramebuffer(s_hostFbo, overWhite ? kHudWhite : kHud) : 0; }
float ZeroPlane() { return s_convNow; }
}  // namespace glstereo
