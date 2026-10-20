// SPDX-License-Identifier: GPL-3.0-or-later
// Stereo for a prebuilt libretro hardware-OpenGL core (first user: mupen64plus-next with GLideN64), with no
// change to the core. The host hands the core every OpenGL entry point: newer ones through get_proc_address,
// the OpenGL 1.1 ones through the core's import table, which this module patches in the loaded module.
//
// Stereo (ACVR_GL_STEREO=1): every framebuffer the core draws into gets a right-eye twin (same size and format,
// textures and renderbuffers alike). Each draw, clear, blit and framebuffer-to-texture copy runs twice: first as
// the core asked (left eye), then into the twins, with every bound texture that is itself a render target swapped
// for its twin. So effects that reuse the picture (the final copy to the host, frame-buffer effects) see the
// right eye's own picture. Readbacks to the emulated console stay on the left eye.
// The 3D vertex shaders (GLideN64's triangle shaders: aPosition with clip-space w, aModify) get one line appended:
//   x += eye * shift * clamp(1 - conv / w, -popout, 1) * w      (clip space; the same rule as our SwanStation patch)
// so far scenery reaches the eyes' separation on the theatre screen, depth `conv` sits on the screen and nearer
// geometry comes in front. 2D (rectangles, screen-space vertices, orthographic w = 1: HUD, text) stays on the
// screen; 2D drawn into the 3D picture before its first depth-tested draw of the frame (skies, backgrounds) goes
// to infinity.
// The shaders also count clip w into a 64-bin log histogram (shader storage buffer); `conv` follows a low
// percentile of it each frame unless set.
//
// Trace mode (ACVR_GL_TRACE=<first frame>,<frames>) logs what the core draws in those frames and saves every
// vertex shader it compiles (ACVR_GL_TRACE_DIR), to see how a core draws before splitting the eyes.
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

namespace glstereo {
namespace {
void (*s_log)(const char *, ...) = nullptr;
bool s_trace = false, s_stereo = false;
unsigned s_traceFirst = 0, s_traceCount = 0, s_frame = 0;
std::string s_traceDir;
bool TraceNow() { return s_trace && s_frame >= s_traceFirst && s_frame < s_traceFirst + s_traceCount; }

// Stereo settings (environment): ACVR_GL_STEREO_CONV (w on the screen plane, or auto), ACVR_GL_STEREO_AUTO
// (share of geometry allowed in front of the screen), ACVR_GL_STEREO_DEPTH (multiplier), ACVR_GL_STEREO_POPOUT,
// ACVR_PS1_IPD and ACVR_PS1_SCREEN_W (metres; the theatre's screen width).
float s_shift = .0267f, s_conv = -1, s_auto = .3f, s_pop = 2, s_convNow = 0;
bool s_convAuto = true, s_stats = false;
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
  X(PFNGLDELETEFRAMEBUFFERSPROC, DeleteFramebuffers) X(PFNGLINVALIDATEFRAMEBUFFERPROC, InvalidateFramebuffer)
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
void (WINAPI *r_Enable)(GLenum) = nullptr;
void (WINAPI *r_Disable)(GLenum) = nullptr;
void (WINAPI *r_CopyTexSubImage2D)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei) = nullptr;

// Our own helpers (resolved on first use, with the core's context current).
#define OWN_PROCS(X) \
  X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) X(PFNGLUNIFORM1FPROC, Uniform1f) X(PFNGLUNIFORM4FPROC, Uniform4f) \
  X(PFNGLCREATEFRAMEBUFFERSPROC, CreateFramebuffers) X(PFNGLNAMEDFRAMEBUFFERTEXTUREPROC, NamedFramebufferTexture) \
  X(PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC, NamedFramebufferRenderbuffer) X(PFNGLCREATETEXTURESPROC, CreateTextures) \
  X(PFNGLTEXTURESTORAGE2DPROC, TextureStorage2D) X(PFNGLGETTEXTURELEVELPARAMETERIVPROC, GetTextureLevelParameteriv) \
  X(PFNGLGETTEXTUREPARAMETERIVPROC, GetTextureParameteriv) X(PFNGLTEXTUREPARAMETERIPROC, TextureParameteri) \
  X(PFNGLCREATERENDERBUFFERSPROC, CreateRenderbuffers) X(PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC, NamedRenderbufferStorageMultisample) \
  X(PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC, GetNamedRenderbufferParameteriv) X(PFNGLCREATEBUFFERSPROC, CreateBuffers) \
  X(PFNGLNAMEDBUFFERDATAPROC, NamedBufferData) X(PFNGLGETNAMEDBUFFERSUBDATAPROC, GetNamedBufferSubData) \
  X(PFNGLCLEARNAMEDBUFFERDATAPROC, ClearNamedBufferData) X(PFNGLBINDBUFFERBASEPROC, BindBufferBase) \
  X(PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC, CheckNamedFramebufferStatus) X(PFNGLGETPROGRAMIVPROC, GetProgramiv)
#define X(type, name) type o_##name = nullptr;
OWN_PROCS(X)
#undef X
bool s_ownLoaded = false, s_ownOk = false;
bool LoadOwn()
{
  if (s_ownLoaded) return s_ownOk;
  s_ownLoaded = true; s_ownOk = true;
#define X(type, name) o_##name = reinterpret_cast<type>(wglGetProcAddress("gl" #name)); if (!o_##name) { s_log("GL stereo: no gl%s", #name); s_ownOk = false; }
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

// ---- stereo objects
struct Program { GLint shift = -1, conv = -1, bg = -1; };
std::map<GLuint, Program> s_programs;
std::map<GLuint, GLuint> s_texTwin, s_rbTwin;                    // core object -> right-eye twin
std::map<GLuint, GLint> s_texWidth;                              // render-target texture -> width
std::set<GLuint> s_renderTargets;                                // core textures used as colour/depth attachments
struct TwinFbo { GLuint fbo = 0; std::map<GLenum, Attach> attached; };
std::map<GLuint, TwinFbo> s_fboTwin;
GLuint s_hist = 0;
unsigned s_dupDraws = 0, s_dupClears = 0, s_dupBlits = 0, s_twinFails = 0, s_bgDraws = 0;
std::vector<unsigned> s_histo(kBins, 0);

struct FrameStats { std::map<GLuint, unsigned> draws, clears; unsigned blits = 0, reads = 0; std::set<std::string> viewports; } s_tstats;
unsigned s_vertexShaders = 0, s_patchedShaders = 0;

// ---- right-eye twins
GLuint TwinTexture(GLuint tex)
{
  auto it = s_texTwin.find(tex); if (it != s_texTwin.end()) return it->second;
  GLint w = 0, h = 0, fmt = 0;
  o_GetTextureLevelParameteriv(tex, 0, GL_TEXTURE_WIDTH, &w); o_GetTextureLevelParameteriv(tex, 0, GL_TEXTURE_HEIGHT, &h);
  o_GetTextureLevelParameteriv(tex, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);
  GLuint t = 0; s_texWidth[tex] = w;
  if (w > 0 && h > 0 && fmt) {
    o_CreateTextures(GL_TEXTURE_2D, 1, &t); o_TextureStorage2D(t, 1, (GLenum)fmt, w, h);
    for (GLenum p : { GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER, GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T }) { GLint v = 0; o_GetTextureParameteriv(tex, p, &v); o_TextureParameteri(t, p, v); }
    if (s_texTwin.size() < 64) s_log("GL stereo: right-eye twin of texture %u: %dx%d format 0x%X", tex, w, h, fmt);
  } else { ++s_twinFails; s_log("GL stereo: texture %u has no size (%dx%d, 0x%X): no twin", tex, w, h, fmt); }
  s_texTwin[tex] = t; return t;
}
GLuint TwinRenderbuffer(GLuint rb)
{
  auto it = s_rbTwin.find(rb); if (it != s_rbTwin.end()) return it->second;
  GLint w = 0, h = 0, fmt = 0, samples = 0;
  o_GetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_WIDTH, &w); o_GetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_HEIGHT, &h);
  o_GetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_INTERNAL_FORMAT, &fmt); o_GetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_SAMPLES, &samples);
  GLuint r = 0;
  if (w > 0 && h > 0) { o_CreateRenderbuffers(1, &r); o_NamedRenderbufferStorageMultisample(r, samples, (GLenum)fmt, w, h); }
  s_rbTwin[rb] = r; return r;
}
// The twin of a core framebuffer, its attachments kept in step with the original's.
GLuint TwinFramebuffer(GLuint fbo)
{
  if (!fbo) return 0;
  auto &twin = s_fboTwin[fbo];
  const auto &want = s_fboAttach[fbo];
  if (!twin.fbo) o_CreateFramebuffers(1, &twin.fbo);
  bool changed = false;
  for (const auto &a : want) {
    auto have = twin.attached.find(a.first);
    if (have != twin.attached.end() && have->second.name == a.second.name && have->second.renderbuffer == a.second.renderbuffer && have->second.level == a.second.level) continue;
    if (!a.second.name) o_NamedFramebufferTexture(twin.fbo, a.first, 0, 0);
    else if (a.second.renderbuffer) o_NamedFramebufferRenderbuffer(twin.fbo, a.first, GL_RENDERBUFFER, TwinRenderbuffer(a.second.name));
    else o_NamedFramebufferTexture(twin.fbo, a.first, TwinTexture(a.second.name), a.second.level);
    twin.attached[a.first] = a.second; changed = true;
  }
  if (changed) {
    const GLenum status = o_CheckNamedFramebufferStatus(twin.fbo, GL_DRAW_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) { ++s_twinFails; s_log("GL stereo: twin of framebuffer %u incomplete (0x%X)", fbo, status); }
  }
  return twin.fbo;
}
bool Twinned(GLuint fbo) { return s_stereo && s_ownOk && fbo && s_fboAttach.count(fbo); }

// Swap every bound render-target texture for its twin (right eye), and back.
struct Swap { GLuint unit, target, tex; };
std::vector<Swap> s_swapped;
void SwapTexturesToRight()
{
  s_swapped.clear();
  for (const auto &u : s_unitTex) for (const auto &t : u.second) {
    if (!t.second || !s_renderTargets.count(t.second)) continue;
    const GLuint twin = TwinTexture(t.second); if (!twin) continue;
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
// Backdrop tracking: per framebuffer, whether a depth-tested draw happened since its last colour clear. Only
// framebuffers that do get depth-tested draws (the game's 3D picture) have backdrops; the copies that make the
// final picture never do.
bool s_depthTest = false, s_backdrop = true;
std::set<GLuint> s_fboHas3D, s_fbo3DEver;
bool Backdrop() { return s_fbo3DEver.count(s_drawFbo) && !s_fboHas3D.count(s_drawFbo); }
// The shift is a share of the picture's width. In clip space it depends on the viewport: GLideN64 draws its
// frame buffer with a viewport wider than the texture (only part of the NDC range lands on the picture).
float NdcPerPicture()
{
  const auto a = s_fboAttach.find(s_drawFbo);
  if (a == s_fboAttach.end() || s_vp[2] <= 0) return 1.f;
  const auto c = a->second.find(GL_COLOR_ATTACHMENT0);
  if (c == a->second.end() || c->second.renderbuffer) return 1.f;
  const auto w = s_texWidth.find(c->second.name);
  return w == s_texWidth.end() || w->second <= 0 ? 1.f : (float)w->second / (float)s_vp[2];
}
void SetEye(float eye)
{
  auto it = s_programs.find(s_program); if (it == s_programs.end()) return;
  const Program &p = it->second;
  const float shift = eye * s_shift * NdcPerPicture();
  if (p.shift >= 0) o_Uniform1f(p.shift, shift);
  if (p.conv >= 0) o_Uniform4f(p.conv, s_convNow, s_pop, eye < 0 ? 1.f : 0.f, 0.f);
  // Backdrops (2D drawn into a framebuffer before any depth-tested draw since its clear: skies, far
  // backgrounds) go to infinity; later 2D (HUD, text) stays on the screen.
  const bool bg = s_backdrop && Backdrop();
  if (p.bg >= 0) o_Uniform1f(p.bg, bg ? shift : 0.f);
  if (bg && eye < 0) ++s_bgDraws;
}
// Runs `draw` for the left eye as asked, then for the right eye into the twins.
template <class F> void Twice(F draw, bool shifted = true)
{
  if (!Twinned(s_drawFbo)) { draw(); return; }
  const GLuint twin = TwinFramebuffer(s_drawFbo);   // also learns the picture's width
  if (shifted) SetEye(-1);
  draw();
  if (!twin) return;
  r_BindFramebuffer(GL_DRAW_FRAMEBUFFER, twin);
  SwapTexturesToRight();
  if (shifted) SetEye(1);
  draw();
  RestoreTextures();
  r_BindFramebuffer(GL_DRAW_FRAMEBUFFER, s_drawFbo);
  if (shifted) SetEye(-1);
}

// ---- shader patch
const char *kStereoTail = R"glsl(
uniform float acvrShift;   // eye * half the eyes' separation on the screen, in NDC
uniform vec4 acvrConv;     // w on the screen plane, pop-out limit, 1 when this pass counts w (left eye)
uniform float acvrShiftBg; // the full shift for 2D backdrops (to infinity), else 0
layout(std430, binding = 13) buffer AcvrHist { uint acvrHist[64]; };
void main()
{
  acvr_main();
  // Perspective vertices only: an orthographic projection (HUD, sprites through the 3D pipeline) has w exactly 1.
  if (aModify[0] == 0.0 && gl_Position.w > 0.0 && abs(gl_Position.w - 1.0) > 1e-5) {
    if (acvrConv.z > 0.5) atomicAdd(acvrHist[clamp(int((log2(gl_Position.w) + 8.0) * 2.0), 0, 63)], 1u);
    gl_Position.x += acvrShift * clamp(1.0 - acvrConv.x / gl_Position.w, -acvrConv.y, 1.0) * gl_Position.w;
  } else gl_Position.x += acvrShiftBg * gl_Position.w;
}
)glsl";
// GLideN64's rectangles (backgrounds, fills, text): only ever a backdrop shift.
const char *kRectTail = R"glsl(
uniform float acvrShiftBg;
void main() { acvr_main(); gl_Position.x += acvrShiftBg * gl_Position.w; }
)glsl";

bool PatchVertexShader(std::string &src)
{
  const bool tri = src.find("aModify") != std::string::npos && src.find("aPosition") != std::string::npos;
  const bool rect = !tri && src.find("aRectPosition") != std::string::npos;
  if (!tri && !rect) return false;
  const auto at = src.find("void main()"); if (at == std::string::npos) return false;
  src.replace(at, 11, "void acvr_main()");
  src += tri ? kStereoTail : kRectTail;
  return true;
}

// ---- wrappers
void WINAPI W_ShaderSource(GLuint shader, GLsizei count, const GLchar *const *strings, const GLint *lengths)
{
  if (s_shaderType[shader] != GL_VERTEX_SHADER || (!s_stereo && s_traceDir.empty())) { r_ShaderSource(shader, count, strings, lengths); return; }
  std::string src;
  for (GLsizei i = 0; i < count; ++i) src += lengths && lengths[i] >= 0 ? std::string(strings[i], lengths[i]) : std::string(strings[i]);
  const bool patched = s_stereo && PatchVertexShader(src);
  if (patched) ++s_patchedShaders;
  if (!s_traceDir.empty()) {
    char path[512]; std::snprintf(path, sizeof path, "%s\\vs-%03u%s.glsl", s_traceDir.c_str(), s_vertexShaders, patched ? "-stereo" : "");
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
  if (!s_stereo || !LoadOwn()) return;
  GLint ok = 0; o_GetProgramiv(program, GL_LINK_STATUS, &ok);
  Program p; if (ok) { p.shift = o_GetUniformLocation(program, "acvrShift"); p.conv = o_GetUniformLocation(program, "acvrConv"); p.bg = o_GetUniformLocation(program, "acvrShiftBg"); }
  if (p.shift >= 0 || p.bg >= 0) s_programs[program] = p; else s_programs.erase(program);
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
    auto it = s_texTwin.find(t[i]);
    if (it != s_texTwin.end()) { if (it->second) r_DeleteTextures(1, &it->second); s_texTwin.erase(it); }
    s_renderTargets.erase(t[i]);
    for (auto &u : s_unitTex) for (auto &b : u.second) if (b.second == t[i]) b.second = 0;
  }
  r_DeleteTextures(n, t);
}
void WINAPI W_DeleteFramebuffers(GLsizei n, const GLuint *f)
{
  for (GLsizei i = 0; i < n; ++i) {
    auto it = s_fboTwin.find(f[i]); if (it != s_fboTwin.end()) { if (it->second.fbo) r_DeleteFramebuffers(1, &it->second.fbo); s_fboTwin.erase(it); }
    s_fboAttach.erase(f[i]);
  }
  r_DeleteFramebuffers(n, f);
}
void WINAPI W_BlitFramebuffer(GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLint h, GLbitfield mask, GLenum filter)
{
  if (TraceNow()) { ++s_tstats.blits; s_log("[gl] blit fbo %u (%d,%d)-(%d,%d) -> fbo %u (%d,%d)-(%d,%d) mask 0x%X", s_readFbo, a, b, c, d, s_drawFbo, e, f, g, h, mask); }
  r_BlitFramebuffer(a, b, c, d, e, f, g, h, mask, filter);
  // Into a twinned framebuffer: again from the source's twin (or the source itself) into the twin.
  if (Twinned(s_drawFbo)) {
    const GLuint dst = TwinFramebuffer(s_drawFbo), src = Twinned(s_readFbo) ? TwinFramebuffer(s_readFbo) : s_readFbo;
    r_BindFramebuffer(GL_READ_FRAMEBUFFER, src); r_BindFramebuffer(GL_DRAW_FRAMEBUFFER, dst);
    r_BlitFramebuffer(a, b, c, d, e, f, g, h, mask, filter);
    r_BindFramebuffer(GL_READ_FRAMEBUFFER, s_readFbo); r_BindFramebuffer(GL_DRAW_FRAMEBUFFER, s_drawFbo);
    ++s_dupBlits;
  }
}
void WINAPI W_CopyTexSubImage2D(GLenum target, GLint level, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h)
{
  r_CopyTexSubImage2D(target, level, xo, yo, x, y, w, h);
  // Into a render-target texture with a twin: the twin gets the copy from the right eye's framebuffer.
  const GLuint tex = s_unitTex[s_activeUnit][target];
  if (s_stereo && s_ownOk && tex && s_texTwin.count(tex) && s_texTwin[tex] && Twinned(s_readFbo)) {
    r_BindFramebuffer(GL_READ_FRAMEBUFFER, TwinFramebuffer(s_readFbo)); r_BindTexture(target, s_texTwin[tex]);
    r_CopyTexSubImage2D(target, level, xo, yo, x, y, w, h);
    r_BindTexture(target, tex); r_BindFramebuffer(GL_READ_FRAMEBUFFER, s_readFbo);
  }
}
void Seen(bool clear)
{
  if (!clear && s_depthTest) { s_fboHas3D.insert(s_drawFbo); s_fbo3DEver.insert(s_drawFbo); }
  if (TraceNow()) {
    if (clear) ++s_tstats.clears[s_drawFbo];
    else {
      if (s_tstats.draws[s_drawFbo] < 60) {
        auto it = s_programs.find(s_program);
        const char *kind = it == s_programs.end() ? "other" : it->second.shift >= 0 ? "triangles" : "rectangles";
        s_log("[gl]   draw %u into fbo %u: program %u (%s), depth test %s, backdrop %s", s_tstats.draws[s_drawFbo], s_drawFbo, s_program, kind, s_depthTest ? "on" : "off", Backdrop() ? "yes" : "no");
      }
      ++s_tstats.draws[s_drawFbo]; char v[64]; std::snprintf(v, sizeof v, "fbo %u vp %d,%d %dx%d", s_drawFbo, s_vp[0], s_vp[1], s_vp[2], s_vp[3]); s_tstats.viewports.insert(v); }
  }
  if (Twinned(s_drawFbo)) { if (clear) ++s_dupClears; else ++s_dupDraws; }
}
void WINAPI W_DrawArrays(GLenum mode, GLint first, GLsizei count) { Seen(false); Twice([&] { r_DrawArrays(mode, first, count); }); }
void WINAPI W_DrawElements(GLenum mode, GLsizei count, GLenum type, const void *idx) { Seen(false); Twice([&] { r_DrawElements(mode, count, type, idx); }); }
void WINAPI W_DrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *idx, GLint base) { Seen(false); Twice([&] { r_DrawElementsBaseVertex(mode, count, type, idx, base); }); }
void WINAPI W_DrawRangeElements(GLenum mode, GLuint s, GLuint e, GLsizei count, GLenum type, const void *idx) { Seen(false); Twice([&] { r_DrawRangeElements(mode, s, e, count, type, idx); }); }
void WINAPI W_DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei n) { Seen(false); Twice([&] { r_DrawArraysInstanced(mode, first, count, n); }); }
void WINAPI W_Clear(GLbitfield mask) { Seen(true); if (mask & GL_COLOR_BUFFER_BIT) s_fboHas3D.erase(s_drawFbo); Twice([&] { r_Clear(mask); }, false); }
void WINAPI W_Enable(GLenum cap) { if (cap == GL_DEPTH_TEST) s_depthTest = true; r_Enable(cap); }
void WINAPI W_Disable(GLenum cap) { if (cap == GL_DEPTH_TEST) s_depthTest = false; r_Disable(cap); }
void WINAPI W_InvalidateFramebuffer(GLenum target, GLsizei n, const GLenum *att) { r_InvalidateFramebuffer(target, n, att); }
void WINAPI W_Viewport(GLint x, GLint y, GLsizei w, GLsizei h) { s_vp[0] = x; s_vp[1] = y; s_vp[2] = w; s_vp[3] = h; r_Viewport(x, y, w, h); }
void WINAPI W_Scissor(GLint x, GLint y, GLsizei w, GLsizei h) { r_Scissor(x, y, w, h); }
void WINAPI W_ReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void *p) { if (TraceNow()) { ++s_tstats.reads; s_log("[gl] readpixels fbo %u %dx%d", s_readFbo, w, h); } r_ReadPixels(x, y, w, h, f, t, p); }

struct Entry { const char *name; void *wrapper; void **real; };
#define E(n) { "gl" #n, (void *)&W_##n, (void **)&r_##n }
const Entry kProcs[] = { E(ShaderSource), E(CreateShader), E(LinkProgram), E(UseProgram), E(BindFramebuffer), E(BlitFramebuffer),
                         E(FramebufferTexture2D), E(FramebufferTexture), E(FramebufferRenderbuffer), E(ActiveTexture),
                         E(DrawElementsBaseVertex), E(DrawRangeElements), E(DrawArraysInstanced), E(CopyTexSubImage2D),
                         E(DeleteFramebuffers), E(InvalidateFramebuffer),
                         // OpenGL 1.1 entry points the core may also ask for by name
                         E(DrawArrays), E(DrawElements), E(Clear), E(Viewport), E(Scissor), E(ReadPixels), E(BindTexture), E(DeleteTextures),
                         E(Enable), E(Disable) };
const Entry kImports[] = { E(DrawArrays), E(DrawElements), E(Clear), E(Viewport), E(Scissor), E(ReadPixels), E(BindTexture), E(DeleteTextures),
                           E(CopyTexSubImage2D), E(Enable), E(Disable) };
#undef E
float EnvF(const char *n, float d) { const char *v = std::getenv(n); return v && *v ? (float)std::atof(v) : d; }
}  // namespace

void Configure(void (*log)(const char *, ...))
{
  s_log = log;
  if (const char *t = std::getenv("ACVR_GL_TRACE")) {
    if (std::sscanf(t, "%u,%u", &s_traceFirst, &s_traceCount) == 2 && s_traceCount) s_trace = true;
  }
  if (const char *d = std::getenv("ACVR_GL_TRACE_DIR")) s_traceDir = d;
  const char *st = std::getenv("ACVR_GL_STEREO");
  s_stereo = st && std::atoi(st) != 0;
  if (s_trace) s_log("GL trace: frames %u to %u", s_traceFirst, s_traceFirst + s_traceCount - 1);
  if (!s_stereo) return;
  const float ipd = EnvF("ACVR_PS1_IPD", .064f), screen = EnvF("ACVR_PS1_SCREEN_W", 2.4f), depth = EnvF("ACVR_GL_STEREO_DEPTH", 1.f);
  s_shift = depth * ipd / std::max(screen, .2f);   // per eye, in NDC across a picture 2 wide: half of (ipd / screen) of its width
  s_pop = EnvF("ACVR_GL_STEREO_POPOUT", 2.f); s_auto = std::clamp(EnvF("ACVR_GL_STEREO_AUTO", .3f), .01f, .9f);
  const char *conv = std::getenv("ACVR_GL_STEREO_CONV");
  s_convAuto = !conv || !*conv || !std::strcmp(conv, "auto");
  if (!s_convAuto) s_convNow = s_conv = std::max((float)std::atof(conv), 1e-4f);
  s_stats = EnvF("ACVR_GL_STEREO_STATS", 0) != 0;
  s_backdrop = EnvF("ACVR_GL_STEREO_BACKDROP", 1) != 0;
  s_log("GL stereo on: shift %.4f NDC per eye (IPD %.3f m, screen %.2f m, depth x%.2f), zero plane %s, pop-out %.1f",
        s_shift, ipd, screen, depth, s_convAuto ? ("auto at the " + std::to_string((int)std::lround(s_auto * 100)) + "th percentile of w").c_str() : conv, s_pop);
}
bool Tracing() { return s_trace; }
bool StereoOn() { return s_stereo; }

void InstallImports(HMODULE core)
{
  if (!s_trace && !s_stereo) return;
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
  if ((!s_trace && !s_stereo) || !real) return real;
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
  if (!s_stereo) return;
  if (!LoadOwn()) { s_log("GL stereo off: this OpenGL lacks direct state access (4.5)"); s_stereo = false; return; }
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
  s_log("GL stereo: host framebuffer %u (%ux%u), right eye in its twin", fbo, w, h);
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
  if (s_stereo && s_ownOk && s_hist) {
    // The w histogram of this frame's 3D vertices (left eye only), then cleared for the next.
    std::vector<unsigned> h(kBins, 0);
    o_GetNamedBufferSubData(s_hist, 0, kBins * sizeof(unsigned), h.data());
    const unsigned zero = 0; o_ClearNamedBufferData(s_hist, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
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
      s_log("GL stereo frame %u: zero plane w %.4g; duplicated %u draws (%u as backdrops), %u clears, %u blits; twins %zu textures, %zu framebuffers (%u failures); patched shaders %u; w:%s",
            frame, s_convNow, s_dupDraws, s_bgDraws, s_dupClears, s_dupBlits, s_texTwin.size(), s_fboTwin.size(), s_twinFails, s_patchedShaders, line.c_str());
      std::fill(s_histo.begin(), s_histo.end(), 0u);
    }
  }
  s_frame = frame;
}

unsigned RightFramebuffer() { return s_stereo && s_ownOk && s_hostFbo ? TwinFramebuffer(s_hostFbo) : 0; }
float ZeroPlane() { return s_convNow; }
}  // namespace glstereo
