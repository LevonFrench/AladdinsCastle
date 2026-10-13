// SPDX-License-Identifier: GPL-3.0-only
// Thin live SteamVR action reader; no game data, simulated motion or renderer.
#include <openvr.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <thread>
#include <string>

static void check(vr::EVRInputError error, const char* op) {
    if (error != vr::VRInputError_None)
        throw std::runtime_error(std::string(op)+": "+std::to_string(error));
}
static void matrix(const vr::HmdMatrix34_t& m) {
    std::cout << '[';
    for (int r=0;r<3;++r) for (int c=0;c<4;++c)
        std::cout << (r||c ? "," : "") << m.m[r][c];
    std::cout << ']';
}
static void vector(const vr::HmdVector3_t& v) {
    std::cout << '[' << v.v[0] << ',' << v.v[1] << ',' << v.v[2] << ']';
}
int main(int argc,char** argv) {
    std::cout << std::setprecision(12);
    if(argc<2) { std::cerr << "pose_feed ACTION_MANIFEST [seconds]\n"; return 2; }
    vr::EVRInitError init{};
    auto* system=vr::VR_Init(&init,vr::VRApplication_Overlay);
    if(init!=vr::VRInitError_None) {
        std::cerr << vr::VR_GetVRInitErrorAsEnglishDescription(init) << '\n'; return 3;
    }
    int result=0;
    try {
        auto* input=vr::VRInput();
        if(!input) throw std::runtime_error("SteamVR input unavailable");
        for (uint32_t device=0;device<vr::k_unMaxTrackedDeviceCount;++device) {
            if(system->GetTrackedDeviceClass(device)!=vr::TrackedDeviceClass_Controller) continue;
            char type[256]{};
            system->GetStringTrackedDeviceProperty(device,vr::Prop_ControllerType_String,type,sizeof(type));
            std::cerr << "controller " << device << " role=" << system->GetControllerRoleForTrackedDeviceIndex(device)
                      << " type=" << type << " connected=" << system->IsTrackedDeviceConnected(device) << '\n';
        }
        check(input->SetActionManifestPath(std::filesystem::absolute(argv[1]).string().c_str()),"manifest");
        vr::VRActionSetHandle_t set;
        check(input->GetActionSetHandle("/actions/wii",&set),"action set");
        const char* names[]={"pose_left","pose_right","stick_left","stick_right",
            "trigger_left","trigger_right","grip_left","grip_right","a_left","a_right",
            "b_left","b_right","menu_left","menu_right","click_left","click_right"};
        vr::VRActionHandle_t actions[16]{};
        for(int i=0;i<16;++i) check(input->GetActionHandle((std::string("/actions/wii/in/")+names[i]).c_str(),&actions[i]),names[i]);
        const auto start=std::chrono::steady_clock::now();
        const double seconds=argc>2?std::stod(argv[2]):0;
        auto next=start;
        while(true) {
            const auto now=std::chrono::steady_clock::now();
            if(seconds>0 && std::chrono::duration<double>(now-start).count()>=seconds) break;
            vr::VREvent_t event{};
            bool quit=false;
            while(system->PollNextEvent(&event,sizeof(event))) {
                if(event.eventType==vr::VREvent_Quit) quit=true;
                if(event.eventType>=1701 && event.eventType<=1708)
                    std::cerr << "input event " << event.eventType << '\n';
            }
            if(quit) break;
            vr::VRActiveActionSet_t active{}; active.ulActionSet=set;
            check(input->UpdateActionState(&active,sizeof(active),1),"update actions");
            vr::TrackedDevicePose_t head{};
            system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding,0,&head,1);
            std::cout << "{\"time\":" << std::chrono::duration<double>(now-start).count()
                      << ",\"head_valid\":" << (head.bPoseIsValid?"true":"false") << ",\"head\":";
            matrix(head.mDeviceToAbsoluteTracking);
            std::cout << ",\"hands\":[";
            for(int hand=0;hand<2;++hand) {
                vr::InputPoseActionData_t pose{};
                auto error=input->GetPoseActionDataRelativeToNow(actions[hand],vr::TrackingUniverseStanding,0,&pose,sizeof(pose),0);
                if(now-start<std::chrono::milliseconds(10) ||
                    (now-start>std::chrono::seconds(1) && now-start<std::chrono::milliseconds(1010))) {
                    vr::InputBindingInfo_t bindings[8]{}; uint32_t count=0;
                    auto be=input->GetActionBindingInfo(actions[hand],bindings,sizeof(bindings[0]),8,&count);
                    std::cerr << "hand " << hand << " pose error=" << error << " active=" << pose.bActive
                              << " binding error=" << be << " count=" << count << '\n';
                    for(uint32_t b=0;b<count && b<8;++b) std::cerr << bindings[b].rchDevicePathName << ' ' << bindings[b].rchInputPathName << '\n';
                }
                vr::InputAnalogActionData_t stick{};
                input->GetAnalogActionData(actions[2+hand],&stick,sizeof(stick),0);
                std::cout << (hand?",":"") << "{\"valid\":"
                    << (error==0 && pose.bActive && pose.pose.bPoseIsValid && pose.pose.bDeviceIsConnected?"true":"false") << ",\"pose\":";
                matrix(pose.pose.mDeviceToAbsoluteTracking);
                std::cout << ",\"velocity\":"; vector(pose.pose.vVelocity);
                std::cout << ",\"omega\":"; vector(pose.pose.vAngularVelocity);
                std::cout << ",\"stick\":[" << (stick.bActive?stick.x:0) << ',' << (stick.bActive?stick.y:0) << "],\"buttons\":[";
                for(int b=0;b<6;++b) {
                    vr::InputDigitalActionData_t data{};
                    const auto e=input->GetDigitalActionData(actions[4+b*2+hand],&data,sizeof(data),0);
                    std::cout << (b?",":"") << (e==0 && data.bActive && data.bState?"true":"false");
                }
                std::cout << "]}";
            }
            std::cout << "]}" << std::endl;
            next+=std::chrono::microseconds(8333);
            std::this_thread::sleep_until(next);
            if(next < std::chrono::steady_clock::now()-std::chrono::milliseconds(50)) next=std::chrono::steady_clock::now();
        }
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; result=4; }
    vr::VR_Shutdown();
    return result;
}
