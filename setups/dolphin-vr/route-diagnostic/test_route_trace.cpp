// Copyright 2026 AladdinsCastle contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ACVRRouteTrace.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::string Read(const std::filesystem::path& path)
{ std::ifstream file(path, std::ios::binary); return {std::istreambuf_iterator<char>(file),{}}; }
struct CommaDecimal : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
std::vector<std::string> Split(const std::string& csv)
{
  std::vector<std::string> fields;std::istringstream input(csv);std::string field;
  while(std::getline(input,field,',')) fields.push_back(field);
  return fields;
}

int main(int argc, char** argv)
{
  try
  {
    Require(argc == 2,"Synthetic output directory required");
    const auto base=std::filesystem::path(argv[1]);
    std::filesystem::create_directories(base);
    const auto root=base/("run-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root),"Fresh synthetic run directory");
    // Only generated numeric fixtures; these have no game geometry or game identity.
    ACVRRouteTrace::Sample s;
    s.active=true; s.virtual_screen=true; s.panes=true;
    s.route=1; s.projection_type=0;
    Require(std::string(ACVRRouteTrace::Reason(s))=="perspective-world","World route");
    s.route=-1; s.projection_type=1;
    Require(std::string(ACVRRouteTrace::Reason(s))=="ortho-screen","Ortho plane route");
    s.projection_type=0;s.viewport_class=3;
    Require(std::string(ACVRRouteTrace::Reason(s))=="small-pane-screen","Small viewport screen reason");
    s.panes=false;
    Require(std::string(ACVRRouteTrace::Reason(s))=="unclassified","Do not invent a reason from route alone");
    s.detect_targets=true;s.viewport_class=4;s.route=0;
    Require(std::string(ACVRRouteTrace::Reason(s))=="preserved-target","Render target preserved");
    s.viewport_class=5;
    Require(std::string(ACVRRouteTrace::Reason(s))=="preserved-target","Offscreen preserved");
    s.explicit_override=-2;
    Require(std::string(ACVRRouteTrace::Reason(s))=="explicit-per-draw-request","Override request is separate from final route");
    s.auto_efb_stage=2;
    Require(std::string(ACVRRouteTrace::Reason(s))=="auto-efb-native-request","Automatic effect request is distinguished from override");
    s.flat=true;
    Require(std::string(ACVRRouteTrace::Reason(s))=="flat","Flat precedence");
    s.active=false;
    Require(std::string(ACVRRouteTrace::Reason(s))=="inactive","Stale uniforms do not prove an active route");
    s={};s.active=true;s.pane_override=true;
    s.projection_type=1;s.route=-1;s.frame[0]=1;
    Require(std::string(ACVRRouteTrace::Reason(s))=="pane-request-not-applied","Ortho draw cannot apply perspective pane");
    s.projection_type=0;s.route=1;s.frame[0]=0;
    Require(std::string(ACVRRouteTrace::Reason(s))=="pane-request-not-applied","Invalid frame cannot apply pane");
    s.frame[0]=1;s.route=2;
    Require(std::string(ACVRRouteTrace::Reason(s))=="pane-route-with-request","Valid perspective pane applied");
    s.route=3;
    Require(std::string(ACVRRouteTrace::Reason(s))=="pane-route-with-request","VR-depth perspective pane applied");
    s.route=1;
    Require(std::string(ACVRRouteTrace::Reason(s))=="pane-request-not-applied","Request alone does not prove applied route");
    s={};s.draw=7;s.display_aspect=1.25f;
    s.frame={1,0,0,320,256};
    s.projection={1.5f,0.2f,2.3f,0.1f,-0.01f,-0.5f};
    s.textures[2]={1,1,0,160,128,2,1};s.used_texture_mask=4;
    s.eye_rows[3]=std::numeric_limits<float>::infinity();
    const auto previous=std::locale::global(std::locale(std::locale::classic(),new CommaDecimal));
    const auto header=ACVRRouteTrace::Columns();
    const auto row=ACVRRouteTrace::Serialize(s,31,1500.25);
    std::locale::global(previous);
    Require(std::count(header.begin(),header.end(),',')==std::count(row.begin(),row.end(),','),"Stable CSV arity under comma locale");
    Require(row.find("1500.25")!=std::string::npos&&row.find("1.25")!=std::string::npos,"Invariant decimal encoding");
    Require(row.find(",NA,")!=std::string::npos&&row.find("inf")==std::string::npos&&row.find("nan")==std::string::npos,"Non-finite values explicitly unavailable");
    Require(row.find(",0,-1,-1,-1,-1,-1,-1")!=std::string::npos,"Absent texture provenance unavailable");
    for (bool flat : {false,true})
    {
      auto masked=s;masked.active=true;masked.flat=flat;masked.route=flat ? 1.0f : 0.0f;
      masked.effective_units_per_meter=123;masked.screen.fill(123);masked.depth.fill(123);masked.eye_rows.fill(123);
      const auto names=Split(header),values=Split(ACVRRouteTrace::Serialize(masked,0,0));
      for(std::size_t i=0;i<names.size();++i)
        if(names[i].starts_with("cached_")) Require(values[i]=="NA","Flat/native routes suppress unused VR tables");
    }
    {
      ACVRRouteTrace::Logger off(std::filesystem::path{});
      Require(!off.IsSampling()&&!off.Record(s),"Opt-out inert");
    }
    const auto rows_path=root/"rows.csv";
    {
      ACVRRouteTrace::Logger logger(rows_path,{2,2,1024*1024,0,600});
      Require(logger.Record(s),"First sampled frame");
      logger.EndFrame();Require(!logger.Record(s),"Interval skips intervening frame");
      logger.EndFrame();Require(logger.Record(s),"Second sampled frame");
      Require(!logger.IsSampling()&&!logger.Record(s),"Row cap closes logger");
    }
    const auto rows=Read(rows_path);
    Require(std::count(rows.begin(),rows.end(),'\n')==3,"Header plus exactly two rows");
    const auto bytes_path=root/"bytes.csv";
    {
      ACVRRouteTrace::Logger logger(bytes_path,{1,20,static_cast<std::uint64_t>(header.size()+10),0,600});
      Require(!logger.Record(s)&&!logger.IsSampling(),"Byte cap stops before partial row");
    }
    Require(Read(bytes_path)==header,"Byte cap preserves complete CSV header");
    const auto invalid=root/"invalid.csv";
    {
      ACVRRouteTrace::Logger logger(invalid,{0,10,1024*1024,0,600});
      Require(!logger.IsSampling()&&!logger.Record(s),"Invalid bounds fail closed");
    }
    Require(!std::filesystem::exists(invalid),"Invalid bounds cause no file creation");
    const auto sentinel=root/"existing.csv";
    { std::ofstream output(sentinel,std::ios::binary);output<<"synthetic sentinel\n"; }
    {
      ACVRRouteTrace::Logger logger(sentinel);
      Require(!logger.IsSampling()&&!logger.Record(s),"Existing destination fails closed");
    }
    Require(Read(sentinel)=="synthetic sentinel\n","Exclusive create preserves earlier evidence");
    {
      ACVRRouteTrace::Logger logger(root/"delayed.csv",{1,10,1024*1024,600,600});
      Require(!logger.Record(s),"Delayed capture emits no premature row");
    }
    {
      ACVRRouteTrace::Logger logger(root/"expired.csv",{1,10,1024*1024,0,1e-12});
      Require(!logger.IsSampling()&&!logger.Record(s),"Time bound stops without sleep");
    }
    std::cout<<"Synthetic route decisions, CSV and bounded opt-in checks passed\n";
    return 0;
  }
  catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
