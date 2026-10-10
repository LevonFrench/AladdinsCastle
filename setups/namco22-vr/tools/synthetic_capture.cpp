// SPDX-License-Identifier: GPL-3.0-only
#include "n22_scene.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <string>

int main(int argc,char **argv) {
    try {
        if(argc!=2 && (argc!=3 || std::string(argv[2])!="--materials")) {
            std::cerr<<"Usage: namco22_synthetic_capture <absolute .local path>/synthetic.ppm [--materials]\n";return 2;
        }
        std::filesystem::path requested=std::filesystem::u8path(argv[1]);
        if(!requested.is_absolute() || requested.extension()!= ".ppm") throw std::runtime_error("Output must be an absolute .ppm path under this checkout's .local directory");
        // Only output in the invoking checkout, not an arbitrary path containing .local.
        auto root=std::filesystem::canonical(std::filesystem::current_path());
        if(!std::filesystem::exists(root/"setups/namco22-vr/CMakeLists.txt") ||
           !std::filesystem::exists(root/"libacvr/include/acvr.h")) throw std::runtime_error("Run from the checkout root");
        std::filesystem::create_directories(root/".local");
        auto local=std::filesystem::canonical(root/".local");
        if(local!=root/".local") throw std::runtime_error("Checkout .local must not redirect outside the checkout");
        auto parent=std::filesystem::canonical(requested.parent_path());
        auto relative=parent.lexically_relative(local);
        if(relative.empty() || *relative.begin()==".." || relative.is_absolute()) throw std::runtime_error("Output parent must resolve within this checkout's .local directory");
        auto output=parent/requested.filename();
        if(std::filesystem::exists(output) || std::filesystem::is_symlink(std::filesystem::symlink_status(output))) throw std::runtime_error("Output exists; choose a new filename");
        n22::Frame frame;
        if(n22::prepare(argc==3?n22::synthetic_material_cube():n22::synthetic_cube(),1,frame)!=ACVR_OK) return 1;
        n22::Image image(1280,480);
        for(uint32_t i=0;i<2;++i) {
            auto eye=n22::desktop_eye(i,i==0?-.032f:.032f,4,640,480);
            // Shared translated/rotated head pose exposes a side of the cube.
            const float yaw=-.25f,head_x=.9f,eye_x=i==0?-.032f:.032f;
            eye.view_from_scene[0]=eye.view_from_scene[10]=std::cos(yaw);
            eye.view_from_scene[8]=std::sin(yaw);eye.view_from_scene[2]=-std::sin(yaw);
            eye.view_from_scene[12]=-std::cos(yaw)*head_x-eye_x;
            eye.view_from_scene[14]=std::sin(yaw)*head_x;
            if(n22::draw_cpu(frame,eye,image)!=ACVR_OK) return 1;
        }
        std::ofstream file(output,std::ios::binary);
        file<<"P6\n"<<image.width<<" "<<image.height<<"\n255\n";
        for(uint32_t rgb:image.rgb) {
            const char bytes[3]={static_cast<char>((rgb>>16)&255),static_cast<char>((rgb>>8)&255),static_cast<char>(rgb&255)};
            file.write(bytes,3);
        }
        if(!file) throw std::runtime_error("Capture write failed");
        std::cout<<"Synthetic CPU capture: "<<output.u8string()<<"\n";
        return 0;
    } catch(const std::exception &e) { std::cerr<<e.what()<<"\n"; return 1; }
}
