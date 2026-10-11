// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "capture_plan.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdint>

namespace room {
namespace {
bool no_reparse(std::filesystem::path path) {
    for(;;){
        const DWORD attributes=GetFileAttributesW(path.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        const auto parent=path.parent_path();
        if(parent==path||parent.empty())return true;
        path=parent;
    }
}
}
bool capture_name_ok(std::string_view name) noexcept {
    if(name.empty()||name.size()>64)return false;
    for(char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                         (c>='0'&&c<='9')||c=='-'||c=='_'))return false;
    return true;
}
bool prepare_capture_directory(const std::filesystem::path &root,std::string_view name,
                               std::filesystem::path &out) noexcept {
    try {
        if(!root.is_absolute()||!capture_name_ok(name)||!no_reparse(root)||
           !std::filesystem::is_directory(root))return false;
        const auto local=root/".local";
        const auto base=local/"gun-room";
        const auto target=base/std::string(name);
        if(!no_reparse(target))return false;
        if(!std::filesystem::exists(local)&&!CreateDirectoryW(local.c_str(),nullptr))return false;
        if(!no_reparse(local)||!std::filesystem::is_directory(local))return false;
        if(!std::filesystem::exists(base)&&!CreateDirectoryW(base.c_str(),nullptr))return false;
        if(!no_reparse(base)||!std::filesystem::is_directory(base))return false;
        if(!CreateDirectoryW(target.c_str(),nullptr))return false; // exclusive: never reuse
        if(!no_reparse(target)){RemoveDirectoryW(target.c_str());return false;}
        out=target;return true;
    }catch(...){return false;}
}
CaptureStatus capture_receipt_status(const std::filesystem::path &directory,
                                     std::string_view expected_model) noexcept {
    try {
        const auto receipt=directory/"receipt.json";
        if(!std::filesystem::exists(receipt))return CaptureStatus::Missing;
        if(!no_reparse(receipt)||!std::filesystem::is_regular_file(receipt)||
           std::filesystem::file_size(receipt)>4096)return CaptureStatus::Invalid;
        std::ifstream in(receipt,std::ios::binary);
        const auto data=nlohmann::json::parse(in);
        if(data.at("schema")!=1||data.at("model").get<std::string>()!=std::string(expected_model))
            return CaptureStatus::Invalid;
        if(data.at("status")=="error")return CaptureStatus::Error;
        if(data.at("status")!="captured"||data.at("actual_openxr_eye_readback")!=true)
            return CaptureStatus::Invalid;
        const auto &hashes=data.at("source_sha256");
        const auto hash_ok=[](const std::string &value,bool optional){
            if(optional&&value=="absent")return true;
            if(value.size()!=64)return false;
            for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
            return true;
        };
        if(!hash_ok(hashes.at("gun_glb").get<std::string>(),false)||
           !hash_ok(hashes.at("hand_assembly").get<std::string>(),true)||
           !hash_ok(hashes.at("right_hand_glb").get<std::string>(),true)||
           !hash_ok(hashes.at("left_hand_glb").get<std::string>(),true))return CaptureStatus::Invalid;
        for(const auto *hand:{"left","right"}){
            const auto width=data.at(hand).at("width").get<int>();
            const auto height=data.at(hand).at("height").get<int>();
            if(width<=0||height<=0||width>4096||height>4096||
               uint64_t(width)*height*3>64*1024*1024)return CaptureStatus::Invalid;
            const auto path=directory/(std::string(hand)+".ppm");
            if(!no_reparse(path)||!std::filesystem::is_regular_file(path))return CaptureStatus::Invalid;
            std::ifstream image(path,std::ios::binary);
            std::string magic;int actual_width=0,actual_height=0,max_value=0;
            image>>magic>>actual_width>>actual_height>>max_value;
            if(!image||magic!="P6"||actual_width!=width||actual_height!=height||max_value!=255||
               image.get()!='\n')return CaptureStatus::Invalid;
            const auto header=static_cast<std::streamoff>(image.tellg());
            if(header<0||std::filesystem::file_size(path)!=static_cast<uint64_t>(header)+uint64_t(width)*height*3)
                return CaptureStatus::Invalid;
        }
        return CaptureStatus::Success;
    }catch(...){return CaptureStatus::Invalid;}
}
}
