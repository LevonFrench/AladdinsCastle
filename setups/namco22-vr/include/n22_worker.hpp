// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "acvr.h"
#include <array>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace n22 {
struct NativeInput { uint64_t tick=0; uint32_t trigger=0; float x=.5f,y=.5f,cover=0; };
// Exact owned video banks; no pointers into emulated RAM survive publication.
enum Bank : size_t { Palette, Mixer, Characters, Text, Sprites, Vics, Spot, BankCount };
inline constexpr std::array<size_t,BankCount> bank_sizes={0x18000,0x400,0x1e000,0x2000,0x30000,0x10000,0x1000};
struct VideoSnapshot {
    uint64_t tick=0;
    std::array<std::vector<uint8_t>,BankCount> banks;
    std::vector<uint32_t> polygon_words;
    std::array<uint16_t,8> czattr{},tilemapattr{};
    std::array<std::array<uint16_t,256>,4> czram{};
    std::array<uint16_t,0x800> spot_words{};
    std::array<uint32_t,32> vics_ctl{};
    bool walk=false,spot_enabled=false;
    uint16_t output_bits=0;
};
class Worker {
public:
    using Entry=std::function<acvr_result(Worker &)>;
    explicit Worker(Entry);
    ~Worker();
    Worker(const Worker &)=delete;
    Worker &operator=(const Worker &)=delete;
    acvr_result request(NativeInput,std::shared_ptr<const VideoSnapshot> &);
    acvr_result release(const std::shared_ptr<const VideoSnapshot> &);
    acvr_result pause(bool);
    void stop(); // cooperative; never terminates a thread or process
    bool stop_requested();
    // Producer calls these from its worker thread only. The C trampoline calls
    // begin before entering lifted C and after each frame-boundary publication.
    bool begin(NativeInput &);
    bool publish(std::shared_ptr<const VideoSnapshot>);
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread thread_;
    NativeInput pending_{};
    std::shared_ptr<const VideoSnapshot> snapshot_;
    uint64_t tick_=0;
    bool permitted_=false,inflight_=false,leased_=false,paused_=false,stopping_=false,finished_=false;
    acvr_result final_=ACVR_STOPPED;
};
}
