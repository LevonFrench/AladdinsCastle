// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <string_view>

namespace room {
bool capture_name_ok(std::string_view name) noexcept;
// Creates exactly one fresh, confined directory. Existing targets and any
// reparse point along the path fail without touching their contents.
bool prepare_capture_directory(const std::filesystem::path &root,std::string_view name,
                               std::filesystem::path &out) noexcept;
enum class CaptureStatus {Missing,Success,Error,Invalid};
CaptureStatus capture_receipt_status(const std::filesystem::path &directory,
                                     std::string_view expected_model) noexcept;
}
