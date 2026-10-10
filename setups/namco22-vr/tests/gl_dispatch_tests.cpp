// SPDX-License-Identifier: GPL-3.0-only
// No GL library/context/window/GPU. Every dispatch address is a local CPU mock.
#include "n22_cpu_backend.hpp"
#include "n22_gl_renderer.hpp"
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
    Context() {
        std::array<float,16> identity{};for(size_t i=0;i<16;i+=5) identity[i]=1;
        matrices[GL_MODELVIEW]={identity};matrices[GL_PROJECTION]={identity};matrices[GL_TEXTURE]={identity};
    }
};
struct Mock {
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
    case n22::glc::MaxTextureUnits:*out=2;break;
    case n22::glc::MaxClipDistances:*out=8;break;
    case GL_DEPTH_BITS:*out=current->depth;break;
    case GL_DOUBLEBUFFER:*out=1;break;
    case n22::glc::ContextProfileMask:*out=current->profile;break;
    default:*out=0;current->error=GL_INVALID_ENUM;break;
    }
}
GLenum N22_GL_CALL get_error() {log("GetError");auto e=current->error;current->error=GL_NO_ERROR;return e;}
const GLubyte *N22_GL_CALL get_string(GLenum name) {
    log("GetString");return reinterpret_cast<const GLubyte *>(name==GL_VERSION?"3.3 MOCK":"GL_ARB_framebuffer_object GL_EXT_framebuffer_sRGB");
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
    log("LoadMatrix");auto &s=current->state;auto &m=s.matrices[s.mode].back();std::copy(v,v+16,m.begin());
    if(s.mode==GL_PROJECTION) current->loaded_projection.push_back(m);else current->loaded_view.push_back(m);
}
void N22_GL_CALL viewport(GLint x,GLint y,GLsizei w,GLsizei h) {log("Viewport");current->state.viewport={x,y,w,h};}
void N22_GL_CALL scissor(GLint x,GLint y,GLsizei w,GLsizei h) {log("Scissor");current->state.scissor={x,y,w,h};}
void N22_GL_CALL enable(GLenum cap) {log("Enable");if(cap==n22::glc::TextureRectangle) current->state.texture_enabled[current->state.active].insert(cap);else current->state.enabled.insert(cap);}
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
void N22_GL_CALL begin(GLenum mode) {log("Begin");if(mode!=GL_TRIANGLES) throw std::runtime_error("triangle stream");}
void N22_GL_CALL end() {log("End");}
void N22_GL_CALL colour(GLubyte,GLubyte,GLubyte,GLubyte) {log("Colour");}
void normalized() {
    for(auto cap:n22::glc::ModernCaps) if(current->state.enabled.count(cap)) throw std::runtime_error("inherited modern state must be disabled");
    for(GLenum i=0;i<8;++i) if(current->state.enabled.count(n22::glc::ClipDistance0+i)) throw std::runtime_error("all clip distances must be disabled");
    for(const auto &u:current->state.texture_enabled) if(!u.second.empty()) throw std::runtime_error("all fixed texture units must be disabled");
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
    const auto found=table.find(name);return found==table.end()?nullptr:found->second;
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
}
int main() {
    try {direct_dispatch();callback_path();std::cout<<checks<<" GL dispatch checks passed (CPU mocks only)\n";return 0;}
    catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}
