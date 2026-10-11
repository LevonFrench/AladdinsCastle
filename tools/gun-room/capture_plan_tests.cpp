// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "capture_plan.hpp"
#include "stereo_capture.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

int main(int argc,char **argv){
    try {
        if(argc!=2)throw std::runtime_error("test build root missing");
        const auto root=std::filesystem::u8path(argv[1])/
            ("capture-plan-"+std::to_string(GetCurrentProcessId()));
        if(!std::filesystem::create_directory(root))throw std::runtime_error("test root create failed");
        // Distinct view payloads prove that PRIMARY_STEREO view 0 is written as
        // left.ppm and view 1 as right.ppm, with the same mapping for metadata.
        struct Eye {unsigned width,height;char pixel;bool gun_drawn;};
        const std::array<Eye,2> views{{{2,1,'L',true},{1,2,'R',false}}};
        for(const auto &output:acvr::kStereoCaptureViews){
            const auto &eye=views.at(output.view_index);
            const auto path=root/output.filename;
            std::ofstream file(path,std::ios::binary);
            file<<"P6\n"<<eye.width<<" "<<eye.height<<"\n255\n";
            for(unsigned i=0;i<eye.width*eye.height*3;++i)file.put(eye.pixel);
        }
        const auto read_all=[](const std::filesystem::path &path){
            std::ifstream file(path,std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file),{});
        };
        if(read_all(root/"left.ppm")!="P6\n2 1\n255\nLLLLLL"||
           read_all(root/"right.ppm")!="P6\n1 2\n255\nRRRRRR"||
           std::string(acvr::kStereoCaptureViews[0].name)!="left"||
           std::string(acvr::kStereoCaptureViews[1].name)!="right"||
           !views.at(acvr::kStereoCaptureViews[0].view_index).gun_drawn||
           views.at(acvr::kStereoCaptureViews[1].view_index).gun_drawn)
            throw std::runtime_error("stereo capture eye order or metadata reversed");
        std::filesystem::remove(root/"left.ppm");
        std::filesystem::remove(root/"right.ppm");
        std::filesystem::path result;
        for(const auto *bad:{"","../escape","a/b","C:x",".hidden","name.ppm"})
            if(room::prepare_capture_directory(root,bad,result))throw std::runtime_error("unsafe name accepted");
        if(std::filesystem::exists(root/".local"))throw std::runtime_error("invalid name wrote output");
        if(!room::prepare_capture_directory(root,"eyes-one",result)||
           result!=root/".local"/"gun-room"/"eyes-one")throw std::runtime_error("fresh capture failed");
        std::ofstream(result/"left.ppm")<<"preserve";
        if(room::prepare_capture_directory(root,"eyes-one",result))throw std::runtime_error("existing capture accepted");
        std::ifstream in(result/"left.ppm");std::string preserved;in>>preserved;
        if(preserved!="preserve")throw std::runtime_error("existing capture changed");
        in.close();
        if(room::capture_receipt_status(result,"fixture")!=room::CaptureStatus::Missing)
            throw std::runtime_error("missing receipt accepted");
        const std::string zeros(64,'0');
        const std::string receipt="{\"schema\":1,\"status\":\"captured\",\"model\":\"fixture\","
            "\"actual_openxr_eye_readback\":true,\"left\":{\"width\":1,\"height\":1},"
            "\"right\":{\"width\":1,\"height\":1},\"source_sha256\":{\"gun_glb\":\""+
            zeros+"\",\"hand_assembly\":\"absent\",\"right_hand_glb\":\"absent\","
            "\"left_hand_glb\":\"absent\"}}";
        std::ofstream(result/"receipt.json")<<receipt;
        if(room::capture_receipt_status(result,"fixture")!=room::CaptureStatus::Invalid)
            throw std::runtime_error("missing PPM accepted");
        const std::string ppm=std::string("P6\n1 1\n255\n",11)+std::string(3,'x');
        {std::ofstream left(result/"left.ppm",std::ios::binary|std::ios::trunc);left.write(ppm.data(),ppm.size());}
        {std::ofstream right(result/"right.ppm",std::ios::binary);right.write(ppm.data(),ppm.size());}
        if(room::capture_receipt_status(result,"fixture")!=room::CaptureStatus::Success||
           room::capture_receipt_status(result,"other")!=room::CaptureStatus::Invalid)
            throw std::runtime_error("receipt source/status validation failed");
        {std::ofstream left(result/"left.ppm",std::ios::binary|std::ios::app);left.put('x');}
        if(room::capture_receipt_status(result,"fixture")!=room::CaptureStatus::Invalid)
            throw std::runtime_error("oversized PPM accepted");
        std::ofstream(result/"receipt.json",std::ios::trunc)
            <<"{\"schema\":1,\"status\":\"error\",\"reason\":\"deadline\",\"model\":\"fixture\"}";
        if(room::capture_receipt_status(result,"fixture")!=room::CaptureStatus::Error)
            throw std::runtime_error("explicit capture error not reported");
        const auto actual=root/"actual";
        std::filesystem::create_directory(actual);
        const auto alias=root/".local"/"gun-room"/"linked";
        std::error_code link_error;
        std::filesystem::create_directory_symlink(actual,alias,link_error);
        if(!link_error){
            if(room::prepare_capture_directory(root,"linked",result))
                throw std::runtime_error("reparse capture target accepted");
            std::filesystem::remove(alias);
        }
        std::filesystem::remove(actual);
        std::filesystem::remove(result/"left.ppm");
        std::filesystem::remove(result/"right.ppm");
        std::filesystem::remove(result/"receipt.json");
        std::filesystem::remove(result);
        std::filesystem::remove(root/".local"/"gun-room");
        std::filesystem::remove(root/".local");
        std::filesystem::remove(root);
        std::cout<<"capture plan checks passed\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
