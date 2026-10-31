// SPDX-License-Identifier: GPL-3.0+
// CPU-only check of the INSIDE pose bookkeeping and upscale cap.
// Build and run through inside_pose_test.py, which extracts the header from the patch.
#include "GS/PS2VRInside.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace PS2VRInside;
namespace {
int failures=0;
void Check(bool ok,const char* what) { std::printf("%s %s\n",ok?"PASS":"FAIL",what); if(!ok)failures++; }
Packet Pose(std::uint64_t sequence,float yaw) { return Synthetic(yaw,{-1,1,-1.037f,1.037f},sequence,static_cast<std::int64_t>(sequence)*11111111); }
}
int main()
{
#ifdef _WIN32
 _putenv_s("PCSX2_INSIDE","1");
#else
 setenv("PCSX2_INSIDE","1",1);
#endif
 // Upscale cap: the owner's headset grid (2592x2688) is larger than 3x a 640x512 canvas.
 const std::array<EyeSize,2> headset{EyeSize{2592,2688},EyeSize{2592,2688}};
 const std::array<EyeSize,2> small{EyeSize{1024,1024},EyeSize{1024,1024}};
 Check(ChooseUpscale(headset,640,512,4)==4,"cap 4 on a large grid selects 4x (unchanged)");
 Check(ChooseUpscale(headset,640,512,3)==3,"cap 3 selects 3x (unchanged)");
 Check(ChooseUpscale(headset,640,512,2)==2,"cap 2 selects 2x (was forced to 3x)");
 Check(ChooseUpscale(headset,640,512,1)==1,"cap 1 selects 1x");
 Check(ChooseUpscale(small,640,512,4)==3,"small grid keeps 3x (unchanged)");
 Check(ChooseUpscale(small,640,512,2)==2,"small grid honours cap 2");
 Check(ChooseUpscale(headset,640,512,9)==4 && ChooseUpscale(headset,640,512,0)==1,"cap clamps to 1..4");
 Check(ChooseUpscale(headset,0,512,2)==2 && ChooseUpscale(headset,0,512,4)==3,"invalid canvas: min(3,cap)");

 // SetRecommended reads PCSX2_INSIDE_MAX_UPSCALE (default 4).
 ResetResolution(); SetRecommended(headset);
 Check(ConsumeUpscale()==4,"default cap requests 4x");
 // A profile cap of 2 is requested, and re-requested while the GS still runs at 3x.
#ifdef _WIN32
 _putenv_s("PCSX2_INSIDE_MAX_UPSCALE","2");
#else
 setenv("PCSX2_INSIDE_MAX_UPSCALE","2",1);
#endif
 ResetResolution(); SetRecommended(headset);
 Check(ConsumeUpscale()==2,"cap 2 requests 2x");
 EnsureUpscale(3);
 Check(ConsumeUpscale()==2,"running at 3x re-requests the 2x cap (was: forced back to 3x)");

 // Synthetic pose: the view matrix is the rigid inverse of the eye pose.
 {
  const Packet p=Pose(1,0.35f);
  bool rigid=Valid(p);
  for(const Eye& e:p.eyes) {
   const auto& m=e.view_from_recenter; const float x=e.pose[0],y=e.pose[1],z=e.pose[2];
   const float vx=m[0]*x+m[4]*y+m[8]*z+m[12], vy=m[1]*x+m[5]*y+m[9]*z+m[13], vz=m[2]*x+m[6]*y+m[10]*z+m[14];
   rigid=rigid && std::abs(vx)<1e-5f && std::abs(vy)<1e-5f && std::abs(vz)<1e-5f;
  }
  Check(rigid,"synthetic eye position maps to the view origin");
 }

 // History: a latched pose can be found again by its sequence until 64 newer latches.
 Reset(); ResetTargets();
 for(std::uint64_t s=1;s<=10;s++) { Publish(Pose(s,0.01f*s)); Latch(); }
 Packet found;
 Check(Find(7,found) && found.sequence==7 && std::abs(found.eyes[0].pose[4]-std::sin(0.035f))<1e-6f,"Find returns the exact latched packet");
 Check(!Find(99,found) && !Find(0,found),"unknown sequence is not found");
 for(std::uint64_t s=11;s<=80;s++) { Publish(Pose(s,0)); Latch(); }
 Check(!Find(7,found) && Find(80,found),"history keeps the last 64 latches");

 // Double-buffered game: frame k draws buffer B with latch k while buffer A,
 // drawn at frame k-1 with latch k-1, is scanned out. The picture's pose is k-1.
 Reset(); ResetTargets();
 int bufA=0,bufB=0;
 Publish(Pose(101,0.1f)); Latch();               // latch for frame k-1
 NoteDraw(0,&bufA,Get().sequence); NoteDraw(1,&bufA,Get().sequence);
 Publish(Pose(102,0.2f)); Latch();               // latch for frame k
 NoteDraw(0,&bufB,Get().sequence); NoteDraw(1,&bufB,Get().sequence);
 NoteDisplayed(0,&bufA); NoteDisplayed(1,&bufA);  // scanout still shows A
 const TargetPose left=TakeDisplayed(0), right=TakeDisplayed(1);
 Check(Get().sequence==102 && left.last==101 && right.last==101,"scanned-out picture was drawn with the previous latch");
 Check(Find(left.last,found) && found.sequence==101,"publication uses the drawing pose, not the newest latch");
 // Showing A again without new draws is a repeat that keeps pose 101.
 const TargetPose again=TakeDisplayed(0);
 Check(again.draws==0 && again.last==101,"repeated scanout keeps its drawing pose");
 // A picture drawn across two latches is reported as mixed.
 NoteDraw(0,&bufB,103); NoteDisplayed(0,&bufB);
 const TargetPose mixed=TakeDisplayed(0);
 Check(mixed.first==102 && mixed.last==103 && mixed.draws==2,"picture drawn with two poses is detected");
 // Eyes are tracked separately.
 NoteDraw(1,&bufB,104); NoteDisplayed(1,&bufB);
 Check(TakeDisplayed(1).last==104,"right eye keeps its own record");
 // Null target or sequence 0 (no valid pose) records nothing.
 int bufC=0; NoteDraw(0,nullptr,5); NoteDraw(0,&bufC,0); NoteDisplayed(0,&bufC);
 Check(TakeDisplayed(0).last==0,"draws without a pose are not recorded");

 std::printf("%s: %d failure(s)\n",failures?"FAIL":"PASS",failures);
 return failures?1:0;
}
