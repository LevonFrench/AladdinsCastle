// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "n22_scene.hpp"
#include "n22_texture_plan.hpp"
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
inline constexpr GLenum RasterizerDiscard=0x8c89,DepthClamp=0x864f,SampleMask=0x8e51,ColourSum=0x8458;
inline constexpr GLenum TextureRectangle=0x84f5,MaxClipDistances=0x0d32,ClipDistance0=0x3000;
inline constexpr GLenum DepthAttachment=0x8d00,AttachmentObjectType=0x8cd0,AttachmentObjectName=0x8cd1;
inline constexpr std::array<GLenum,5> ModernCaps{FramebufferSrgb,RasterizerDiscard,DepthClamp,SampleMask,ColourSum};
inline constexpr GLbitfield MultisampleBit=0x20000000u;
inline constexpr GLenum PixelUnpackBuffer=0x88ec,PixelUnpackBinding=0x88ef,SamplerBinding=0x8919;
inline constexpr GLenum UnpackImageHeight=0x806e,UnpackSkipImages=0x806d,ClampToEdge=0x812f;
inline constexpr GLenum Combine=0x8570,CombineRgb=0x8571,CombineAlpha=0x8572,RgbScale=0x8573,PrimaryColour=0x8577;
inline constexpr GLenum Source0Rgb=0x8580,Source1Rgb=0x8581,Source0Alpha=0x8588,Source1Alpha=0x8589;
inline constexpr GLenum Operand0Rgb=0x8590,Operand1Rgb=0x8591,Operand0Alpha=0x8598,Operand1Alpha=0x8599;
inline constexpr GLenum Interpolate=0x8575,Constant=0x8576,Previous=0x8578,Source2Rgb=0x8582,Operand2Rgb=0x8592;
inline constexpr GLenum ClampFragmentColour=0x891b;
inline constexpr std::array<GLenum,17> EnvironmentParams{GL_TEXTURE_ENV_MODE,CombineRgb,CombineAlpha,RgbScale,GL_ALPHA_SCALE,
    Source0Rgb,Source1Rgb,Source2Rgb,Operand0Rgb,Operand1Rgb,Operand2Rgb,Source0Alpha,Source1Alpha,0x858a,Operand0Alpha,Operand1Alpha,0x859a};
inline constexpr std::array<GLenum,8> UnpackParams{GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_PIXELS,
    UnpackImageHeight,UnpackSkipImages,GL_UNPACK_SWAP_BYTES,GL_UNPACK_LSB_FIRST};
inline constexpr std::array<GLenum,9> TransferParams{GL_RED_SCALE,GL_GREEN_SCALE,GL_BLUE_SCALE,GL_ALPHA_SCALE,
    GL_RED_BIAS,GL_GREEN_BIAS,GL_BLUE_BIAS,GL_ALPHA_BIAS,GL_MAP_COLOR};
inline constexpr std::array<GLenum,8> ImagingCaps{0x80d0,0x80d1,0x80d2,0x8010,0x8011,0x8012,0x8024,0x802e};
inline constexpr GLenum ColourMatrix=0x80b1;
}
#define N22_GL_FUNCTIONS(X) \
 X(GetIntegerv,void,(GLenum,GLint *)) \
 X(GetError,GLenum,(void)) \
 X(GetString,const GLubyte *,(GLenum)) \
 X(PushAttrib,void,(GLbitfield)) X(PopAttrib,void,(void)) \
 X(MatrixMode,void,(GLenum)) X(PushMatrix,void,(void)) X(PopMatrix,void,(void)) \
 X(LoadMatrixf,void,(const GLfloat *)) X(Viewport,void,(GLint,GLint,GLsizei,GLsizei)) \
 X(Scissor,void,(GLint,GLint,GLsizei,GLsizei)) X(Enable,void,(GLenum)) X(Disable,void,(GLenum)) \
 X(IsEnabled,GLboolean,(GLenum)) \
 X(GetFramebufferAttachmentParameteriv,void,(GLenum,GLenum,GLenum,GLint *)) \
 X(DepthFunc,void,(GLenum)) X(DepthMask,void,(GLboolean)) X(DepthRange,void,(GLdouble,GLdouble)) \
 X(ClearDepth,void,(GLdouble)) X(ClearColor,void,(GLfloat,GLfloat,GLfloat,GLfloat)) \
 X(ColorMask,void,(GLboolean,GLboolean,GLboolean,GLboolean)) X(Clear,void,(GLbitfield)) \
 X(PolygonMode,void,(GLenum,GLenum)) X(Begin,void,(GLenum)) X(End,void,(void)) \
 X(Color4ub,void,(GLubyte,GLubyte,GLubyte,GLubyte)) X(Vertex3f,void,(GLfloat,GLfloat,GLfloat)) \
 X(Vertex4f,void,(GLfloat,GLfloat,GLfloat,GLfloat)) X(ActiveTexture,void,(GLenum)) \
 X(UseProgram,void,(GLuint)) X(BindFramebuffer,void,(GLenum,GLuint))
#define N22_GL_TEXTURE_FUNCTIONS(X) \
 X(GetFloatv,void,(GLenum,GLfloat *)) X(GenTextures,void,(GLsizei,GLuint *)) X(DeleteTextures,void,(GLsizei,const GLuint *)) \
 X(BindTexture,void,(GLenum,GLuint)) X(TexParameteri,void,(GLenum,GLenum,GLint)) \
 X(TexImage2D,void,(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void *)) \
 X(PixelStorei,void,(GLenum,GLint)) X(BindBuffer,void,(GLenum,GLuint)) X(BindSampler,void,(GLuint,GLuint)) \
 X(PixelTransferf,void,(GLenum,GLfloat)) \
 X(TexEnvi,void,(GLenum,GLenum,GLint)) X(TexEnvf,void,(GLenum,GLenum,GLfloat)) \
 X(TexEnvfv,void,(GLenum,GLenum,const GLfloat *)) X(GetTexEnviv,void,(GLenum,GLenum,GLint *)) X(GetTexEnvfv,void,(GLenum,GLenum,GLfloat *)) \
 X(ClampColor,void,(GLenum,GLenum)) \
 X(TexCoord2f,void,(GLfloat,GLfloat)) X(Color4f,void,(GLfloat,GLfloat,GLfloat,GLfloat)) X(ShadeModel,void,(GLenum))
// Kept outside the core table only to make the EXT framebuffer fallback clear.
struct GlDispatch {
#define N22_DECLARE(name,ret,args) using name##Fn=ret(N22_GL_CALL *)args;name##Fn name=nullptr;
    N22_GL_FUNCTIONS(N22_DECLARE)
    N22_GL_TEXTURE_FUNCTIONS(N22_DECLARE)
    using CheckFramebufferStatusFn=GLenum(N22_GL_CALL *)(GLenum);
    CheckFramebufferStatusFn CheckFramebufferStatus=nullptr;
    using DrawBufferFn=void(N22_GL_CALL *)(GLenum);
    DrawBufferFn DrawBuffer=nullptr;
#undef N22_DECLARE
};
enum class GlPhase {None,Entry,Preflight,State,Upload,Draw,Restore,Cleanup};
struct GlDiagnostic {GlPhase phase=GlPhase::None;GLenum error=GL_NO_ERROR;GLenum framebuffer_status=0,restore_error=GL_NO_ERROR,cleanup_error=GL_NO_ERROR;};
class GlRenderer {
public:
    GlRenderer()=default;
    GlRenderer(const GlRenderer &)=delete;
    GlRenderer &operator=(const GlRenderer &)=delete;
    GlRenderer(GlRenderer &&)=delete;
    GlRenderer &operator=(GlRenderer &&)=delete;
    // Resolves through the borrowed runtime device. Never creates a context,
    // loads a library or calls a GL command here. Function code must stay loaded.
    acvr_result initialize(const acvr_graphics_device &);
    acvr_result draw(const Frame &,const acvr_draw_info &);
    acvr_result release_frame(uint64_t);
    acvr_result shutdown(); // explicit while borrowed owner context is current
    bool owner_thread() const {return !ready_ || owner_==std::this_thread::get_id();}
    GlDiagnostic diagnostic() const {return diagnostic_;}
    static constexpr uint32_t required_capabilities() {return ACVR_CAP_REQUIRES_SHARED_DEPTH;}
private:
    GlDispatch gl_{};
    GlDiagnostic diagnostic_{};
    std::thread::id owner_{};
    bool ready_=false;
    const Frame *resource_frame_=nullptr; // identity only; never dereferenced after release
    uint64_t resource_frame_id_=0;
    TexturePlan texture_plan_;
    std::vector<GLuint> textures_;
};
}
