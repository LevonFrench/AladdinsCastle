// SPDX-License-Identifier: MIT
#define NOMINMAX
#include <windows.h>
#include "grip_calibration.hpp"
#include "grip_calibration_file.hpp"
#include "grip_calibration_adapter.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
unsigned checks=0;
void check(bool value){if(!value)throw std::runtime_error("grip check "+std::to_string(++checks));++checks;}
struct Feed {
    room::GripCalibration calibration;
    room::GripSample sample{};
    Feed(){sample.display_id=0;sample.sample_time_ns=1000000000;sample.wall_time_ns=1000000000;
        sample.focused=sample.tracked=sample.actions_active=sample.trigger_up=true;}
    room::GripEvent next(bool secondary=false,float x=0,float y=0){
        ++sample.display_id;sample.sample_time_ns+=50000000;sample.wall_time_ns+=50000000;
        sample.secondary_down=secondary;sample.stick_x=x;sample.stick_y=y;
        return calibration.sample(sample);
    }
    void enter(){
        next();for(int i=0;i<18;++i)next(true);
        check(next()==room::GripEvent::None);check(calibration.editing());
    }
};
void state_tests(){
    using room::GripEvent;using room::GripFeedback;
    Feed f;
    for(int i=0;i<18;++i)f.next(true); // held before first sample cannot arm
    check(!f.calibration.editing());f.next();f.enter();
    for(int i=0;i<10;++i)check(f.next(false,1,1)==GripEvent::Preview);
    check(std::abs(f.calibration.preview().yaw_deg+15)<.01f);
    check(std::abs(f.calibration.preview().pitch_deg-15)<.01f);
    const auto q=room::grip_quaternion(f.calibration.preview());
    check(std::abs(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]-1)<.0001f);
    // q * -Z * q^-1: negative yaw points right, positive pitch points up.
    const float muzzle_x=-2*(q[0]*q[2]+q[3]*q[1]);
    const float muzzle_y=-2*(q[1]*q[2]-q[3]*q[0]);
    check(muzzle_x>0&&muzzle_y>0);
    for(int i=0;i<25;++i)f.next(true);
    check(f.next()==GripEvent::Save);f.calibration.saved();
    check(f.calibration.feedback()==GripFeedback::Saved&&!f.calibration.editing());
    f.next();f.enter();f.next(false,-1,0);
    check(f.next(true)==GripEvent::None); // begins save hold
    check(f.next(false)==GripEvent::Restore); // short tap cancels
    check(f.calibration.feedback()==GripFeedback::Cancelled);
    f.next();f.enter();f.next(false,1,0);
    f.sample.focused=false;
    check(f.next()==GripEvent::Restore&&!f.calibration.editing());
    f.sample.focused=true;f.next();f.enter();f.next(false,1,0);
    const auto repeated=f.sample;
    check(f.calibration.sample(repeated)==GripEvent::Restore);
    check(f.calibration.feedback()==GripFeedback::Unavailable);
    f.next();f.enter();f.next(false,1,0);
    check(f.calibration.no_sample(f.sample.wall_time_ns+250000000)==GripEvent::Restore);
    check(f.calibration.feedback()==GripFeedback::Unavailable);
    Feed g;g.next();g.enter();
    for(int i=0;i<25;++i)g.next(true);
    check(g.next()!=GripEvent::Save); // unchanged draft is not saved
    g.next(false,0,0); // release guard after hold
    g.next(true,1,0);check(!g.calibration.editing());
    for(int condition=0;condition<4;++condition){
        Feed loss;loss.next();loss.enter();loss.next(false,1,0);
        if(condition==0)loss.sample.tracked=false;
        if(condition==1)loss.sample.actions_active=false;
        if(condition==2)loss.sample.trigger_up=false;
        if(condition==3)loss.sample.stick_y=std::nanf("");
        const auto event=condition==3?loss.next(false,0,std::nanf("")):loss.next();
        check(event==GripEvent::Restore&&!loss.calibration.editing());
        check(loss.calibration.feedback()==GripFeedback::Unavailable);
    }
    Feed failed;failed.next();failed.enter();failed.next(false,1,0);
    for(int i=0;i<25;++i)failed.next(true);
    check(failed.next()==GripEvent::Save);
    failed.calibration.save_failed();
    check(failed.calibration.feedback()==GripFeedback::SaveFailed&&
          failed.calibration.preview().yaw_deg==failed.calibration.committed().yaw_deg);
    check(!room::angles_ok({61,0})&&!room::angles_ok({0,-46}));
}
void selected_hand_tests(){
    acvr_controller_sample controls{};ACVR_INIT(&controls);
    acvr_tracking tracking{};ACVR_INIT(&tracking);
    controls.display_id=tracking.display_id=7;
    controls.sample_time_ns=tracking.sample_time_ns=700;
    controls.focused=1;
    controls.active[ACVR_HAND_RIGHT]=15;
    controls.secondary_down[ACVR_HAND_RIGHT]=1;
    controls.stick_x[ACVR_HAND_RIGHT]=.7f;
    controls.trigger[ACVR_HAND_LEFT]=.8f;
    tracking.right.grip_flags=tracking.right.aim_flags=3;
    room::GripSample sample{};
    check(room::selected_grip_sample(controls,tracking,ACVR_HAND_RIGHT,900,sample));
    check(sample.tracked&&sample.actions_active&&sample.secondary_down&&sample.trigger_up&&sample.stick_x==.7f);
    check(room::selected_grip_sample(controls,tracking,ACVR_HAND_LEFT,900,sample));
    check(!sample.tracked&&!sample.actions_active&&!sample.secondary_down&&!sample.trigger_up&&sample.stick_x==0);
    tracking.display_id=8;
    check(!room::selected_grip_sample(controls,tracking,ACVR_HAND_RIGHT,900,sample));
    tracking.display_id=7;tracking.sample_time_ns=701;
    check(!room::selected_grip_sample(controls,tracking,ACVR_HAND_RIGHT,900,sample));
    check(!room::selected_grip_sample(controls,tracking,2,900,sample));
}
void panel_layout_tests(){
    const auto portrait=room::grip_panel_layout(2592,2688);
    const auto landscape=room::grip_panel_layout(960,540);
    for(const auto test:{std::pair<room::GripPanelLayout,std::pair<float,float>>{portrait,{2592.f,2688.f}},
                         {landscape,{960.f,540.f}}}){
        const auto &p=test.first;const float w=test.second.first,h=test.second.second;
        check(p.scale>0&&p.x>=.10f*w&&p.x+p.width<=.90f*w);
        check(p.y>=.55f*h&&p.y+p.height<=.80f*h);
        check(std::abs(p.width-520.f*p.scale)<.01f&&
              std::abs(p.height-65.f*p.scale)<.01f);
    }
    check(14.f*portrait.scale>=48.f); // actual reported eye target: >48 px glyph height
    check(14.f*landscape.scale>=18.f);
    check(room::grip_panel_layout(320,240).scale==0); // refuse unreadable tiny target
}
void file_tests(const std::filesystem::path &base){
    const auto root=base/("grip-fixture-"+std::to_string(GetCurrentProcessId())+"-"+
                          std::to_string(GetTickCount64()));
    std::filesystem::create_directories(root);
    room::GripAngles result{};
    check(room::load_grip(root,"fixture",0,result)==room::GripLoad::Missing);
    check(room::save_grip(root,"fixture",0,{-12.5f,8.25f}));
    check(room::load_grip(root,"fixture",0,result)==room::GripLoad::Loaded);
    check(std::abs(result.yaw_deg+12.5f)<.001f&&std::abs(result.pitch_deg-8.25f)<.001f);
    check(room::load_grip(root,"fixture",1,result)==room::GripLoad::Missing);
    check(room::load_grip(root,"other",0,result)==room::GripLoad::Missing);
    check(!room::save_grip(root,"../escape",0,{0,0}));
    check(!room::save_grip(root,"fixture",1,{0,46}));
    const auto path=room::grip_file(root,"fixture",0);
    HANDLE blocked_replace=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL,nullptr);
    check(blocked_replace!=INVALID_HANDLE_VALUE);
    check(!room::save_grip(root,"fixture",0,{2,3}));
    CloseHandle(blocked_replace);
    check(room::load_grip(root,"fixture",0,result)==room::GripLoad::Loaded&&
          result.yaw_deg==-12.5f&&result.pitch_deg==8.25f);
    auto lock=path;lock+=L".lock";
    HANDLE held=CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL,nullptr);
    check(held!=INVALID_HANDLE_VALUE);
    check(!room::save_grip(root,"fixture",0,{1,2}));
    CloseHandle(held);DeleteFileW(lock.c_str());
    check(room::load_grip(root,"fixture",0,result)==room::GripLoad::Loaded&&result.yaw_deg==-12.5f);
    {std::ofstream out(path,std::ios::trunc);out<<"{\"schema\":1,\"schema\":1,\"model_id\":\"fixture\",\"hand\":\"right\",\"yaw_deg\":0,\"pitch_deg\":0}";}
    check(room::load_grip(root,"fixture",0,result)==room::GripLoad::Error);
    {std::ofstream out(path,std::ios::trunc);out<<"{\"schema\":1,\"model_id\":\"other\",\"hand\":\"right\",\"yaw_deg\":0,\"pitch_deg\":0}";}
    check(room::load_grip(root,"fixture",0,result)==room::GripLoad::Error);
    {std::ofstream out(path,std::ios::trunc);out<<"{\"schema\":true,\"model_id\":\"fixture\",\"hand\":\"right\",\"yaw_deg\":0,\"pitch_deg\":0}";}
    check(room::load_grip(root,"fixture",0,result)==room::GripLoad::Error);
    check(room::save_grip(root,"fixture",0,{3,-4}));
    check(room::load_grip(root,"fixture",0,result)==room::GripLoad::Loaded&&result.pitch_deg==-4);
    const auto outside=root/"sentinel.json";
    {std::ofstream out(outside);out<<"untouched";}
    const auto alias=room::grip_file(root,"alias",0);
    try {
        std::filesystem::create_symlink(outside,alias);
        check(room::load_grip(root,"alias",0,result)==room::GripLoad::Error);
        check(!room::save_grip(root,"alias",0,{1,2}));
        std::ifstream verify(outside);std::string sentinel;verify>>sentinel;
        check(sentinel=="untouched");
    }catch(const std::filesystem::filesystem_error&){
        // Symlink creation can require developer mode; other refusal checks run everywhere.
    }
    try {
        const auto linked_root=root/"linked-root";
        std::filesystem::create_directory_symlink(root,linked_root);
        check(room::load_grip(linked_root,"fixture",0,result)==room::GripLoad::Error);
        check(!room::save_grip(linked_root,"fixture",0,{1,2}));
    }catch(const std::filesystem::filesystem_error&){
        // Directory symlink creation can require developer mode too.
    }
    const auto blocked=root/"blocked";std::filesystem::create_directory(blocked);
    {std::ofstream out(blocked/".local");out<<"file instead of directory";}
    check(room::load_grip(blocked,"fixture",0,result)==room::GripLoad::Error);
    check(!room::save_grip(blocked,"fixture",0,{1,2}));
    std::filesystem::remove_all(root);
}
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    try {state_tests();selected_hand_tests();panel_layout_tests();
         file_tests(std::filesystem::absolute(argv[1]));return 0;}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
