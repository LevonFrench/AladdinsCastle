// SPDX-License-Identifier: GPL-3.0-or-later
// ACVRMG v4 reader and theatre client. MAME owns emulation, calibration and saves.
#include "acvr_mono_frames.h"
#include "BodyMotion.h"
#include "TheatreClient.h"
#include "TheatreRender.h"
#include "CleanQuit.h"
#include "Png.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>

using namespace acvr_publish;
using theatre::Mat4;
namespace fs=std::filesystem;

struct Options {
 DWORD pid=0;
 unsigned seconds=3600;
 std::string model,metadata,stop,proof;
 bool offline=false,script=false,crt=false,motion=false,body=false;
 double captureSeconds=36;
};
struct Snapshot {
 unsigned w=0,h=0,orientation=0;float aspect=4.f/3;
 std::uint64_t serial=0;double seconds=0;
 acvr_mono::gun_state gun{};
 std::vector<std::uint32_t> pixels;
};
class Wire {
 HANDLE mapping=nullptr,mutex=nullptr;
 acvr_mono::frame* shared=nullptr;
public:
 ~Wire(){if(shared)UnmapViewOfFile(shared);if(mapping)CloseHandle(mapping);if(mutex)CloseHandle(mutex);}
 bool connect(DWORD pid){
  if(shared)return true;
  char name[80],lock[96];acvr_mono::mapping_name(name,sizeof name,pid);std::snprintf(lock,sizeof lock,"%s-lock",name);
  if(!mapping)mapping=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,name);
  if(!mutex)mutex=OpenMutexA(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,lock);
  if(mapping && mutex)shared=static_cast<acvr_mono::frame*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(acvr_mono::frame)));
  return shared!=nullptr;
 }
 bool exchange(const acvr_mono::controls& input,const acvr_mono::motion* body,bool exit,Snapshot& out){
  if(!shared)return false;
  DWORD state=WaitForSingleObject(mutex,0);
  if(state!=WAIT_OBJECT_0 && state!=WAIT_ABANDONED)return false;
  struct Unlock{HANDLE m;~Unlock(){ReleaseMutex(m);}} unlock{mutex};
  // A dead writer might have left a partial frame. Skip abandoned ownership.
  if(state==WAIT_ABANDONED)return false;
  auto& f=*shared;
  if(f.tag!=acvr_mono::magic || f.version!=4)
   throw std::runtime_error("invalid ACVRMG v4 header");
  f.input[0]=input;
  if(body)f.body=*body;
  if(exit)f.exit_request=1;
  if(!f.w || !f.h || !f.serial)return false;
  if(f.w>1024 || f.h>1024 || f.stride!=f.w || !std::isfinite(f.physical_aspect) || f.physical_aspect<.1f || f.physical_aspect>10.f)
   throw std::runtime_error("invalid ACVRMG v4 picture extent");
  if(out.serial!=f.serial){
   out.w=f.w;out.h=f.h;out.aspect=f.physical_aspect;out.orientation=f.orientation;
   out.serial=f.serial;out.seconds=f.emulated_seconds;out.gun=f.gun[0];
   out.pixels.assign(f.pixels,f.pixels+std::size_t(f.w)*f.h);
  }
  return true;
 }
};

struct EyeTarget {
 GLuint fbo=0,depth=0;unsigned w=0,h=0;
 void bind(GLuint colour,unsigned width,unsigned height){
  if(!fbo)glGenFramebuffers(1,&fbo);
  if(!depth)tgl::GenRenderbuffers(1,&depth);
  if(w!=width || h!=height){tgl::BindRenderbuffer(GL_RENDERBUFFER,depth);tgl::RenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);w=width;h=height;}
  glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour,0);
  tgl::FramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
  if(tgl::CheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("theatre framebuffer incomplete");
 }
 void release(){glDeleteFramebuffers(1,&fbo);tgl::DeleteRenderbuffers(1,&depth);fbo=depth=0;}
};

class Viewer:public TheatreClient {
 Options options;
 Wire wire;
 Snapshot snapshot;
 theatre::Gun gun;
 theatre::Renderer renderer;
 theatre::EyeScene scene;
 GLuint native=0,prescaled=0,copyFbo[2]{};
 EyeTarget eyes[2];
 unsigned factor=0;
 std::uint64_t uploaded=0;
 bool placed=false,previousTrigger=false;
 std::atomic<bool> stopRequested{false};
 CleanQuit quit;
 body_motion::Mapper bodyMapper;
 acvr_mono::motion body{};
 FILE* trace=nullptr;
 unsigned lastButtons=~0u;
 bool initialized=false,quitLogged=false;
public:
 std::atomic<bool> exitSent{false};
 bool captured=false;
 unsigned pulses=0;
 std::uint64_t kickAt=0;   // last shot edge: the model's [motion.recoil] slide kicks back, holds, returns
 explicit Viewer(const Options& o):options(o){gun.load(o.model,o.metadata);}
 void request_stop(){stopRequested=true;}
 const Snapshot& picture()const{return snapshot;}
 const theatre::EyeScene& current_scene()const{return scene;}
 void initialize()override{
  if(!tgl::Load())throw std::runtime_error("OpenGL entry points unavailable");
  initialized=true;renderer.init(&gun);
  glGenTextures(1,&native);glGenTextures(1,&prescaled);glGenFramebuffers(2,copyFbo);
  scene.screen.width=3.2f;scene.screen.distance=2.5f;scene.screen.curve=0;scene.crt=options.crt;
  trace=std::fopen("viewer-input.tsv","w");
  if(trace)std::fprintf(trace,"serial\temulated_seconds\tu\tv\toffscreen\trequested_buttons\tapplied_buttons\tcross_u\tcross_v\n");
 }
 bool update(const TheatreFrame& frame)override{
  if(!wire.connect(options.pid))return false;
  if(frame.views.valid && (!placed || frame.recenter)){
   float centre[3]{};for(int i=0;i<3;++i)centre[i]=(frame.views.eyes[0].position[i]+frame.views.eyes[1].position[i])*.5f;
   const auto& q=frame.views.eyes[0].orientation;
   const float yaw=std::atan2(2*(q[0]*q[2]+q[3]*q[1]),1-2*(q[0]*q[0]+q[1]*q[1]));
   scene.screen.frame=Mat4::Translation(centre[0],centre[1],centre[2])*Mat4::Rotation(0,1,0,yaw);placed=true;
  }
  // keep a 4:3 picture 2.4 m tall; an open-frame (wider) picture adds width at the sides
  scene.screen.width=std::max(3.2f,2.4f*snapshot.aspect);scene.screen.height=scene.screen.width/snapshot.aspect;
  float u=.5f,v=.5f;bool on=false;
  Mat4 aim=Mat4::FromPose(frame.aim.position,frame.aim.orientation);
  if(frame.focused && frame.aim.valid){const float direction[3]={-aim.m[8],-aim.m[9],-aim.m[10]};on=scene.screen.hit(frame.aim.position,direction,u,v);}
  scene.showGun=!options.motion && frame.focused && frame.grip.valid && frame.aim.valid;
  scene.showDot=!options.motion && frame.focused && frame.aim.valid && on;scene.dotU=u;scene.dotV=v;scene.trigger=frame.trigger;
  if(scene.showGun)scene.gunRoot=gun.alignedRoot(Mat4::FromPose(frame.grip.position,frame.grip.orientation),aim,0);
  quit.poll(GetTickCount64(),frame.focused && frame.quitHeld,stopRequested || (!options.stop.empty() && fs::exists(options.stop)));
  // U4 has no drawn ring in this viewer: say so in the log once the hold outlives the recentre window.
  if(frame.quitProgress>0 && !quitLogged){std::fprintf(stderr,"viewer: both stick clicks held; quit and save in %.0f s if still held\n",3*(1-quit.progress(GetTickCount64())));quitLogged=true;}
  if(!frame.quitHeld)quitLogged=false;
  acvr_mono::controls input{};input.heartbeat_ms=GetTickCount64();
  input.valid=frame.focused && !quit.requested();input.off_screen=!on;input.u=std::clamp(u,0.f,1.f);input.v=std::clamp(v,0.f,1.f);
  input.trigger=input.valid && frame.aim.valid && frame.trigger>=.55f;
  input.button2=input.valid && frame.button2;input.start=input.valid && frame.start;input.coin=input.valid && frame.coin;
  if(input.trigger && !previousTrigger){++pulses;queue_shot_pulse();kickAt=GetTickCount64();}previousTrigger=input.trigger;
  {const float slideS=gun.recoilMs()/1000.f,since=(GetTickCount64()-kickAt)/1000.f;scene.recoil=kickAt && since<2*slideS?(since<slideS?1.f:2.f-since/slideS):0.f;}   // back, hold, return
  if(quit.requested()){input={};input.heartbeat_ms=GetTickCount64();}
  if(options.motion){
   // body controls: the aim pose gives the right hand's heading, the grip poses give height
   body_motion::Input in;
   if(frame.views.valid){in.head.valid=true;for(int i=0;i<3;++i)in.head.position[i]=(frame.views.eyes[0].position[i]+frame.views.eyes[1].position[i])*.5f;for(int i=0;i<4;++i)in.head.orientation[i]=frame.views.eyes[0].orientation[i];}
   if(frame.focused && frame.aim.valid){in.right.valid=true;for(int i=0;i<4;++i)in.right.orientation[i]=frame.aim.orientation[i];for(int i=0;i<3;++i)in.right.position[i]=frame.grip.valid?frame.grip.position[i]:frame.aim.position[i];}
   if(frame.focused && frame.leftGrip.valid){in.left=body_motion::Pose{};in.left.valid=true;for(int i=0;i<3;++i)in.left.position[i]=frame.leftGrip.position[i];for(int i=0;i<4;++i)in.left.orientation[i]=frame.leftGrip.orientation[i];}
   in.rightTrigger=frame.focused?frame.trigger:0;in.leftTrigger=frame.focused?frame.leftTrigger:0;in.seconds=GetTickCount64()/1000.0;
   for(int i=0;i<2;++i){in.leftStick[i]=frame.focused?frame.leftStick[i]:0;in.rightStick[i]=frame.focused?frame.rightStick[i]:0;}
   body={};body.heartbeat_ms=GetTickCount64();
   // thumbsticks and triggers by default; body motion (roll, tilt, lean, twist, pedalling) only with --body
   if(options.body)body.valid=input.valid && bodyMapper.Map(in,body.axis);
   else {bodyMapper.MapSticks(in,body.axis);body.valid=input.valid;}
   if(quit.requested())body.valid=0;
  }
  const bool complete=wire.exchange(input,options.motion?&body:nullptr,quit.requested(),snapshot);
  if(quit.requested() && complete)exitSent=true;
  const unsigned buttons=input.trigger|(input.button2<<1)|(input.start<<2)|(input.coin<<3);
  if(trace && snapshot.serial && (buttons!=lastButtons || snapshot.serial%60==0)){
   std::fprintf(trace,"%llu\t%.4f\t%.5f\t%.5f\t%u\t%u\t%u\t%.5f\t%.5f\n",(unsigned long long)snapshot.serial,snapshot.seconds,u,v,input.off_screen,buttons,snapshot.gun.buttons,snapshot.gun.cross_u,snapshot.gun.cross_v);std::fflush(trace);lastButtons=buttons;
  }
  if(complete){scene.screen.height=scene.screen.width/snapshot.aspect;upload(frame);}
  return uploaded!=0;
 }
 void upload(const TheatreFrame& frame){
  if(!snapshot.w || !snapshot.h)return;
  unsigned scale=8;
  const auto screenInverse=scene.screen.frame.RigidInverse();
  for(unsigned eye=0;eye<2;++eye){
   const auto& v=frame.views.eyes[eye];
   const float localZ=screenInverse.m[2]*v.position[0]+screenInverse.m[6]*v.position[1]+screenInverse.m[10]*v.position[2]+screenInverse.m[14];
   const float distance=std::max(.1f,std::abs(scene.screen.distance+localZ));
   float footprintX=frame.width[eye]*scene.screen.width/(distance*(std::tan(v.fov[1])-std::tan(v.fov[0])));
   float footprintY=frame.height[eye]*scene.screen.height/(distance*(std::tan(v.fov[2])-std::tan(v.fov[3])));
   scale=std::min(scale,unsigned(std::max(1.f,std::floor(std::min(footprintX/snapshot.w,footprintY/snapshot.h)))));
  }
  scale=std::clamp(scale,1u,8u);
  if(uploaded==snapshot.serial && factor==scale)return;
  glPixelStorei(GL_UNPACK_ALIGNMENT,4);glBindTexture(GL_TEXTURE_2D,native);
  glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,snapshot.w,snapshot.h,0,GL_BGRA,GL_UNSIGNED_BYTE,snapshot.pixels.data());
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D,prescaled);
  glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,snapshot.w*scale,snapshot.h*scale,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,copyFbo[0]);glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,native,0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,copyFbo[1]);glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,prescaled,0);
  if(tgl::CheckFramebufferStatus(GL_READ_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE || tgl::CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("prescale framebuffer incomplete");
  glEnable(GL_FRAMEBUFFER_SRGB);glBlitFramebuffer(0,0,snapshot.w,snapshot.h,0,0,snapshot.w*scale,snapshot.h*scale,GL_COLOR_BUFFER_BIT,GL_NEAREST);glDisable(GL_FRAMEBUFFER_SRGB);
  glBindTexture(GL_TEXTURE_2D,prescaled);tgl::GenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  scene.picture=prescaled;scene.pictureW=snapshot.w*scale;scene.pictureH=snapshot.h*scale;factor=scale;uploaded=snapshot.serial;
 }
 void draw_eye(unsigned eye,unsigned colour,const TheatreFrame& frame,bool srgb)override{
  eyes[eye].bind(colour,frame.width[eye],frame.height[eye]);
  if(srgb)glEnable(GL_FRAMEBUFFER_SRGB);else glDisable(GL_FRAMEBUFFER_SRGB);
  const auto& v=frame.views.eyes[eye];
  const auto projection=Mat4::Projection(std::tan(v.fov[0]),std::tan(v.fov[1]),std::tan(v.fov[3]),std::tan(v.fov[2]));
  renderer.draw(scene,projection,Mat4::FromPose(v.position,v.orientation).RigidInverse(),frame.width[eye],frame.height[eye],!srgb);
  glDisable(GL_FRAMEBUFFER_SRGB);
 }
 std::vector<std::uint32_t> readback(unsigned eye,const TheatreFrame& frame){
  const auto w=frame.width[eye],h=frame.height[eye];std::vector<std::uint32_t> raw(std::size_t(w)*h),top(raw.size());
  glPixelStorei(GL_PACK_ALIGNMENT,4);glReadPixels(0,0,w,h,GL_BGRA,GL_UNSIGNED_BYTE,raw.data());
  for(unsigned y=0;y<h;++y)std::copy_n(raw.data()+std::size_t(h-1-y)*w,w,top.data()+std::size_t(y)*w);
  return top;
 }
 void capture(const TheatreFrame& frame){
  fs::create_directories(options.proof);
  auto save=[&](std::string name,const std::vector<std::uint32_t>& pixels,unsigned w,unsigned h){if(!acvr_png::write((fs::path(options.proof)/name).string().c_str(),pixels.data(),w,h))throw std::runtime_error("proof write failed");};
  save("source.png",snapshot.pixels,snapshot.w,snapshot.h);
  auto original=scene;
  std::uint64_t nearCount[2]{},farCount[2]{},dotCount[2]{},recoilCount[2]{};
  for(unsigned eye=0;eye<2;++eye){
   GLuint colour=0;glGenTextures(1,&colour);glBindTexture(GL_TEXTURE_2D,colour);glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,frame.width[eye],frame.height[eye],0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
   scene=original;scene.showGun=scene.showDot=false;draw_eye(eye,colour,frame,true);auto picture=readback(eye,frame);save(eye?"right-picture.png":"left-picture.png",picture,frame.width[eye],frame.height[eye]);
   scene=original;scene.showDot=false;scene.recoil=0;draw_eye(eye,colour,frame,true);auto front=readback(eye,frame);save(eye?"right-front.png":"left-front.png",front,frame.width[eye],frame.height[eye]);
   scene.gunRoot.m[14]-=5;draw_eye(eye,colour,frame,true);auto behind=readback(eye,frame);save(eye?"right-behind.png":"left-behind.png",behind,frame.width[eye],frame.height[eye]);
   scene=original;scene.showGun=false;draw_eye(eye,colour,frame,true);auto dot=readback(eye,frame);save(eye?"right-dot.png":"left-dot.png",dot,frame.width[eye],frame.height[eye]);
   scene=original;scene.showDot=false;scene.recoil=1;draw_eye(eye,colour,frame,true);auto recoil=readback(eye,frame);save(eye?"right-recoil.png":"left-recoil.png",recoil,frame.width[eye],frame.height[eye]);
   for(std::size_t i=0;i<picture.size();++i){nearCount[eye]+=(front[i]!=picture[i]);recoilCount[eye]+=(recoil[i]!=front[i]);farCount[eye]+=(behind[i]!=picture[i]);dotCount[eye]+=(dot[i]!=picture[i]);}
   glDeleteTextures(1,&colour);
   if(!options.motion && (nearCount[eye]<100 || farCount[eye]!=0 || dotCount[eye]<4))throw std::runtime_error("gun/dot depth proof failed");
   if(!options.motion && gun.hasRecoil() && recoilCount[eye]==0)throw std::runtime_error("recoil proof failed: the slide did not move");
  }
  scene=original;glFinish();if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GL error during proof");
  if(GetModuleHandleA("openxr_loader.dll"))throw std::runtime_error("offline proof opened an XR loader");
  std::ofstream f(fs::path(options.proof)/"native-proof.json");
  f<<"{\n\"protocol\":4,\"serial\":"<<snapshot.serial<<",\"emulated_seconds\":"<<snapshot.seconds<<",\"native_width\":"<<snapshot.w<<",\"native_height\":"<<snapshot.h<<",\"physical_aspect\":"<<snapshot.aspect<<",\"orientation_already_applied\":"<<snapshot.orientation<<",\"integer_prescale\":"<<factor<<",\"aim_u\":"<<scene.dotU<<",\"aim_v\":"<<scene.dotV<<",\"mame_cross_u\":"<<snapshot.gun.cross_u<<",\"mame_cross_v\":"<<snapshot.gun.cross_v<<",\"left_near_pixels\":"<<nearCount[0]<<",\"right_near_pixels\":"<<nearCount[1]<<",\"behind_pixels\":0,\"dot_pixels\":["<<dotCount[0]<<","<<dotCount[1]<<"],\"recoil_pixels\":["<<recoilCount[0]<<","<<recoilCount[1]<<"],\"trigger_pulses\":"<<pulses<<",\"GL_errors\":0,\"OpenXR_started\":false,\"headset_tested\":false\n}\n";
  captured=true;
 }
 void shutdown()override{
  if(!initialized)return;
  if(trace){std::fclose(trace);trace=nullptr;}
  renderer.release();for(auto& eye:eyes)eye.release();glDeleteTextures(1,&native);glDeleteTextures(1,&prescaled);glDeleteFramebuffers(2,copyFbo);initialized=false;
 }
};

TheatreFrame synthetic(double seconds,bool script){
 TheatreFrame f;f.focused=true;f.views.valid=true;
 for(unsigned eye=0;eye<2;++eye){f.views.eyes[eye].position[0]=eye?.032f:-.032f;f.width[eye]=f.height[eye]=1440;auto& v=f.views.eyes[eye];v.fov[0]=v.fov[3]=-.785398163f;v.fov[1]=v.fov[2]=.785398163f;}
 f.aim.valid=f.grip.valid=true;f.aim.position[0]=.16f;f.aim.position[1]=-.28f;f.aim.position[2]=-.32f;
 float u=.5f,v=.5f;
 if(script){f.coin=seconds>=24 && seconds<25 && int(seconds*10)%2==0;f.start=seconds>=26 && seconds<27;
  f.trigger=seconds>=28 && seconds<33 && int(seconds*10)%3==0;f.button2=seconds>=30 && seconds<30.5;
  if(seconds>=31 && seconds<32){u=1.4f;f.trigger=1;}
 }
 const float target[3]={(u-.5f)*3.2f,(.5f-v)*2.4f,-2.5f};
 float d[3]={target[0]-f.aim.position[0],target[1]-f.aim.position[1],target[2]-f.aim.position[2]};float n=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);for(float& x:d)x/=n;
 float q[4]={d[1],-d[0],0,1-d[2]};n=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[3]*q[3]);for(unsigned i=0;i<4;++i)f.aim.orientation[i]=q[i]/n;f.grip=f.aim;
 return f;
}

int main(int argc,char** argv){
 Options o;
 try{
  for(int i=1;i<argc;++i){std::string a=argv[i];auto value=[&](){if(++i>=argc)throw std::runtime_error("missing option value");return std::string(argv[i]);};
   if(a=="--pid")o.pid=std::stoul(value());else if(a=="--model")o.model=value();else if(a=="--metadata")o.metadata=value();
   else if(a=="--seconds")o.seconds=std::stoul(value());else if(a=="--stop-file")o.stop=value();else if(a=="--offline-proof"){o.offline=true;o.proof=value();}
   else if(a=="--script")o.script=true;else if(a=="--motion")o.motion=true;else if(a=="--body")o.motion=o.body=true;else if(a=="--crt")o.crt=true;else if(a=="--capture-seconds")o.captureSeconds=std::stod(value());else throw std::runtime_error("unknown option: "+a);
  }
  if(!o.pid || o.model.empty() || o.metadata.empty() || !o.seconds || o.seconds>86400 || !std::isfinite(o.captureSeconds) || o.captureSeconds<1)throw std::runtime_error("required: --pid --model --metadata; bounded --seconds, optional --offline-proof directory --script");
  std::fprintf(stderr,"viewer: monitoring publisher %lu, offline=%d\n",o.pid,o.offline);std::fflush(stderr);
  HANDLE process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,o.pid);if(!process)throw std::runtime_error("cannot monitor publisher PID");
  std::fprintf(stderr,"viewer: creating hidden GL context\n");std::fflush(stderr);
  auto* window=Host_CreateWindow("ACVR gun theatre",0,0,16,16,Host_WINDOW_OPENGL|Host_WINDOW_HIDDEN);
  if(!window || !Host_GL_CreateContext(window) || !Host_LoadGL())throw std::runtime_error("hidden OpenGL context unavailable");
  std::fprintf(stderr,"viewer: loading local gun\n");std::fflush(stderr);
  Viewer client(o);bool started=false;int result=0;
  const auto begin=GetTickCount64();std::uint64_t exitAt=0;
  try{
   if(o.offline){
    if(GetModuleHandleA("openxr_loader.dll"))throw std::runtime_error("offline proof refuses a loaded XR loader");
    std::fprintf(stderr,"viewer: initializing offline renderer\n");std::fflush(stderr);client.initialize();
   }else {started=start_theatre(window,&client);if(!started)throw std::runtime_error("presenter start failed");}
   while(WaitForSingleObject(process,0)==WAIT_TIMEOUT){
    MSG msg;while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}
    if(GetTickCount64()-begin>=std::uint64_t(o.seconds)*1000)client.request_stop();
    if(o.offline){
     auto frame=synthetic(client.picture().seconds,o.script);const bool ready=client.update(frame);
     if(ready && !client.captured && client.picture().seconds>=o.captureSeconds){
      client.capture(frame);
      if(!o.stop.empty())std::ofstream(o.stop)<<"quit\n";else client.request_stop();
     }
    }else if(!theatre_running()){client.request_stop();result=1;break;}
    if(client.exitSent){if(!exitAt)exitAt=GetTickCount64();else if(GetTickCount64()-exitAt>15000){result=1;break;}}
    // An absent mapping cannot turn a bounded connection attempt into a hang.
    if(GetTickCount64()-begin>(std::uint64_t(o.seconds)+15)*1000){result=1;break;}
    Sleep(o.offline?1:10);
   }
   if(o.offline && !client.captured)result=1;
  }catch(...){client.request_stop();if(o.offline){auto neutral=TheatreFrame{};try{client.update(neutral);}catch(...){}}if(started)stop();else client.shutdown();Host_DestroyWindow(window);CloseHandle(process);throw;}
  if(started){if(!stop())result=1;}else client.shutdown();
  DWORD code=0;if(GetExitCodeProcess(process,&code) && code!=0)result=1;
  Host_DestroyWindow(window);CloseHandle(process);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"gun viewer: %s\n",e.what());return 1;}
}
