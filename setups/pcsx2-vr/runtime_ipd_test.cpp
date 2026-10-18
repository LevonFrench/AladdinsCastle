// SPDX-License-Identifier: GPL-3.0+
#include "GS/PS2VRScreenDepth.h"
#include <cassert>
#include <limits>
#include <cstdio>
int main(int argc,char** argv){
 using namespace PS2VRScreenDepth;
 const bool on=argc>1 && argv[1][0]=='1';assert(Enabled()==on);
 const float configured=.063f;auto equal=[](float a,float b){return std::abs(a-b)<1e-6f;};
 Reset();assert(equal(Select(configured,false,1,1000),configured));
 const bool accepted=Publish({-.036f,1,2},{.036f,1,2},true,1000);assert(accepted==on);
 assert(equal(Select(configured,false,2,1001),on?.072f:configured));
 Publish({-.032f,0,0},{.032f,0,0},true,1002);
 assert(equal(Select(configured,false,2,1003),on?.072f:configured)); // frame held
 assert(equal(Select(configured,false,3,1003),on?.064f:configured));
 assert(equal(Select(configured,true,4,1003),configured)); // INSIDE untouched
 const float c=.6f,s=.8f;
 Publish({3-.036f*c,2,1-.036f*s},{3+.036f*c,2,1+.036f*s},true,1100);
 assert(equal(Select(configured,false,5,1101),on?.072f:configured)); // translated/yawed baseline
 assert(equal(Select(configured,false,6,1600),configured)); // exact stale boundary
 assert(equal(Select(configured,false,7,1099),configured)); // backward time
 assert(!Publish({0,0,0},{1,0,0},true,1700));
 assert(equal(Select(configured,false,8,1701),configured));
 assert(!Publish({0,0,0},{std::numeric_limits<float>::quiet_NaN(),0,0},true,1702));
 assert(!Publish({0,0,0},{.072f,0,0},false,1703));
 assert(equal(Select(configured,false,9,1704),configured));
 Reset();assert(equal(Select(configured,false,9,1800),configured));
 std::printf("PASS runtime-IPD opt-in=%d: synthetic 72/64 mm, rigid view-pair rotation/translation, frame latch, INSIDE unchanged, stale/invalid/reset fallback\n",on);
}

