// SPDX-License-Identifier: GPL-3.0-only
#include "n22_worker.hpp"
#include "n22_source_hooks.h"
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>

extern "C" {
void n22_fixture_entry(void);
void n22_fixture_poll_forever(void);
void n22_fixture_bad_video(void);
void n22_fixture_return(void);
}
namespace {
int checks=0;
void check(bool ok,const char *s) {++checks;if(!ok) throw std::runtime_error(s);}
struct State {
    n22::Worker &worker;
    n22::NativeInput input{};
    uint32_t applies=0,pressed=0;
    uint16_t outputs=0;
    std::shared_ptr<const n22::VideoSnapshot> staged{};
    std::promise<void> *started=nullptr;
};
int begin(void *p) {
    auto &s=*static_cast<State *>(p);
    if(!s.worker.begin(s.input)) return 0;
    ++s.applies;if(s.input.trigger&ACVR_INPUT_PRESSED) ++s.pressed;
    s.staged.reset();
    if(s.started) {s.started->set_value();s.started=nullptr;}
    return 1;
}
int publish(void *p) {auto &s=*static_cast<State *>(p);return s.worker.publish(s.staged)?1:-1;}
int stop(void *p) {return static_cast<State *>(p)->worker.stop_requested()?1:0;}
int video(void *p,const void *data) {
    auto &s=*static_cast<State *>(p);
    if(!data || s.staged) return -1;
    auto snapshot=std::make_shared<n22::VideoSnapshot>();snapshot->tick=s.input.tick;snapshot->output_bits=s.outputs;
    snapshot->banks[n22::Mixer]={static_cast<uint8_t>(*static_cast<const uint32_t *>(data)),static_cast<uint8_t>(s.input.trigger)};
    s.staged=std::move(snapshot);return 1;
}
void output(void *p,uint16_t value) {static_cast<State *>(p)->outputs=value;}
acvr_result run(State &s,void (*entry)(void)) {
    acvr_ss22_hooks hooks{&s,begin,publish,stop,video,output,nullptr};
    if(acvr_ss22_bind_hooks(&hooks)!=ACVR_OK) return ACVR_ERROR;
    auto result=acvr_ss22_run_entry(entry);acvr_ss22_bind_hooks(nullptr);return result;
}
void lifecycle() {
    State *state=nullptr;
    n22::Worker worker([&](n22::Worker &w) {State s{w};state=&s;return run(s,n22_fixture_entry);});
    std::shared_ptr<const n22::VideoSnapshot> first,second;
    auto input=n22::NativeInput{1,ACVR_INPUT_HELD|ACVR_INPUT_PRESSED,.2f,.3f,1};
    check(worker.request(input,first)==ACVR_OK && first->tick==1,"one permit produces one tick");
    check(state->applies==1 && state->pressed==1 && first->output_bits==1,"input/output produced once per native tick");
    auto original=first->banks[n22::Mixer];input.trigger=0;
    check(first->banks[n22::Mixer]==original,"input copied before producer use");
    input.tick=2;
    check(worker.request(input,second)==ACVR_BAD_STATE && !second && state->applies==1,"lease forbids extra permit and repeated input edge");
    check(worker.pause(true)==ACVR_OK && first->banks[n22::Mixer]==original,"pause keeps published snapshot stable");
    check(worker.release(first)==ACVR_OK,"release lease");
    check(worker.request(input,second)==ACVR_BAD_STATE && state->applies==1,"paused worker does not step");
    check(worker.pause(false)==ACVR_OK,"resume");
    input.trigger=ACVR_INPUT_HELD;
    check(worker.request(input,second)==ACVR_OK && second->tick==2 && second->output_bits==2,"second permit exactly one new tick");
    check(state->applies==2 && state->pressed==1 && first->banks[n22::Mixer]==original,"old copied snapshot survives later publication");
    check(worker.release(first)==ACVR_BAD_STATE,"wrong lease cannot release current publication");
    check(worker.release(second)==ACVR_OK,"release second lease");
    input.tick=4;check(worker.request(input,second)==ACVR_BAD_ARGUMENT,"skipped native tick rejected");
    input.tick=3;input.trigger=ACVR_INPUT_RELEASED|ACVR_INPUT_HELD;
    check(worker.request(input,second)==ACVR_BAD_ARGUMENT,"invalid digital edge rejected");
    input.trigger=ACVR_INPUT_RELEASED;
    check(worker.request(input,second)==ACVR_OK && state->applies==3 && state->pressed==1,"release edge consumed once");
    worker.stop();
    check(second->tick==3 && second->banks[n22::Mixer][0]==3,"cooperative stop preserves owner-held snapshot");
    check(worker.release(second)==ACVR_OK,"stopped worker allows lease release");
    input.tick=4;check(worker.request(input,second)==ACVR_STOPPED && !second,"no permit after stop");
}
void failures_and_stop() {
    for(auto entry:{n22_fixture_bad_video,n22_fixture_return}) {
        n22::Worker worker([entry](n22::Worker &w) {State s{w};return run(s,entry);});
        std::shared_ptr<const n22::VideoSnapshot> out;
        auto result=worker.request({1,0,.5f,.5f,0},out);
        check((result==ACVR_ERROR || result==ACVR_BAD_STATE) && !out,"failed snapshot or unexpected return releases waiter");
    }
    n22::Worker waiting([](n22::Worker &w) {State s{w};return run(s,n22_fixture_entry);});
    waiting.stop();check(waiting.stop_requested(),"stop wakes initial permit wait");
    std::promise<void> started;auto ready=started.get_future();
    bool destructed=false;
    n22::Worker busy([&](n22::Worker &w) {
        struct Guard {bool &done;~Guard() {done=true;}} guard{destructed};
        State s{w};s.started=&started;return run(s,n22_fixture_poll_forever);
    });
    std::shared_ptr<const n22::VideoSnapshot> out;
    auto request=std::async(std::launch::async,[&] {return busy.request({1,0,.5f,.5f,0},out);});
    const bool entered=ready.wait_for(std::chrono::seconds(5))==std::future_status::ready;
    if(!entered) busy.stop();
    check(entered,"busy slice entered");
    busy.stop();
    check(request.get()==ACVR_STOPPED && !out,"slice poll safely stops nonreturning C entry");
    check(destructed,"C trampoline returns normally through surrounding C++ destruction");
}
}
int main() {
    try {lifecycle();failures_and_stop();std::cout<<checks<<" worker lifecycle checks passed\n";return 0;}
    catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}
