// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <chrono>
namespace acvr_wait {
enum class Result { ready, stopped, deadline, error };
// A finite timeout is mandatory on EVERY runtime call, including retries.
// XR_TIMEOUT_EXPIRED is a positive result and must never be treated as ready.
template<class Wait,class Stop>
Result image(Wait wait,Stop stop)
{
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(250);
  while(!stop()) {
    const auto result=wait(10000000LL); // 10 ms, in OpenXR nanoseconds
    if(result==0) return stop()?Result::stopped:Result::ready;
    if(result!=1) return Result::error;
    if(std::chrono::steady_clock::now()>=deadline) return Result::deadline;
  }
  return Result::stopped;
}
}
