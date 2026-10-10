// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "n22_scene.hpp"
#include <thread>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define N22_GL_CALL APIENTRY
#else
#define N22_GL_CALL
#endif
#include <GL/gl.h>

namespace n22 {
namespace glc {
// Post-1.1 registry constants, absent from the installed Windows gl.h.
inline constexpr GLenum Framebuffer=0x8d40,FramebufferBinding=0x8ca6;
inline constexpr GLenum ReadFramebufferBinding=0x8caa,ReadFramebuffer=0x8ca8,DrawFramebuffer=0x8ca9;
inline constexpr GLenum FramebufferComplete=0x8cd5,ColourAttachment0=0x8ce0,MaxTextureUnits=0x84e2;
inline constexpr GLenum ContextProfileMask=0x9126;
inline constexpr GLint CompatibilityProfileBit=2;
inline constexpr GLenum CurrentProgram=0x8b8d,ActiveTexture=0x84e0,Texture0=0x84c0;
inline constexpr GLenum FramebufferSrgb=0x8db9,Srgb8Alpha8=0x8c43,Texture3D=0x806f,TextureCube=0x8513;
inline constexpr GLenum Multisample=0x809d,SampleAlphaCoverage=0x809e,SampleAlphaOne=0x809f,SampleCoverage=0x80a0;
inline constexpr GLbitfield MultisampleBit=0x20000000u;
}
#define N22_GL_FUNCTIONS(X) \
 X(GetIntegerv,void,(GLenum,GLint *)) \
 X(GetError,GLenum,(void)) \
 X(GetString,const GLubyte *,(GLenum)) \
 X(PushAttrib,void,(GLbitfield)) X(PopAttrib,void,(void)) \
 X(MatrixMode,void,(GLenum)) X(PushMatrix,void,(void)) X(PopMatrix,void,(void)) \
 X(LoadMatrixf,void,(const GLfloat *)) X(Viewport,void,(GLint,GLint,GLsizei,GLsizei)) \
 X(Scissor,void,(GLint,GLint,GLsizei,GLsizei)) X(Enable,void,(GLenum)) X(Disable,void,(GLenum)) \
 X(DepthFunc,void,(GLenum)) X(DepthMask,void,(GLboolean)) X(DepthRange,void,(GLdouble,GLdouble)) \
 X(ClearDepth,void,(GLdouble)) X(ClearColor,void,(GLfloat,GLfloat,GLfloat,GLfloat)) \
 X(ColorMask,void,(GLboolean,GLboolean,GLboolean,GLboolean)) X(Clear,void,(GLbitfield)) \
 X(PolygonMode,void,(GLenum,GLenum)) X(Begin,void,(GLenum)) X(End,void,(void)) \
 X(Color4ub,void,(GLubyte,GLubyte,GLubyte,GLubyte)) X(Vertex3f,void,(GLfloat,GLfloat,GLfloat)) \
 X(Vertex4f,void,(GLfloat,GLfloat,GLfloat,GLfloat)) X(ActiveTexture,void,(GLenum)) \
 X(UseProgram,void,(GLuint)) X(BindFramebuffer,void,(GLenum,GLuint))
// Kept outside the core table only to make the EXT framebuffer fallback clear.
struct GlDispatch {
#define N22_DECLARE(name,ret,args) using name##Fn=ret(N22_GL_CALL *)args;name##Fn name=nullptr;
    N22_GL_FUNCTIONS(N22_DECLARE)
    using CheckFramebufferStatusFn=GLenum(N22_GL_CALL *)(GLenum);
    CheckFramebufferStatusFn CheckFramebufferStatus=nullptr;
    using DrawBufferFn=void(N22_GL_CALL *)(GLenum);
    DrawBufferFn DrawBuffer=nullptr;
#undef N22_DECLARE
};
enum class GlPhase {None,Entry,Preflight,State,Draw,Restore};
struct GlDiagnostic {GlPhase phase=GlPhase::None;GLenum error=GL_NO_ERROR;GLenum framebuffer_status=0,restore_error=GL_NO_ERROR;};
class GlRenderer {
public:
    // Resolves through the borrowed runtime device. Never creates a context,
    // loads a library or calls a GL command here. Function code must stay loaded.
    acvr_result initialize(const acvr_graphics_device &);
    acvr_result draw(const Frame &,const acvr_draw_info &);
    GlDiagnostic diagnostic() const {return diagnostic_;}
    static constexpr uint32_t required_capabilities() {return ACVR_CAP_REQUIRES_SHARED_DEPTH;}
private:
    GlDispatch gl_{};
    GlDiagnostic diagnostic_{};
    std::thread::id owner_{};
    bool ready_=false;
};
}
