// SPDX-License-Identifier: GPL-3.0-only
// No GL library/context/window/GPU. Every dispatch address is a local CPU mock.
#include "n22_cpu_backend.hpp"
#include "n22_gl_renderer.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <future>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks=0;
void check(bool ok,const char *what) {++checks;if(!ok) throw std::runtime_error(what);}
template<class T> T record() {T out{};ACVR_INIT(&out);return out;}
struct Context {
    GLuint draw=9,read=10,program=45;
    GLenum active=n22::glc::Texture0+1,mode=GL_TEXTURE;
    std::array<GLint,4> viewport{3,4,90,80},scissor{7,8,70,60};
    std::set<GLenum> enabled{GL_BLEND,GL_SCISSOR_TEST,n22::glc::SampleCoverage,
        n22::glc::RasterizerDiscard,n22::glc::DepthClamp,n22::glc::ColourSum,n22::glc::SampleMask,
        n22::glc::FramebufferSrgb,n22::glc::ClipDistance0+7};
    GLenum depth_func=GL_GREATER,polygon=GL_LINE;
    GLboolean depth_mask=GL_FALSE;
    std::array<GLdouble,2> depth_range{.1,.8};GLdouble clear_depth=.3;
    std::array<GLfloat,4> clear_colour{.1f,.2f,.3f,.4f};
    std::array<GLboolean,4> colour_mask{GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE};
    std::map<GLenum,std::set<GLenum>> texture_enabled{{n22::glc::Texture0,{GL_TEXTURE_2D,n22::glc::TextureRectangle}},
        {n22::glc::Texture0+1,{n22::glc::TextureCube,GL_TEXTURE_GEN_R,n22::glc::TextureRectangle}}};
    std::map<GLenum,std::vector<std::array<float,16>>> matrices;
    std::map<GLuint,GLenum> draw_buffers{{7,GL_NONE},{9,GL_BACK}};
    GLuint unpack_buffer=33;
    std::map<GLenum,GLuint> texture_binding{{n22::glc::Texture0,55},{n22::glc::Texture0+1,56}},samplers{{n22::glc::Texture0,21},{n22::glc::Texture0+1,22}};
    std::map<GLenum,GLint> unpack{{GL_UNPACK_ALIGNMENT,8},{GL_UNPACK_ROW_LENGTH,77},{GL_UNPACK_SKIP_ROWS,3},{GL_UNPACK_SKIP_PIXELS,5},
        {n22::glc::UnpackImageHeight,99},{n22::glc::UnpackSkipImages,2},{GL_UNPACK_SWAP_BYTES,1},{GL_UNPACK_LSB_FIRST,1}};
    std::map<GLenum,std::array<float,16>> texture_matrices;
    std::map<GLenum,std::map<GLenum,float>> environment{{n22::glc::Texture0,{{GL_TEXTURE_ENV_MODE,static_cast<float>(GL_BLEND)},{n22::glc::RgbScale,2.f}}}};
    std::map<GLenum,std::array<float,4>> environment_colour{{n22::glc::Texture0+1,{.2f,.4f,.6f,.8f}}};
    std::map<GLenum,float> transfer{{GL_RED_SCALE,.25f},{GL_GREEN_SCALE,2.f},{GL_BLUE_SCALE,.5f},{GL_ALPHA_SCALE,.3f},
        {GL_RED_BIAS,.1f},{GL_GREEN_BIAS,.2f},{GL_BLUE_BIAS,.3f},{GL_ALPHA_BIAS,.4f},{GL_MAP_COLOR,1.f}};
    GLenum shade_model=GL_FLAT;
    GLenum fragment_clamp=GL_FALSE; // hostile: no intermediate combiner clamp
    std::array<float,4> current_colour{.7f,.6f,.5f,.4f};
    std::array<float,2> current_uv{.8f,.9f};
    Context() {
        std::array<float,16> identity{};for(size_t i=0;i<16;i+=5) identity[i]=1;
        matrices[GL_MODELVIEW]={identity};matrices[GL_PROJECTION]={identity};matrices[GL_TEXTURE]={identity};
        identity[12]=.75f;texture_matrices[n22::glc::Texture0]=identity;
        identity[12]=.33f;texture_matrices[n22::glc::Texture0+1]=identity;
        const std::array<GLint,17> values{GL_BLEND,GL_ADD,GL_MODULATE,2,2,
            n22::glc::Constant,n22::glc::Previous,GL_TEXTURE,GL_SRC_COLOR,GL_SRC_COLOR,GL_SRC_ALPHA,
            GL_TEXTURE,n22::glc::PrimaryColour,n22::glc::Constant,GL_ONE_MINUS_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA};
        for(size_t i=0;i<values.size();++i) environment[n22::glc::Texture0+1][n22::glc::EnvironmentParams[i]]=static_cast<float>(values[i]);
    }
};
struct Mock {
    struct Upload {uint32_t width=0,height=0;std::vector<uint8_t> bytes;std::map<GLenum,GLint> parameters;};
    Context state;
    std::vector<Context> attributes;
    std::vector<std::array<float,4>> vertices;
    std::vector<std::array<float,16>> loaded_projection,loaded_view;
    std::vector<std::array<GLint,4>> clears;
    std::vector<std::string> calls;
    std::set<std::string> missing;
    GLenum error=GL_NO_ERROR,status=n22::glc::FramebufferComplete;
    GLint depth=24,profile=n22::glc::CompatibilityProfileBit;
    GLint colour_type=GL_TEXTURE,depth_type=GL_TEXTURE,colour_name=41,depth_name=42;
    bool fail_attachment_query=false;
    bool fail_vertex=false,fail_projection_push=false,fail_restore=false;
    GLint max_texture=2048,texture_units=2;
    GLuint next_texture=100;
    size_t upload_count=0,fail_upload_number=0;
    bool fail_generation=false,fail_delete=false,in_begin=false,fail_fog_query=false,fail_clamp=false;
    bool imaging=false;
    std::set<GLuint> live_textures;
    std::vector<GLuint> generated,deleted,vertex_textures;
    std::map<GLuint,Upload> uploads;
    std::vector<std::array<float,4>> colours;
    std::vector<std::array<float,2>> texcoords;
    std::vector<uint32_t> composed_samples;
    std::vector<float> composed_alpha;
    explicit Mock() {calls.reserve(10000);vertices.reserve(10000);}
};
thread_local Mock *current;
void log(const char *s) {current->calls.emplace_back(s);}
void N22_GL_CALL get_integer(GLenum p,GLint *out) {
    log("GetInteger");auto &s=current->state;
    switch(p) {
    case n22::glc::FramebufferBinding:*out=static_cast<GLint>(s.draw);break;
    case n22::glc::ReadFramebufferBinding:*out=static_cast<GLint>(s.read);break;
    case n22::glc::CurrentProgram:*out=static_cast<GLint>(s.program);break;
    case n22::glc::ActiveTexture:*out=static_cast<GLint>(s.active);break;
    case GL_MATRIX_MODE:*out=static_cast<GLint>(s.mode);break;
    case GL_ATTRIB_STACK_DEPTH:*out=static_cast<GLint>(current->attributes.size());break;
    case GL_MAX_ATTRIB_STACK_DEPTH:*out=16;break;
    case GL_PROJECTION_STACK_DEPTH:*out=static_cast<GLint>(s.matrices[GL_PROJECTION].size());break;
    case GL_MODELVIEW_STACK_DEPTH:*out=static_cast<GLint>(s.matrices[GL_MODELVIEW].size());break;
    case GL_MAX_PROJECTION_STACK_DEPTH:*out=4;break;
    case GL_MAX_MODELVIEW_STACK_DEPTH:*out=32;break;
    case n22::glc::MaxTextureUnits:*out=current->texture_units;break;
    case n22::glc::MaxClipDistances:*out=8;break;
    case GL_DEPTH_BITS:*out=current->depth;break;
    case GL_DOUBLEBUFFER:*out=1;break;
    case n22::glc::ContextProfileMask:*out=current->profile;break;
    case GL_MAX_TEXTURE_SIZE:*out=current->max_texture;break;
    case n22::glc::PixelUnpackBinding:*out=static_cast<GLint>(s.unpack_buffer);break;
    case GL_TEXTURE_BINDING_2D:*out=static_cast<GLint>(s.texture_binding[s.active]);break;
    case n22::glc::SamplerBinding:*out=static_cast<GLint>(s.samplers[s.active]);break;
    case n22::glc::ClampFragmentColour:*out=static_cast<GLint>(s.fragment_clamp);break;
    default:if(s.unpack.count(p)) *out=s.unpack[p];else {*out=0;current->error=GL_INVALID_ENUM;}break;
    }
}
GLenum N22_GL_CALL get_error() {log("GetError");auto e=current->error;current->error=GL_NO_ERROR;return e;}
const GLubyte *N22_GL_CALL get_string(GLenum name) {
    log("GetString");return reinterpret_cast<const GLubyte *>(name==GL_VERSION?"3.3 MOCK":current->imaging?"GL_ARB_framebuffer_object GL_EXT_framebuffer_sRGB GL_ARB_imaging":"GL_ARB_framebuffer_object GL_EXT_framebuffer_sRGB");
}
void N22_GL_CALL push_attrib(GLbitfield) {log("PushAttrib");current->attributes.push_back(current->state);}
void N22_GL_CALL pop_attrib() {
    log("PopAttrib");auto &s=current->state;auto before=current->attributes.back();current->attributes.pop_back();
    // Deliberately do not restore modern caps, clip distances or rectangles via
    // legacy attribs: the implementation must restore these explicitly.
    for(auto cap:n22::glc::ModernCaps) {before.enabled.erase(cap);if(s.enabled.count(cap)) before.enabled.insert(cap);}
    for(GLenum i=0;i<8;++i) {const auto cap=n22::glc::ClipDistance0+i;before.enabled.erase(cap);if(s.enabled.count(cap)) before.enabled.insert(cap);}
    for(auto &unit:before.texture_enabled) {
        unit.second.erase(n22::glc::TextureRectangle);
        if(s.texture_enabled[unit.first].count(n22::glc::TextureRectangle)) unit.second.insert(n22::glc::TextureRectangle);
    }
    // Matrix stacks and framebuffer/program/active selector are restored explicitly.
    s.viewport=before.viewport;s.scissor=before.scissor;s.enabled=before.enabled;s.draw_buffers=before.draw_buffers;
    s.depth_func=before.depth_func;s.depth_mask=before.depth_mask;s.depth_range=before.depth_range;s.clear_depth=before.clear_depth;
    s.clear_colour=before.clear_colour;s.colour_mask=before.colour_mask;s.polygon=before.polygon;s.texture_enabled=before.texture_enabled;
    // Unit1 env/constant intentionally need explicit restoration as well.
    const auto fog_env=s.environment[n22::glc::Texture0+1];const auto fog_colour=s.environment_colour[n22::glc::Texture0+1];
    s.environment=before.environment;s.environment[n22::glc::Texture0+1]=fog_env;
    s.environment_colour=before.environment_colour;s.environment_colour[n22::glc::Texture0+1]=fog_colour;
    s.shade_model=before.shade_model;s.transfer=before.transfer;
    s.current_colour=before.current_colour;s.current_uv=before.current_uv;
    if(current->fail_restore) current->error=GL_INVALID_VALUE;
}
void N22_GL_CALL matrix_mode(GLenum mode) {log("MatrixMode");current->state.mode=mode;}
void N22_GL_CALL push_matrix() {
    log("PushMatrix");auto &s=current->state;
    if(current->fail_projection_push && s.mode==GL_PROJECTION) {current->error=GL_STACK_OVERFLOW;return;}
    auto &stack=s.matrices[s.mode];stack.push_back(stack.back());
}
void N22_GL_CALL pop_matrix() {log("PopMatrix");current->state.matrices[current->state.mode].pop_back();}
void N22_GL_CALL load_matrix(const GLfloat *v) {
    log("LoadMatrix");auto &s=current->state;
    if(s.mode==GL_TEXTURE) {std::copy(v,v+16,s.texture_matrices[s.active].begin());return;}
    auto &m=s.matrices[s.mode].back();std::copy(v,v+16,m.begin());
    if(s.mode==GL_PROJECTION) current->loaded_projection.push_back(m);else current->loaded_view.push_back(m);
}
void N22_GL_CALL viewport(GLint x,GLint y,GLsizei w,GLsizei h) {log("Viewport");current->state.viewport={x,y,w,h};}
void N22_GL_CALL scissor(GLint x,GLint y,GLsizei w,GLsizei h) {log("Scissor");current->state.scissor={x,y,w,h};}
void N22_GL_CALL enable(GLenum cap) {log("Enable");if(cap==n22::glc::TextureRectangle || cap==GL_TEXTURE_2D) current->state.texture_enabled[current->state.active].insert(cap);else current->state.enabled.insert(cap);}
void N22_GL_CALL disable(GLenum cap) {log("Disable");current->state.enabled.erase(cap);current->state.texture_enabled[current->state.active].erase(cap);}
GLboolean N22_GL_CALL is_enabled(GLenum cap) {
    log("IsEnabled");return (cap==n22::glc::TextureRectangle?current->state.texture_enabled[current->state.active].count(cap):current->state.enabled.count(cap))?GL_TRUE:GL_FALSE;
}
void N22_GL_CALL depth_func(GLenum value) {log("DepthFunc");current->state.depth_func=value;if(value!=GL_LEQUAL) throw std::runtime_error("forward depth policy");}
void N22_GL_CALL depth_mask(GLboolean value) {log("DepthMask");current->state.depth_mask=value;if(value!=GL_TRUE) throw std::runtime_error("depth writes");}
void N22_GL_CALL depth_range(GLdouble a,GLdouble b) {log("DepthRange");current->state.depth_range={a,b};if(a!=0 || b!=1) throw std::runtime_error("clip depth range");}
void N22_GL_CALL clear_depth(GLdouble d) {log("ClearDepth");current->state.clear_depth=d;if(d!=1) throw std::runtime_error("forward depth clear");}
void N22_GL_CALL clear_colour(GLfloat a,GLfloat b,GLfloat c,GLfloat d) {log("ClearColour");current->state.clear_colour={a,b,c,d};}
void N22_GL_CALL colour_mask(GLboolean a,GLboolean b,GLboolean c,GLboolean d) {log("ColourMask");current->state.colour_mask={a,b,c,d};}
void N22_GL_CALL clear(GLbitfield bits) {
    log("Clear");if(bits!=(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT) || !current->state.enabled.count(GL_SCISSOR_TEST)) throw std::runtime_error("clear must use scoped colour/depth rectangle");
    current->clears.push_back(current->state.scissor);
}
void N22_GL_CALL polygon_mode(GLenum,GLenum mode) {log("PolygonMode");current->state.polygon=mode;}
void N22_GL_CALL begin(GLenum mode) {log("Begin");if(mode!=GL_TRIANGLES || current->in_begin) throw std::runtime_error("triangle stream");current->in_begin=true;}
void N22_GL_CALL end() {log("End");current->in_begin=false;}
void N22_GL_CALL colour(GLubyte,GLubyte,GLubyte,GLubyte) {log("Colour");}
void normalized() {
    for(auto cap:n22::glc::ModernCaps) if(current->state.enabled.count(cap)) throw std::runtime_error("inherited modern state must be disabled");
    for(GLenum i=0;i<8;++i) if(current->state.enabled.count(n22::glc::ClipDistance0+i)) throw std::runtime_error("all clip distances must be disabled");
    const bool textured=current->state.texture_enabled[n22::glc::Texture0].count(GL_TEXTURE_2D)!=0;
    const bool fog=current->state.texture_enabled[n22::glc::Texture0+1].count(GL_TEXTURE_2D)!=0;
    for(const auto &u:current->state.texture_enabled) for(auto cap:u.second)
        if((u.first!=n22::glc::Texture0 && !(fog && u.first==n22::glc::Texture0+1)) || cap!=GL_TEXTURE_2D) throw std::runtime_error("only explicit material/fog units may be enabled");
    if(textured) {
        const auto &s=current->state;
        if(s.active!=n22::glc::Texture0 || s.samplers.at(n22::glc::Texture0)!=0 || s.texture_matrices.at(n22::glc::Texture0)[12]!=0 ||
           s.environment.at(n22::glc::Texture0).at(n22::glc::RgbScale)!=4 || s.shade_model!=GL_SMOOTH ||
           !current->uploads.count(s.texture_binding.at(n22::glc::Texture0))) throw std::runtime_error("material state must be explicit");
        const auto &texture=current->uploads.at(s.texture_binding.at(n22::glc::Texture0));
        const auto x=std::min(texture.width-1,static_cast<uint32_t>(std::max(0.f,s.current_uv[0])*static_cast<float>(texture.width)));
        const auto y=std::min(texture.height-1,static_cast<uint32_t>(std::max(0.f,s.current_uv[1])*static_cast<float>(texture.height)));
        if(fog) {
            const auto &env=s.environment.at(n22::glc::Texture0+1);
            if(s.fragment_clamp!=GL_TRUE || s.samplers.at(n22::glc::Texture0+1)!=0 || s.texture_matrices.at(n22::glc::Texture0+1)[12]!=0 ||
               env.at(GL_TEXTURE_ENV_MODE)!=n22::glc::Combine || env.at(n22::glc::CombineRgb)!=n22::glc::Interpolate ||
               env.at(n22::glc::RgbScale)!=1 || env.at(GL_ALPHA_SCALE)!=1 || env.at(n22::glc::Source0Rgb)!=n22::glc::Previous ||
               env.at(n22::glc::Source1Rgb)!=n22::glc::Constant || env.at(n22::glc::Source2Rgb)!=n22::glc::PrimaryColour ||
               env.at(n22::glc::Operand0Rgb)!=GL_SRC_COLOR || env.at(n22::glc::Operand1Rgb)!=GL_SRC_COLOR || env.at(n22::glc::Operand2Rgb)!=GL_SRC_ALPHA ||
               env.at(n22::glc::CombineAlpha)!=GL_REPLACE || env.at(n22::glc::Source0Alpha)!=n22::glc::Previous ||
               env.at(n22::glc::Operand0Alpha)!=GL_SRC_ALPHA || s.environment.at(n22::glc::Texture0).at(n22::glc::CombineAlpha)!=GL_REPLACE ||
               s.environment.at(n22::glc::Texture0).at(n22::glc::Source0Alpha)!=GL_TEXTURE)
                throw std::runtime_error("explicit shade then fog, opaque alpha combiner required");
            const auto &dummy=current->uploads.at(s.texture_binding.at(n22::glc::Texture0+1));
            if(dummy.width!=1 || dummy.height!=1 || dummy.bytes!=std::vector<uint8_t>{255,255,255,255}) throw std::runtime_error("complete private fog texture");
        }
        uint32_t rgb=0;for(size_t c=0;c<3;++c) {
            float value=std::clamp(texture.bytes[(size_t(y)*texture.width+x)*4+c]*s.current_colour[c]*4,0.f,255.f);
            if(fog) value=value*s.current_colour[3]+s.environment_colour.at(n22::glc::Texture0+1)[c]*255*(1-s.current_colour[3]);
            rgb|=static_cast<uint32_t>(std::clamp(value,0.f,255.f))<<((2-c)*8);
        }
        current->composed_samples.push_back(rgb);
        current->composed_alpha.push_back(s.environment.at(n22::glc::Texture0).at(n22::glc::CombineAlpha)==GL_REPLACE?1:s.current_colour[3]);
    }
    current->vertex_textures.push_back(textured?current->state.texture_binding[n22::glc::Texture0]:0);
}
void N22_GL_CALL vertex3(GLfloat x,GLfloat y,GLfloat z) {
    log("Vertex3");normalized();
    current->vertices.push_back({x,y,z,1});if(current->fail_vertex) current->error=GL_INVALID_OPERATION;
}
void N22_GL_CALL vertex4(GLfloat x,GLfloat y,GLfloat z,GLfloat w) {log("Vertex4");normalized();current->vertices.push_back({x,y,z,w});}
void N22_GL_CALL active_texture(GLenum unit) {log("ActiveTexture");current->state.active=unit;}
void N22_GL_CALL use_program(GLuint program) {log("UseProgram");current->state.program=program;}
void N22_GL_CALL bind(GLenum target,GLuint fbo) {
    log("BindFramebuffer");if(target!=n22::glc::ReadFramebuffer) current->state.draw=fbo;
    if(target!=n22::glc::DrawFramebuffer) current->state.read=fbo;
}
GLenum N22_GL_CALL status(GLenum) {log("FramebufferStatus");return current->status;}
void N22_GL_CALL attachment(GLenum target,GLenum at,GLenum param,GLint *out) {
    log("Attachment");if(target!=n22::glc::DrawFramebuffer || current->state.draw!=7) throw std::runtime_error("query supplied draw FBO");
    const bool depth=at==n22::glc::DepthAttachment;
    if(param==n22::glc::AttachmentObjectType) *out=depth?current->depth_type:current->colour_type;
    else if(param==n22::glc::AttachmentObjectName) *out=depth?current->depth_name:current->colour_name;
    else throw std::runtime_error("attachment parameter");
    if(current->fail_attachment_query) current->error=GL_INVALID_OPERATION;
}
void N22_GL_CALL draw_buffer(GLenum buffer) {log("DrawBuffer");current->state.draw_buffers[current->state.draw]=buffer;}
void N22_GL_CALL get_float(GLenum what,GLfloat *out) {
    log("GetFloat");
    if(what==GL_TEXTURE_MATRIX) {const auto &m=current->state.texture_matrices[current->state.active];std::copy(m.begin(),m.end(),out);}
    else if(what==n22::glc::ColourMatrix) {std::fill(out,out+16,0.f);for(size_t i=0;i<16;i+=5) out[i]=1;}
    else if((what>=0x801c && what<=0x8023) || (what>=0x80b4 && what<=0x80bb)) *out=(what-(what<0x80b4?0x801c:0x80b4))<4?1.f:0.f;
    else throw std::runtime_error("float query");
}
void N22_GL_CALL gen_textures(GLsizei n,GLuint *out) {
    log("GenTextures");for(GLsizei i=0;i<n;++i) {
        if(current->fail_generation && i>0) {out[i]=0;continue;}
        out[i]=current->next_texture++;current->live_textures.insert(out[i]);current->generated.push_back(out[i]);
    }
    if(current->fail_generation) current->error=GL_OUT_OF_MEMORY;
}
void N22_GL_CALL delete_textures(GLsizei n,const GLuint *names) {
    log("DeleteTextures");for(GLsizei i=0;i<n;++i) {
        if(!current->live_textures.erase(names[i])) throw std::runtime_error("delete only owned live textures exactly once");
        current->deleted.push_back(names[i]);current->uploads.erase(names[i]);
    }
    if(current->fail_delete) current->error=GL_INVALID_VALUE;
}
void N22_GL_CALL bind_texture(GLenum target,GLuint name) {log("BindTexture");if(current->in_begin || target!=GL_TEXTURE_2D) throw std::runtime_error("texture binds outside Begin");current->state.texture_binding[current->state.active]=name;}
void N22_GL_CALL tex_parameter(GLenum target,GLenum pname,GLint value) {log("TexParameter");if(target!=GL_TEXTURE_2D) throw std::runtime_error("2D texture parameter");current->uploads[current->state.texture_binding[current->state.active]].parameters[pname]=value;}
void N22_GL_CALL tex_image(GLenum target,GLint level,GLint internal,GLsizei w,GLsizei h,GLint border,GLenum format,GLenum type,const void *data) {
    log("TexImage");auto &s=current->state;
    if(target!=GL_TEXTURE_2D || level || internal!=GL_RGBA8 || border || format!=GL_RGBA || type!=GL_UNSIGNED_BYTE ||
       s.unpack_buffer || s.active!=n22::glc::Texture0 || s.samplers[s.active]) throw std::runtime_error("normalized owned RGBA upload");
    for(auto p:n22::glc::UnpackParams) if(s.unpack[p]!=(p==GL_UNPACK_ALIGNMENT?1:0)) throw std::runtime_error("hostile unpack state normalized");
    for(size_t i=0;i<n22::glc::TransferParams.size();++i) if(s.transfer[n22::glc::TransferParams[i]]!=(i<4?1.f:0.f)) throw std::runtime_error("pixel transfer normalized");
    auto &upload=current->uploads[s.texture_binding[s.active]];upload.width=static_cast<uint32_t>(w);upload.height=static_cast<uint32_t>(h);
    const auto *bytes=static_cast<const uint8_t *>(data);upload.bytes.assign(bytes,bytes+size_t(w)*size_t(h)*4);
    ++current->upload_count;if(current->upload_count==current->fail_upload_number) current->error=GL_OUT_OF_MEMORY;
}
void N22_GL_CALL pixel_store(GLenum p,GLint value) {log("PixelStore");current->state.unpack[p]=value;}
void N22_GL_CALL pixel_transfer(GLenum p,GLfloat value) {log("PixelTransfer");current->state.transfer[p]=value;}
void N22_GL_CALL bind_buffer(GLenum target,GLuint name) {log("BindBuffer");if(target!=n22::glc::PixelUnpackBuffer) throw std::runtime_error("only unpack buffer borrowed");current->state.unpack_buffer=name;}
void N22_GL_CALL bind_sampler(GLuint unit,GLuint name) {log("BindSampler");current->state.samplers[n22::glc::Texture0+unit]=name;}
void N22_GL_CALL tex_env_i(GLenum target,GLenum p,GLint value) {log("TexEnvI");if(target!=GL_TEXTURE_ENV) throw std::runtime_error("texture environment");current->state.environment[current->state.active][p]=static_cast<float>(value);}
void N22_GL_CALL tex_env_f(GLenum target,GLenum p,GLfloat value) {log("TexEnvF");if(target!=GL_TEXTURE_ENV) throw std::runtime_error("texture environment");current->state.environment[current->state.active][p]=value;}
void N22_GL_CALL tex_env_fv(GLenum target,GLenum p,const GLfloat *value) {
    log("TexEnvFV");if(current->in_begin || target!=GL_TEXTURE_ENV || p!=GL_TEXTURE_ENV_COLOR) throw std::runtime_error("fog constant outside Begin");
    std::copy(value,value+4,current->state.environment_colour[current->state.active].begin());
}
void N22_GL_CALL get_tex_env_i(GLenum target,GLenum p,GLint *value) {
    log("GetTexEnvI");if(target!=GL_TEXTURE_ENV) throw std::runtime_error("env query");
    *value=static_cast<GLint>(current->state.environment.at(current->state.active).at(p));
    if(current->fail_fog_query) current->error=GL_INVALID_ENUM;
}
void N22_GL_CALL get_tex_env_fv(GLenum target,GLenum p,GLfloat *value) {
    log("GetTexEnvFV");if(target!=GL_TEXTURE_ENV || p!=GL_TEXTURE_ENV_COLOR) throw std::runtime_error("constant query");
    const auto &colour=current->state.environment_colour.at(current->state.active);std::copy(colour.begin(),colour.end(),value);
}
void N22_GL_CALL clamp_colour(GLenum target,GLenum value) {
    log("ClampColour");if(target!=n22::glc::ClampFragmentColour) throw std::runtime_error("fragment clamp only");
    current->state.fragment_clamp=value;
    if(current->fail_clamp && value==GL_TRUE) current->error=GL_INVALID_OPERATION;
}
void N22_GL_CALL tex_coord(GLfloat u,GLfloat v) {log("TexCoord");current->texcoords.push_back({u,v});current->state.current_uv={u,v};}
void N22_GL_CALL colour_f(GLfloat r,GLfloat g,GLfloat b,GLfloat a) {log("ColourF");current->colours.push_back({r,g,b,a});current->state.current_colour={r,g,b,a};}
void N22_GL_CALL shade_model(GLenum model) {log("ShadeModel");current->state.shade_model=model;}
template<class T> void *address(T fn) {void *out=nullptr;static_assert(sizeof(out)==sizeof(fn));std::memcpy(&out,&fn,sizeof(out));return out;}
void *ACVR_CALL resolve(void *user,const char *name) {
    auto &m=*static_cast<Mock *>(user);if(m.missing.count(name)) return nullptr;
    const std::map<std::string,void *> table={
        {"glGetIntegerv",address(get_integer)},{"glGetError",address(get_error)},{"glGetString",address(get_string)},
        {"glPushAttrib",address(push_attrib)},{"glPopAttrib",address(pop_attrib)},{"glMatrixMode",address(matrix_mode)},
        {"glPushMatrix",address(push_matrix)},{"glPopMatrix",address(pop_matrix)},{"glLoadMatrixf",address(load_matrix)},
        {"glViewport",address(viewport)},{"glScissor",address(scissor)},{"glEnable",address(enable)},{"glDisable",address(disable)},
        {"glIsEnabled",address(is_enabled)},{"glGetFramebufferAttachmentParameteriv",address(attachment)},
        {"glGetFramebufferAttachmentParameterivEXT",address(attachment)},
        {"glDepthFunc",address(depth_func)},{"glDepthMask",address(depth_mask)},{"glDepthRange",address(depth_range)},
        {"glClearDepth",address(clear_depth)},{"glClearColor",address(clear_colour)},{"glColorMask",address(colour_mask)},
        {"glClear",address(clear)},{"glPolygonMode",address(polygon_mode)},{"glBegin",address(begin)},{"glEnd",address(end)},
        {"glColor4ub",address(colour)},{"glVertex3f",address(vertex3)},{"glVertex4f",address(vertex4)},
        {"glActiveTexture",address(active_texture)},{"glActiveTextureARB",address(active_texture)},{"glUseProgram",address(use_program)},
        {"glBindFramebuffer",address(bind)},{"glBindFramebufferEXT",address(bind)},
        {"glCheckFramebufferStatus",address(status)},{"glCheckFramebufferStatusEXT",address(status)},{"glDrawBuffer",address(draw_buffer)}};
    const std::map<std::string,void *> texture_table={{"glGetFloatv",address(get_float)},{"glGenTextures",address(gen_textures)},{"glDeleteTextures",address(delete_textures)},
        {"glBindTexture",address(bind_texture)},{"glTexParameteri",address(tex_parameter)},{"glTexImage2D",address(tex_image)},
        {"glPixelStorei",address(pixel_store)},{"glBindBuffer",address(bind_buffer)},{"glBindSampler",address(bind_sampler)},
        {"glPixelTransferf",address(pixel_transfer)},
        {"glTexEnvi",address(tex_env_i)},{"glTexEnvf",address(tex_env_f)},{"glTexEnvfv",address(tex_env_fv)},
        {"glGetTexEnviv",address(get_tex_env_i)},{"glGetTexEnvfv",address(get_tex_env_fv)},
        {"glClampColor",address(clamp_colour)},
        {"glTexCoord2f",address(tex_coord)},{"glColor4f",address(colour_f)},{"glShadeModel",address(shade_model)}};
    const auto found=table.find(name);if(found!=table.end()) return found->second;
    const auto texture_found=texture_table.find(name);return texture_found==texture_table.end()?nullptr:texture_found->second;
}
acvr_graphics_device device(Mock &m) {auto d=record<acvr_graphics_device>();d.api=ACVR_GRAPHICS_GL;d.flags=ACVR_DEVICE_GL_COMPATIBILITY;d.context=1;d.get_proc=resolve;d.proc_user=&m;return d;}
acvr_draw_info draw_info(acvr_eye &e) {
    auto d=record<acvr_draw_info>();d.frame_id=1;ACVR_INIT(&d.target);
    d.target.api=ACVR_GRAPHICS_GL;d.target.width=640;d.target.height=240;d.target.framebuffer=7;
    d.target.colour_image=41;d.target.depth_image=42;d.target.depth_format=0x81a6;d.target.colour_format=GL_RGBA8;
    d.target.array_layers=1;d.target.sample_count=1;d.view_count=1;d.view_stride=sizeof(e);d.views=&e;return d;
}
void same_state(const Context &a,const Context &b) {
    check(a.draw==b.draw && a.read==b.read && a.program==b.program && a.active==b.active && a.mode==b.mode,"FBO/read FBO/program/selector ownership restored");
    check(a.viewport==b.viewport && a.scissor==b.scissor && a.enabled==b.enabled && a.draw_buffers==b.draw_buffers,"server attributes and target draw buffer restored");
    check(a.matrices==b.matrices,"matrix values and stack depth restored");
    check(a.depth_func==b.depth_func && a.depth_mask==b.depth_mask && a.depth_range==b.depth_range && a.clear_depth==b.clear_depth,
        "depth policy and clear value restored without clearing target after draw");
    check(a.clear_colour==b.clear_colour && a.colour_mask==b.colour_mask && a.polygon==b.polygon && a.texture_enabled==b.texture_enabled,
        "colour/raster/multitexture context state restored");
    check(a.unpack_buffer==b.unpack_buffer && a.unpack==b.unpack && a.texture_binding==b.texture_binding && a.samplers==b.samplers,
        "unit0 binding/samplers and unpack PBO/pixel state restored");
    check(a.texture_matrices==b.texture_matrices && a.environment==b.environment && a.environment_colour==b.environment_colour && a.shade_model==b.shade_model && a.transfer==b.transfer,"texture matrices/env/constant, pixel transfer and smooth shade state restored");
    check(a.current_colour==b.current_colour && a.current_uv==b.current_uv,"current colour and UV attributes restored");
    check(a.fragment_clamp==b.fragment_clamp,"explicit fragment clamping state restored");
}
void direct_dispatch() {
    Mock m;current=&m;auto d=device(m);n22::GlRenderer renderer;
    auto bad=d;bad.flags=0;check(renderer.initialize(bad)==ACVR_UNSUPPORTED && m.calls.empty(),"core-only device rejected without graphics commands");
    bad.flags=ACVR_DEVICE_GLES|ACVR_DEVICE_GL_COMPATIBILITY;
    check(renderer.initialize(bad)==ACVR_UNSUPPORTED,"GLES/compat contradiction rejected");
    bad=d;bad.context=0;check(renderer.initialize(bad)==ACVR_BAD_ARGUMENT && m.calls.empty(),"missing runtime context rejected without GL commands");
    m.missing={"glBindFramebuffer","glCheckFramebufferStatus","glGetFramebufferAttachmentParameteriv"};
    check(renderer.initialize(d)==ACVR_OK && m.calls.empty(),"EXT fallbacks resolve without GL execution");
    n22::Frame frame;check(n22::prepare(n22::synthetic_cube(),1,frame)==ACVR_OK,"synthetic scene");
    auto e=n22::desktop_eye(0,-.032f,4,320,240);auto info=draw_info(e);auto original=m.state;
    check(renderer.draw(frame,info)==ACVR_OK,"actual scene draw through CPU dispatch mocks");same_state(original,m.state);
    check(m.state.enabled.count(n22::glc::RasterizerDiscard) && m.state.enabled.count(n22::glc::ClipDistance0+7),"hostile modern and upper clip enables explicitly restored");
    check(m.vertices.size()==36 && m.clears==std::vector<std::array<GLint,4>>{{0,0,320,240}},"cube vertices and scoped clear");
    check(m.loaded_view.back()[12]==e.view_from_scene[12] && m.loaded_projection.back()[8]==e.projection_from_view[8],"supplied eye matrices used directly");
    auto old_vertices=m.vertices;auto old_id=frame.id;e.eye_index=1;e.rect_x=320;info.display_id=2;
    check(renderer.draw(frame,info)==ACVR_OK && frame.id==old_id && m.vertices.size()==72,"second eye reuses immutable frame");same_state(original,m.state);
    check(m.clears.back()==std::array<GLint,4>{320,0,320,240},"right eye clear preserves left rectangle");
    e.view_from_scene[12]=-.2f;check(renderer.draw(frame,info)==ACVR_OK && m.loaded_view.back()[12]==-.2f,"latest pose replay");
    check(m.vertices[36]==old_vertices[0],"replay does not change scene coordinates");
    size_t before=m.calls.size();info.target.depth_image=0;
    check(renderer.draw(frame,info)==ACVR_UNSUPPORTED && m.calls.size()==before,"missing shared depth rejected before commands");
    info.target.depth_image=42;info.target.colour_image=0;
    check(renderer.draw(frame,info)==ACVR_UNSUPPORTED && m.calls.size()==before,"missing colour image rejected before commands");
    info.target.colour_image=uint64_t(UINT32_MAX)+1;
    check(renderer.draw(frame,info)==ACVR_BAD_ARGUMENT && m.calls.size()==before,"oversized colour image rejected before commands");
    info.target.colour_image=41;info.target.depth_image=uint64_t(UINT32_MAX)+1;
    check(renderer.draw(frame,info)==ACVR_BAD_ARGUMENT && m.calls.size()==before,"oversized depth image rejected before commands");
    info.target.depth_image=42;info.frame_id=2;
    check(renderer.draw(frame,info)==ACVR_BAD_STATE && m.calls.size()==before,"wrong lease frame id rejected before commands");
    info.frame_id=1;e.rect_x=640;
    check(renderer.draw(frame,info)==ACVR_BAD_ARGUMENT && m.calls.size()==before,"outside viewport rejected before commands");
    e.rect_x=320;m.profile=1;before=m.clears.size();
    check(renderer.draw(frame,info)==ACVR_UNSUPPORTED && m.clears.size()==before,"claimed compatibility checked against profile");
    m.profile=2;m.depth=0;
    check(renderer.draw(frame,info)==ACVR_UNSUPPORTED && m.clears.size()==before,"absent actual attached depth rejected without clearing");same_state(original,m.state);
    m.depth=24;
    for(auto member:{&Mock::colour_name,&Mock::depth_name,&Mock::colour_type,&Mock::depth_type}) {
        const auto prior=m.*member;m.*member=123;
        check(renderer.draw(frame,info)==ACVR_BAD_ARGUMENT && m.clears.size()==before,"mismatched attachment identity/type rejected before clear");same_state(original,m.state);
        m.*member=prior;
    }
    m.fail_attachment_query=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && m.clears.size()==before && renderer.diagnostic().error==GL_INVALID_OPERATION,"attachment query error rejects target before clear");same_state(original,m.state);
    m.fail_attachment_query=false;m.status=0x8cd6;
    check(renderer.draw(frame,info)==ACVR_BAD_STATE && renderer.diagnostic().framebuffer_status==0x8cd6,"incomplete framebuffer surfaced");same_state(original,m.state);
    m.status=n22::glc::FramebufferComplete;m.error=GL_INVALID_ENUM;before=m.calls.size();
    check(renderer.draw(frame,info)==ACVR_BAD_STATE && m.calls.size()==before+1 && renderer.diagnostic().phase==n22::GlPhase::Entry,"caller error captured without draining or drawing");
    m.fail_projection_push=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && renderer.diagnostic().error==GL_STACK_OVERFLOW,"failed push surfaced");same_state(original,m.state);
    m.fail_projection_push=false;m.fail_vertex=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && renderer.diagnostic().phase==n22::GlPhase::Draw,"draw error surfaced");same_state(original,m.state);
    m.fail_restore=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && renderer.diagnostic().error==GL_INVALID_OPERATION &&
          renderer.diagnostic().restore_error==GL_INVALID_VALUE,"draw and restore errors separately retained");same_state(original,m.state);
    m.fail_restore=false;m.fail_vertex=false;
    before=m.calls.size();
    auto wrong_thread=std::async(std::launch::async,[&] {return renderer.draw(frame,info);});
    check(wrong_thread.get()==ACVR_BAD_STATE && m.calls.size()==before,"non-owner thread rejected before GL dispatch");
    frame.materials.materials.emplace_back();
    check(renderer.draw(frame,info)==ACVR_UNSUPPORTED && m.calls.size()==before,"invalid material addressing cannot silently fall back to flat GL draw");
    frame.materials.materials.clear();frame.triangles[0].material=0;
    check(renderer.draw(frame,info)==ACVR_BAD_ARGUMENT && m.calls.size()==before,"material triangle without valid packet rejected before commands");
    frame.triangles[0].material=n22::NoMaterial;
    // Direction vertices and modified far projection keep backdrops at infinity.
    auto sky=n22::synthetic_cube();sky.polygons[0].layer=n22::Layer::Backdrop;
    check(n22::prepare(sky,1,frame)==ACVR_OK,"synthetic infinity layer");m.vertices.clear();m.loaded_projection.clear();
    check(renderer.draw(frame,info)==ACVR_OK && m.vertices[0][3]==0 && m.loaded_projection[0][10]==-1 &&
          m.loaded_projection[0][14]==0,"sky ignores translation and sits on far clip plane");
}
void callback_path() {
    Mock m;current=&m;auto api=record<acvr_backend_api>();check(acvr_backend_query(1,&api)==ACVR_OK && api.supported_graphics==0,"production factory still disabled");
    auto open=record<acvr_open_info>();ACVR_INIT(&open.graphics);open.game_id_utf8="synthetic-system22";
    auto meta=record<acvr_backend_info>();acvr_backend *b=nullptr;
    check(api.game_open(&open,&b,&meta)==ACVR_OK,"synthetic harness");
    check(n22::configure_gl_draw(b,device(m))==ACVR_OK,"private borrowed-dispatch binding");
    check(n22::stage_cpu_scene(b,n22::synthetic_cube())==ACVR_OK,"same owned scene path");
    auto input=record<acvr_inputs>();input.tick_id=1;api.game_set_inputs(b,&input);
    auto step=record<acvr_step_info>();step.tick_id=1;auto fi=record<acvr_frame_info>();acvr_frame *f=nullptr;
    check(api.game_step(b,&step,&f,&fi)==ACVR_OK,"same immutable lease");
    auto eye=n22::desktop_eye(0,0,4,320,240);auto info=draw_info(eye);
    check(api.game_draw_eye(b,f,&info)==ACVR_OK && m.vertices.size()==36,"public callback uses compiled GL draw implementation");
    check(n22::configure_gl_draw(b,device(m))==ACVR_BAD_STATE,"no binding change during lease");
    api.game_release_frame(b,f);api.game_close(b);
    check(n22::GlRenderer::required_capabilities()==ACVR_CAP_REQUIRES_SHARED_DEPTH,"v0.2 shared-depth requirement");
}
void uploaded_texels(const n22::Frame &frame,const Mock &m) {
    n22::TexturePlan plan;check(n22::prepare_texture_plan(frame,plan)==ACVR_OK,"independent CPU extent preparation");
    check(m.live_textures.size()==plan.rectangles.size(),"one owned texture per deduplicated rectangle");
    size_t i=0;for(auto name:m.live_textures) {
        const auto &upload=m.uploads.at(name);const auto &rect=plan.rectangles[i++];
        check(upload.width==rect.width && upload.height==rect.height && upload.bytes==rect.rgba,"every uploaded byte equals owned CPU expected texels");
        check(upload.parameters.at(GL_TEXTURE_MIN_FILTER)==GL_NEAREST && upload.parameters.at(GL_TEXTURE_MAG_FILTER)==GL_NEAREST &&
              upload.parameters.at(GL_TEXTURE_WRAP_S)==n22::glc::ClampToEdge && upload.parameters.at(GL_TEXTURE_WRAP_T)==n22::glc::ClampToEdge,"nearest filtering and extent-edge clamp explicit");
    }
}
void material_lifecycle() {
    Mock m;current=&m;n22::GlRenderer renderer;check(renderer.initialize(device(m))==ACVR_OK,"material dispatch resolves without context creation");
    auto in=n22::synthetic_material_cube();in.polygons[0].attributes[0].brightness=128;in.polygons[0].attributes[1].brightness=255;
    n22::Frame frame;check(n22::prepare(in,1,frame)==ACVR_OK,"immutable checker material lease");
    auto eye=n22::desktop_eye(0,-.032f,4,320,240);auto info=draw_info(eye);const auto original=m.state;
    check(renderer.draw(frame,info)==ACVR_OK && m.upload_count==6 && m.vertices.size()==36,"first eye uploads six exact face textures once");same_state(original,m.state);uploaded_texels(frame,m);
    check(m.colours[0][0]==.5f && m.colours[1][0]==255.f/256.f && m.colours[2][0]==.25f,"over-bright vertex data preserved before RGB scale4");
    for(size_t k=0;k<3;++k) {
        const auto &a=frame.triangles[0].attributes[k];uint32_t expected=0;
        check(n22::sample_material(frame.materials,0,a.u,a.v,a.brightness,expected)==ACVR_OK &&
              m.composed_samples[k]==expected,"mock nearest texture/combine bright sample agrees with CPU expected RGB");
    }
    check(m.texcoords[0]==std::array<float,2>{.5f/16,.5f/16} && m.texcoords[1][0]==15.5f/16,"raw normalized texel centres without native inverse-depth divide");
    const auto names=m.live_textures;const auto pixels=m.uploads;eye.eye_index=1;eye.rect_x=320;
    check(renderer.draw(frame,info)==ACVR_OK && m.upload_count==6 && m.live_textures==names && m.deleted.empty(),"second eye retains uploads and object identities");same_state(original,m.state);
    eye.view_from_scene[12]=-.3f;check(renderer.draw(frame,info)==ACVR_OK && m.upload_count==6 && m.loaded_view.back()[12]==-.3f,"latest-pose replay does not rebake or reupload");
    check(m.uploads.begin()->second.bytes==pixels.begin()->second.bytes,"replay texture bytes immutable");
    n22::Frame next=frame;next.id=2;info.frame_id=2;const size_t commands=m.calls.size();
    check(renderer.draw(next,info)==ACVR_BAD_STATE && m.calls.size()==commands,"new frame cannot replace active texture lease");
    auto wrong=std::async(std::launch::async,[&] {return renderer.release_frame(1);});
    check(wrong.get()==ACVR_BAD_STATE && m.calls.size()==commands && m.live_textures==names,"non-owner release leaves resources for owner cleanup");
    auto wrong_shutdown=std::async(std::launch::async,[&] {return renderer.shutdown();});
    check(wrong_shutdown.get()==ACVR_BAD_STATE && m.calls.size()==commands,"non-owner shutdown never dispatches GL");
    check(renderer.release_frame(1)==ACVR_OK && m.live_textures.empty() && m.deleted.size()==6 && m.clears.size()==3,"release retires only own textures once without clearing shared depth");same_state(original,m.state);
    check(renderer.release_frame(1)==ACVR_OK && m.deleted.size()==6,"duplicate release idempotent");
    // Explicit negative/wrapped UVs, swapped/flipped cells and every cmode.
    in=n22::synthetic_material_cube();in.materials.cells.insert(in.materials.cells.begin(),{255,0,12});
    in.materials.cells.push_back({0xff00,0,10});in.materials.cells.push_back({0xffff,0,12});
    in.materials.cells.push_back({0x10000,0,12});in.materials.cells.push_back({0x100ff,0,10});
    in.materials.cells.push_back({0x1ff00,0,12});in.materials.cells.push_back({0x1ffff,0,10});
    std::sort(in.materials.cells.begin(),in.materials.cells.end(),[](auto a,auto b){return a.index<b.index;});
    in.materials.cells[0].attribute=10;
    in.materials.materials[0].texbank=1;
    for(size_t i=0;i<in.materials.palette.size();++i) in.materials.palette[i]=static_cast<uint32_t>(i&255)*0x010101;
    for(uint8_t mode=0;mode<16;++mode) {
        in.materials.materials[0].cmode=mode;
        for(size_t i=0;i<2;++i) for(auto &a:in.polygons[i].attributes) {a.u-=4;a.v-=4;}
        // Restore each next iteration's intended -3.5..11.5 span below.
        check(n22::prepare(in,mode+2,frame)==ACVR_OK,"mode/wrap material fixture");info.frame_id=frame.id;
        check(renderer.draw(frame,info)==ACVR_OK,"texture upload supports cmode and negative UV wrapping");uploaded_texels(frame,m);same_state(original,m.state);
        check(renderer.release_frame(frame.id)==ACVR_OK,"per-mode lease release");
        for(size_t i=0;i<2;++i) for(auto &a:in.polygons[i].attributes) {a.u+=4;a.v+=4;}
    }
    in.materials.materials[0].objectflags=6;in.materials.materials[0].cz_adjust=0x1234;in.materials.palette[0x1234]=0x123456;
    for(size_t i=0;i<2;++i) for(auto &a:in.polygons[i].attributes) a.brightness=255;
    check(n22::prepare(in,20,frame)==ACVR_OK,"no-shade solid material fixture");info.frame_id=20;m.colours.clear();
    check(renderer.draw(frame,info)==ACVR_OK && m.colours[0][0]==.25f && m.uploads.begin()->second.bytes==std::vector<uint8_t>{0x12,0x34,0x56,255},"solid samples exact palette pen without applying brightness");
    check(renderer.shutdown()==ACVR_OK && m.live_textures.empty(),"explicit close-style shutdown cleans active lease");
    const auto count=m.deleted.size();check(renderer.shutdown()==ACVR_OK && m.deleted.size()==count,"shutdown idempotent");
}
void material_failures() {
    Mock m;current=&m;n22::GlRenderer renderer;check(renderer.initialize(device(m))==ACVR_OK,"failure fixture binding");
    n22::Frame frame;check(n22::prepare(n22::synthetic_material_cube(),1,frame)==ACVR_OK,"failure fixture packet");
    auto eye=n22::desktop_eye(0,0,4,320,240);auto info=draw_info(eye);const auto original=m.state;
    m.max_texture=8;
    check(renderer.draw(frame,info)==ACVR_UNSUPPORTED && m.generated.empty() && m.clears.empty(),"GL max-size capability rejects before resource allocation/clear");same_state(original,m.state);
    m.max_texture=2048;m.imaging=true;m.state.enabled.insert(n22::glc::ImagingCaps[0]);
    check(renderer.draw(frame,info)==ACVR_UNSUPPORTED && m.generated.empty() && m.clears.empty(),"active optional imaging subset rejects colour-altering upload");
    m.state.enabled.erase(n22::glc::ImagingCaps[0]);m.imaging=false;
    m.fail_generation=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && m.live_textures.empty() && m.deleted.size()==1 && m.clears.empty(),"partial generation rolls back once without target clear");same_state(original,m.state);
    m.fail_generation=false;m.fail_upload_number=m.upload_count+2;m.fail_delete=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && m.live_textures.empty() && m.clears.empty() &&
          renderer.diagnostic().phase==n22::GlPhase::Upload && renderer.diagnostic().error==GL_OUT_OF_MEMORY &&
          renderer.diagnostic().cleanup_error==GL_INVALID_VALUE,"upload error and rollback errors preserved separately");same_state(original,m.state);
    m.fail_delete=false;m.fail_upload_number=0;m.fail_vertex=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && m.live_textures.size()==6,"draw failure retains known lease resources for cleanup");same_state(original,m.state);
    const auto prior=m.deleted.size();check(renderer.release_frame(1)==ACVR_OK && m.live_textures.empty() && m.deleted.size()==prior+6,"failed draw lease still releases exactly once");
    m.fail_vertex=false;
    m.imaging=true;
    check(renderer.draw(frame,info)==ACVR_OK,"retry with canonical optional imaging state uploads complete lease");
    m.fail_delete=true;check(renderer.release_frame(1)==ACVR_ERROR && m.live_textures.empty() && renderer.diagnostic().cleanup_error==GL_INVALID_VALUE,"cleanup failures observable while completing release");
    m.fail_delete=false;check(renderer.shutdown()==ACVR_OK,"cleanup complete on shutdown");
    Mock missing;current=&missing;missing.missing.insert("glTexImage2D");n22::GlRenderer incomplete;
    check(incomplete.initialize(device(missing))==ACVR_UNSUPPORTED && missing.calls.empty(),"missing upload function cannot admit material renderer");
    Mock mixed;current=&mixed;n22::GlRenderer mix;mix.initialize(device(mixed));
    frame.triangles[0].material=n22::NoMaterial;
    check(mix.draw(frame,info)==ACVR_OK && mixed.vertex_textures[0]==0 && mixed.vertex_textures[3]!=0,"mixed explicit flat/textured triangles switch state outside Begin");
    check(mix.shutdown()==ACVR_OK && mixed.live_textures.empty(),"mixed lease cleanup complete");
}
void material_callback_path() {
    Mock m;current=&m;auto api=record<acvr_backend_api>();acvr_backend_query(1,&api);
    auto open=record<acvr_open_info>();ACVR_INIT(&open.graphics);open.game_id_utf8="synthetic-system22";
    auto meta=record<acvr_backend_info>();acvr_backend *b=nullptr;api.game_open(&open,&b,&meta);n22::configure_gl_draw(b,device(m));
    check(n22::stage_cpu_scene(b,n22::synthetic_material_cube())==ACVR_OK,"backend stages owned texture packet");
    auto inputs=record<acvr_inputs>();inputs.tick_id=1;api.game_set_inputs(b,&inputs);
    auto step=record<acvr_step_info>();step.tick_id=1;auto fi=record<acvr_frame_info>();acvr_frame *lease=nullptr;api.game_step(b,&step,&lease,&fi);
    auto eye=n22::desktop_eye(0,0,4,320,240);auto info=draw_info(eye);
    check(api.game_draw_eye(b,lease,&info)==ACVR_OK && m.upload_count==6,"real callback uploads the immutable texture lease");
    api.game_release_frame(b,lease);check(m.live_textures.empty() && m.deleted.size()==6,"public void release cleans GPU resources before dropping CPU lease");
    check(n22::stage_cpu_scene(b,n22::synthetic_material_cube())==ACVR_OK,"second stage after release");
    inputs.tick_id=2;api.game_set_inputs(b,&inputs);step.tick_id=2;step.simulation_time_ns=1000000000000ULL/59906;api.game_step(b,&step,&lease,&fi);info.frame_id=2;
    check(api.game_draw_eye(b,lease,&info)==ACVR_OK && m.upload_count==12,"new native tick gets its own textures");
    api.game_close(b);check(m.live_textures.empty() && m.deleted.size()==12,"public close retires unreleased texture lease while context lives");
}
n22::SceneInput fog_material_scene() {
    auto in=n22::synthetic_material_cube();std::fill(in.materials.palette.begin(),in.materials.palette.end(),0xc81164);
    in.fog.policy=n22::FogPolicy::Super22Table;in.fog.tick=1;in.fog.attributes[4]=4;in.fog.rgb={10,80,30};
    in.fog.tables[0][0]=0;in.fog.tables[0][1]=128;in.fog.tables[0][2]=255;
    for(auto &p:in.polygons) {
        p.fog.provided=p.fog.has_native_depth=true;p.fog.tick=1;p.fog.native_depth={0,256,512};
        for(auto &a:p.attributes) a.brightness=128;
    }
    in.polygons[1].fog.colour_word=0x8000; // mixed unfogged texture
    in.polygons.back().fog.colour_word=0x8000;in.polygons.back().material=n22::NoMaterial; // mixed flat
    return in;
}
void fog_lifecycle() {
    Mock m;current=&m;n22::GlRenderer renderer;check(renderer.initialize(device(m))==ACVR_OK,"fog dispatch resolves without context creation");
    auto in=fog_material_scene();n22::Frame frame;check(n22::prepare(in,1,frame)==ACVR_OK,"fog samples prepared once per owned lease");
    const auto frozen=frame.triangles[0].fog_samples.alpha;in.fog.tables[0].fill(0);in.polygons[0].fog.native_depth.fill(0);
    auto eye=n22::desktop_eye(0,-.032f,4,320,240);auto info=draw_info(eye);const auto original=m.state;
    const auto result=renderer.draw(frame,info);
    check(result==ACVR_OK && m.upload_count==7 && m.vertices.size()==36 && m.clears.size()==1,"single draw per triangle, six material textures plus one private fog dummy");same_state(original,m.state);uploaded_texels(frame,m);
    check(m.composed_samples[0]==0xff22c8 && m.composed_samples[1]==0x843972 && m.composed_samples[2]==0x0a501e,"GL mock clamps high shade before interpolation to unshaded fog RGB");
    check(m.colours[0][3]==1 && m.colours[1][3]==127.f/255.f && m.colours[2][3]==0,"primary alpha carries frozen native unfogged weights");
    check(std::all_of(m.composed_alpha.begin(),m.composed_alpha.end(),[](float a){return a==1;}),"all fogged/unfogged textured output remains opaque");
    check(m.composed_samples[3]==0xff22c8 && m.colours[3][3]==1 && m.vertex_textures.back()==0,"fog disables cleanly on unfogged texture and explicit flat triangle");
    const auto handles=m.live_textures;const auto first_vertices=m.vertices;const auto first_colours=m.colours;
    eye.eye_index=1;eye.rect_x=320;eye.view_from_scene[12]=-.2f;
    check(renderer.draw(frame,info)==ACVR_OK && m.upload_count==7 && m.live_textures==handles && m.loaded_view.back()[12]==-.2f,"second eye/latest pose reuses frozen weights and all handles");same_state(original,m.state);
    check(std::equal(first_vertices.begin(),first_vertices.end(),m.vertices.begin()+36) && std::equal(first_colours.begin(),first_colours.end(),m.colours.begin()+static_cast<ptrdiff_t>(first_colours.size())) && frame.triangles[0].fog_samples.alpha==frozen,"replay passes identical geometry/native fog data under new eye matrices");
    size_t before=m.calls.size();auto wrong=std::async(std::launch::async,[&]{return renderer.release_frame(1);});
    check(wrong.get()==ACVR_BAD_STATE && m.calls.size()==before && m.live_textures==handles,"fog dummy included in owner-thread cleanup gate");
    check(renderer.release_frame(1)==ACVR_OK && m.live_textures.empty() && m.deleted.size()==7 && m.clears.size()==2,"owner release retires material and fog textures once without clearing shared depth");same_state(original,m.state);
    check(renderer.shutdown()==ACVR_OK && m.deleted.size()==7,"post-release shutdown does not double delete");
}
void fog_failures() {
    Mock m;current=&m;n22::GlRenderer renderer;renderer.initialize(device(m));n22::Frame frame;n22::prepare(fog_material_scene(),1,frame);
    auto eye=n22::desktop_eye(0,0,4,320,240);auto info=draw_info(eye);const auto original=m.state;
    for(auto layer:{n22::Layer::Hud,n22::Layer::Backdrop,n22::Layer::GunFlash}) {
        auto bad=frame;bad.triangles[0].layer=layer;
        check(renderer.draw(bad,info)==ACVR_UNSUPPORTED && m.calls.empty(),"unsupported enabled fog layer rejects before GL queries/uploads/clear");
    }
    auto bad=frame;bad.triangles[0].material=n22::NoMaterial;
    check(renderer.draw(bad,info)==ACVR_UNSUPPORTED && m.calls.empty(),"enabled flat fog cannot fall back silently");
    bad=frame;bad.materials.materials[0].objectflags=1;
    check(renderer.draw(bad,info)==ACVR_UNSUPPORTED && m.calls.empty(),"enabled solid fog rejects before GL");
    bad=frame;bad.fog.tick=2;check(renderer.draw(bad,info)==ACVR_BAD_ARGUMENT && m.calls.empty(),"stale fog binding cannot draw");
    m.texture_units=1;check(renderer.draw(frame,info)==ACVR_UNSUPPORTED && m.generated.empty() && m.clears.empty(),"two-unit fog requirement rejects before resource generation/clear");same_state(original,m.state);
    m.texture_units=2;m.fail_fog_query=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && m.generated.empty() && m.clears.empty(),"unit1 query failure restores selector before any mutations/uploads");same_state(original,m.state);
    m.fail_fog_query=false;m.fail_generation=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && m.live_textures.empty() && m.deleted.size()==1 && m.clears.empty(),"partial fog resource generation rolls back once before clear");same_state(original,m.state);
    m.fail_generation=false;m.fail_upload_number=m.upload_count+7;m.fail_delete=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && m.live_textures.empty() && m.clears.empty() && renderer.diagnostic().phase==n22::GlPhase::Upload && renderer.diagnostic().cleanup_error==GL_INVALID_VALUE,"failure uploading last dummy rolls back complete lease and retains first/cleanup errors");same_state(original,m.state);
    m.fail_delete=false;m.fail_upload_number=0;m.fail_clamp=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && m.live_textures.size()==7 && m.clears.empty() && renderer.diagnostic().phase==n22::GlPhase::State,"clamp setup failure preserves target and known owner lease");same_state(original,m.state);
    check(renderer.release_frame(1)==ACVR_OK && m.live_textures.empty(),"partial state setup releases its private lease");
    m.fail_clamp=false;m.fail_vertex=true;
    check(renderer.draw(frame,info)==ACVR_ERROR && m.live_textures.size()==7 && renderer.diagnostic().phase==n22::GlPhase::Draw,"fog draw failure retains immutable owner lease");same_state(original,m.state);
    const size_t deleted=m.deleted.size();m.fail_delete=true;
    check(renderer.release_frame(1)==ACVR_ERROR && m.live_textures.empty() && m.deleted.size()==deleted+7,"cleanup failure still retires all private fog/material names exactly once");
    m.fail_delete=false;m.fail_vertex=false;renderer.shutdown();
    for(const char *name:{"glTexEnvfv","glGetTexEnviv","glGetTexEnvfv","glClampColor"}) {
        Mock missing;current=&missing;missing.missing.insert(name);n22::GlRenderer incomplete;
        check(incomplete.initialize(device(missing))==ACVR_UNSUPPORTED && missing.calls.empty(),"missing required fog dispatch rejects without GL");
    }
    // Synthetic malformed GenTextures returning the saved unit1 binding must
    // never lead us to delete that alleged borrowed texture name.
    Mock alias;current=&alias;alias.next_texture=56;n22::GlRenderer guarded;guarded.initialize(device(alias));
    check(guarded.draw(frame,info)==ACVR_ERROR && alias.clears.empty() && std::find(alias.deleted.begin(),alias.deleted.end(),GLuint{56})==alias.deleted.end(),"unit1 borrowed binding excluded from rollback deletion");same_state(original,alias.state);
}
void fog_callback_path() {
    Mock m;current=&m;auto api=record<acvr_backend_api>();acvr_backend_query(1,&api);
    check(api.supported_graphics==0,"fog cannot admit native factory/graphics");
    auto open=record<acvr_open_info>();ACVR_INIT(&open.graphics);open.game_id_utf8="synthetic-system22";
    auto meta=record<acvr_backend_info>();acvr_backend *b=nullptr;api.game_open(&open,&b,&meta);n22::configure_gl_draw(b,device(m));
    check(n22::stage_cpu_scene(b,fog_material_scene())==ACVR_OK,"backend stages copied fog/material packet");
    auto input=record<acvr_inputs>();input.tick_id=1;api.game_set_inputs(b,&input);
    auto step=record<acvr_step_info>();step.tick_id=1;auto fi=record<acvr_frame_info>();acvr_frame *lease=nullptr;
    check(api.game_step(b,&step,&lease,&fi)==ACVR_OK,"native-style lease caches vertex fog once");
    auto eye=n22::desktop_eye(0,0,4,320,240);auto info=draw_info(eye);
    check(api.game_draw_eye(b,lease,&info)==ACVR_OK && m.upload_count==7,"actual callback applies same-draw fog through CPU mocks");
    api.game_release_frame(b,lease);check(m.live_textures.empty() && m.deleted.size()==7,"public owner release retires fog dummy too");api.game_close(b);
}
}
int main() {
    try {direct_dispatch();callback_path();material_lifecycle();material_failures();material_callback_path();fog_lifecycle();fog_failures();fog_callback_path();std::cout<<checks<<" GL dispatch checks passed (CPU mocks only)\n";return 0;}
    catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}
