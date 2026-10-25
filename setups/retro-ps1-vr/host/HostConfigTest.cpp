// SPDX-License-Identifier: GPL-3.0-or-later
// Checks of the host's settings registry and profile reader. Takes the profile files to check as arguments:
//   g++ -std=c++17 -I host host/HostConfigTest.cpp -o HostConfigTest && HostConfigTest profiles/*.toml
// Needs no core, game, window or headset runtime.
#define NOMINMAX
#include "HostConfig.h"
#include <cstdio>

namespace {
int g_failed = 0;
void Check(bool ok, const std::string &what) { if (!ok) { ++g_failed; std::printf("FAIL: %s\n", what.c_str()); } }

void ClearEnvironment() { for (const auto &s : hostcfg::Registry()) hostcfg::SetEnv(s.name, ""); }
const hostcfg::Entry *Get(const hostcfg::Resolved &r, const std::string &name)
{
  for (const auto &e : r.entries) if (name == e.setting->name) return &e;
  return nullptr;
}
bool Throws(const std::string &text)
{
  try { hostcfg::LoadProfileText(text, "test.toml"); } catch (const std::runtime_error &) { return true; }
  return false;
}
std::string Message(const std::string &text)
{
  try { hostcfg::LoadProfileText(text, "test.toml"); } catch (const std::runtime_error &e) { return e.what(); }
  return "";
}
std::size_t Changed(const hostcfg::Resolved &r)
{
  return (std::size_t)std::count_if(r.entries.begin(), r.entries.end(), [](const hostcfg::Entry &e) { return e.changed; });
}
}

int main(int argc, char **argv)
{
  // The registry: unique names, ACVR_ prefix, a known number of values for the lists.
  std::set<std::string> names;
  for (const auto &s : hostcfg::Registry()) {
    Check(names.insert(s.name).second, std::string("duplicate registry name ") + s.name);
    Check(std::strncmp(s.name, "ACVR_", 5) == 0, std::string("registry name without ACVR_ ") + s.name);
    Check(s.type != hostcfg::Type::Floats || s.count != 0, std::string("Floats without a count ") + s.name);
  }

  // The reader accepts the subset and refuses the rest, with the line number.
  const std::string header = "schema = 1\n";
  Check(!Throws(header), "a minimal profile loads");
  Check(Throws(""), "schema is required");
  Check(Throws("schema = 2\n"), "schema 2 is refused");
  Check(Throws(header + "colour = \"red\"\n"), "unknown top-level key");
  Check(Throws(header + "[settings]\nACVR_NOT_A_SETTING = 1\n"), "unknown setting");
  Check(Message(header + "[settings]\nACVR_PS1_SB = 1\n").find("ACVR_PS1_SBS") != std::string::npos, "a typo names the closest setting");
  Check(Throws(header + "[settings]\nACVR_PS1_STOP_FILE = \"x\"\n"), "a path setting is refused");
  Check(Throws(header + "[settings]\nACVR_PS1_SBS = \"yes\"\n"), "a flag must be a flag");
  Check(Throws(header + "[settings]\nACVR_XR_DISTANCE = \"auto\"\n"), "auto only where the setting has it");
  Check(!Throws(header + "[settings]\nACVR_PS1_LETTERBOX = \"auto\"\n"), "auto where the setting has it");
  Check(Throws(header + "[settings]\nACVR_PS1_AIM = [1, 0, 1]\n"), "a list with too few numbers");
  Check(!Throws(header + "[settings]\nACVR_PS1_AIM = [1.0, -0.0114, 1, 0.0156]\n"), "a list of four numbers");
  Check(Throws(header + "[settings]\nACVR_PS1_SBS = 1\nACVR_PS1_SBS = 0\n"), "duplicate key");
  Check(Throws(header + "[settings]\n[settings]\n"), "duplicate table");
  Check(Throws(header + "a.b = 1\n"), "dotted key");
  Check(Throws(header + "[[settings]]\n"), "array of tables");
  Check(Throws(header + "notes = \"\"\"two\nlines\"\"\"\n"), "multi-line string");
  Check(Throws(header + "notes = \"unterminated\n"), "unterminated string");
  Check(Throws(header + "notes = enabled\n"), "a bare word is not a value");
  Check(Throws(header + "[core_options]\nx = true\n"), "a core option must be text or a number");
  Check(Throws(header + "system = \"psp\"\n"), "unknown system");
  Check(Throws(header + "default_view = \"nope\"\n"), "default_view must name a view");
  Check(Throws(header + "[views.a]\ncolour = \"red\"\n"), "unknown view key");
  Check(Message(header + "[views.a]\nlabel = \"x\" junk\n").find("test.toml:3:") == 0, "errors carry file:line");
  {
    const auto p = hostcfg::LoadProfileText("schema = 1\r\n# comment\r\nname = 'a # b'  # trailing\r\n[pad]\r\nmap = { rtrig = \"a\", lclick = \"start\" }\r\n", "crlf.toml");
    Check(p.name == "a # b", "a literal string keeps its # and CRLF lines read");
    Check(p.base.settings.count("ACVR_PAD_MAP") && p.base.settings.at("ACVR_PAD_MAP") == "rtrig=a,lclick=start", "an inline pad map keeps its order");
  }
  Check(!hostcfg::LoadProfileText(header + "system = \"3ds\"\n[settings]\nACVR_PS1_AIM = [1,0,1,0]\n", "w.toml").warnings.empty(), "a setting that does not apply to the system warns");

  // Each profile file loads, and the Star Fox one gives the settings of the documented environment path.
  for (int i = 1; i < argc; ++i) {
    hostcfg::Profile p;
    try { p = hostcfg::LoadProfile(argv[i]); } catch (const std::exception &e) { Check(false, std::string(argv[i]) + ": " + e.what()); continue; }
    Check(p.warnings.empty(), std::string(argv[i]) + " has settings that do not apply to its system");
    ClearEnvironment();
    std::map<std::string, std::string> options;
    hostcfg::Resolved r = hostcfg::Resolve(&p, "", options);
    std::printf("%s: %zu settings set, %zu core options\n", argv[i], Changed(r), options.size());
    if (p.name == "Star Fox 64 3D") {
      Check(options["citra_layout_option"] == "single_screen" && options["citra_render_3d"] == "side-by-side-full" &&
            options["citra_factor_3d"] == "40" && options["citra_resolution_factor"] == "3" && options.size() == 4, "star fox core options");
      Check(Get(r, "ACVR_PS1_SBS")->value == "1" && Get(r, "ACVR_PS1_SBS")->source == "profile", "star fox side by side");
      Check(!Get(r, "ACVR_PS1_LETTERBOX")->changed, "star fox letterbox stays automatic");
      Check(Get(r, "ACVR_PAD_MAP")->value == "rtrig=a,ra=y,rgrip=x,lgrip=b,ltrig=l,rb=r,rleft=l,rright=r,lclick=start", "star fox pad map");
      Check(p.controls == "pad" && r.view == "theatre", "star fox controls and default view");
      Check(std::getenv("ACVR_PS1_SBS") && std::string(std::getenv("ACVR_PS1_SBS")) == "1", "the profile value reached the environment");
      // The environment wins, the command line wins over both.
      ClearEnvironment(); hostcfg::SetEnv("ACVR_PS1_SBS", "0"); hostcfg::SetEnv("ACVR_PS1_LETTERBOX", "0.2");
      std::map<std::string, std::string> cli{{"citra_factor_3d", "70"}};
      r = hostcfg::Resolve(&p, "", cli);
      Check(Get(r, "ACVR_PS1_SBS")->value == "0" && Get(r, "ACVR_PS1_SBS")->source.find("environment") == 0, "environment over profile");
      Check(Get(r, "ACVR_PS1_LETTERBOX")->value == "0.2", "environment sets what the profile leaves automatic");
      Check(cli["citra_factor_3d"] == "70" && cli["citra_resolution_factor"] == "3", "command line option over profile");
      // A view layers over the base.
      ClearEnvironment(); options.clear();
      r = hostcfg::Resolve(&p, "swap-eyes", options);
      Check(Get(r, "ACVR_PS1_SWAP_EYES")->value == "1" && Get(r, "ACVR_PS1_SBS")->value == "1", "a view adds to the base");
      bool refused = false;
      try { hostcfg::Resolve(&p, "nope", options); } catch (const std::runtime_error &) { refused = true; }
      Check(refused, "an unknown view is refused");
    }
  }

  // No profile: nothing is set.
  ClearEnvironment();
  std::map<std::string, std::string> none;
  hostcfg::Resolved r = hostcfg::Resolve(nullptr, "", none);
  Check(none.empty() && Changed(r) == 0, "with no profile and no environment every setting is at its default");
  hostcfg::SetEnv("ACVR_PS1_SB", "1"); hostcfg::SetEnv("ACVR_XR_DISTANCE", "far"); hostcfg::SetEnv("ACVR_M2_STEREO", "1");
  r = hostcfg::Resolve(nullptr, "", none);
  bool typo = false, bad = false, other = false;
  for (const auto &w : r.warnings) {
    typo = typo || w.find("ACVR_PS1_SB ") == 0;
    bad = bad || w.find("ACVR_XR_DISTANCE=far") == 0;
    other = other || w.find("ACVR_M2_STEREO") != std::string::npos;
  }
  Check(typo, "an unknown ACVR_ variable is warned about");
  Check(bad, "a value of the wrong type is warned about");
  Check(!other, "another program's variable is not");
  hostcfg::SetEnv("ACVR_PS1_SB", ""); hostcfg::SetEnv("ACVR_XR_DISTANCE", ""); hostcfg::SetEnv("ACVR_M2_STEREO", "");

  std::printf(g_failed ? "%d checks failed\n" : "all checks passed\n", g_failed);
  return g_failed ? 1 : 0;
}
