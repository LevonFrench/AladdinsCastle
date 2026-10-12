// SPDX-License-Identifier: MIT
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <unknwn.h>
#include <GL/gl.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_NO_PROTOTYPES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "openxr_gl_host.hpp"
#include "view_math.hpp"
#include "stereo_capture.hpp"
#include "fixed_capture_pose.hpp"
#include "gl_swapchain_format.hpp"
#ifdef ACVR_GUN_GL
#include "gun_gl.hpp"
#include "hand_assembly.hpp"
#endif
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace acvr {
namespace {
template<class T> T record() { T value{}; ACVR_INIT(&value); return value; }
acvr_pose pose(XrPosef p) {
    auto out=record<acvr_pose>();
    out.position_m[0]=p.position.x;out.position_m[1]=p.position.y;out.position_m[2]=p.position.z;
    out.orientation_xyzw[0]=p.orientation.x;out.orientation_xyzw[1]=p.orientation.y;
    out.orientation_xyzw[2]=p.orientation.z;out.orientation_xyzw[3]=p.orientation.w;
    return out;
}
acvr_pose identity_pose() {
    auto out=record<acvr_pose>();out.orientation_xyzw[3]=1;return out;
}
uint32_t flags(XrSpaceLocationFlags in) {
    return ((in&XR_SPACE_LOCATION_POSITION_VALID_BIT)?ACVR_POSE_POSITION_VALID:0u)|
           ((in&XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)?ACVR_POSE_ORIENTATION_VALID:0u);
}
constexpr GLenum kDepth=0x81A6; // GL_DEPTH_COMPONENT24
constexpr GLenum kMajor=0x821B,kMinor=0x821C;
using CreateContextFn=HGLRC (WINAPI *)(HDC,HGLRC,const int *);
using GenFramebuffersFn=void (APIENTRY *)(GLsizei,GLuint *);
using DeleteFramebuffersFn=void (APIENTRY *)(GLsizei,const GLuint *);
using BindFramebufferFn=void (APIENTRY *)(GLenum,GLuint);
using FramebufferTexture2DFn=void (APIENTRY *)(GLenum,GLenum,GLenum,GLuint,GLint);
using CheckFramebufferStatusFn=GLenum (APIENTRY *)(GLenum);
constexpr GLenum kFrame=0x8D40,kColourAttachment=0x8CE0,kDepthAttachment=0x8D00,
                 kComplete=0x8CD5;
constexpr GLenum kReadFramebuffer=0x8CA8,kDrawFramebuffer=0x8CA9,
                 kReadFramebufferBinding=0x8CAA,kDrawFramebufferBinding=0x8CA6,
                 kPixelPackBufferBinding=0x88ED;
struct Gl {
    GenFramebuffersFn gen_fbo{};DeleteFramebuffersFn delete_fbo{};BindFramebufferFn bind_fbo{};
    FramebufferTexture2DFn attach_colour{};CheckFramebufferStatusFn check_fbo{};
};
void *gl_proc(void *,const char *name) {
    auto value=reinterpret_cast<void *>(wglGetProcAddress(name));
    if(!value||value==reinterpret_cast<void *>(1)||value==reinterpret_cast<void *>(2)||
       value==reinterpret_cast<void *>(3)||value==reinterpret_cast<void *>(-1))
        value=reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"opengl32.dll"),name));
    return value;
}
#ifdef ACVR_GUN_GL
Matrix product(const Matrix &a,const Matrix &b) {
    Matrix out{};
    for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)
        out[c*4+r]+=a[k*4+r]*b[c*4+k];
    return out;
}
Matrix mount_matrix(const std::array<float,3> &t,const std::array<float,4> &q) {
    const double x=q[0],y=q[1],z=q[2],w=q[3];
    return {float(1-2*y*y-2*z*z),float(2*x*y+2*z*w),float(2*x*z-2*y*w),0,
        float(2*x*y-2*z*w),float(1-2*x*x-2*z*z),float(2*y*z+2*x*w),0,
        float(2*x*z+2*y*w),float(2*y*z-2*x*w),float(1-2*x*x-2*y*y),0,
        t[0],t[1],t[2],1};
}
#endif
bool read_bounded(const std::filesystem::path &path,size_t max,std::vector<uint8_t> &data) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)return false;
    const auto size=in.tellg();if(size<=0||uint64_t(size)>max)return false;
    data.resize(size_t(size));in.seekg(0);in.read(reinterpret_cast<char *>(data.data()),size);
    return bool(in);
}
bool no_reparse_components(std::filesystem::path path){
    for(;;){
        const DWORD attributes=GetFileAttributesW(path.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        const auto parent=path.parent_path();
        if(parent==path||parent.empty())return true;
        path=parent;
    }
}
std::string sha256(const std::vector<uint8_t> &data){
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    std::string result;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return result;
    ULONG length=0,received=0;
    if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),
                         sizeof(length),&received,0)>=0&&length>0&&length<1024*1024){
        std::vector<uint8_t> object(length);std::array<uint8_t,32> digest{};
        if(BCryptCreateHash(algorithm,&hash,object.data(),length,nullptr,0,0)>=0&&
           BCryptHashData(hash,const_cast<PUCHAR>(data.data()),static_cast<ULONG>(data.size()),0)>=0&&
           BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0){
            constexpr char hex[]="0123456789abcdef";result.reserve(64);
            for(uint8_t v:digest){result+=hex[v>>4];result+=hex[v&15];}
        }
    }
    if(hash)BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm,0);return result;
}
bool write_exclusive(const std::filesystem::path &path,const std::string &header,
                     const std::vector<uint8_t> &data){
    const HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    const auto write=[&](const uint8_t *bytes,size_t size){
        size_t offset=0;
        while(offset<size){DWORD written=0;
            const auto amount=static_cast<DWORD>(std::min<size_t>(size-offset,0x7fffffff));
            if(!WriteFile(file,bytes+offset,amount,&written,nullptr)||written==0)return false;
            offset+=written;
        }
        return true;
    };
    const bool good=write(reinterpret_cast<const uint8_t *>(header.data()),header.size())&&
                    write(data.data(),data.size())&&FlushFileBuffers(file)!=0;
    CloseHandle(file);
    if(!good)DeleteFileW(path.c_str());
    return good;
}
template<class T> bool resolve(T &out,const char *name) {
    out=reinterpret_cast<T>(gl_proc(nullptr,name));return out!=nullptr;
}
struct EyeImage { GLuint colour=0,fbo=0,depth=0; };
struct EyeSwapchain {
    XrSwapchain handle=XR_NULL_HANDLE;
    uint32_t width=0,height=0,index=0;
    bool acquired=false;
    std::vector<XrSwapchainImageOpenGLKHR> images;
    std::vector<EyeImage> targets;
};
class OpenXrGlHost final : public Host {
public:
    OpenXrGlHost(std::string name,std::string root,std::string model):
        application_name_(std::move(name)),content_root_(std::move(root)),model_id_(std::move(model)){}
    ~OpenXrGlHost() override { shutdown(); }
    acvr_result initialize();
    acvr_graphics_device device() const override {
        auto d=record<acvr_graphics_device>();d.api=ACVR_GRAPHICS_GL;
        d.flags=ACVR_DEVICE_GL_COMPATIBILITY;
        d.device=reinterpret_cast<uintptr_t>(dc_);d.context=reinterpret_cast<uintptr_t>(context_);
        d.get_proc=gl_proc;return d;
    }
    bool headless() const noexcept override {return false;}
    bool supports_calibration_samples() const noexcept override {return true;}
#ifdef ACVR_GUN_GL
    GunMount primary_anchor_mount(uint32_t hand) const noexcept override {
        if(!hands_ready_)return {"grip"};
        if(hand>=2)return {};
        const auto &mount=hand_assembly_.primary_mount_by_hand[hand];
        return {mount.node,mount.translation,mount.rotation_xyzw};
    }
    GunMount support_anchor_mount(uint32_t primary) const noexcept override {
        if(!hands_ready_)return {"grip_two"};
        if(!hand_assembly_.has_support_grip||primary>=2)return {};
        const auto &mount=hand_assembly_.support_by_primary[primary];
        return {mount.node,mount.translation,mount.rotation_xyzw};
    }
#endif
    acvr_result activate() override {
        if(active_)return ACVR_BAD_STATE;
        previous_dc_=wglGetCurrentDC();previous_context_=wglGetCurrentContext();
        if(previous_dc_!=dc_||previous_context_!=context_)
            if(!wglMakeCurrent(dc_,context_))return ACVR_BAD_STATE;
        active_=true;return ACVR_OK;
    }
    void deactivate() noexcept override {
        if(!active_)return;
        if(previous_dc_!=dc_||previous_context_!=context_)
            wglMakeCurrent(previous_dc_,previous_context_);
        active_=false;
    }
    acvr_result configure(float scale,uint32_t mode) override {
        if(!std::isfinite(scale)||scale<=0) return ACVR_BAD_ARGUMENT;
        scale_=scale;anchor_mode_=mode;return ACVR_OK;
    }
    acvr_result begin(Display &) override;
    acvr_result bind_world_target(const acvr_draw_info &info) override {
        if(!active_||!in_frame_||wglGetCurrentContext()!=context_||wglGetCurrentDC()!=dc_)
            return ACVR_BAD_STATE;
        if(info.view_count!=1||!info.views||info.views->eye_index>=2) return ACVR_BAD_ARGUMENT;
        const auto &s=eyes_[info.views->eye_index];
        if(!s.acquired||s.index>=s.targets.size()) return ACVR_BAD_STATE;
        const auto &target=s.targets[s.index];
        if(info.target.api!=ACVR_GRAPHICS_GL||info.target.framebuffer!=target.fbo||
           info.target.colour_image!=target.colour||info.target.depth_image!=target.depth||
           info.target.width!=s.width||info.target.height!=s.height) return ACVR_BAD_ARGUMENT;
        gl_.bind_fbo(kFrame,target.fbo);
        return ACVR_OK;
    }
    acvr_result end(bool) override;
    void output(const acvr_output_event &) override {}
    void cancel_effects() noexcept override {}
#ifdef ACVR_GUN_GL
    bool supports_guns() const noexcept override {return true;}
    acvr_result draw_gun(const acvr_draw_info &info,const GunDraw &gun) override {
        if(!gun_ready_) {auto r=gun_gl_.initialize(device());if(r!=ACVR_OK)return r;gun_ready_=true;}
        auto result=gun_gl_.draw(info,gun);if(result!=ACVR_OK||!hands_ready_)return result;
        if(!gun.asset||gun.scene_from_node.size()!=gun.asset->nodes.size())return ACVR_BAD_ARGUMENT;
        std::string anchor_error;
        if(!validate_hand_gun_nodes(hand_assembly_,*gun.asset,anchor_error))return ACVR_BAD_ARGUMENT;
        result=draw_hand(info,gun,gun.hand,false);
        if(result==ACVR_OK&&gun.support_grip&&hand_assembly_.has_support_grip)
            result=draw_hand(info,gun,1-gun.hand,true);
        if(result==ACVR_OK)result=draw_labels(info,gun);
        if(result==ACVR_OK&&info.views&&info.views->eye_index<2)
            gun_drawn_[info.views->eye_index]=true;
        return result;
    }
#endif
private:
    acvr_result init_context();
    acvr_result init_xr();
    acvr_result init_swapchains();
    acvr_result init_actions();
    acvr_result fill_tracking(Display &);
    acvr_result configure_capture();
    bool read_capture_eye(unsigned eye) noexcept;
    bool write_capture_receipt(const char *status,const char *reason) noexcept;
    bool write_capture_pair() noexcept;
#ifdef ACVR_GUN_GL
    acvr_result prepare_hands();
    acvr_result draw_hand(const acvr_draw_info &,const GunDraw &,uint32_t,bool support);
    acvr_result draw_labels(const acvr_draw_info &,const GunDraw &);
#endif
    void shutdown() noexcept;
    template<class T> bool proc(const char *name,T &out) {
        PFN_xrVoidFunction p=nullptr;
        if(XR_FAILED(xrGetInstanceProcAddr_(instance_,name,&p))||!p)return false;
        out=reinterpret_cast<T>(p);return true;
    }
    HMODULE loader_=nullptr;
    std::string application_name_;
    std::string content_root_,model_id_;
    struct CapturedEye {uint32_t width=0,height=0;std::vector<uint8_t> rgb;};
    std::filesystem::path capture_dir_;
    std::array<CapturedEye,2> captured_{};
    std::string assembly_hash_,gun_hash_,right_hand_hash_,left_hand_hash_;
    bool capture_enabled_=false,capture_done_=false,capture_failed_=false;
    bool fixed_capture_=false,fixed_capture_ready_=false;
    std::array<acvr_pose,2> fixed_capture_hands_{};
    bool gun_drawn_[2]{};
    HWND window_=nullptr;HDC dc_=nullptr;HGLRC context_=nullptr;
    Gl gl_{};
    PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr_=nullptr;
#define XR_FN(name) PFN_xr##name name##_=nullptr
    XR_FN(CreateInstance);XR_FN(EnumerateInstanceExtensionProperties);XR_FN(DestroyInstance);XR_FN(GetSystem);XR_FN(EnumerateViewConfigurationViews);
    XR_FN(GetOpenGLGraphicsRequirementsKHR);XR_FN(CreateSession);XR_FN(DestroySession);
    XR_FN(CreateReferenceSpace);XR_FN(DestroySpace);XR_FN(EnumerateSwapchainFormats);
    XR_FN(CreateSwapchain);XR_FN(DestroySwapchain);XR_FN(EnumerateSwapchainImages);
    XR_FN(PollEvent);XR_FN(BeginSession);XR_FN(EndSession);XR_FN(WaitFrame);XR_FN(BeginFrame);
    XR_FN(EndFrame);XR_FN(LocateViews);XR_FN(LocateSpace);XR_FN(AcquireSwapchainImage);
    XR_FN(WaitSwapchainImage);XR_FN(ReleaseSwapchainImage);
    XR_FN(StringToPath);XR_FN(CreateActionSet);XR_FN(DestroyActionSet);XR_FN(CreateAction);
    XR_FN(CreateActionSpace);XR_FN(SuggestInteractionProfileBindings);XR_FN(AttachSessionActionSets);
    XR_FN(SyncActions);XR_FN(GetActionStateBoolean);XR_FN(GetActionStateFloat);
#undef XR_FN
    XrInstance instance_=XR_NULL_HANDLE;XrSystemId system_=XR_NULL_SYSTEM_ID;
    XrSession session_=XR_NULL_HANDLE;XrSpace stage_=XR_NULL_HANDLE,view_space_=XR_NULL_HANDLE;
    XrActionSet actions_=XR_NULL_HANDLE;XrAction grip_=XR_NULL_HANDLE,aim_=XR_NULL_HANDLE;
    XrAction trigger_=XR_NULL_HANDLE,select_=XR_NULL_HANDLE,squeeze_=XR_NULL_HANDLE,primary_=XR_NULL_HANDLE;
    XrAction secondary_=XR_NULL_HANDLE,stick_x_=XR_NULL_HANDLE,stick_y_=XR_NULL_HANDLE;
    XrPath hand_paths_[2]{};XrSpace grip_spaces_[2]{},aim_spaces_[2]{};
    int64_t colour_format_=0;
    EyeSwapchain eyes_[2]{};std::array<acvr_eye,2> acvr_eyes_{};
    std::array<XrCompositionLayerProjectionView,2> layer_eyes_{};
    XrView xr_views_[2]{{XR_TYPE_VIEW},{XR_TYPE_VIEW}};
    XrFrameState frame_{XR_TYPE_FRAME_STATE};
    acvr_pose anchor_=identity_pose();
    float scale_=1;uint32_t anchor_mode_=ACVR_ANCHOR_RAIL;
    bool running_=false,exiting_=false,in_frame_=false,submit_=false,first_anchor_=true;
    HDC previous_dc_=nullptr;HGLRC previous_context_=nullptr;bool active_=false;
    XrSessionState session_state_=XR_SESSION_STATE_UNKNOWN;
    uint64_t waited_frames_=0,render_requested_frames_=0,focused_frames_=0,submitted_frames_=0;
    bool have_render_request_=false,last_render_request_=false;
    bool xr_time_known_=false;XrTime last_xr_time_=0;int64_t monotonic_time_=0;
#ifdef ACVR_GUN_GL
    GunGlRenderer gun_gl_;bool gun_ready_=false;
    HandAssembly hand_assembly_{};
    std::vector<InspectionLabel> inspection_labels_{};
    std::array<GunAsset,2> hand_assets_{};
    std::array<std::vector<HandMotion>,2> hand_motions_{};
    std::array<ControllerButtons,2> hand_controls_{};
    bool hands_ready_=false;
    GLuint label_font_=0;
#endif
};

acvr_result OpenXrGlHost::init_context() {
    const auto module=GetModuleHandleW(nullptr);
    WNDCLASSW cls{};cls.style=CS_OWNDC;cls.lpfnWndProc=DefWindowProcW;cls.hInstance=module;
    cls.lpszClassName=L"ACVR_OpenXR_GL_Hidden";
    if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return ACVR_ERROR;
    window_=CreateWindowExW(0,cls.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,module,nullptr);
    if(!window_)return ACVR_ERROR;
    dc_=GetDC(window_);if(!dc_)return ACVR_ERROR;
    PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cDepthBits=24;pfd.iLayerType=PFD_MAIN_PLANE;
    const int format=ChoosePixelFormat(dc_,&pfd);
    if(!format||!SetPixelFormat(dc_,format,&pfd))return ACVR_ERROR;
    HGLRC legacy=wglCreateContext(dc_);if(!legacy||!wglMakeCurrent(dc_,legacy))return ACVR_ERROR;
    auto create=reinterpret_cast<CreateContextFn>(wglGetProcAddress("wglCreateContextAttribsARB"));
    if(!create){wglMakeCurrent(nullptr,nullptr);wglDeleteContext(legacy);return ACVR_UNSUPPORTED;}
    // WGL_CONTEXT_MAJOR/MINOR_VERSION_ARB and COMPATIBILITY_PROFILE_BIT_ARB.
    const int attribs[]{0x2091,3,0x2092,3,0x9126,0x00000002,0};
    context_=create(dc_,nullptr,attribs);
    wglMakeCurrent(nullptr,nullptr);wglDeleteContext(legacy);
    if(!context_||!wglMakeCurrent(dc_,context_))return ACVR_UNSUPPORTED;
    return (resolve(gl_.gen_fbo,"glGenFramebuffers")&&resolve(gl_.delete_fbo,"glDeleteFramebuffers")&&
        resolve(gl_.bind_fbo,"glBindFramebuffer")&&resolve(gl_.attach_colour,"glFramebufferTexture2D")&&
        resolve(gl_.check_fbo,"glCheckFramebufferStatus"))?
        ACVR_OK:ACVR_UNSUPPORTED;
}

acvr_result OpenXrGlHost::init_xr() {
    // A caller may point at a previously installed loader. Never search CWD/PATH.
    wchar_t path[32768]{};const DWORD n=GetEnvironmentVariableW(L"ACVR_OPENXR_LOADER_DLL",path,32768);
    if(n && n<32768) {
        if(!(n>2 && path[1]==L':' && (path[2]==L'\\'||path[2]==L'/')))return ACVR_BAD_ARGUMENT;
        loader_=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
    } else loader_=LoadLibraryExW(L"openxr_loader.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!loader_)return ACVR_UNSUPPORTED;
    xrGetInstanceProcAddr_=reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(loader_,"xrGetInstanceProcAddr"));
    if(!xrGetInstanceProcAddr_)return ACVR_UNSUPPORTED;
    if(!proc("xrCreateInstance",CreateInstance_))return ACVR_UNSUPPORTED;
    const char *extensions[]{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,XR_META_TOUCH_CONTROLLER_PLUS_EXTENSION_NAME};
    uint32_t extension_count=1;
    if(proc("xrEnumerateInstanceExtensionProperties",EnumerateInstanceExtensionProperties_)) {
        uint32_t count=0;
        if(XR_SUCCEEDED(EnumerateInstanceExtensionProperties_(nullptr,0,&count,nullptr))&&count&&count<4096) {
            std::vector<XrExtensionProperties> available(count);
            for(auto &entry:available)entry.type=XR_TYPE_EXTENSION_PROPERTIES;
            if(XR_SUCCEEDED(EnumerateInstanceExtensionProperties_(nullptr,count,&count,available.data()))&&
               std::any_of(available.begin(),available.end(),[](const XrExtensionProperties &entry){
                   return std::strcmp(entry.extensionName,XR_META_TOUCH_CONTROLLER_PLUS_EXTENSION_NAME)==0;
               }))extension_count=2;
        }
    }
    XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
    std::memcpy(info.applicationInfo.applicationName,application_name_.c_str(),application_name_.size()+1);
    std::memcpy(info.applicationInfo.engineName,"libacvr",sizeof("libacvr"));
    // This provider uses core 1.0 plus explicitly enabled extensions. A newer
    // header must not silently raise the runtime's minimum core requirement.
    info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
    info.enabledExtensionCount=extension_count;info.enabledExtensionNames=extensions;
    const auto created=CreateInstance_(&info,&instance_);
    if(XR_FAILED(created)){
        std::fprintf(stderr,"libacvr: xrCreateInstance failed (%d), requested OpenXR 1.0\n",int(created));
        return ACVR_UNSUPPORTED;
    }
#define LOAD(name) if(!proc("xr" #name,name##_))return ACVR_UNSUPPORTED
    LOAD(DestroyInstance);LOAD(GetSystem);LOAD(EnumerateViewConfigurationViews);
    LOAD(GetOpenGLGraphicsRequirementsKHR);LOAD(CreateSession);LOAD(DestroySession);
    LOAD(CreateReferenceSpace);LOAD(DestroySpace);LOAD(EnumerateSwapchainFormats);
    LOAD(CreateSwapchain);LOAD(DestroySwapchain);LOAD(EnumerateSwapchainImages);
    LOAD(PollEvent);LOAD(BeginSession);LOAD(EndSession);LOAD(WaitFrame);LOAD(BeginFrame);
    LOAD(EndFrame);LOAD(LocateViews);LOAD(LocateSpace);LOAD(AcquireSwapchainImage);
    LOAD(WaitSwapchainImage);LOAD(ReleaseSwapchainImage);LOAD(StringToPath);
    LOAD(CreateActionSet);LOAD(DestroyActionSet);LOAD(CreateAction);LOAD(CreateActionSpace);
    LOAD(SuggestInteractionProfileBindings);LOAD(AttachSessionActionSets);LOAD(SyncActions);
    LOAD(GetActionStateBoolean);LOAD(GetActionStateFloat);
#undef LOAD
    XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};system_info.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if(XR_FAILED(GetSystem_(instance_,&system_info,&system_)))return ACVR_UNSUPPORTED;
    XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    if(XR_FAILED(GetOpenGLGraphicsRequirementsKHR_(instance_,system_,&requirements)))return ACVR_UNSUPPORTED;
    GLint major=0,minor=0;glGetIntegerv(kMajor,&major);glGetIntegerv(kMinor,&minor);
    const XrVersion version=XR_MAKE_VERSION(static_cast<uint64_t>(major),static_cast<uint64_t>(minor),0);
    if(version<requirements.minApiVersionSupported||version>requirements.maxApiVersionSupported)
        return ACVR_UNSUPPORTED;
    XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    binding.hDC=dc_;binding.hGLRC=context_;
    XrSessionCreateInfo session_info{XR_TYPE_SESSION_CREATE_INFO};session_info.next=&binding;session_info.systemId=system_;
    if(XR_FAILED(CreateSession_(instance_,&session_info,&session_)))return ACVR_UNSUPPORTED;
    XrReferenceSpaceCreateInfo reference{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    reference.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;reference.poseInReferenceSpace.orientation.w=1;
    if(XR_FAILED(CreateReferenceSpace_(session_,&reference,&stage_)))return ACVR_ERROR;
    reference.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;
    if(XR_FAILED(CreateReferenceSpace_(session_,&reference,&view_space_)))return ACVR_ERROR;
    const auto swapchains=init_swapchains();
    if(swapchains!=ACVR_OK){std::fprintf(stderr,"libacvr: swapchain initialization failed (%d)\n",int(swapchains));return swapchains;}
    const auto actions=init_actions();
    if(actions!=ACVR_OK){std::fprintf(stderr,"libacvr: action initialization failed (%d)\n",int(actions));return actions;}
    return ACVR_OK;
}

acvr_result OpenXrGlHost::init_swapchains() {
    uint32_t count=0;
    if(XR_FAILED(EnumerateViewConfigurationViews_(instance_,system_,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr))||count!=2)
        return ACVR_UNSUPPORTED;
    std::array<XrViewConfigurationView,2> views{{{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}}};
    if(XR_FAILED(EnumerateViewConfigurationViews_(instance_,system_,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,views.data())))return ACVR_ERROR;
    count=0;if(XR_FAILED(EnumerateSwapchainFormats_(session_,0,&count,nullptr))||!count)return ACVR_ERROR;
    std::vector<int64_t> formats(count);
    if(XR_FAILED(EnumerateSwapchainFormats_(session_,count,&count,formats.data())))return ACVR_ERROR;
    colour_format_=select_gl_swapchain_format(formats.data(),formats.size());
    if(!colour_format_){
        std::fprintf(stderr,"libacvr: no supported RGBA8/sRGB8-alpha8 swapchain; runtime formats:");
        for(const auto format:formats)std::fprintf(stderr," %lld",static_cast<long long>(format));
        std::fprintf(stderr,"\n");return ACVR_UNSUPPORTED;
    }
    std::fprintf(stderr,"libacvr: selected swapchain format 0x%llx\n",static_cast<long long>(colour_format_));
    for(unsigned eye=0;eye<2;++eye) {
        auto &s=eyes_[eye];s.width=views[eye].recommendedImageRectWidth;s.height=views[eye].recommendedImageRectHeight;
        if(!s.width||!s.height)return ACVR_ERROR;
        XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};info.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        info.format=colour_format_;info.sampleCount=1;info.width=s.width;info.height=s.height;
        info.faceCount=1;info.arraySize=1;info.mipCount=1;
        const auto created=CreateSwapchain_(session_,&info,&s.handle);
        if(XR_FAILED(created)){std::fprintf(stderr,"libacvr: eye %u xrCreateSwapchain failed (%d)\n",eye,int(created));return ACVR_ERROR;}
        count=0;if(XR_FAILED(EnumerateSwapchainImages_(s.handle,0,&count,nullptr))||!count)return ACVR_ERROR;
        s.images.resize(count);s.targets.resize(count);
        for(auto &image:s.images)image.type=XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
        if(XR_FAILED(EnumerateSwapchainImages_(s.handle,count,&count,
            reinterpret_cast<XrSwapchainImageBaseHeader *>(s.images.data()))))return ACVR_ERROR;
        for(uint32_t i=0;i<count;++i) {
            auto &target=s.targets[i];target.colour=s.images[i].image;
            gl_.gen_fbo(1,&target.fbo);glGenTextures(1,&target.depth);
            gl_.bind_fbo(kFrame,target.fbo);
            gl_.attach_colour(kFrame,kColourAttachment,GL_TEXTURE_2D,target.colour,0);
            // acvr_render_target defines depth_image as a texture name, and
            // GunGlRenderer verifies the borrowed attachment type and identity.
            glBindTexture(GL_TEXTURE_2D,target.depth);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D,0,kDepth,s.width,s.height,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
            gl_.attach_colour(kFrame,kDepthAttachment,GL_TEXTURE_2D,target.depth,0);
            const auto status=gl_.check_fbo(kFrame);
            if(status!=kComplete){std::fprintf(stderr,"libacvr: eye %u image %u framebuffer incomplete (0x%x), GL error 0x%x\n",eye,i,unsigned(status),unsigned(glGetError()));return ACVR_ERROR;}
        }
    }
    glBindTexture(GL_TEXTURE_2D,0);gl_.bind_fbo(kFrame,0);return ACVR_OK;
}

acvr_result OpenXrGlHost::init_actions() {
    XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::memcpy(set.actionSetName,"acvr_hands",sizeof("acvr_hands"));
    std::memcpy(set.localizedActionSetName,"ACVR hands",sizeof("ACVR hands"));
    if(XR_FAILED(CreateActionSet_(instance_,&set,&actions_)))return ACVR_ERROR;
    const char *paths[]{"/user/hand/right","/user/hand/left"};
    for(unsigned i=0;i<2;++i)if(XR_FAILED(StringToPath_(instance_,paths[i],&hand_paths_[i])))return ACVR_ERROR;
    const auto create=[&](XrActionType type,const char *name,XrAction &out) {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};info.actionType=type;
        std::snprintf(info.actionName,sizeof(info.actionName),"%s",name);
        std::snprintf(info.localizedActionName,sizeof(info.localizedActionName),"%s",name);
        info.countSubactionPaths=2;info.subactionPaths=hand_paths_;
        const auto result=CreateAction_(actions_,&info,&out);
        if(XR_FAILED(result))std::fprintf(stderr,"libacvr: xrCreateAction %s failed (%d)\n",name,int(result));
        return XR_SUCCEEDED(result);
    };
    if(!create(XR_ACTION_TYPE_POSE_INPUT,"grip_pose",grip_)||!create(XR_ACTION_TYPE_POSE_INPUT,"aim_pose",aim_)||
       !create(XR_ACTION_TYPE_FLOAT_INPUT,"trigger_value",trigger_)||
       !create(XR_ACTION_TYPE_FLOAT_INPUT,"squeeze_value",squeeze_)||
       !create(XR_ACTION_TYPE_BOOLEAN_INPUT,"select_click",select_)||
       !create(XR_ACTION_TYPE_BOOLEAN_INPUT,"primary_click",primary_)||
       !create(XR_ACTION_TYPE_BOOLEAN_INPUT,"secondary_click",secondary_)||
       !create(XR_ACTION_TYPE_FLOAT_INPUT,"cal_stick_x",stick_x_)||
       !create(XR_ACTION_TYPE_FLOAT_INPUT,"cal_stick_y",stick_y_))return ACVR_ERROR;
    for(unsigned i=0;i<2;++i) {
        XrActionSpaceCreateInfo info{XR_TYPE_ACTION_SPACE_CREATE_INFO};info.poseInActionSpace.orientation.w=1;
        info.subactionPath=hand_paths_[i];info.action=grip_;
        if(XR_FAILED(CreateActionSpace_(session_,&info,&grip_spaces_[i])))return ACVR_ERROR;
        info.action=aim_;if(XR_FAILED(CreateActionSpace_(session_,&info,&aim_spaces_[i])))return ACVR_ERROR;
    }
    // Suggest the portable simple profile and two common tracked-controller
    // profiles. Optional profile failures do not invalidate the session.
    const auto suggest=[&](const char *profile_path,const char *fire_path,XrAction fire_action,
                           const char *hold_path,bool required){
        XrPath profile=XR_NULL_PATH;
        if(XR_FAILED(StringToPath_(instance_,profile_path,&profile)))return !required;
        std::array<XrActionSuggestedBinding,16> bindings{};uint32_t n=0;
        for(unsigned i=0;i<2;++i){
            const std::string prefix=paths[i];XrPath path=XR_NULL_PATH;
            for(const auto pair:{std::pair<const char *,XrAction>{"/input/grip/pose",grip_},
                                 {"/input/aim/pose",aim_},{fire_path,fire_action},
                                 {hold_path,squeeze_}}){
                if(pair.first && XR_SUCCEEDED(StringToPath_(instance_,(prefix+pair.first).c_str(),&path)))
                    bindings[n++]={pair.second,path};
            }
            const bool touch=std::strcmp(profile_path,"/interaction_profiles/oculus/touch_controller")==0||
                std::strcmp(profile_path,"/interaction_profiles/meta/touch_plus_controller")==0||
                std::strcmp(profile_path,"/interaction_profiles/meta/touch_controller_plus")==0;
            const bool index=std::strcmp(profile_path,"/interaction_profiles/valve/index_controller")==0;
            const char *primary_path=nullptr;
            if(touch)
                primary_path=i==0?"/input/a/click":"/input/x/click";
            else if(index)
                primary_path="/input/a/click";
            if(primary_path&&XR_SUCCEEDED(StringToPath_(instance_,(prefix+primary_path).c_str(),&path)))
                bindings[n++]={primary_,path};
            if(touch||index) {
                const char *secondary_path=touch?(i==0?"/input/b/click":"/input/y/click"):"/input/b/click";
                for(const auto pair:{std::pair<const char *,XrAction>{secondary_path,secondary_},
                                     {"/input/thumbstick/x",stick_x_},{"/input/thumbstick/y",stick_y_}})
                    if(XR_SUCCEEDED(StringToPath_(instance_,(prefix+pair.first).c_str(),&path)))
                        bindings[n++]={pair.second,path};
            }
        }
        XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggested.interactionProfile=profile;suggested.countSuggestedBindings=n;suggested.suggestedBindings=bindings.data();
        const auto result=SuggestInteractionProfileBindings_(instance_,&suggested);
        if(XR_FAILED(result)&&required)std::fprintf(stderr,"libacvr: required bindings %s failed (%d)\n",profile_path,int(result));
        return XR_SUCCEEDED(result)||!required;
    };
    if(!suggest("/interaction_profiles/khr/simple_controller","/input/select/click",select_,nullptr,true))return ACVR_ERROR;
    suggest("/interaction_profiles/oculus/touch_controller","/input/trigger/value",trigger_,"/input/squeeze/value",false);
    suggest("/interaction_profiles/valve/index_controller","/input/trigger/value",trigger_,"/input/squeeze/value",false);
    // An OpenXR 1.1 runtime may expose these promoted profiles without the
    // extension name. Suggestions are optional and fail closed if unsupported.
    suggest("/interaction_profiles/meta/touch_plus_controller","/input/trigger/value",trigger_,"/input/squeeze/value",false);
    suggest("/interaction_profiles/meta/touch_controller_plus","/input/trigger/value",trigger_,"/input/squeeze/value",false);
    XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach.countActionSets=1;attach.actionSets=&actions_;
    const auto attached=AttachSessionActionSets_(session_,&attach);
    if(XR_FAILED(attached))std::fprintf(stderr,"libacvr: xrAttachSessionActionSets failed (%d)\n",int(attached));
    return XR_SUCCEEDED(attached)?ACVR_OK:ACVR_ERROR;
}

acvr_result OpenXrGlHost::fill_tracking(Display &d) {
    d.calibration=record<acvr_controller_sample>();
    d.tracking=record<acvr_tracking>();d.tracking.head=identity_pose();
    d.tracking.right=record<acvr_hand_tracking>();d.tracking.left=record<acvr_hand_tracking>();
    d.tracking.right.hand=ACVR_HAND_RIGHT;d.tracking.left.hand=ACVR_HAND_LEFT;
    d.tracking.right.grip=d.tracking.right.aim=d.tracking.left.grip=d.tracking.left.aim=identity_pose();
    // XrTime is monotonic but runtime-specific. Map deltas to the host's
    // positive domain; all samples and display predictions use this one clock.
    if(!xr_time_known_){last_xr_time_=frame_.predictedDisplayTime;monotonic_time_=1;xr_time_known_=true;}
    else {
        const auto delta=frame_.predictedDisplayTime-last_xr_time_;
        if(delta<=0||delta>INT64_MAX-monotonic_time_)return ACVR_ERROR;
        monotonic_time_+=delta;last_xr_time_=frame_.predictedDisplayTime;
    }
    d.tracking.sample_time_ns=d.tracking.predicted_display_time_ns=monotonic_time_;
    XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};
    if(XR_FAILED(LocateSpace_(view_space_,stage_,frame_.predictedDisplayTime,&head)))return ACVR_ERROR;
    d.tracking.head_flags=flags(head.locationFlags);
    if(d.tracking.head_flags==3)d.tracking.head=pose(head.pose);
    if(first_anchor_ && d.focused && d.tracking.head_flags==3 && anchor_mode_==ACVR_ANCHOR_COCKPIT) {
        // A vertical first gaze has no usable horizontal heading. Keep the
        // initial anchor pending until a focused frame supplies one.
        if(recenter_anchor(d.tracking.head,1.6f,anchor_)==ACVR_OK)first_anchor_=false;
    }
    d.scene_from_stage=anchor_;
    XrActiveActionSet active{actions_,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
    sync.countActiveActionSets=1;sync.activeActionSets=&active;
    if(XR_FAILED(SyncActions_(session_,&sync)))return ACVR_ERROR;
    for(unsigned i=0;i<2;++i) {
        auto &hand=i==0?d.tracking.right:d.tracking.left;
        for(unsigned j=0;j<2;++j) {
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
            if(XR_FAILED(LocateSpace_(j==0?grip_spaces_[i]:aim_spaces_[i],stage_,frame_.predictedDisplayTime,&location)))return ACVR_ERROR;
            auto &f=j==0?hand.grip_flags:hand.aim_flags;
            auto &p=j==0?hand.grip:hand.aim;
            f=flags(location.locationFlags);if(f==3)p=pose(location.pose);
        }
        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.subactionPath=hand_paths_[i];
        XrActionStateBoolean select{XR_TYPE_ACTION_STATE_BOOLEAN};get.action=select_;
        if(XR_SUCCEEDED(GetActionStateBoolean_(session_,&get,&select))&&select.isActive){
            d.trigger[i]=select.currentState!=0;
            d.calibration.active[i]|=ACVR_CALIB_TRIGGER_ACTIVE;
        }
        XrActionStateFloat trigger{XR_TYPE_ACTION_STATE_FLOAT};get.action=trigger_;
        if(XR_SUCCEEDED(GetActionStateFloat_(session_,&get,&trigger))&&trigger.isActive&&
           std::isfinite(trigger.currentState)&&trigger.currentState>=0.f&&trigger.currentState<=1.f) {
            d.trigger[i]=trigger.currentState>=.55f;
            d.calibration.active[i]|=ACVR_CALIB_TRIGGER_ACTIVE;
            d.calibration.trigger[i]=trigger.currentState;
        } else if(d.calibration.active[i]&ACVR_CALIB_TRIGGER_ACTIVE)
            d.calibration.trigger[i]=d.trigger[i]?1.f:0.f;
        d.controllers[i].trigger=d.trigger[i]?1.f:0.f;
        XrActionStateFloat squeeze{XR_TYPE_ACTION_STATE_FLOAT};get.action=squeeze_;
        if(XR_SUCCEEDED(GetActionStateFloat_(session_,&get,&squeeze))&&squeeze.isActive&&
           std::isfinite(squeeze.currentState)&&squeeze.currentState>=0.f&&squeeze.currentState<=1.f)
            d.controllers[i].grip=squeeze.currentState;
        XrActionStateBoolean primary{XR_TYPE_ACTION_STATE_BOOLEAN};get.action=primary_;
        if(XR_SUCCEEDED(GetActionStateBoolean_(session_,&get,&primary))&&primary.isActive)
            d.controllers[i].primary=primary.currentState!=0;
        XrActionStateBoolean secondary{XR_TYPE_ACTION_STATE_BOOLEAN};get.action=secondary_;
        if(XR_SUCCEEDED(GetActionStateBoolean_(session_,&get,&secondary))&&secondary.isActive){
            d.calibration.active[i]|=ACVR_CALIB_SECONDARY_ACTIVE;
            d.calibration.secondary_down[i]=secondary.currentState?1u:0u;
        }
        for(const auto pair:{std::pair<XrAction,uint32_t>{stick_x_,ACVR_CALIB_STICK_X_ACTIVE},
                             {stick_y_,ACVR_CALIB_STICK_Y_ACTIVE}}){
            XrActionStateFloat axis{XR_TYPE_ACTION_STATE_FLOAT};get.action=pair.first;
            if(XR_SUCCEEDED(GetActionStateFloat_(session_,&get,&axis))&&axis.isActive&&
               std::isfinite(axis.currentState)&&std::abs(axis.currentState)<=1.f){
                d.calibration.active[i]|=pair.second;
                (pair.second==ACVR_CALIB_STICK_X_ACTIVE?d.calibration.stick_x[i]:d.calibration.stick_y[i])=axis.currentState;
            }
        }
#ifdef ACVR_GUN_GL
        hand_controls_[i]=(hand.grip_flags&3u)==3u?d.controllers[i]:ControllerButtons{};
#endif
    }
    if(fixed_capture_){
        // Diagnostic mode must never silently fall back to live hands or input.
        if(d.tracking.head_flags!=3)return ACVR_ERROR;
        if(!fixed_capture_ready_){
            if(!fixed_capture_poses(d.tracking.head,fixed_capture_hands_))return ACVR_ERROR;
            fixed_capture_ready_=true;
        }
        for(unsigned i=0;i<2;++i){
            auto &hand=i==0?d.tracking.right:d.tracking.left;
            hand.grip=hand.aim=fixed_capture_hands_[i];
            hand.grip_flags=hand.aim_flags=ACVR_POSE_POSITION_VALID|ACVR_POSE_ORIENTATION_VALID;
            d.trigger[i]=d.reload[i]=false;d.controllers[i]={};
#ifdef ACVR_GUN_GL
            hand_controls_[i]={};
#endif
        }
        d.calibration=record<acvr_controller_sample>();
    }
    uint32_t view_count=0;XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
    locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locate.displayTime=frame_.predictedDisplayTime;locate.space=stage_;
    XrViewState state{XR_TYPE_VIEW_STATE};
    if(XR_FAILED(LocateViews_(session_,&locate,&state,2,&view_count,xr_views_))||view_count!=2)return ACVR_ERROR;
    if((state.viewStateFlags&(XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT))!=
       (XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
        d.should_render=false;return ACVR_OK;
    }
    for(unsigned i=0;i<2;++i) {
        auto &eye=acvr_eyes_[i];eye=record<acvr_eye>();eye.eye_index=i;
        eye.rect_width=eyes_[i].width;eye.rect_height=eyes_[i].height;
        const auto &f=xr_views_[i].fov;
        if(compose_eye(anchor_,pose(xr_views_[i].pose),scale_,
                       {f.angleLeft,f.angleRight,f.angleDown,f.angleUp},.05f,100.f,ACVR_GRAPHICS_GL,eye)!=ACVR_OK) {
            d.should_render=false;return ACVR_OK;
        }
        auto &layer=layer_eyes_[i];layer={XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        layer.pose=xr_views_[i].pose;layer.fov=f;layer.subImage.swapchain=eyes_[i].handle;
        layer.subImage.imageRect.extent.width=static_cast<int32_t>(eyes_[i].width);
        layer.subImage.imageRect.extent.height=static_cast<int32_t>(eyes_[i].height);
    }
    return ACVR_OK;
}

acvr_result OpenXrGlHost::begin(Display &d) {
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    XrResult polled=XR_SUCCESS;
    while((polled=PollEvent_(instance_,&event))==XR_SUCCESS) {
        if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            const auto &state=*reinterpret_cast<XrEventDataSessionStateChanged *>(&event);
            if(state.session==session_) {
                std::fprintf(stderr,"libacvr: OpenXR session state %d\n",int(state.state));
                session_state_=state.state;
                if(state.state==XR_SESSION_STATE_READY && !running_) {
                    XrSessionBeginInfo info{XR_TYPE_SESSION_BEGIN_INFO};
                    info.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if(XR_FAILED(BeginSession_(session_,&info)))return ACVR_ERROR;
                    running_=true;
                } else if(state.state==XR_SESSION_STATE_STOPPING && running_) {
                    if(XR_FAILED(EndSession_(session_)))return ACVR_ERROR;
                    running_=false;exiting_=true;
                } else if(state.state==XR_SESSION_STATE_EXITING||state.state==XR_SESSION_STATE_LOSS_PENDING)
                    exiting_=true;
            }
        } else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)exiting_=true;
        event={XR_TYPE_EVENT_DATA_BUFFER};
    }
    if(polled!=XR_EVENT_UNAVAILABLE)return ACVR_ERROR;
    if(exiting_)return ACVR_STOPPED;
    if(!running_)return ACVR_MORE;
    XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};frame_={XR_TYPE_FRAME_STATE};
    if(XR_FAILED(WaitFrame_(session_,&wait,&frame_)))return ACVR_ERROR;
    XrFrameBeginInfo begin_info{XR_TYPE_FRAME_BEGIN_INFO};
    if(XR_FAILED(BeginFrame_(session_,&begin_info)))return ACVR_ERROR;
    in_frame_=true;submit_=frame_.shouldRender!=0;
    ++waited_frames_;
    if(submit_)++render_requested_frames_;
    if(session_state_==XR_SESSION_STATE_FOCUSED)++focused_frames_;
    if(!have_render_request_||last_render_request_!=submit_) {
        std::fprintf(stderr,"libacvr: OpenXR shouldRender=%d at session state %d\n",
                     int(submit_),int(session_state_));
        have_render_request_=true;last_render_request_=submit_;
    }
    gun_drawn_[0]=gun_drawn_[1]=false;
    const auto abort_frame=[&]() {end(false);return ACVR_ERROR;};
    d.focused=session_state_==XR_SESSION_STATE_FOCUSED;d.should_render=submit_;
    if(fill_tracking(d)!=ACVR_OK)return abort_frame();
    if(!d.should_render)return ACVR_OK;
    for(unsigned i=0;i<2;++i) {
        auto &s=eyes_[i];XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if(XR_FAILED(AcquireSwapchainImage_(s.handle,&acquire,&s.index)))return abort_frame();
        s.acquired=true;
        XrSwapchainImageWaitInfo image_wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};image_wait.timeout=XR_INFINITE_DURATION;
        if(XR_FAILED(WaitSwapchainImage_(s.handle,&image_wait)))return abort_frame();
        if(s.index>=s.targets.size())return abort_frame();
        const auto &target=s.targets[s.index];
        auto t=record<acvr_render_target>();t.api=ACVR_GRAPHICS_GL;t.width=s.width;t.height=s.height;
        t.array_layers=t.sample_count=1;t.colour_format=colour_format_;t.depth_format=kDepth;
        t.colour_image=target.colour;t.depth_image=target.depth;t.framebuffer=target.fbo;
        auto draw=record<acvr_draw_info>();draw.target=t;draw.view_count=1;
        draw.view_stride=sizeof(acvr_eye);draw.views=&acvr_eyes_[i];d.eyes[i]=draw;
    }
    return ACVR_OK;
}

acvr_result OpenXrGlHost::end(bool rendered) {
    if(!in_frame_)return ACVR_BAD_STATE;
    bool good=true;
    glFlush();
    const bool capture_now=capture_enabled_&&!capture_done_&&!capture_failed_&&rendered&&submit_&&
                           (!fixed_capture_||(fixed_capture_ready_&&gun_drawn_[0]&&gun_drawn_[1]));
    bool read_ok=true;
    if(capture_now)for(unsigned i=0;i<2;++i)read_ok=read_capture_eye(i)&&read_ok;
    for(auto &s:eyes_)if(s.acquired) {
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        if(XR_FAILED(ReleaseSwapchainImage_(s.handle,&release)))good=false;
        s.acquired=false;
    }
    XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projection.space=stage_;projection.viewCount=2;projection.views=layer_eyes_.data();
    const XrCompositionLayerBaseHeader *layers[]{reinterpret_cast<const XrCompositionLayerBaseHeader *>(&projection)};
    XrFrameEndInfo info{XR_TYPE_FRAME_END_INFO};info.displayTime=frame_.predictedDisplayTime;
    info.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    info.layerCount=rendered&&submit_&&good?1u:0u;info.layers=info.layerCount?layers:nullptr;
    const auto result=EndFrame_(session_,&info);in_frame_=false;
    if(XR_SUCCEEDED(result)&&info.layerCount)++submitted_frames_;
    if(capture_now){
        if(!read_ok||!good||XR_FAILED(result)||!write_capture_pair()){
            capture_failed_=true;
            write_capture_receipt("error",!read_ok?"readback":(!good||XR_FAILED(result)?"frame_submission":"file_write"));
            return ACVR_ERROR;
        }
        capture_done_=true;
    }
    return good&&XR_SUCCEEDED(result)?ACVR_OK:ACVR_ERROR;
}

acvr_result OpenXrGlHost::configure_capture() {
    wchar_t fixed[3]{};
    const DWORD fixed_length=GetEnvironmentVariableW(L"ACVR_GUN_ROOM_FIXED_POSES",fixed,3);
    if(fixed_length&&(fixed_length!=1||fixed[0]!=L'1'))return ACVR_BAD_ARGUMENT;
    fixed_capture_=fixed_length==1;
    const DWORD length=GetEnvironmentVariableW(L"ACVR_GUN_ROOM_CAPTURE_DIR",nullptr,0);
    if(!length)return !fixed_capture_&&GetLastError()==ERROR_ENVVAR_NOT_FOUND?ACVR_OK:ACVR_BAD_ARGUMENT;
    if(length>1024||content_root_.empty()||model_id_.empty())return ACVR_BAD_ARGUMENT;
    std::vector<wchar_t> value(length);
    if(GetEnvironmentVariableW(L"ACVR_GUN_ROOM_CAPTURE_DIR",value.data(),length)!=length-1)return ACVR_BAD_ARGUMENT;
    try {
        const auto root=std::filesystem::u8path(content_root_).lexically_normal();
        capture_dir_=std::filesystem::path(value.data()).lexically_normal();
        const auto base=(root/".local"/"gun-room").lexically_normal();
        const auto leaf=capture_dir_.filename().string();
        if(!capture_dir_.is_absolute()||capture_dir_.parent_path()!=base||leaf.empty()||leaf.size()>64||
           !std::filesystem::is_directory(capture_dir_)||!std::filesystem::is_empty(capture_dir_)||
           !no_reparse_components(capture_dir_))return ACVR_BAD_ARGUMENT;
        for(char c:leaf)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                             (c>='0'&&c<='9')||c=='-'||c=='_'))return ACVR_BAD_ARGUMENT;
        for(char c:model_id_)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return ACVR_BAD_ARGUMENT;
        std::vector<uint8_t> bytes;
        if(!read_bounded(root/"assets"/"guns"/(model_id_+".glb"),32*1024*1024,bytes))return ACVR_BAD_ARGUMENT;
        gun_hash_=sha256(bytes);if(gun_hash_.empty())return ACVR_ERROR;
        const auto optional_hash=[&](const std::filesystem::path &path,std::string &dest){
            if(!std::filesystem::exists(path)){dest="absent";return true;}
            if(!read_bounded(path,32*1024*1024,bytes))return false;
            dest=sha256(bytes);return !dest.empty();
        };
        if(!optional_hash(root/"assets"/"hands"/"assembly.json",assembly_hash_))return ACVR_BAD_ARGUMENT;
        right_hand_hash_=left_hand_hash_="absent";
        capture_enabled_=true;return ACVR_OK;
    }catch(...){return ACVR_BAD_ARGUMENT;}
}

bool OpenXrGlHost::read_capture_eye(unsigned eye) noexcept {
    try {
        if(eye>1||!eyes_[eye].acquired||eyes_[eye].index>=eyes_[eye].targets.size())return false;
        const auto &s=eyes_[eye];
        if(!s.width||!s.height||s.width>4096||s.height>4096||
           uint64_t(s.width)*s.height*3>64*1024*1024)return false;
        auto &capture=captured_[eye];capture.width=s.width;capture.height=s.height;
        capture.rgb.resize(size_t(s.width)*s.height*3);
        GLint old_read=0,old_draw=0,old_alignment=0,old_row=0,old_skip_rows=0,old_skip_pixels=0,pbo=0;
        glGetIntegerv(kReadFramebufferBinding,&old_read);
        glGetIntegerv(kDrawFramebufferBinding,&old_draw);
        glGetIntegerv(GL_PACK_ALIGNMENT,&old_alignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH,&old_row);
        glGetIntegerv(GL_PACK_SKIP_ROWS,&old_skip_rows);
        glGetIntegerv(GL_PACK_SKIP_PIXELS,&old_skip_pixels);
        glGetIntegerv(kPixelPackBufferBinding,&pbo);
        if(glGetError()!=GL_NO_ERROR||pbo!=0)return false;
        gl_.bind_fbo(kReadFramebuffer,s.targets[s.index].fbo);
        GLint eye_read=0;glGetIntegerv(GL_READ_BUFFER,&eye_read);
        glReadBuffer(kColourAttachment);
        glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);
        glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);
        glReadPixels(0,0,static_cast<GLsizei>(s.width),static_cast<GLsizei>(s.height),
                     GL_RGB,GL_UNSIGNED_BYTE,capture.rgb.data());
        const bool good=glGetError()==GL_NO_ERROR;
        glPixelStorei(GL_PACK_ALIGNMENT,old_alignment);glPixelStorei(GL_PACK_ROW_LENGTH,old_row);
        glPixelStorei(GL_PACK_SKIP_ROWS,old_skip_rows);glPixelStorei(GL_PACK_SKIP_PIXELS,old_skip_pixels);
        glReadBuffer(static_cast<GLenum>(eye_read));
        gl_.bind_fbo(kReadFramebuffer,static_cast<GLuint>(old_read));
        gl_.bind_fbo(kDrawFramebuffer,static_cast<GLuint>(old_draw));
        if(glGetError()!=GL_NO_ERROR||!good)return false;
        const size_t stride=size_t(s.width)*3;
        for(size_t top=0,bottom=s.height-1;top<bottom;++top,--bottom)
            for(size_t x=0;x<stride;++x)std::swap(capture.rgb[top*stride+x],capture.rgb[bottom*stride+x]);
        return true;
    }catch(...){return false;}
}
bool OpenXrGlHost::write_capture_receipt(const char *status,const char *reason) noexcept {
    try {
        std::ostringstream json;
        json<<"{\"schema\":1,\"status\":\""<<status<<"\",\"reason\":\""<<reason
            <<"\",\"model\":\""<<model_id_<<"\",\"actual_openxr_eye_readback\":"
            <<(std::strcmp(status,"captured")==0?"true":"false")
            <<",\"controller_pose_source\":\""<<(fixed_capture_?"fixed-authoring-poses":"openxr-actions")
            <<"\",\"controller_tracking_verified\":false";
        for(const auto &view:kStereoCaptureViews){
            const auto index=view.view_index;
            json<<",\""<<view.name<<"\":{\"width\":"<<captured_[index].width
                <<",\"height\":"<<captured_[index].height
                <<",\"gun_overlay_drawn\":"<<(gun_drawn_[index]?"true":"false")<<"}";
        }
        json<<",\"source_sha256\":{\"gun_glb\":\""<<gun_hash_
            <<"\",\"hand_assembly\":\""<<assembly_hash_
            <<"\",\"right_hand_glb\":\""<<right_hand_hash_
            <<"\",\"left_hand_glb\":\""<<left_hand_hash_<<"\"}}\n";
        return write_exclusive(capture_dir_/"receipt.json",json.str(),{});
    }catch(...){return false;}
}
bool OpenXrGlHost::write_capture_pair() noexcept {
    try {
        const auto save=[&](const std::filesystem::path &path,const CapturedEye &eye){
            const auto header="P6\n"+std::to_string(eye.width)+" "+std::to_string(eye.height)+"\n255\n";
            return write_exclusive(path,header,eye.rgb);
        };
        const auto left=capture_dir_/kStereoCaptureViews[0].filename;
        const auto right=capture_dir_/kStereoCaptureViews[1].filename;
        const bool left_written=save(left,captured_[kStereoCaptureViews[0].view_index]);
        const bool right_written=left_written&&save(right,captured_[kStereoCaptureViews[1].view_index]);
        if(right_written&&write_capture_receipt("captured","ok"))return true;
        if(left_written)DeleteFileW(left.c_str());
        if(right_written)DeleteFileW(right.c_str());
        return false;
    }catch(...){return false;}
}

acvr_result OpenXrGlHost::initialize() {
    const auto capture=configure_capture();if(capture!=ACVR_OK)return capture;
#ifdef ACVR_GUN_GL
    const auto prepared=prepare_hands();if(prepared!=ACVR_OK)return prepared;
#endif
    auto result=init_context();if(result!=ACVR_OK)return result;
    return init_xr();
}
#ifdef ACVR_GUN_GL
acvr_result OpenXrGlHost::prepare_hands() {
    if(content_root_.empty()||model_id_.empty())return ACVR_OK;
    const auto root=std::filesystem::u8path(content_root_);
    const auto manifest=root/"assets"/"hands"/"assembly.json";
    if(!std::filesystem::exists(manifest))
        return capture_enabled_&&assembly_hash_!="absent"?ACVR_BAD_ARGUMENT:ACVR_OK;
    std::vector<uint8_t> bytes;std::string error;
    if(!read_bounded(manifest,1024*1024,bytes))return ACVR_BAD_ARGUMENT;
    if(capture_enabled_&&sha256(bytes)!=assembly_hash_)return ACVR_BAD_ARGUMENT;
    if(!decode_hand_assembly(std::string_view(reinterpret_cast<const char *>(bytes.data()),bytes.size()),
                             model_id_,hand_assembly_,error))return ACVR_BAD_ARGUMENT;
    const auto gun_path=root/"assets"/"guns"/(model_id_+".glb");
    GunAsset gun_asset;
    if(!read_bounded(gun_path,32*1024*1024,bytes)||
       (capture_enabled_&&sha256(bytes)!=gun_hash_)||!decode_gun_glb(bytes,gun_asset,error)||
       !validate_hand_gun_nodes(hand_assembly_,gun_asset,error))return ACVR_BAD_ARGUMENT;
    for(unsigned i=0;i<2;++i){
        const auto asset=root/std::filesystem::u8path(hand_assembly_.hands[i].asset);
        if(!read_bounded(asset,32*1024*1024,bytes))return ACVR_BAD_ARGUMENT;
        if(capture_enabled_){auto &hash=i==0?right_hand_hash_:left_hand_hash_;
            hash=sha256(bytes);if(hash.empty())return ACVR_ERROR;}
        if(!decode_hand_glb(bytes,hand_assets_[i],error))return ACVR_BAD_ARGUMENT;
        if(!validate_hand_pose_nodes(hand_assembly_.hands[i],hand_assets_[i],error))return ACVR_BAD_ARGUMENT;
        auto metadata=asset;metadata.replace_extension(".toml");
        if(!read_bounded(metadata,1024*1024,bytes)||!decode_hand_motions(
            std::string_view(reinterpret_cast<const char *>(bytes.data()),bytes.size()),
            hand_assets_[i],asset.stem().string(),i==0?"right":"left",hand_motions_[i],error))return ACVR_BAD_ARGUMENT;
    }
    std::string set_text;
    const auto defaults=root/"data"/"controls"/"defaults.toml";
    if(std::filesystem::exists(defaults)){
        if(!read_bounded(defaults,1024*1024,bytes))return ACVR_BAD_ARGUMENT;
        std::string set_id;
        if(!select_shared_control_set(
            std::string_view(reinterpret_cast<const char *>(bytes.data()),bytes.size()),model_id_,set_id,error))
            return ACVR_BAD_ARGUMENT;
        if(!read_bounded(root/"data"/"controls"/(set_id+".toml"),1024*1024,bytes))return ACVR_BAD_ARGUMENT;
        set_text.assign(reinterpret_cast<const char *>(bytes.data()),bytes.size());
    }
    if(!describe_inspection_labels(hand_assembly_,set_text,model_id_,inspection_labels_,error))return ACVR_BAD_ARGUMENT;
    hands_ready_=true;return ACVR_OK;
}
acvr_result OpenXrGlHost::draw_hand(const acvr_draw_info &info,const GunDraw &gun,uint32_t hand_index,bool support) {
    if(hand_index>1||!gun.asset||gun.scene_from_node.size()!=gun.asset->nodes.size())return ACVR_BAD_ARGUMENT;
    const auto &mount=hand_assembly_.hands[hand_index];
    const auto &attachment=support?hand_assembly_.support_by_primary[gun.hand]:mount;
    const auto gun_node=std::find_if(gun.asset->nodes.begin(),gun.asset->nodes.end(),
        [&](const GunNode &node){return node.name==attachment.node;});
    if(gun_node==gun.asset->nodes.end())return ACVR_BAD_ARGUMENT;
    const auto gun_index=size_t(gun_node-gun.asset->nodes.begin());
    const auto root=product(gun.scene_from_node[gun_index],mount_matrix(attachment.translation,attachment.rotation_xyzw));
    const auto &asset=hand_assets_[hand_index];GunDraw draw;
    draw.asset=&asset;draw.visible=true;draw.laser_mode=ACVR_LASER_OFF;draw.lod=0;
    draw.body={.51f,.58f,.66f,1};draw.accent={.66f,.72f,.79f,1};
    const auto &control=hand_controls_[hand_index];
    const HandDriveLevels levels{control.trigger,control.grip,control.primary?1.f:0.f};
    if(!pose_hand_nodes(asset,mount,hand_motions_[hand_index],levels,root,draw.scene_from_node))return ACVR_BAD_ARGUMENT;
    return gun_gl_.draw(info,draw);
}
acvr_result OpenXrGlHost::draw_labels(const acvr_draw_info &info,const GunDraw &gun) {
    if(gun.hand>1||!gun.asset||!info.views)return ACVR_BAD_ARGUMENT;
    // A/X is a room-only inspect modifier: one held button reveals every part.
    // It does not claim the same button operates the labelled game action.
    if(!hand_controls_[gun.hand].primary||inspection_labels_.size()!=hand_assembly_.labels.size())return ACVR_OK;
    if(!label_font_){
        const HFONT font=CreateFontW(-20,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,ANSI_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FF_DONTCARE,L"Arial");
        if(!font)return ACVR_ERROR;
        const auto old=SelectObject(dc_,font);
        label_font_=glGenLists(128);
        const bool good=label_font_&&wglUseFontBitmapsW(dc_,0,128,label_font_)!=0;
        SelectObject(dc_,old);DeleteObject(font);
        if(!good){if(label_font_)glDeleteLists(label_font_,128);label_font_=0;return ACVR_ERROR;}
    }
    GLint previous_mode=0;glGetIntegerv(GL_MATRIX_MODE,&previous_mode);
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_LIST_BIT|GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadMatrixf(info.views->projection_from_view);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadMatrixf(info.views->view_from_scene);
    glColor3f(.92f,.95f,1.f);glListBase(label_font_);
    for(size_t i=0;i<hand_assembly_.labels.size();++i){
        const auto &label=hand_assembly_.labels[i];
        const auto node=std::find_if(gun.asset->nodes.begin(),gun.asset->nodes.end(),
            [&](const GunNode &n){return n.name==label.node;});
        if(node==gun.asset->nodes.end())continue;
        const auto &m=gun.scene_from_node[size_t(node-gun.asset->nodes.begin())];
        const float x=m[0]*label.local_offset[0]+m[4]*label.local_offset[1]+m[8]*label.local_offset[2]+m[12];
        const float y=m[1]*label.local_offset[0]+m[5]*label.local_offset[1]+m[9]*label.local_offset[2]+m[13];
        const float z=m[2]*label.local_offset[0]+m[6]*label.local_offset[1]+m[10]*label.local_offset[2]+m[14];
        glRasterPos3f(x,y,z);
        const auto &description=inspection_labels_[i];
        std::string text=description.part+" | "+description.game_action+" | "+description.controller_button;
        for(char &c:text){if(c=='_')c=' ';else if(c>='a'&&c<='z')c=char(c-'a'+'A');
            else if(static_cast<unsigned char>(c)>127)c='?';}
        glCallLists(static_cast<GLsizei>(text.size()),GL_UNSIGNED_BYTE,text.data());
    }
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(previous_mode);glPopAttrib();
    return glGetError()==GL_NO_ERROR?ACVR_OK:ACVR_ERROR;
}
#endif
void OpenXrGlHost::shutdown() noexcept {
    const auto caller_dc=wglGetCurrentDC();const auto caller_context=wglGetCurrentContext();
    const bool bound=context_&&dc_&&wglMakeCurrent(dc_,context_);
    if(in_frame_&&bound)end(false);
    if(waited_frames_)std::fprintf(stderr,
        "libacvr: OpenXR frames waited=%llu render_requested=%llu focused=%llu submitted=%llu\n",
        static_cast<unsigned long long>(waited_frames_),
        static_cast<unsigned long long>(render_requested_frames_),
        static_cast<unsigned long long>(focused_frames_),
        static_cast<unsigned long long>(submitted_frames_));
#ifdef ACVR_GUN_GL
    if(gun_ready_&&bound)gun_gl_.shutdown();
    if(label_font_&&bound)glDeleteLists(label_font_,128);
#endif
    for(auto &s:eyes_) {
        for(auto &target:s.targets) {
            if(target.fbo&&gl_.delete_fbo&&bound)gl_.delete_fbo(1,&target.fbo);
            if(target.depth&&bound)glDeleteTextures(1,&target.depth);
        }
        if(s.handle&&DestroySwapchain_)DestroySwapchain_(s.handle);
    }
    for(auto s:grip_spaces_)if(s&&DestroySpace_)DestroySpace_(s);
    for(auto s:aim_spaces_)if(s&&DestroySpace_)DestroySpace_(s);
    if(view_space_&&DestroySpace_)DestroySpace_(view_space_);
    if(stage_&&DestroySpace_)DestroySpace_(stage_);
    if(actions_&&DestroyActionSet_)DestroyActionSet_(actions_);
    if(session_&&DestroySession_)DestroySession_(session_);
    if(instance_&&DestroyInstance_)DestroyInstance_(instance_);
    if(context_){wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context_);}
    if(caller_context&&caller_context!=context_)wglMakeCurrent(caller_dc,caller_context);
    if(dc_&&window_)ReleaseDC(window_,dc_);
    if(window_)DestroyWindow(window_);
    if(loader_)FreeLibrary(loader_);
}
} // namespace
acvr_result make_openxr_gl_host(const char *name,const char *content_root,const char *model_id,std::unique_ptr<Host> &out) {
    out.reset();
    if(!name||!*name||std::strlen(name)>=XR_MAX_APPLICATION_NAME_SIZE)return ACVR_BAD_ARGUMENT;
    const auto caller_dc=wglGetCurrentDC();const auto caller_context=wglGetCurrentContext();
    try {
        auto host=std::make_unique<OpenXrGlHost>(name,content_root?content_root:"",model_id?model_id:"");
        const auto result=host->initialize();
        if(result==ACVR_OK)out=std::move(host);
        wglMakeCurrent(caller_dc,caller_context);
        return result;
    } catch (...) {wglMakeCurrent(caller_dc,caller_context);return ACVR_ERROR;}
}
} // namespace acvr
