// SPDX-License-Identifier: MIT
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include "acvr.h"
#include "room_scene.hpp"
#include "capture_plan.hpp"
#include "control_stdin.hpp"
#include "grip_calibration.hpp"
#include "grip_calibration_file.hpp"
#include "grip_calibration_draw.hpp"
#include "grip_calibration_adapter.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

using BindFbo=void (APIENTRY *)(GLenum,GLuint);
struct acvr_frame {uint64_t id=0;};
struct acvr_backend {
    room::Scene scene;
    acvr_frame frame;
    BindFbo bind_fbo=nullptr;
    room::Ray ray{};
    const room::GripCalibration *calibration=nullptr;
    unsigned hand=ACVR_HAND_RIGHT;
};
namespace {
const room::GripCalibration *current_calibration=nullptr;
unsigned current_hand=ACVR_HAND_RIGHT;
template<class T> T record(){T value{};ACVR_INIT(&value);return value;}
acvr_control_desc gun_control=record<acvr_control_desc>();
acvr_result ACVR_CALL open(const acvr_open_info *info,acvr_backend **out,acvr_backend_info *details){
    if(!info||!out||!details||info->graphics.api!=ACVR_GRAPHICS_GL||!info->graphics.get_proc)return ACVR_BAD_ARGUMENT;
    auto backend=new acvr_backend;
    backend->calibration=current_calibration;backend->hand=current_hand;
    backend->bind_fbo=reinterpret_cast<BindFbo>(info->graphics.get_proc(info->graphics.proc_user,"glBindFramebuffer"));
    if(!backend->bind_fbo){delete backend;return ACVR_UNSUPPORTED;}
    gun_control.kind=ACVR_CONTROL_GUN;gun_control.player=0;gun_control.semantic=0;
    gun_control.name_utf8="room gun";
    details->native_rate_num=90;details->native_rate_den=1;
    details->player_count=details->gun_count=1;details->scene_units_per_metre=1;
    details->capabilities=ACVR_CAP_RAYCAST|ACVR_CAP_REQUIRES_SHARED_DEPTH;
    details->control_count=1;details->control_stride=sizeof(gun_control);details->controls=&gun_control;
    *out=backend;return ACVR_OK;
}
void ACVR_CALL close(acvr_backend *b){delete b;}
acvr_result ACVR_CALL pause(acvr_backend *,uint32_t){return ACVR_OK;}
acvr_result ACVR_CALL inputs(acvr_backend *b,const acvr_inputs *in){
    if(!b||!in||in->gun_count!=1)return ACVR_BAD_ARGUMENT;
    if(in->guns[0].trigger&ACVR_INPUT_PRESSED)b->scene.shoot(b->ray);
    return ACVR_OK;
}
acvr_result ACVR_CALL step(acvr_backend *b,const acvr_step_info *s,acvr_frame **out,acvr_frame_info *info){
    if(!b||!s||!out||!info)return ACVR_BAD_ARGUMENT;
    b->frame.id=s->tick_id;*out=&b->frame;
    info->frame_id=s->tick_id;info->camera_count=1;info->primary_camera_id=0;
    return ACVR_OK;
}
void ACVR_CALL release(acvr_backend *,acvr_frame *){}
acvr_result ACVR_CALL camera(acvr_backend *,const acvr_frame *,uint32_t id,acvr_game_camera *out){
    if(id!=0||!out)return ACVR_BAD_ARGUMENT;
    out->camera_id=0;out->raster_width=960;out->raster_height=540;
    out->focal_x_px=out->focal_y_px=480;out->centre_x_px=480;out->centre_y_px=270;
    out->viewport_px[2]=960;out->viewport_px[3]=540;
    out->near_scene=.05f;out->far_scene=100;
    for(unsigned i=0;i<4;++i)out->view_from_scene[5*i]=1;
    return ACVR_OK;
}
acvr_result ACVR_CALL raycast(acvr_backend *b,const acvr_frame *,const acvr_ray *in,acvr_hit *out){
    if(!b||!in||!out)return ACVR_BAD_ARGUMENT;
    for(unsigned i=0;i<3;++i){b->ray.origin[i]=in->origin_scene[i];b->ray.direction[i]=in->direction_scene[i];}
    const int hit=b->scene.intersect(b->ray);
    out->found=hit>=0?1u:0u;out->camera_id=hit>=0?0u:ACVR_NO_CAMERA;
    if(hit>=0) {
        const auto &target=b->scene.targets[static_cast<size_t>(hit)];
        out->screen_x=.5f+target.center[0]*.2f;out->screen_y=.5f-(target.center[1]-1.5f)*.2f;
        out->flags=ACVR_HIT_GUN_COORDS;
    }
    return ACVR_OK;
}
void draw_disc(float x,float y,float z,float radius,bool hit){
    glPushMatrix();glTranslatef(x,y,z);glColor3f(hit?1.f:.86f,hit?.73f:.26f,hit?.18f:.32f);
    glBegin(GL_TRIANGLE_FAN);glVertex3f(0,0,0);
    for(int i=0;i<=32;++i){const float a=float(i)*.19634954084936207f;
        glVertex3f(std::cos(a)*radius,std::sin(a)*radius,0);}
    glEnd();glPopMatrix();
}
acvr_result ACVR_CALL draw(acvr_backend *b,const acvr_frame *,const acvr_draw_info *d){
    if(!b||!d||d->view_count!=1||!d->views||!d->target.framebuffer)return ACVR_BAD_ARGUMENT;
    b->bind_fbo(0x8D40,static_cast<GLuint>(d->target.framebuffer));
    // Room colours are linear, just like the gun overlay's material output.
    if(d->target.colour_format==0x8c43)glEnable(0x8db9); // FRAMEBUFFER_SRGB
    else glDisable(0x8db9);
    glViewport(0,0,static_cast<GLsizei>(d->target.width),static_cast<GLsizei>(d->target.height));
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
    glClearColor(.07f,.10f,.17f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);glLoadMatrixf(d->views->projection_from_view);
    glMatrixMode(GL_MODELVIEW);glLoadMatrixf(d->views->view_from_scene);
    glBegin(GL_QUADS);
    glColor3f(.17f,.21f,.27f);glVertex3f(-3,0,1);glVertex3f(3,0,1);
    glVertex3f(3,0,-8);glVertex3f(-3,0,-8);
    glColor3f(.22f,.27f,.35f);glVertex3f(-3,0,-8);glVertex3f(3,0,-8);
    glVertex3f(3,3,-8);glVertex3f(-3,3,-8);
    glEnd();
    for(const auto &target:b->scene.targets)draw_disc(target.center[0],target.center[1],target.center[2],target.radius,target.lit);
    const auto &ray=b->ray;
    glLineWidth(2);glColor3f(.36f,.84f,1);glBegin(GL_LINES);
    glVertex3fv(ray.origin.data());glVertex3f(ray.origin[0]+ray.direction[0]*5,
        ray.origin[1]+ray.direction[1]*5,ray.origin[2]+ray.direction[2]*5);glEnd();
    if(b->calibration)room::draw_grip_status(*b->calibration,b->hand,d->target.width,d->target.height);
    return ACVR_OK;
}
acvr_result ACVR_CALL outputs(acvr_backend *,acvr_outputs *out){if(!out)return ACVR_BAD_ARGUMENT;out->count=0;return ACVR_OK;}
acvr_backend_api backend_api(){
    auto api=record<acvr_backend_api>();api.abi_version=ACVR_ABI_VERSION;
    api.supported_graphics=ACVR_GRAPHICS_BIT(ACVR_GRAPHICS_GL);
    api.game_open=open;api.game_close=close;api.game_pause=pause;api.game_set_inputs=inputs;
    api.game_step=step;api.game_release_frame=release;api.game_camera=camera;
    api.game_draw_eye=draw;api.game_poll_outputs=outputs;api.game_raycast=raycast;return api;
}
struct Options {
    std::filesystem::path root,angle_file,preview,capture_dir;
    std::string model="arc-pistol-slide";
    std::string capture_name;
    std::string app_name;
    float right=0,left=0;
    uint32_t hand=ACVR_HAND_RIGHT;
    int seconds=0;
    bool xr=false,have_right=false,have_left=false,replace_preview=false,update_angles=false;
    bool save_angles=false,control_stdin=false;
    bool calibrate_grip=false,fixed_poses=false;
};
bool filename(const std::filesystem::path &p,const char *extension){
    if(p.empty()||p.has_parent_path()||p.extension()!=extension)return false;
    const auto name=p.string();
    if(name.front()=='.'||name.size()>80)return false;
    for(char c:name)if(!(c>='a'&&c<='z')&&!(c>='A'&&c<='Z')&&
                        !(c>='0'&&c<='9')&&c!='-'&&c!='_'&&c!='.')return false;
    return true;
}
bool angle_value(const char *text,float &out){
    errno=0;char *end=nullptr;const float value=std::strtof(text,&end);
    if(errno==ERANGE||end==text||*end||!std::isfinite(value)||std::abs(value)>180)return false;
    out=value;return true;
}
bool no_reparse_components(std::filesystem::path path){
    for(;;){
        const DWORD attributes=GetFileAttributesW(path.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        const auto parent=path.parent_path();
        if(parent==path||parent.empty())return true;
        path=parent;
    }
}
bool write_private_atomic(const std::filesystem::path &path,const std::string &body,bool replace){
    if(!no_reparse_components(path))return false;
    if(!replace&&std::filesystem::exists(path))return false;
    std::filesystem::path temporary;
    HANDLE file=INVALID_HANDLE_VALUE;
    for(unsigned attempt=0;attempt<32&&file==INVALID_HANDLE_VALUE;++attempt){
        temporary=path;
        temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+
            std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(attempt);
        file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE&&GetLastError()!=ERROR_FILE_EXISTS)return false;
    }
    if(file==INVALID_HANDLE_VALUE)return false;
    bool okay=true;
    size_t offset=0;
    while(offset<body.size()){
        DWORD written=0;
        const auto amount=static_cast<DWORD>(std::min<size_t>(body.size()-offset,0x7fffffff));
        if(!WriteFile(file,body.data()+offset,amount,&written,nullptr)||written==0){okay=false;break;}
        offset+=written;
    }
    if(okay)okay=FlushFileBuffers(file)!=0;
    CloseHandle(file);
    if(okay&&no_reparse_components(path)){
        const DWORD flags=MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0);
        okay=MoveFileExW(temporary.c_str(),path.c_str(),flags)!=0;
    }else okay=false;
    if(!okay)DeleteFileW(temporary.c_str());
    return okay;
}
bool parse(int argc,char **argv,Options &o){
    for(int i=1;i<argc;++i){
        const std::string key=argv[i];
        if(key=="--xr")o.xr=true;
        else if(key=="--fixed-poses")o.fixed_poses=true;
        else if(key=="--calibrate-grip")o.calibrate_grip=true;
        else if(key=="--control-stdin")o.control_stdin=true;
        else if(key=="--replace-preview")o.replace_preview=true;
        else if(key=="--update-angle-file")o.update_angles=true;
        else if(i+1>=argc)return false;
        else if(key=="--root")o.root=argv[++i];
        else if(key=="--model")o.model=argv[++i];
        else if(key=="--hand"){
            const std::string hand=argv[++i];
            if(hand!="right"&&hand!="left")return false;
            o.hand=hand=="right"?ACVR_HAND_RIGHT:ACVR_HAND_LEFT;
        }
        else if(key=="--angle-file")o.angle_file=argv[++i];
        else if(key=="--preview-svg")o.preview=argv[++i];
        else if(key=="--capture-eyes"){o.capture_name=argv[++i];if(o.capture_name.empty())return false;}
        else if(key=="--seconds"){
            const char *value=argv[++i];errno=0;char *end=nullptr;
            const long seconds=std::strtol(value,&end,10);
            if(errno==ERANGE||end==value||*end||seconds<5||seconds>60)return false;
            o.seconds=static_cast<int>(seconds);
        }
        else if(key=="--right-angle-deg"){if(!angle_value(argv[++i],o.right))return false;o.have_right=true;}
        else if(key=="--left-angle-deg"){if(!angle_value(argv[++i],o.left))return false;o.have_left=true;}
        else return false;
    }
    if(!o.xr&&o.preview.empty())return false;
    if(o.control_stdin&&!o.xr)return false;
    if(o.fixed_poses&&(!o.xr||o.capture_name.empty()||o.calibrate_grip))return false;
    if(o.calibrate_grip&&(!o.xr||!o.preview.empty()||!o.capture_name.empty()||
                          !o.angle_file.empty()||o.have_right||o.have_left||o.update_angles))return false;
    if(o.seconds&&!o.xr)return false;
    if(!o.capture_name.empty()&&(!o.xr||!room::capture_name_ok(o.capture_name)))return false;
    if(o.root.empty()||!o.root.is_absolute()||!std::filesystem::is_directory(o.root))return false;
    if(!no_reparse_components(o.root))return false;
    for(char c:o.model)if(!(c>='a'&&c<='z')&&!(c>='0'&&c<='9')&&c!='-')return false;
    if(o.model.empty()||o.model.size()>64)return false;
    if(!o.preview.empty()&&!filename(o.preview,".svg"))return false;
    if(!o.angle_file.empty()&&!filename(o.angle_file,".txt"))return false;
    if((o.have_left||o.have_right)&&o.angle_file.empty())return false;
    if(o.update_angles&&o.angle_file.empty())return false;
    const auto model=o.root/"assets"/"guns"/(o.model+".glb");
    const auto metadata=o.root/"data"/"guns"/(o.model+".toml");
    if(!std::filesystem::is_regular_file(model)||!std::filesystem::is_regular_file(metadata))return false;
    try {
        std::ifstream in(o.root/"data"/"product.json");
        const auto product=nlohmann::json::parse(in);
        if(product.at("schema")!=1||!product.at("display_name").is_string())return false;
        o.app_name=product.at("display_name").get<std::string>()+" Gun Room";
        if(o.app_name.empty()||o.app_name.size()>=128)return false; // OpenXR applicationName byte limit
    } catch (...) {return false;}
    const auto output_dir=o.root/".local"/"gun-room";
    if(!no_reparse_components(output_dir))return false;
    if(std::filesystem::exists(output_dir)&&!std::filesystem::is_directory(output_dir))return false;
    if(!o.capture_name.empty()&&(std::filesystem::exists(output_dir/o.capture_name)||
                                 !no_reparse_components(output_dir/o.capture_name)))return false;
    if(!o.preview.empty())o.preview=output_dir/o.preview;
    if(!o.angle_file.empty())o.angle_file=output_dir/o.angle_file;
    if(!o.preview.empty()&&!no_reparse_components(o.preview))return false;
    if(!o.angle_file.empty()&&!no_reparse_components(o.angle_file))return false;
    if(!o.preview.empty()&&std::filesystem::exists(o.preview)&&
       (!std::filesystem::is_regular_file(o.preview)||!o.replace_preview))return false;
    if(!o.angle_file.empty()){
        const bool existed=std::filesystem::exists(o.angle_file);
        if(existed&&!std::filesystem::is_regular_file(o.angle_file))return false;
        if(existed){
            std::ifstream in(o.angle_file);std::string hand,number;
            while(in>>hand){
                if(!(in>>number))return false;
                float degrees=0;if(!angle_value(number.c_str(),degrees))return false;
                if(hand=="right"&&!o.have_right)o.right=degrees;
                else if(hand=="left"&&!o.have_left)o.left=degrees;
                else if(hand!="right"&&hand!="left")return false;
            }
        }
        if(o.have_right||o.have_left){if(existed&&!o.update_angles)return false;o.save_angles=true;}
    }
    return true;
}
}
int main(int argc,char **argv){
    Options o;
    bool valid=false;try {valid=parse(argc,argv,o);}catch(...) {valid=false;}
    if(!valid){
        std::cerr<<"Usage: acvr_gun_room --root ABS_PATH [--xr] [--preview-svg NAME.svg] "
                 "[--replace-preview] [--model ID] [--hand right|left] [--angle-file NAME.txt] "
                 "[--right-angle-deg N] [--left-angle-deg N] [--update-angle-file] "
                 "[--capture-eyes FRESH_NAME (requires --xr)] [--fixed-poses (capture only)] [--seconds 5..60] "
                 "[--control-stdin (requires --xr and a pipe on stdin)] "
                 "[--calibrate-grip (requires --xr; no capture or legacy angle options)]\n";return 2;
    }
    room::GripAngles persisted{};
    const bool use_persisted=o.xr&&o.angle_file.empty()&&!o.have_right&&!o.have_left;
    if(use_persisted){
        const auto load=room::load_grip(o.root,o.model,o.hand,persisted);
        if(load==room::GripLoad::Error){std::cerr<<"Invalid private grip calibration\n";return 2;}
    }
    room::ControlStdin control;
    if(o.control_stdin&&!control.attach(GetStdHandle(STD_INPUT_HANDLE))){
        std::cerr<<"--control-stdin requires the launcher pipe on stdin\n";return 2;
    }
    if(!o.preview.empty()||o.save_angles){
        try {std::filesystem::create_directories(o.root/".local"/"gun-room");}
        catch (...) {return 3;}
    }
    if(!o.preview.empty()&&!write_private_atomic(o.preview,room::preview_svg(room::Scene{}),o.replace_preview))return 3;
    if(o.save_angles){
        std::ostringstream out;out<<"right "<<o.right<<"\nleft "<<o.left<<"\n";
        if(!write_private_atomic(o.angle_file,out.str(),o.update_angles))return 3;
    }
    if(!o.xr)return 0;
    room::GripCalibration calibration(persisted);
    current_calibration=o.calibrate_grip?&calibration:nullptr;
    current_hand=o.hand;
    if(!o.capture_name.empty()&&!room::prepare_capture_directory(o.root,o.capture_name,o.capture_dir)){
        std::cerr<<"Capture directory must be fresh and private\n";return 3;
    }
    if(!SetEnvironmentVariableW(L"ACVR_GUN_ROOM_CAPTURE_DIR",
                                o.capture_name.empty()?nullptr:o.capture_dir.c_str()))return 3;
    if(!SetEnvironmentVariableW(L"ACVR_GUN_ROOM_FIXED_POSES",o.fixed_poses?L"1":nullptr))return 3;
    const auto error_receipt=[&](const char *reason){
        if(o.capture_name.empty()||std::filesystem::exists(o.capture_dir/"receipt.json"))return;
        const auto body="{\"schema\":1,\"status\":\"error\",\"reason\":\""+
            std::string(reason)+"\",\"model\":\""+o.model+
            "\",\"actual_openxr_eye_readback\":false}\n";
        if(!write_private_atomic(o.capture_dir/"receipt.json",body,false))
            std::cerr<<"Could not write capture error receipt\n";
    };
    const auto model=o.root/"assets"/"guns"/(o.model+".glb");
    const auto metadata=o.root/"data"/"guns"/(o.model+".toml");
    if(!std::filesystem::is_regular_file(model)||!std::filesystem::is_regular_file(metadata))return 2;
    const auto model_path=model.string(),meta_path=metadata.string(),root_path=o.root.string();
    auto slot=record<acvr_gun_slot_config>();slot.slot=slot.player=0;slot.hand=o.hand;
    slot.model_id_utf8=o.model.c_str();slot.model_path_utf8=model_path.c_str();slot.metadata_path_utf8=meta_path.c_str();
    slot.grip_node_utf8="grip";slot.muzzle_node_utf8="muzzle";
    slot.show_gun=1;slot.laser_mode=ACVR_LASER_OFF; // The room draws the aim ray itself.
    const auto angle=use_persisted?room::grip_quaternion(persisted):
                      room::yaw_angle(o.hand==ACVR_HAND_RIGHT?o.right:o.left);
    for(unsigned i=0;i<4;++i){slot.angle_xyzw[i]=angle[i];slot.body_rgba[i]=i==3?1.f:.8f;slot.accent_rgba[i]=i==3?1.f:.35f;}
    auto config=record<acvr_runtime_config>();config.graphics_api=ACVR_GRAPHICS_GL;
    config.anchor_mode=ACVR_ANCHOR_RAIL;config.fallback_aim_distance_m=5;
    config.max_catchup_ticks=4;config.game_id_utf8=o.app_name.c_str();
    config.content_root_utf8=root_path.c_str();config.gun_slot_count=1;
    config.gun_slot_stride=sizeof(slot);config.gun_slots=&slot;
    config.gun_policy=record<acvr_gun_policy>();config.gun_policy.p1_hand=o.hand;
    config.gun_policy.two_guns=ACVR_TWO_GUNS_OFF;
    auto api=backend_api();acvr_runtime *runtime=nullptr;
    if(o.control_stdin){
        const auto request=control.poll();
        if(request==room::ControlPoll::Close)return 0;
        if(request==room::ControlPoll::Error){
            std::cerr<<"Malformed or failed stdin control command\n";
            error_receipt("control_stdin");return 9;
        }
    }
    const auto created=acvr_runtime_create(&config,&api,&runtime);
    if(created!=ACVR_OK){std::cerr<<"OpenXR room create failed: "<<created<<"\n";
        error_receipt("runtime_create");return 4;}
    const auto deadline=std::chrono::steady_clock::now()+
        std::chrono::seconds(o.seconds?o.seconds:15);
    const auto receipt_status=[&](){return o.capture_name.empty()?room::CaptureStatus::Missing:
        room::capture_receipt_status(o.capture_dir,o.model);};
    int code=0;
    bool cooperative_close=false,control_error=false;
    const auto now_ns=[](){return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();};
    const auto set_angle=[&](room::GripAngles value){
        const auto q=room::grip_quaternion(value);
        auto update=record<acvr_gun_angle_update>();update.slot=0;
        std::copy(q.begin(),q.end(),update.angle_xyzw);
        return acvr_runtime_set_gun_angle(runtime,&update)==ACVR_OK;
    };
    const auto apply_event=[&](room::GripEvent event){
        if(event==room::GripEvent::None)return true;
        if(event==room::GripEvent::Save){
            if(room::save_grip(o.root,o.model,o.hand,calibration.preview())){calibration.saved();return true;}
            calibration.save_failed();return set_angle(calibration.committed());
        }
        return set_angle(calibration.preview());
    };
    for(;;){
        if(o.control_stdin){
            const auto request=control.poll();
            if(request==room::ControlPoll::Close){cooperative_close=true;break;}
            if(request==room::ControlPoll::Error){
                std::cerr<<"Malformed or failed stdin control command\n";
                control_error=true;code=9;break;
            }
        }
        if((o.seconds||!o.capture_name.empty())&&std::chrono::steady_clock::now()>=deadline){
            std::cerr<<"OpenXR room deadline elapsed\n";code=8;break;
        }
        const auto result=acvr_runtime_tick(runtime);
        if(result==ACVR_STOPPED)break;
        if(result==ACVR_MORE){
            if(o.calibrate_grip&&!apply_event(calibration.no_sample(now_ns()))){code=5;break;}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;
        }
        if(result!=ACVR_OK){std::cerr<<"OpenXR room tick failed: "<<result<<"\n";code=5;break;}
        if(o.calibrate_grip){
            auto controls=record<acvr_controller_sample>();
            auto tracking=record<acvr_tracking>();
            tracking.head=record<acvr_pose>();
            tracking.right=record<acvr_hand_tracking>();tracking.left=record<acvr_hand_tracking>();
            tracking.right.grip=tracking.right.aim=record<acvr_pose>();
            tracking.left.grip=tracking.left.aim=record<acvr_pose>();
            const auto control_result=acvr_runtime_get_controller_sample(runtime,&controls);
            const auto tracking_result=acvr_runtime_get_tracking(runtime,&tracking);
            room::GripEvent event=room::GripEvent::None;
            room::GripSample sample{};
            if(control_result==ACVR_OK&&tracking_result==ACVR_OK&&
               room::selected_grip_sample(controls,tracking,o.hand,now_ns(),sample))
                event=calibration.sample(sample);
            else event=calibration.unavailable();
            if(!apply_event(event)){std::cerr<<"Grip calibration angle update failed\n";code=5;break;}
        }
        const auto receipt=receipt_status();
        if(receipt==room::CaptureStatus::Success){
            std::cout<<"Actual OpenXR eye capture: "<<o.capture_dir.string()<<"\n";break;
        }
        if(receipt==room::CaptureStatus::Error||receipt==room::CaptureStatus::Invalid){
            std::cerr<<"OpenXR eye capture receipt reports failure\n";code=5;break;
        }
    }
    if(o.calibrate_grip)apply_event(calibration.cancel());
    if(acvr_runtime_destroy(runtime)!=ACVR_OK)code=6;
    if(code!=0)error_receipt(control_error?"control_stdin":(code==8?"deadline":
                                      (code==6?"runtime_destroy":"runtime_tick")));
    if(code==0&&!cooperative_close&&!o.capture_name.empty()&&
       receipt_status()!=room::CaptureStatus::Success){
        std::cerr<<"OpenXR stopped without a completed eye capture\n";code=7;
        error_receipt("stopped_without_capture");
    }
    return code;
}
