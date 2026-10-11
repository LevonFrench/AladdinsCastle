// SPDX-License-Identifier: MIT
#include "grip_calibration.hpp"
#include <algorithm>
#include <cmath>

namespace room {
namespace {
constexpr int64_t enter_hold=800000000,save_hold=1200000000,stale_gap=200000000;
constexpr float pi=3.14159265358979323846f;
float axis(float value){return std::abs(value)<.25f?0.f:value;}
}
bool angles_ok(GripAngles value){return std::isfinite(value.yaw_deg)&&std::isfinite(value.pitch_deg)&&
    std::abs(value.yaw_deg)<=60.f&&std::abs(value.pitch_deg)<=45.f;}
std::array<float,4> grip_quaternion(GripAngles value){
    if(!angles_ok(value))return {0,0,0,0};
    const float y=value.yaw_deg*pi/360.f,p=value.pitch_deg*pi/360.f;
    const float sy=std::sin(y),cy=std::cos(y),sp=std::sin(p),cp=std::cos(p);
    return {cy*sp,sy*cp,-sy*sp,cy*cp}; // q_yaw * q_pitch
}
GripPanelLayout grip_panel_layout(uint32_t eye_width,uint32_t eye_height){
    if(eye_width<320||eye_height<240)return {};
    // Occupy at most 78% of horizontal field and 18% of vertical field.
    // Placing the panel at 60% eye height keeps it central and below the aim.
    constexpr float base_width=520.f,base_height=65.f;
    const float scale=std::min(.78f*static_cast<float>(eye_width)/base_width,
                               .18f*static_cast<float>(eye_height)/base_height);
    if(scale<.8f)return {};
    const float width=base_width*scale,height=base_height*scale;
    return {(static_cast<float>(eye_width)-width)*.5f,
            static_cast<float>(eye_height)*.60f,scale,width,height};
}
GripCalibration::GripCalibration(GripAngles initial):committed_(initial),draft_(initial){
    if(!angles_ok(initial)){committed_={};draft_={};}
}
GripEvent GripCalibration::restore(GripFeedback reason){
    const bool changed=editing_&&(draft_.yaw_deg!=committed_.yaw_deg||draft_.pitch_deg!=committed_.pitch_deg);
    draft_=committed_;editing_=false;feedback_=reason;
    down_=false;long_hold_=false;require_release_=true;down_since_=0;
    return changed?GripEvent::Restore:GripEvent::None;
}
GripEvent GripCalibration::cancel(){return restore(GripFeedback::Cancelled);}
GripEvent GripCalibration::unavailable(){return restore(GripFeedback::Unavailable);}
GripEvent GripCalibration::no_sample(int64_t now){
    if(seen_&&(now<last_wall_||now-last_wall_>stale_gap))return restore(GripFeedback::Unavailable);
    return GripEvent::None;
}
GripEvent GripCalibration::sample(const GripSample &s){
    if(!s.display_id||s.sample_time_ns<=0||s.wall_time_ns<=0||
       (seen_&&(s.display_id<=last_display_||s.sample_time_ns<=last_sample_)))
        return unavailable();
    if(seen_&&(s.wall_time_ns<=last_wall_||s.wall_time_ns-last_wall_>stale_gap||
               s.sample_time_ns-last_sample_>stale_gap)) {
        last_display_=s.display_id;last_sample_=s.sample_time_ns;last_wall_=s.wall_time_ns;
        return restore(GripFeedback::Unavailable);
    }
    const int64_t elapsed=seen_?s.wall_time_ns-last_wall_:0;
    seen_=true;last_display_=s.display_id;last_sample_=s.sample_time_ns;last_wall_=s.wall_time_ns;
    if(!s.focused||!s.tracked||!s.actions_active||!s.trigger_up||
       !std::isfinite(s.stick_x)||!std::isfinite(s.stick_y)||
       std::abs(s.stick_x)>1.f||std::abs(s.stick_y)>1.f)
        return restore(GripFeedback::Unavailable);
    if(require_release_){
        if(!s.secondary_down)require_release_=false;
        return GripEvent::None;
    }
    if(s.secondary_down){
        if(axis(s.stick_x)!=0||axis(s.stick_y)!=0){
            if(editing_)return restore(GripFeedback::Cancelled);
            down_=false;long_hold_=false;require_release_=true;return GripEvent::None;
        }
        if(!down_){down_=true;down_since_=s.wall_time_ns;long_hold_=false;}
        if(s.wall_time_ns-down_since_ >= (editing_?save_hold:enter_hold))long_hold_=true;
        return GripEvent::None;
    }
    if(down_){
        down_=false;
        if(!editing_){
            if(long_hold_&&axis(s.stick_x)==0&&axis(s.stick_y)==0){
                editing_=true;draft_=committed_;feedback_=GripFeedback::Editing;
            }
            long_hold_=false;return GripEvent::None;
        }
        if(long_hold_){
            long_hold_=false;
            if(axis(s.stick_x)==0&&axis(s.stick_y)==0&&
               std::abs(draft_.yaw_deg-committed_.yaw_deg)+
               std::abs(draft_.pitch_deg-committed_.pitch_deg)>=.5f){
                require_release_=true;return GripEvent::Save;
            }
            return GripEvent::None;
        }
        return restore(GripFeedback::Cancelled);
    }
    if(!editing_)return GripEvent::None;
    const float dt=static_cast<float>(std::min<int64_t>(elapsed,50000000))/1e9f;
    const GripAngles before=draft_;
    draft_.yaw_deg=std::clamp(draft_.yaw_deg-axis(s.stick_x)*30.f*dt,-60.f,60.f);
    draft_.pitch_deg=std::clamp(draft_.pitch_deg+axis(s.stick_y)*30.f*dt,-45.f,45.f);
    return before.yaw_deg!=draft_.yaw_deg||before.pitch_deg!=draft_.pitch_deg?
        GripEvent::Preview:GripEvent::None;
}
void GripCalibration::saved(){committed_=draft_;editing_=false;feedback_=GripFeedback::Saved;require_release_=true;}
void GripCalibration::save_failed(){restore(GripFeedback::SaveFailed);}
}
