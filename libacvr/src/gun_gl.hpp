// SPDX-License-Identifier: MIT
#pragma once
#include "gun_model.hpp"
#include <thread>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define ACVR_GL_CALL APIENTRY
#else
#define ACVR_GL_CALL
#endif
#include <GL/gl.h>

namespace acvr {
// Internal programmable GL path. No bundled loader, context creation or platform
// library loading. The supplied get_proc must resolve core and extension names.
#define ACVR_GUN_GL_FUNCTIONS(X) \
 X(GetError,GLenum,(void)) X(GetIntegerv,void,(GLenum,GLint *)) \
 X(IsEnabled,GLboolean,(GLenum)) X(Enable,void,(GLenum)) X(Disable,void,(GLenum)) \
 X(CreateShader,GLuint,(GLenum)) X(ShaderSource,void,(GLuint,GLsizei,const char *const *,const GLint *)) \
 X(CompileShader,void,(GLuint)) X(GetShaderiv,void,(GLuint,GLenum,GLint *)) X(DeleteShader,void,(GLuint)) \
 X(CreateProgram,GLuint,(void)) X(AttachShader,void,(GLuint,GLuint)) X(LinkProgram,void,(GLuint)) \
 X(GetProgramiv,void,(GLuint,GLenum,GLint *)) X(DeleteProgram,void,(GLuint)) \
 X(GetUniformLocation,GLint,(GLuint,const char *)) X(UseProgram,void,(GLuint)) \
 X(UniformMatrix4fv,void,(GLint,GLsizei,GLboolean,const GLfloat *)) \
 X(GenVertexArrays,void,(GLsizei,GLuint *)) X(BindVertexArray,void,(GLuint)) X(DeleteVertexArrays,void,(GLsizei,const GLuint *)) \
 X(GenBuffers,void,(GLsizei,GLuint *)) X(BindBuffer,void,(GLenum,GLuint)) X(DeleteBuffers,void,(GLsizei,const GLuint *)) \
 X(BufferData,void,(GLenum,std::ptrdiff_t,const void *,GLenum)) \
 X(EnableVertexAttribArray,void,(GLuint)) X(VertexAttribPointer,void,(GLuint,GLint,GLenum,GLboolean,GLsizei,const void *)) \
 X(BindFramebuffer,void,(GLenum,GLuint)) X(CheckFramebufferStatus,GLenum,(GLenum)) \
 X(GetFramebufferAttachmentParameteriv,void,(GLenum,GLenum,GLenum,GLint *)) \
 X(PushAttrib,void,(GLbitfield)) X(PopAttrib,void,(void)) \
 X(Viewport,void,(GLint,GLint,GLsizei,GLsizei)) X(Scissor,void,(GLint,GLint,GLsizei,GLsizei)) \
 X(DepthMask,void,(GLboolean)) X(DepthFunc,void,(GLenum)) X(DepthRange,void,(GLdouble,GLdouble)) \
 X(ColorMask,void,(GLboolean,GLboolean,GLboolean,GLboolean)) X(PolygonMode,void,(GLenum,GLenum)) \
 X(BlendFunc,void,(GLenum,GLenum)) X(BlendEquation,void,(GLenum)) X(DrawArrays,void,(GLenum,GLint,GLsizei))
struct GunGlDispatch {
#define ACVR_GL_FIELD(name,ret,args) using name##Fn=ret(ACVR_GL_CALL *)args; name##Fn name=nullptr;
    ACVR_GUN_GL_FUNCTIONS(ACVR_GL_FIELD)
#undef ACVR_GL_FIELD
};
struct GunGlVertex { float position[3],colour[4]; };
// CPU packing is also the actual upload path, not a parallel test renderer.
acvr_result pack_gun_vertices(const GunDraw &,const acvr_eye &,std::vector<GunGlVertex> &,uint32_t &opaque_count);
class GunGlRenderer {
public:
    // Requires current desktop compatibility GL >=3.3. Compiles GLSL and creates
    // private VAO/VBO/program. No shaders or meshes are downloaded.
    acvr_result initialize(const acvr_graphics_device &);
    acvr_result draw(const acvr_draw_info &,const GunDraw &);
    // Owner must call while the borrowed context is still current, before host
    // destruction. Destructor deliberately never makes contextless GL calls.
    acvr_result shutdown();
    const std::string &diagnostic() const {return diagnostic_;}
private:
    GunGlDispatch gl_{};
    GLuint program_=0,vao_=0,buffer_=0;
    GLint matrix_=-1;
    bool ready_=false;
    std::thread::id owner_{};
    std::string diagnostic_;
};
}
