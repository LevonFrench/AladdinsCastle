// SPDX-License-Identifier: MIT
#define NOMINMAX
#include <windows.h>
#include "grip_calibration_file.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <fstream>
#include <set>

namespace room {
namespace {
bool identifier(const std::string &value){
    if(value.empty()||value.size()>64)return false;
    for(char c:value)if(!(c>='a'&&c<='z')&&!(c>='0'&&c<='9')&&c!='-')return false;
    return true;
}
bool no_reparse(std::filesystem::path path){
    for(;;){
        const DWORD attributes=GetFileAttributesW(path.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES){
            const DWORD error=GetLastError();
            if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)return false;
        }
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        const auto parent=path.parent_path();
        if(parent==path||parent.empty())return true;
        path=parent;
    }
}
const char *hand_name(unsigned hand){return hand==0?"right":hand==1?"left":nullptr;}
bool private_path(const std::filesystem::path &root,const std::string &model,unsigned hand){
    if(!root.is_absolute()||!identifier(model)||!hand_name(hand)||
       !std::filesystem::is_directory(root)||!no_reparse(grip_file(root,model,hand)))return false;
    const auto local=root/".local",room=local/"gun-room",calibration=room/"calibration";
    for(const auto &directory:{local,room,calibration})
        if(std::filesystem::exists(directory)&&!std::filesystem::is_directory(directory))return false;
    return true;
}
bool read_body(const std::filesystem::path &path,std::string &body){
    std::ifstream file(path,std::ios::binary);
    if(!file)return false;
    file.seekg(0,std::ios::end);
    const std::streamoff length=file.tellg();
    if(length<0||length>512)return false;
    body.resize(static_cast<size_t>(length));
    file.seekg(0,std::ios::beg);
    file.read(body.data(),static_cast<std::streamsize>(body.size()));
    return file.gcount()==static_cast<std::streamsize>(body.size());
}
bool write_body(HANDLE file,const std::string &body){
    DWORD written=0;
    return body.size()<4096&&WriteFile(file,body.data(),static_cast<DWORD>(body.size()),&written,nullptr)&&
        written==body.size()&&FlushFileBuffers(file);
}
}
std::filesystem::path grip_file(const std::filesystem::path &root,const std::string &model,unsigned hand){
    const char *name=hand_name(hand);
    if(!name||!identifier(model))return {};
    return root/".local"/"gun-room"/"calibration"/(model+"-"+name+".json");
}
GripLoad load_grip(const std::filesystem::path &root,const std::string &model,
                   unsigned hand,GripAngles &out){
    try {
        if(!private_path(root,model,hand))return GripLoad::Error;
        const auto path=grip_file(root,model,hand);
        if(!std::filesystem::exists(path))return GripLoad::Missing;
        if(!std::filesystem::is_regular_file(path))return GripLoad::Error;
        std::string body;if(!read_body(path,body))return GripLoad::Error;
        std::set<std::string> keys;
        bool duplicate=false;
        const auto parsed=nlohmann::json::parse(body,[&](int,nlohmann::json::parse_event_t event,nlohmann::json &value){
            if(event==nlohmann::json::parse_event_t::key&&!keys.insert(value.get<std::string>()).second)
                duplicate=true;
            return true;
        });
        if(duplicate||!parsed.is_object()||parsed.size()!=5||
           !parsed.at("schema").is_number_integer()||parsed.at("schema")!=1||
           !parsed.at("model_id").is_string()||parsed.at("model_id")!=model||
           !parsed.at("hand").is_string()||parsed.at("hand")!=hand_name(hand)||
           !parsed.at("yaw_deg").is_number()||!parsed.at("pitch_deg").is_number())return GripLoad::Error;
        const GripAngles loaded{parsed.at("yaw_deg").get<float>(),parsed.at("pitch_deg").get<float>()};
        if(!angles_ok(loaded))return GripLoad::Error;
        out=loaded;return GripLoad::Loaded;
    }catch(...){return GripLoad::Error;}
}
bool save_grip(const std::filesystem::path &root,const std::string &model,
               unsigned hand,GripAngles angles){
    HANDLE mutex=INVALID_HANDLE_VALUE;
    std::filesystem::path temporary;
    try {
    if(!angles_ok(angles)||!private_path(root,model,hand))return false;
    const auto path=grip_file(root,model,hand),directory=path.parent_path();
    try {std::filesystem::create_directories(directory);}catch(...){return false;}
    if(!no_reparse(path)||!std::filesystem::is_directory(directory)||
       (std::filesystem::exists(path)&&!std::filesystem::is_regular_file(path)))return false;
    auto lock=path;lock+=L".lock";
    if(!no_reparse(lock))return false;
    mutex=CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
                      FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr);
    if(mutex==INVALID_HANDLE_VALUE)return false;
    bool okay=false;
    if(no_reparse(path)){
        const nlohmann::json data={{"schema",1},{"model_id",model},{"hand",hand_name(hand)},
                                   {"yaw_deg",angles.yaw_deg},{"pitch_deg",angles.pitch_deg}};
        const std::string body=data.dump()+"\n";
        for(unsigned attempt=0;attempt<32&&!okay;++attempt){
            temporary=path;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+
                std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(attempt);
            HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE){if(GetLastError()!=ERROR_FILE_EXISTS)break;continue;}
            okay=write_body(file,body);CloseHandle(file);
            if(okay&&no_reparse(path))okay=MoveFileExW(temporary.c_str(),path.c_str(),
                                             MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
            if(!okay)DeleteFileW(temporary.c_str());
        }
    }
    CloseHandle(mutex);mutex=INVALID_HANDLE_VALUE;
    return okay;
    }catch(...){
        if(mutex!=INVALID_HANDLE_VALUE)CloseHandle(mutex);
        if(!temporary.empty())DeleteFileW(temporary.c_str());
        return false;
    }
}
}
