// SPDX-License-Identifier: GPL-3.0-only
#include "n22_worker.hpp"
#include <cmath>
#include <limits>

namespace n22 {
Worker::Worker(Entry entry) {
    thread_=std::thread([this,entry=std::move(entry)] {
        acvr_result result=ACVR_ERROR;
        try { result=entry(*this); } catch(...) { result=ACVR_ERROR; }
        std::lock_guard<std::mutex> lock(mutex_);
        finished_=true;inflight_=false;final_=result==ACVR_OK?ACVR_BAD_STATE:result;cv_.notify_all();
    });
}
Worker::~Worker() { stop(); }
acvr_result Worker::request(NativeInput in,std::shared_ptr<const VideoSnapshot> &out) {
    out.reset();
    if(!std::isfinite(in.x) || !std::isfinite(in.y) || !std::isfinite(in.cover) ||
       in.cover<0 || in.cover>1 || (in.trigger&~(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED|ACVR_INPUT_RELEASED))!=0 ||
       ((in.trigger&ACVR_INPUT_PRESSED) && !(in.trigger&ACVR_INPUT_HELD)) ||
       ((in.trigger&ACVR_INPUT_RELEASED) && (in.trigger&(ACVR_INPUT_PRESSED|ACVR_INPUT_HELD)))) return ACVR_BAD_ARGUMENT;
    std::unique_lock<std::mutex> lock(mutex_);
    if(stopping_ || finished_) return final_;
    if(leased_ || paused_ || permitted_ || inflight_) return ACVR_BAD_STATE;
    if(tick_==std::numeric_limits<uint64_t>::max() || in.tick!=tick_+1) return ACVR_BAD_ARGUMENT;
    pending_=in;permitted_=true;cv_.notify_all();
    cv_.wait(lock,[this] {return leased_ || finished_ || stopping_;});
    if(leased_) {out=snapshot_;return ACVR_OK;}
    return finished_?final_:ACVR_STOPPED;
}
bool Worker::begin(NativeInput &out) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock,[this] {return stopping_ || (permitted_ && !paused_ && !leased_);});
    if(stopping_) return false;
    out=pending_;tick_=out.tick;permitted_=false;inflight_=true;return true;
}
bool Worker::publish(std::shared_ptr<const VideoSnapshot> snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_ || !inflight_ || !snapshot || snapshot->tick!=tick_) return false;
    snapshot_=std::move(snapshot);inflight_=false;leased_=true;cv_.notify_all();return true;
}
acvr_result Worker::release(const std::shared_ptr<const VideoSnapshot> &snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!leased_ || !snapshot || snapshot.get()!=snapshot_.get()) return ACVR_BAD_STATE;
    leased_=false;snapshot_.reset();cv_.notify_all();return ACVR_OK;
}
acvr_result Worker::pause(bool paused) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_ || finished_) return final_;
    if(inflight_ || permitted_) return ACVR_BAD_STATE;
    paused_=paused;return ACVR_OK;
}
bool Worker::stop_requested() {std::lock_guard<std::mutex> lock(mutex_);return stopping_;}
void Worker::stop() {
    {std::lock_guard<std::mutex> lock(mutex_);stopping_=true;cv_.notify_all();}
    if(thread_.joinable()) thread_.join();
}
}
