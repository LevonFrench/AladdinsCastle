// SPDX-License-Identifier: MIT
#pragma once
#include "grip_calibration.hpp"
#include <filesystem>
#include <string>

namespace room {
enum class GripLoad {Missing,Loaded,Error};
std::filesystem::path grip_file(const std::filesystem::path &root,
                                const std::string &model,unsigned hand);
GripLoad load_grip(const std::filesystem::path &root,const std::string &model,
                   unsigned hand,GripAngles &out);
bool save_grip(const std::filesystem::path &root,const std::string &model,
               unsigned hand,GripAngles angles);
}
