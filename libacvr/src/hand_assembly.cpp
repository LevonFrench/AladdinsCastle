// SPDX-License-Identifier: MIT
#include "hand_assembly.hpp"
#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace acvr {
namespace {
using Json=nlohmann::json;
void require(bool ok,const char *reason){if(!ok)throw std::runtime_error(reason);}
template<size_t N> std::array<float,N> numbers(const Json &value){
    require(value.is_array()&&value.size()==N,"invalid hand vector");
    std::array<float,N> out{};
    for(size_t i=0;i<N;++i){require(value[i].is_number(),"non-number hand vector");
        const auto n=value[i].get<double>();require(std::isfinite(n)&&std::abs(n)<1e5,"nonfinite hand vector");
        out[i]=static_cast<float>(n);}
    return out;
}
bool safe_asset(const std::string &path){
    if(path.rfind("assets/hands/",0)!=0||path.size()>160||path.size()<=13||path.find("..")!=std::string::npos)return false;
    for(char c:path)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='/'||c=='-'||c=='_'||c=='.'))return false;
    return path.size()>=4&&path.substr(path.size()-4)==".glb";
}
Matrix product(const Matrix &a,const Matrix &b){
    Matrix out{};
    for (unsigned c = 0; c < 4; ++c) {
        for (unsigned r = 0; r < 4; ++r) {
            for (unsigned k = 0; k < 4; ++k) {
                out[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
            }
        }
    }
    return out;
}
Matrix rotation(const std::array<float,4> &q){
    const double x=q[0],y=q[1],z=q[2],w=q[3];
    return {float(1-2*y*y-2*z*z),float(2*x*y+2*z*w),float(2*x*z-2*y*w),0,
        float(2*x*y-2*z*w),float(1-2*x*x-2*z*z),float(2*y*z+2*x*w),0,
        float(2*x*z+2*y*w),float(2*y*z-2*x*w),float(1-2*x*x-2*y*y),0,
        0,0,0,1};
}
}
bool hand_assembly_lists_model(std::string_view json,std::string_view model_id,
                               bool &listed,std::string &error) noexcept {
    try {
        require(!json.empty()&&json.size()<=1024*1024&&model_id.size()<=64,"hand assembly size out of bounds");
        const auto data=Json::parse(json.begin(),json.end(),
            [](int depth,Json::parse_event_t,Json &){require(depth<=64,"hand JSON nesting too deep");return true;});
        require(data.is_object()&&data.contains("models")&&data.at("models").is_object(),"hand assembly has no models");
        listed=data.at("models").contains(std::string(model_id));error.clear();return true;
    }catch(const std::exception &e){try{error=e.what();}catch(...){}return false;}
    catch(...){try{error="hand assembly decode failed";}catch(...){}return false;}
}
bool decode_hand_assembly(std::string_view json,std::string_view model_id,HandAssembly &out,std::string &error) noexcept {
    try {
        require(!json.empty()&&json.size()<=1024*1024&&model_id.size()<=64,"hand assembly size out of bounds");
        const auto data=Json::parse(json.begin(),json.end(),
            [](int depth,Json::parse_event_t,Json &){require(depth<=64,"hand JSON nesting too deep");return true;});
        require(data.at("schema")=="hand-assembly/1"&&data.at("units")=="metres"&&
                data.at("quaternion_order")=="xyzw"&&data.at("hand_root")=="grip","unsupported hand assembly contract");
        const auto &axes=data.at("axes");
        require(axes.at("up")=="+Y"&&axes.at("forward")=="-Z"&&axes.at("handedness")=="right","unsupported hand axes");
        const auto &model=data.at("models").at(std::string(model_id));
        HandAssembly decoded;
        for(unsigned i=0;i<2;++i){
            const auto &entry=model.at("hands").at(i==0?"right":"left");auto &hand=decoded.hands[i];
            hand.asset=entry.at("asset").get<std::string>();hand.node=entry.at("node").get<std::string>();
            require(safe_asset(hand.asset)&&(hand.node=="grip"||hand.node=="grip_two"),"invalid hand asset/mount");
            hand.translation=numbers<3>(entry.at("translation"));
            hand.rotation_xyzw=numbers<4>(entry.at("rotation_xyzw"));
            double norm=0;for(float v:hand.rotation_xyzw)norm+=double(v)*v;
            require(std::abs(norm-1)<.001,"nonunit hand rotation");
            if(entry.contains("finger_pose")){
                const auto &finger=entry.at("finger_pose");
                require(finger.is_object()&&finger.size()<=16,"invalid finger poses");
                for(auto it=finger.begin();it!=finger.end();++it){
                    HandMount::FingerPose pose;pose.node=it.key();
                    require(!pose.node.empty()&&pose.node.size()<=128,"invalid finger node");
                    const auto &detail=it.value();
                    if(detail.contains("translation_offset"))pose.translation_offset=numbers<3>(detail.at("translation_offset"));
                    if(detail.contains("rotation_xyzw"))pose.rotation_xyzw=numbers<4>(detail.at("rotation_xyzw"));
                    if(detail.contains("scale"))pose.scale=numbers<3>(detail.at("scale"));
                    double length=0;for(float v:pose.rotation_xyzw)length+=double(v)*v;
                    require(std::abs(length-1)<.001,"nonunit finger rotation");
                    for(float v:pose.scale)require(v>0&&v<=4,"invalid finger scale");
                    hand.finger_pose.push_back(std::move(pose));
                }
            }
        }
        const auto anchor_mount=[&](const Json &value,HandMount &mount){
            mount.node=value.at("node").get<std::string>();
            require(mount.node=="grip"||mount.node=="grip_two","invalid gun mount node");
            mount.translation=numbers<3>(value.at("translation"));
            mount.rotation_xyzw=numbers<4>(value.at("rotation_xyzw"));
            double norm=0;for(float v:mount.rotation_xyzw)norm+=double(v)*v;
            require(std::abs(norm-1)<.001,"nonunit gun mount rotation");
        };
        const auto &primary=model.at("primary_mount_by_hand");
        require(primary.is_object()&&primary.size()==2,"incomplete primary-by-hand mounts");
        anchor_mount(primary.at("right"),decoded.primary_mount_by_hand[0]);
        anchor_mount(primary.at("left"),decoded.primary_mount_by_hand[1]);
        if(model.contains("support_grip")){
            decoded.has_support_grip=true;
            anchor_mount(model.at("support_grip"),decoded.support_grip);
            decoded.support_by_primary.fill(decoded.support_grip);
        }
        if(model.contains("support_by_primary")){
            const auto &by_primary=model.at("support_by_primary");
            require(by_primary.is_object()&&by_primary.size()==2,"incomplete support-by-primary mounts");
            decoded.has_support_grip=true;
            anchor_mount(by_primary.at("right"),decoded.support_by_primary[0]);
            anchor_mount(by_primary.at("left"),decoded.support_by_primary[1]);
        }
        const auto &anchors=model.at("label_anchors");require(anchors.is_object()&&anchors.size()<=32,"invalid label anchors");
        for(auto it=anchors.begin();it!=anchors.end();++it){
            HandLabelAnchor label;label.semantic=it.key();
            require(!label.semantic.empty()&&label.semantic.size()<=64,"invalid label semantic");
            label.node=it.value().at("node").get<std::string>();
            require(!label.node.empty()&&label.node.size()<=128,"invalid label node");
            label.local_offset=numbers<3>(it.value().at("local_offset"));
            label.bind_position=numbers<3>(it.value().at("bind_position"));
            decoded.labels.push_back(std::move(label));
        }
        out=std::move(decoded);error.clear();return true;
    }catch(const std::exception &e){try{error=e.what();}catch(...){}return false;}
    catch(...){try{error="hand assembly decode failed";}catch(...){}return false;}
}
bool decode_hand_motions(std::string_view text,const GunAsset &asset,
                         std::string_view expected_id,std::string_view expected_hand,
                         std::vector<HandMotion> &out,std::string &error) noexcept {
    try {
        require(!text.empty()&&text.size()<=1024*1024,"hand metadata size out of bounds");
        const auto doc=toml::parse(text);
        require(doc["version"].value_or(std::string{})=="hand-assembly/1"&&
                doc["asset_kind"].value_or(std::string{})=="hand"&&
                doc["root"].value_or(std::string{})=="grip","invalid hand metadata");
        require(doc["id"].value_or(std::string{})==expected_id&&
                doc["hand"].value_or(std::string{})==expected_hand,"hand identity mismatch");
        const auto *table=doc["motion"].as_table();require(table&&table->size()<=16,"invalid hand motions");
        std::vector<HandMotion> decoded;
        for(const auto &[name,item]:*table){
            (void)name;const auto *entry=item.as_table();require(entry,"invalid motion entry");
            HandMotion m;m.node=(*entry)["node"].value_or(std::string{});
            m.drive=(*entry)["drive"].value_or(std::string{});
            require(m.drive=="trigger"||m.drive=="grip"||m.drive=="primary","invalid hand motion drive");
            require((*entry)["kind"].value_or(std::string{})=="rotate","invalid hand motion kind");
            const auto *axis=(*entry)["axis"].as_array();const auto *range=(*entry)["range"].as_array();
            require(axis&&axis->size()==3&&range&&range->size()==2,"invalid hand motion vector");
            for(unsigned i=0;i<3;++i){const auto value=(*axis)[i].value<double>();require(value&&std::isfinite(*value),"invalid hand axis");m.axis[i]=float(*value);}
            const auto lo=(*range)[0].value<double>(),hi=(*range)[1].value<double>();
            require(lo&&hi&&std::isfinite(*lo)&&std::isfinite(*hi)&&*lo<=*hi&&*lo>=-3.14&&*hi<=3.14,"invalid hand range");
            m.minimum=float(*lo);m.maximum=float(*hi);
            double norm=0;for(float v:m.axis)norm+=double(v)*v;
            require(std::abs(norm-1)<.001,"nonunit hand axis");
            require(std::any_of(asset.nodes.begin(),asset.nodes.end(),[&](const GunNode &n){return n.name==m.node;}),"missing hand motion node");
            decoded.push_back(std::move(m));
        }
        out=std::move(decoded);error.clear();return true;
    }catch(const std::exception &e){try{error=e.what();}catch(...){}return false;}
    catch(...){try{error="hand motion decode failed";}catch(...){}return false;}
}
namespace {
// sRGB hex "#rrggbb" to linear RGBA (alpha 1), by the standard sRGB transfer function.
std::array<float,4> linear_hex(const std::string &hex){
    require(hex.size()==7&&hex[0]=='#',"invalid hand tint colour");
    const auto digit=[](char c){
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='f')return c-'a'+10;
        if(c>='A'&&c<='F')return c-'A'+10;
        throw std::runtime_error("invalid hand tint colour");
    };
    std::array<float,4> out{0,0,0,1};
    for(unsigned i=0;i<3;++i){
        const double s=(digit(hex[1+2*i])*16+digit(hex[2+2*i]))/255.0;
        out[i]=float(s<=.04045?s/12.92:std::pow((s+.055)/1.055,2.4));
    }
    return out;
}
}
bool decode_hand_tint(std::string_view text,HandTint &out,std::string &error) noexcept {
    try {
        require(!text.empty()&&text.size()<=1024*1024,"hand metadata size out of bounds");
        const auto doc=toml::parse(text);
        const auto name=doc["tints"]["default"].value_or(std::string{});
        require(!name.empty()&&name.size()<=64,"hand metadata has no default tint");
        HandTint decoded;
        decoded.body=linear_hex(doc["tints"][name]["body"].value_or(std::string{}));
        decoded.accent=linear_hex(doc["tints"][name]["accent"].value_or(std::string{}));
        out=decoded;error.clear();return true;
    }catch(const std::exception &e){try{error=e.what();}catch(...){}return false;}
    catch(...){try{error="hand tint decode failed";}catch(...){}return false;}
}
bool validate_hand_pose_nodes(const HandMount &mount,const GunAsset &asset,
                              std::string &error) noexcept {
    try {
        for(const auto &pose:mount.finger_pose)
            require(std::any_of(asset.nodes.begin(),asset.nodes.end(),
                [&](const GunNode &node){return node.name==pose.node;}),"missing hand finger-pose node");
        error.clear();return true;
    }catch(const std::exception &e){try{error=e.what();}catch(...){}return false;}
    catch(...){try{error="hand pose validation failed";}catch(...){}return false;}
}
bool validate_hand_gun_nodes(const HandAssembly &assembly,const GunAsset &asset,
                             std::string &error) noexcept {
    try {
        const auto has=[&](const std::string &name){return std::any_of(asset.nodes.begin(),asset.nodes.end(),
            [&](const GunNode &node){return node.name==name;});};
        for(const auto &hand:assembly.hands)require(has(hand.node),"missing glove attachment node");
        for(const auto &mount:assembly.primary_mount_by_hand)require(has(mount.node),"missing primary gun mount node");
        if(assembly.has_support_grip)for(const auto &mount:assembly.support_by_primary)
            require(has(mount.node),"missing support mount node");
        for(const auto &label:assembly.labels)require(has(label.node),"missing label anchor node");
        error.clear();return true;
    }catch(const std::exception &e){try{error=e.what();}catch(...){}return false;}
    catch(...){try{error="gun anchor validation failed";}catch(...){}return false;}
}
bool select_shared_control_set(std::string_view defaults,std::string_view model_id,
                               std::string &set_id,std::string &error) noexcept {
    try {
        require(!defaults.empty()&&defaults.size()<=1024*1024,"control defaults size out of bounds");
        const auto doc=toml::parse(defaults);
        require(doc["version"].value_or(std::string{})=="0.1","unsupported control defaults");
        const auto *rules=doc["rule"].as_array();require(rules&&rules->size()<=128,"invalid control rules");
        std::string selected;
        for(const auto &rule:*rules){
            const auto *row=rule.as_table();require(row,"invalid control rule");
            if((*row)["gun_model"].value_or(std::string{})==model_id){
                selected=(*row)["control_set"].value_or(std::string{});break;
            }
        }
        require(!selected.empty()&&selected.size()<=64,"missing model control set");
        for(char c:selected)require((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-',"unsafe control set id");
        set_id=std::move(selected);error.clear();return true;
    }catch(const std::exception &e){try{error=e.what();}catch(...){}return false;}
    catch(...){try{error="control defaults decode failed";}catch(...){}return false;}
}
bool describe_inspection_labels(const HandAssembly &assembly,std::string_view control_set,
                                std::string_view model_id,std::vector<InspectionLabel> &out,
                                std::string &error) noexcept {
    try {
        require(control_set.size()<=1024*1024,"control set size out of bounds");
        std::vector<InspectionLabel> labels;
        labels.reserve(assembly.labels.size());
        for(const auto &anchor:assembly.labels)
            labels.push_back({anchor.semantic,anchor.node,anchor.semantic,"UNCONFIRMED","UNCONFIRMED"});
        if(!control_set.empty()){
            const auto doc=toml::parse(control_set);
            require(doc["version"].value_or(std::string{})=="0.1"&&
                    doc["gun_model"].value_or(std::string{})==model_id,"control set/model mismatch");
            const auto controller=[](const std::string &control)->std::string {
                if(control=="trigger")return "TRIGGER (CANDIDATE)";
                if(control=="grip")return "GRIP (CANDIDATE)";
                if(control=="primary")return "A/X (CANDIDATE)";
                if(control=="secondary")return "B/Y (CANDIDATE)";
                if(control=="thumbstick_click")return "STICK CLICK (CANDIDATE)";
                if(control=="offscreen_trigger")return "OFFSCREEN TRIGGER (CANDIDATE)";
                return "UNCONFIRMED";
            };
            const auto *elements=doc["element"].as_array();
            const auto *unmapped=doc["unmapped_part"].as_array();
            require((elements||unmapped)&&(!elements||elements->size()<=256)&&
                    (!unmapped||unmapped->size()<=256),"invalid control parts");
            if(elements)for(const auto &element:*elements){
                const auto *row=element.as_table();require(row,"invalid control element");
                if((*row)["slot"].value_or(int64_t{-1})!=0)continue;
                const auto node=(*row)["node"].value_or(std::string{});
                if(node.empty())continue; // virtual control has no physical gun anchor
                for(auto &label:labels)if(label.node==node){
                    const auto part=(*row)["part"].value_or(std::string{});
                    const auto action=(*row)["label"].value_or(std::string{});
                    const auto *binding=(*row)["binding"].as_table();
                    require(!part.empty()&&part.size()<=128&&!action.empty()&&action.size()<=128&&binding,
                            "invalid inspection element");
                    label.part=part;
                    label.game_action=action+" (UNCONFIRMED)";
                    label.controller_button=controller((*binding)["control"].value_or(std::string{}));
                }
            }
            if(unmapped){
                for(const auto &part:*unmapped){
                    const auto *row=part.as_table();require(row,"invalid unmapped part");
                    if((*row)["slot"].value_or(int64_t{-1})!=0)continue;
                    const auto node=(*row)["node"].value_or(std::string{});
                    const auto name=(*row)["label"].value_or(std::string{});
                    require(name.size()<=128,"unmapped part label too long");
                    for(auto &label:labels)if(label.node==node&&!node.empty()&&!name.empty()){
                        label.part=name;label.game_action="UNCONFIRMED";
                        label.controller_button="UNCONFIRMED";
                    }
                }
            }
        }
        out=std::move(labels);error.clear();return true;
    }catch(const std::exception &e){try{error=e.what();}catch(...){}return false;}
    catch(...){try{error="inspection labels decode failed";}catch(...){}return false;}
}
bool pose_hand_nodes(const GunAsset &asset,const HandMount &mount,
                     const std::vector<HandMotion> &motions,HandDriveLevels levels,
                     const Matrix &scene_from_grip,std::vector<Matrix> &out) noexcept {
    try {
        std::string pose_error;
        if(!validate_hand_pose_nodes(mount,asset,pose_error))return false;
        for(float v:scene_from_grip)if(!std::isfinite(v))return false;
        for(float v:{levels.trigger,levels.grip,levels.primary})if(!std::isfinite(v))return false;
        std::vector<Matrix> local;local.reserve(asset.nodes.size());
        for(const auto &node:asset.nodes){
            const auto pose=std::find_if(mount.finger_pose.begin(),mount.finger_pose.end(),
                [&](const HandMount::FingerPose &p){return p.node==node.name;});
            const auto motion=std::find_if(motions.begin(),motions.end(),
                [&](const HandMotion &m){return m.node==node.name;});
            Matrix adjust=rotation(pose==mount.finger_pose.end()?
                std::array<float,4>{0,0,0,1}:pose->rotation_xyzw);
            if(motion!=motions.end()){
                const float level=motion->drive=="trigger"?levels.trigger:
                    motion->drive=="grip"?levels.grip:levels.primary;
                const float angle=motion->minimum+(motion->maximum-motion->minimum)*std::clamp(level,0.f,1.f);
                const float s=std::sin(angle*.5f),c=std::cos(angle*.5f);
                const std::array<float,4> q{motion->axis[0]*s,motion->axis[1]*s,motion->axis[2]*s,c};
                adjust=product(rotation(q),adjust); // drive before authored rest rotation
            }
            auto value=product(adjust,node.local);
            for(unsigned i=0;i<3;++i)value[12+i]=node.local[12+i]+(pose==mount.finger_pose.end()?0:pose->translation_offset[i]);
            if(pose!=mount.finger_pose.end())for(unsigned col=0;col<3;++col)
                for(unsigned row=0;row<3;++row)value[col*4+row]*=pose->scale[col];
            local.push_back(value);
        }
        std::vector<Matrix> placed(asset.nodes.size());std::vector<bool> resolved(asset.nodes.size());
        size_t count=0;
        while(count<asset.nodes.size()){
            bool advanced=false;
            for(size_t i=0;i<asset.nodes.size();++i)if(!resolved[i]){
                const auto parent=asset.nodes[i].parent;
                if(parent>=0&&!resolved[size_t(parent)])continue;
                placed[i]=product(parent<0?scene_from_grip:placed[size_t(parent)],local[i]);
                for(float v:placed[i])if(!std::isfinite(v))return false;
                resolved[i]=true;++count;advanced=true;
            }
            if(!advanced)return false;
        }
        out=std::move(placed);return true;
    }catch(...){return false;}
}
}
