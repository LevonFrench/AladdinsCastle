// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <mutex>
#include <openxr/openxr.h>
namespace acvr_publish {
template<class T> class InputLease {
 mutable std::mutex mutex;
 T value{};
 std::uint64_t stamp=0;
 bool valid=false;
public:
 void publish(std::uint64_t now,const T& input){std::lock_guard<std::mutex> lock(mutex);stamp=now;value=input;valid=true;}
 T get(std::uint64_t now)const{
  std::lock_guard<std::mutex> lock(mutex);
  return valid && now>=stamp && now-stamp<250 ? value:T{};
 }
 void clear(){std::lock_guard<std::mutex> lock(mutex);value={};stamp=0;valid=false;}
};
class SessionLoop {
 bool begun=false,focus=false,exit=false;
public:
 template<class Begin,class End,class Clear> void transition(XrSessionState state,Begin begin,End end,Clear clear){
  focus=state==XR_SESSION_STATE_FOCUSED;
  if(!focus)clear();
  if(state==XR_SESSION_STATE_READY && !begun)begun=begin();
  if(state==XR_SESSION_STATE_STOPPING){if(begun)end();begun=false;}
  if(state==XR_SESSION_STATE_EXITING || state==XR_SESSION_STATE_LOSS_PENDING)exit=true;
 }
 bool running()const{return begun;}
 bool focused()const{return focus;}
 bool exiting()const{return exit;}
};
// Errors belong to the current context. The cap also bounds a lost context.
template<class Get,class Report> unsigned drain_errors(Get get,Report report){
 unsigned count=0;
 while(count<64){const auto error=get();if(!error)break;report(unsigned(error));++count;}
 return count;
}
}
