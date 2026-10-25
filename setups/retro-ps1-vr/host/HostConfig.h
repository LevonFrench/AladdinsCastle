// SPDX-License-Identifier: GPL-3.0-or-later
// Host configuration for acvr-ps1: one registry of every ACVR_* setting the program (and the cores we patch) reads,
// and per-game profiles (--profile <file.toml> [--view <name>]).
//
// Precedence, lowest to highest: the code's own default, the profile's base tables, the profile's chosen view,
// the environment (ACVR_*), the command line (--option, --controls, --gun, --port). A profile never writes over
// an environment variable that is already set (empty counts as unset, as it does everywhere in the host), and
// running with no profile changes nothing.
//
// How a profile reaches the code: the host reads its settings with getenv in many places. Rather than touch each,
// the resolved profile values are put into this process's environment (_putenv) before the core loads, for the
// names the environment does not already set. The registry therefore lists the names, types and where each
// applies, validates what a profile or the environment gives, and logs the result; the defaults column documents
// the default the code uses at the read site (it does not drive it yet).
//
// Profile files are TOML, read by a small strict reader (below). The host has no TOML dependency: the Hub's toml++
// is a FetchContent library built with CMake, and this program is one g++ command line. The reader takes the subset
// the profiles use and refuses anything else with the line number: [a.b] tables, key = value, "strings" and
// 'literal strings' on one line, integers, floats, true/false, [one-line arrays of scalars], { one-line inline
// tables }, # comments. Not supported: dotted keys, multi-line strings or arrays, [[arrays of tables]], dates.
// Unknown keys in a profile are errors, so a typo fails loudly instead of running untuned.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace hostcfg {

enum class Type { Flag, Float, Int, Text, Floats, Path };
// Systems a setting applies to (the cores the host runs): PlayStation, Saturn, Nintendo 64, Nintendo 3DS.
enum : unsigned { kPs1 = 1, kSaturn = 2, kN64 = 4, k3ds = 8, kAll = 15 };

struct Setting {
  const char *name;
  Type type;
  int count;           // Floats: the number of values (negative: 1 up to that many)
  const char *def;     // the default the code uses, as text
  unsigned systems;
  bool autoOk;         // "auto" is a value: the host works it out (and a view can put it back)
  bool core;           // read by a patched core from the environment; the host only passes it on
  const char *doc;
};

// Path settings are machine facts (a folder or file on one PC): a profile may not set them.
inline const std::vector<Setting> &Registry()
{
  static const std::vector<Setting> r = {
    // Theatre screen
    {"ACVR_XR_DISTANCE",     Type::Float, 0, "2.5",   kAll, false, false, "metres to the screen"},
    {"ACVR_PS1_SCREEN_W",    Type::Float, 0, "2.4",   kAll, false, false, "screen width in metres"},
    {"ACVR_PS1_CURVE",       Type::Float, 0, "1",     kAll, false, false, "1 curves around you, 0 is flat"},
    {"ACVR_PS1_LIFT",        Type::Float, 0, "0",     kAll, false, false, "screen centre height above your eyes, metres"},
    {"ACVR_PS1_CRT",         Type::Flag,  0, "0",     kAll, false, false, "1 adds scanlines"},
    {"ACVR_PS1_SBS",         Type::Flag,  0, "0",     kPs1 | kSaturn | k3ds, false, false, "1 when the core draws both eyes side by side in one picture"},
    {"ACVR_PS1_SWAP_EYES",   Type::Flag,  0, "0",     kAll, false, false, "1 swaps the two eyes' halves"},
    {"ACVR_PS1_LETTERBOX",   Type::Float, 0, "auto",  k3ds, true,  false, "share of rows cut top and bottom of each side-by-side eye; auto reads it from the core's frame"},
    {"ACVR_PS1_3DS_TOUCH",   Type::Flag,  0, "0",     k3ds, false, false, "1 shows the 3DS bottom screen as a touch panel; the right controller's ray is the stylus (needs ACVR_PS1_3DS_TOP and ACVR_PS1_3DS_BOTTOM)"},
    {"ACVR_PS1_3DS_TOP",     Type::Floats, 4, "none", k3ds, false, false, "the top screen's rectangle of the core's frame, as shares of its width and height: x, y, w, h from the top left (both eyes when side by side)"},
    {"ACVR_PS1_3DS_BOTTOM",  Type::Floats, 4, "none", k3ds, false, false, "the bottom screen's rectangle of the core's frame, same units; also where the stylus is sent"},
    {"ACVR_PS1_3DS_STYLUS",  Type::Text,  0, "rtrig", k3ds, false, false, "the VR control that puts the stylus down while the ray is on the panel (rtrig, ra, rb, rgrip, lgrip, ...)"},
    {"ACVR_PS1_3DS_PANEL",   Type::Floats, 3, "0.8,1,-0.55", k3ds, false, false, "the touch panel's width, its distance ahead and its height against your eyes, metres"},
    {"ACVR_PS1_IPD",        Type::Float, 0, "0.064", kPs1 | kSaturn | kN64, false, false, "eye separation in metres (GL stereo and the patched cores)"},
    {"ACVR_SCREEN_FRAME",    Type::Flag,  0, "1 (0 in a wide view)", kAll, true, false, "the frame round the screen"},
    {"ACVR_SCREEN_FADE",     Type::Float, 0, "0 (0.06 in a wide view)", kAll, true, false, "edges fading into the dark"},
    {"ACVR_PS1_STOP_FILE",   Type::Path,  0, "none",  kAll, false, false, "creating this file asks the game to quit and save (set by the Hub)"},
    {"ACVR_RAM_WATCH",       Type::Text,  0, "none",  kPs1 | kSaturn | kN64, false, false, "console memory words (hex, comma separated) logged at each offline snapshot"},
    // Gun games
    {"ACVR_PS1_AIM",         Type::Floats, 4, "1,0,1,0", kPs1 | kSaturn, false, false, "aim trim: x scale, x shift, y scale, y shift"},
    {"ACVR_PS1_FACE_BUTTONS", Type::Flag, 0, "0",     kPs1 | kSaturn, false, false, "pad face buttons on the controllers, gun buttons on the grips"},
    {"ACVR_PS1_HIDE_FLASH",  Type::Flag,  0, "1",     kPs1 | kSaturn, false, false, "keep the games' per-shot white flash frames out of the headset"},
    {"ACVR_PS1_FLASH_CHANNELS", Type::Text, 0, "rgb", kPs1 | kSaturn, false, false, "colour channels that must be bright for a flash (r, g, b)"},
    {"ACVR_PS1_FLASH_SHARE", Type::Int,   0, "85",    kPs1 | kSaturn, false, false, "percent of bright samples that makes a flash (50 to 100)"},
    {"ACVR_PS1_CHEAT_FILE",  Type::Path,  0, "none",  kPs1 | kSaturn, false, false, "a DuckStation or RetroArch .cht file"},
    {"ACVR_PS1_CHEATS",      Type::Text,  0, "none",  kPs1 | kSaturn, false, false, "the entries of that file to enable, separated by ;"},
    {"ACVR_PS1_GUN_MODEL",   Type::Path,  0, "none",  kPs1 | kSaturn, false, false, "the gun's .glb"},
    {"ACVR_PS1_GUN_META",    Type::Path,  0, "none",  kPs1 | kSaturn, false, false, "the gun's .toml"},
    // Pad modes (--controls pad|wheel|aim)
    {"ACVR_PAD_MAP",         Type::Text,  0, "per mode", kAll, false, false, "VR control to RetroPad button map, rtrig=a,ra=y,..."},
    {"ACVR_PAD_WHEEL_DEG",   Type::Float, 0, "75",    kN64, false, false, "roll for full lock, degrees (roll steering only)"},
    {"ACVR_N64_ROLL_STEER",  Type::Flag,  0, "0",     kN64, false, false, "1 steers by rolling the controller (off by default: the left stick steers)"},
    {"ACVR_PAD_ROLL_STEER",  Type::Flag,  0, "0",     kN64, false, false, "same as ACVR_N64_ROLL_STEER"},
    {"ACVR_PAD_AIM_HOLD",    Type::Text,  0, "rgrip", kN64, false, false, "VR control that enters aim mode, or always"},
    {"ACVR_PAD_AIM_BUTTON",  Type::Text,  0, "r",     kN64, false, false, "the game's aim-mode RetroPad button, or none"},
    {"ACVR_PAD_AIM",         Type::Floats, 4, "1,0,1,0", kN64, false, false, "aim trim: x scale, x shift, y scale, y shift"},
    {"ACVR_PAD_AIM_CURVE",   Type::Float, 0, "1",     kN64, false, false, "stick = |stick| ^ curve"},
    {"ACVR_PAD_AIM_DOT",     Type::Flag,  0, "0",     kAll, false, false, "1 shows the aim dot in pad modes"},
    {"ACVR_PAD_AIM_ON",      Type::Text,  0, "hud",   kN64, false, false, "world: aim at the 3D world surface instead of the HUD panel"},
    {"ACVR_PAD_FOLLOW_U",    Type::Text,  0, "none",  kN64, false, false, "the game's marker, horizontal: c0,addr:k,addr:k"},
    {"ACVR_PAD_FOLLOW_V",    Type::Text,  0, "none",  kN64, false, false, "the game's marker, vertical: c0,addr:k,addr:k"},
    {"ACVR_PAD_FOLLOW_GAIN", Type::Floats, 6, "6,40,6,40,0,0", kN64, false, false, "marker follow gains: px,dx,py,dy,ix,iy"},
    // GL stereo and HUD layer (cores drawing with OpenGL)
    {"ACVR_GL_STEREO",       Type::Flag,  0, "0",     kPs1 | kN64, false, false, "1 draws the core's pictures twice, one per eye"},
    {"ACVR_GL_STEREO_CONV",  Type::Float, 0, "auto",  kPs1 | kN64, true,  false, "clip w on the screen plane"},
    {"ACVR_GL_STEREO_AUTO",  Type::Float, 0, "0.3",   kPs1 | kN64, false, false, "percentile of w used for the automatic zero plane"},
    {"ACVR_GL_STEREO_DEPTH", Type::Float, 0, "1",     kPs1 | kN64, false, false, "multiplier on the stereo effect"},
    {"ACVR_GL_STEREO_POPOUT", Type::Float, 0, "2",    kPs1 | kN64, false, false, "how far in front of the screen things may come"},
    {"ACVR_GL_STEREO_BACKDROP", Type::Flag, 0, "1",   kPs1 | kN64, false, false, "2D drawn before the 3D goes to infinity"},
    {"ACVR_GL_STEREO_STATS", Type::Flag,  0, "0",     kPs1 | kN64, false, false, "1 logs a histogram of w"},
    {"ACVR_GL_3D_NO_DEPTH",  Type::Flag,  0, "0",     kPs1 | kN64, false, false, "1 counts triangle draws as 3D for games that skip the depth test"},
    {"ACVR_GL_HUD",          Type::Flag,  0, "0",     kPs1 | kN64, false, false, "1 splits the game's 2D overlay onto its own panel"},
    {"ACVR_GL_WIDE",         Type::Floats, -2, "1,1", kPs1 | kN64, false, false, "projection squeeze kx,ky about the picture's centre (1 = off)"},
    {"ACVR_GL_WIDE_SQUEEZE", Type::Flag,  0, "1",     kPs1 | kN64, false, false, "0 when the game itself draws the wide view"},
    {"ACVR_GL_TRACE",        Type::Floats, 2, "off",  kPs1 | kN64, false, false, "first frame,frames to trace"},
    {"ACVR_GL_TRACE_DIR",    Type::Path,  0, "none",  kPs1 | kN64, false, false, "where the trace saves vertex shaders"},
    {"ACVR_GL_TRACE_DRAWS",  Type::Int,   0, "60",    kPs1 | kN64, false, false, "draws logged per traced frame"},
    {"ACVR_HUD_DISTANCE",    Type::Float, 0, "1.8",   kPs1 | kN64, false, false, "HUD panel distance, metres"},
    {"ACVR_HUD_WIDTH",       Type::Float, 0, "auto",  kPs1 | kN64, true,  false, "HUD panel width, metres (auto: the game's view angle)"},
    {"ACVR_HUD_SCALE",       Type::Float, 0, "1",     kPs1 | kN64, false, false, "multiplier on the automatic HUD width"},
    {"ACVR_HUD_LIFT",        Type::Float, 0, "-0.08", kPs1 | kN64, false, false, "HUD panel height against the screen, metres"},
    {"ACVR_HUD_OPACITY",     Type::Float, 0, "1",     kPs1 | kN64, false, false, "HUD panel opacity"},
    {"ACVR_HUD_KEY_BLACK",   Type::Flag,  0, "0",     kPs1 | kN64, false, false, "1 shows pure black HUD pixels as transparent"},
    // Read by the patched cores (swanstation-stereo.patch, beetle-saturn-depth.patch)
    {"ACVR_PS1_STEREO",      Type::Flag,  0, "0",     kPs1, false, true, "SwanStation stereo patch on"},
    {"ACVR_PS1_STEREO_CONV", Type::Float, 0, "auto",  kPs1, true,  true, "PGXP depth w on the screen"},
    {"ACVR_PS1_STEREO_AUTO", Type::Float, 0, "0.3",   kPs1, false, true, "percentile for the automatic zero plane"},
    {"ACVR_PS1_STEREO_DEPTH", Type::Float, 0, "1",    kPs1, false, true, "multiplier on the effect"},
    {"ACVR_PS1_STEREO_POPOUT", Type::Float, 0, "2",   kPs1, false, true, "how far in front of the screen things may come"},
    {"ACVR_PS1_STEREO_STATS", Type::Flag, 0, "0",     kPs1, false, true, "1 logs a histogram of w"},
    {"ACVR_PS1_DIORAMA",     Type::Flag,  0, "0",     kPs1, false, true, "depth from draw order for sprite games (prototype)"},
    {"ACVR_PS1_DIORAMA_FAR", Type::Float, 0, "0.6",   kPs1, false, true, "diorama far shift"},
    {"ACVR_PS1_DIORAMA_NEAR", Type::Float, 0, "-0.6", kPs1, false, true, "diorama near shift"},
    {"ACVR_SAT_STEREO",      Type::Flag,  0, "0",     kSaturn, false, true, "Beetle Saturn depth experiment on"},
    {"ACVR_SAT_STEREO_FAR",  Type::Float, 0, "1",     kSaturn, false, true, "far limit"},
    {"ACVR_SAT_STEREO_POPOUT", Type::Float, 0, "0.3", kSaturn, false, true, "pop-out limit"},
    {"ACVR_SAT_STEREO_FLOOR", Type::Float, 0, "0.15", kSaturn, false, true, "depth at the bottom for rotating floors"},
    {"ACVR_SAT_STEREO_DEPTH", Type::Text, 0, "order", kSaturn, false, true, "size: rank by screen size instead of draw order"},
    {"ACVR_SAT_STEREO_SCALE", Type::Float, 0, "1",    kSaturn, false, true, "multiplier on the effect"},
    {"ACVR_SAT_STEREO_DEBUG", Type::Flag, 0, "0",     kSaturn, false, true, "1 shows the depth map as the right eye"},
  };
  return r;
}

inline std::string Upper(std::string s) { for (auto &c : s) c = (char)std::toupper((unsigned char)c); return s; }

inline const Setting *Find(const std::string &name)
{
  for (const auto &s : Registry()) if (name == s.name) return &s;
  return nullptr;
}

// ACVR_* names that belong to the other ACVR programs (Model 2 and 3 viewers, Wii, the Hub, the tools): the host
// does not read them and does not warn about them.
inline bool OwnedElsewhere(const std::string &name)
{
  static const char *prefixes[] = {"ACVR_M2_", "ACVR_S22_", "ACVR_WII_", "ACVR_GUN_ROOM_", "ACVR_OPENXR_", "ACVR_VR_"};
  static const char *exact[] = {"ACVR_CONTROLS", "ACVR_CONVERGENCE", "ACVR_EYE_SEP", "ACVR_FOV_SCALE", "ACVR_INPUT_SCRIPT", "ACVR_NO_CULL",
                                "ACVR_STICK_GRAB", "ACVR_WHEEL_GRAB", "ACVR_STEREO", "ACVR_VR", "ACVR_WORLD_SCALE", "ACVR_XR",
                                "ACVR_XR_HEIGHT", "ACVR_XR_SIZE", "ACVR_LEASE_DIR", "ACVR_HEADSET_MARKER", "ACVR_CAPTURE_DIR", "ACVR_CAPTURE_FRAME"};
  for (const char *p : prefixes) if (name.compare(0, std::strlen(p), p) == 0) return true;
  for (const char *e : exact) if (name == e) return true;
  return false;
}

inline int Distance(const std::string &a, const std::string &b)
{
  std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
  for (std::size_t j = 0; j <= b.size(); ++j) prev[j] = (int)j;
  for (std::size_t i = 1; i <= a.size(); ++i) {
    cur[0] = (int)i;
    for (std::size_t j = 1; j <= b.size(); ++j) cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
    prev.swap(cur);
  }
  return prev[b.size()];
}
inline std::string Closest(const std::string &name)
{
  int best = 4; const char *found = nullptr; const std::string up = Upper(name);
  for (const auto &s : Registry()) { const int d = Distance(up, s.name); if (d < best) { best = d; found = s.name; } }
  return found ? found : "";
}

inline std::string Num(double v)
{
  char b[48];
  if (std::fabs(v) < 1e15 && v == std::floor(v)) std::snprintf(b, sizeof b, "%.0f", v); else std::snprintf(b, sizeof b, "%.9g", v);
  return b;
}
inline bool IsNumber(const std::string &s, double *out = nullptr)
{
  if (s.empty()) return false;
  char *end = nullptr; const double v = std::strtod(s.c_str(), &end);
  if (end == s.c_str() || *end || !std::isfinite(v)) return false;
  if (out) *out = v;
  return true;
}
inline bool FloatsOk(const std::string &s, int count)
{
  std::size_t n = 0, from = 0;
  for (;;) {
    const auto comma = s.find(',', from);
    if (!IsNumber(s.substr(from, comma == std::string::npos ? std::string::npos : comma - from))) return false;
    ++n; if (comma == std::string::npos) break; from = comma + 1;
  }
  return count < 0 ? n >= 1 && n <= (std::size_t)-count : n == (std::size_t)count;
}

// What is wrong with a value for a setting; empty when it is fine. Used for the environment (a warning) and for
// profiles (an error).
inline std::string ValueIssue(const Setting &s, const std::string &v)
{
  if (s.autoOk && v == "auto") return "";
  switch (s.type) {
  case Type::Flag: { double n; if (!IsNumber(v, &n)) return "is not a number (use 0 or 1)"; return ""; }
  case Type::Float: return IsNumber(v) ? "" : "is not a number";
  case Type::Int: { double n; if (!IsNumber(v, &n) || n != std::floor(n)) return "is not a whole number"; return ""; }
  case Type::Floats: return FloatsOk(v, s.count) ? "" : (s.count < 0 ? "needs 1 to " + std::to_string(-s.count) + " comma-separated numbers" : "needs " + std::to_string(s.count) + " comma-separated numbers");
  default: return "";
  }
}

// ---- a strict reader for the TOML subset profiles use
struct Val {
  enum class K { str, num, flag, arr, tab } k = K::str;
  std::string s; double n = 0; bool b = false, isInt = false, fixed = false;
  std::vector<Val> a; std::vector<std::string> keys; std::vector<Val> vals;
  const Val *find(const std::string &key) const { for (std::size_t i = 0; i < keys.size(); ++i) if (keys[i] == key) return &vals[i]; return nullptr; }
};

class Toml {
public:
  static Val Parse(const std::string &text, const std::string &file)
  {
    Toml t; t.file_ = file; return t.run(text);
  }
private:
  std::string file_; std::size_t line_ = 0;
  [[noreturn]] void fail(const std::string &m) const { throw std::runtime_error(file_ + ":" + std::to_string(line_) + ": " + m); }
  static void skip(const std::string &s, std::size_t &p) { while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) ++p; }
  static bool end(const std::string &s, std::size_t p) { skip(s, p); return p >= s.size() || s[p] == '#'; }

  std::string str(const std::string &s, std::size_t &p)
  {
    const char q = s[p++]; std::string out;
    if (p + 1 < s.size() && s[p] == q && s[p + 1] == q) fail("multi-line strings are not supported");
    for (; p < s.size(); ++p) {
      const char c = s[p];
      if (c == q) { ++p; return out; }
      if (q == '"' && c == '\\') {
        if (++p >= s.size()) break;
        switch (s[p]) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        default: fail(std::string("unsupported escape \\") + s[p]);
        }
        continue;
      }
      out += c;
    }
    fail("unterminated string");
  }
  std::string key(const std::string &s, std::size_t &p)
  {
    skip(s, p);
    if (p < s.size() && (s[p] == '"' || s[p] == '\'')) return str(s, p);
    const std::size_t b = p;
    while (p < s.size() && (std::isalnum((unsigned char)s[p]) || s[p] == '_' || s[p] == '-')) ++p;
    if (p == b) fail("a key was expected");
    return s.substr(b, p - b);
  }
  Val value(const std::string &s, std::size_t &p, int depth)
  {
    if (depth > 3) fail("values nested too deeply");
    skip(s, p); if (p >= s.size()) fail("a value was expected");
    Val v; const char c = s[p];
    if (c == '"' || c == '\'') { v.k = Val::K::str; v.s = str(s, p); return v; }
    if (c == '[') {
      ++p; v.k = Val::K::arr;
      for (;;) {
        skip(s, p); if (p >= s.size()) fail("arrays must be on one line");
        if (s[p] == ']') { ++p; break; }
        Val e = value(s, p, depth + 1);
        if (e.k == Val::K::arr || e.k == Val::K::tab) fail("arrays of arrays or tables are not supported");
        v.a.push_back(e); skip(s, p);
        if (p < s.size() && s[p] == ',') ++p; else if (p >= s.size() || s[p] != ']') fail("',' or ']' expected");
      }
      return v;
    }
    if (c == '{') {
      ++p; v.k = Val::K::tab; v.fixed = true;
      for (;;) {
        skip(s, p); if (p >= s.size()) fail("inline tables must be on one line");
        if (s[p] == '}') { ++p; break; }
        const std::string k = key(s, p); skip(s, p);
        if (p < s.size() && s[p] == '.') fail("dotted keys are not supported");
        if (p >= s.size() || s[p] != '=') fail("'=' expected");
        ++p; Val e = value(s, p, depth + 1);
        if (e.k == Val::K::tab) fail("inline tables inside inline tables are not supported");
        if (v.find(k)) fail("duplicate key '" + k + "'");
        v.keys.push_back(k); v.vals.push_back(e); skip(s, p);
        if (p < s.size() && s[p] == ',') ++p; else if (p >= s.size() || s[p] != '}') fail("',' or '}' expected");
      }
      return v;
    }
    const std::size_t b = p;
    while (p < s.size() && !std::strchr(" \t,]}#", s[p])) ++p;
    const std::string tok = s.substr(b, p - b);
    if (tok == "true" || tok == "false") { v.k = Val::K::flag; v.b = tok == "true"; return v; }
    if (tok.empty() || tok.find_first_not_of("0123456789+-.eE") != std::string::npos) fail("cannot read the value '" + tok + "' (strings need quotes)");
    char *e = nullptr; v.n = std::strtod(tok.c_str(), &e);
    if (*e || !std::isfinite(v.n)) fail("cannot read the number '" + tok + "'");
    v.k = Val::K::num; v.isInt = tok.find_first_of(".eE") == std::string::npos;
    return v;
  }
  Val run(std::string text)
  {
    Val root; root.k = Val::K::tab;
    Val *table = &root; std::set<std::string> defined;
    std::size_t at = 0;
    while (at <= text.size()) {
      std::size_t nl = text.find('\n', at); if (nl == std::string::npos) nl = text.size();
      std::string ln = text.substr(at, nl - at); at = nl + 1; ++line_;
      if (!ln.empty() && ln.back() == '\r') ln.pop_back();
      std::size_t p = 0; skip(ln, p);
      if (p >= ln.size() || ln[p] == '#') continue;
      if (ln[p] == '[') {
        if (p + 1 < ln.size() && ln[p + 1] == '[') fail("arrays of tables are not supported");
        ++p; std::vector<std::string> path; std::string joined;
        for (;;) {
          path.push_back(key(ln, p)); joined += (joined.empty() ? "" : ".") + path.back(); skip(ln, p);
          if (p < ln.size() && ln[p] == '.') { ++p; continue; }
          break;
        }
        if (p >= ln.size() || ln[p] != ']') fail("']' expected");
        if (!end(ln, p + 1)) fail("text after the table header");
        if (!defined.insert(joined).second) fail("table [" + joined + "] is defined twice");
        Val *t = &root;
        for (const auto &part : path) {
          Val *child = nullptr;
          for (std::size_t j = 0; j < t->keys.size(); ++j) if (t->keys[j] == part) child = &t->vals[j];
          if (!child) { Val nt; nt.k = Val::K::tab; t->keys.push_back(part); t->vals.push_back(nt); child = &t->vals.back(); }
          else if (child->k != Val::K::tab || child->fixed) fail("'" + part + "' is already a value, not a table");
          t = child;
        }
        table = t; continue;
      }
      const std::string k = key(ln, p); skip(ln, p);
      if (p < ln.size() && ln[p] == '.') fail("dotted keys are not supported (use a [table] header)");
      if (p >= ln.size() || ln[p] != '=') fail("'=' expected after the key '" + k + "'");
      ++p; Val v = value(ln, p, 0);
      if (!end(ln, p)) fail("text after the value of '" + k + "'");
      if (table->find(k)) fail("duplicate key '" + k + "'");
      table->keys.push_back(k); table->vals.push_back(v);
    }
    return root;
  }
};

// ---- profiles
struct Layer {
  std::map<std::string, std::string> settings;   // ACVR_* name -> text; "auto" puts the host's own choice back
  std::map<std::string, std::string> options;    // core option -> value
};
struct View { std::string label, evidence, notes; Layer layer; };
struct Profile {
  std::string path, name, system, notes, controls, gun, defaultView;
  int port = 0; unsigned systemMask = 0;
  Layer base;
  std::map<std::string, View> views;
  std::vector<std::string> warnings;
};

inline unsigned SystemMask(const std::string &name)
{
  return name == "ps1" ? kPs1 : name == "saturn" ? kSaturn : name == "n64" ? kN64 : name == "3ds" ? k3ds : 0u;
}

namespace detail {
struct PadKey { const char *key, *env; };
inline const PadKey *PadKeys(std::size_t &n)
{
  static const PadKey k[] = {{"map", "ACVR_PAD_MAP"}, {"wheel_deg", "ACVR_PAD_WHEEL_DEG"}, {"aim_hold", "ACVR_PAD_AIM_HOLD"},
    {"aim_button", "ACVR_PAD_AIM_BUTTON"}, {"aim", "ACVR_PAD_AIM"}, {"aim_curve", "ACVR_PAD_AIM_CURVE"}, {"aim_dot", "ACVR_PAD_AIM_DOT"},
    {"aim_on", "ACVR_PAD_AIM_ON"}, {"follow_u", "ACVR_PAD_FOLLOW_U"}, {"follow_v", "ACVR_PAD_FOLLOW_V"}, {"follow_gain", "ACVR_PAD_FOLLOW_GAIN"}};
  n = sizeof k / sizeof k[0]; return k;
}
}

inline Profile FromToml(const Val &root, const std::string &file)
{
  Profile p; p.path = file;
  auto fail = [&](const std::string &m) { throw std::runtime_error(file + ": " + m); };
  auto needTab = [&](const Val &v, const std::string &where) { if (v.k != Val::K::tab) fail(where + " must be a table"); };
  auto text = [&](const Val &v, const std::string &where) -> std::string { if (v.k != Val::K::str) fail(where + " must be text in quotes"); return v.s; };
  auto allowed = [&](const Val &t, const std::string &where, std::initializer_list<const char *> names) {
    for (const auto &k : t.keys) { bool ok = false; for (const char *n : names) ok = ok || k == n; if (!ok) fail(where + ": unknown key '" + k + "'"); }
  };
  // A setting's value as the text the host's getenv sites read.
  auto toText = [&](const Setting &s, const Val &v, const std::string &where) -> std::string {
    std::string out;
    switch (s.type) {
    case Type::Flag:
      if (v.k == Val::K::flag) out = v.b ? "1" : "0";
      else if (v.k == Val::K::num && v.isInt && (v.n == 0 || v.n == 1)) out = Num(v.n);
      else fail(where + " must be true, false, 0 or 1");
      break;
    case Type::Float:
      if (v.k == Val::K::num) out = Num(v.n);
      else if (v.k == Val::K::str && s.autoOk && v.s == "auto") out = "auto";
      else fail(where + (s.autoOk ? " must be a number or \"auto\"" : " must be a number"));
      break;
    case Type::Int:
      if (v.k == Val::K::num && v.isInt) out = Num(v.n); else fail(where + " must be a whole number");
      break;
    case Type::Floats:
      if (v.k == Val::K::arr) { for (const auto &e : v.a) { if (e.k != Val::K::num) fail(where + " must hold numbers"); out += (out.empty() ? "" : ",") + Num(e.n); } }
      else if (v.k == Val::K::str) out = v.s;
      else fail(where + " must be an array of numbers");
      { const std::string issue = ValueIssue(s, out); if (!issue.empty()) fail(where + " " + issue); }
      break;
    case Type::Text:
      if (v.k == Val::K::str) out = v.s; else fail(where + " must be text in quotes");
      break;
    case Type::Path: break;
    }
    return out;
  };
  auto addSetting = [&](Layer &layer, const std::string &name, const Val &v, const std::string &where) {
    const Setting *s = Find(name);
    if (!s) { const std::string maybe = Closest(name); fail(where + ": unknown setting '" + name + "'" + (maybe.empty() ? "" : " (did you mean " + maybe + "?)")); }
    if (s->type == Type::Path) fail(where + ": " + name + " is a path on one machine; set it in the launcher's environment, not in a profile");
    layer.settings[name] = toText(*s, v, where + "." + name);
    if (p.systemMask && !(s->systems & p.systemMask)) p.warnings.push_back(name + " does not apply to " + p.system + " (profile " + file + ")");
  };
  auto readLayer = [&](const Val &t, const std::string &where, Layer &layer) {
    if (const Val *s = t.find("settings")) { needTab(*s, where + "settings"); for (std::size_t i = 0; i < s->keys.size(); ++i) addSetting(layer, s->keys[i], s->vals[i], where + "settings"); }
    if (const Val *o = t.find("core_options")) {
      needTab(*o, where + "core_options");
      for (std::size_t i = 0; i < o->keys.size(); ++i) {
        const Val &v = o->vals[i];
        if (v.k == Val::K::str) layer.options[o->keys[i]] = v.s;
        else if (v.k == Val::K::num) layer.options[o->keys[i]] = Num(v.n);
        else fail(where + "core_options." + o->keys[i] + " must be text or a number (quote words like enabled)");
      }
    }
    if (const Val *pd = t.find("pad")) {
      needTab(*pd, where + "pad");
      std::size_t n = 0; const detail::PadKey *keys = detail::PadKeys(n);
      allowed(*pd, where + "pad", {"map", "wheel_deg", "aim_hold", "aim_button", "aim", "aim_curve", "aim_dot", "aim_on", "follow_u", "follow_v", "follow_gain"});
      for (std::size_t i = 0; i < pd->keys.size(); ++i) {
        const char *env = nullptr; for (std::size_t j = 0; j < n; ++j) if (pd->keys[i] == keys[j].key) env = keys[j].env;
        const Val &v = pd->vals[i];
        if (pd->keys[i] == "map" && v.k == Val::K::tab) {
          std::string out;
          for (std::size_t j = 0; j < v.keys.size(); ++j) {
            if (v.vals[j].k != Val::K::str || v.vals[j].s.empty() || v.vals[j].s.find_first_of(",=") != std::string::npos) fail(where + "pad.map." + v.keys[j] + " must be a RetroPad button name in quotes");
            out += (out.empty() ? "" : ",") + v.keys[j] + "=" + v.vals[j].s;
          }
          if (out.empty()) fail(where + "pad.map is empty");
          Val asText; asText.k = Val::K::str; asText.s = out;
          addSetting(layer, env, asText, where + "pad");
        } else addSetting(layer, env, v, where + "pad");
      }
    }
  };

  needTab(root, "the file");
  allowed(root, "top level", {"schema", "name", "system", "notes", "controls", "gun", "port", "default_view", "core_options", "settings", "pad", "views"});
  const Val *schema = root.find("schema");
  if (!schema || schema->k != Val::K::num || schema->n != 1) fail("schema = 1 is required");
  if (const Val *v = root.find("name")) p.name = text(*v, "name");
  if (const Val *v = root.find("notes")) p.notes = text(*v, "notes");
  if (const Val *v = root.find("system")) {
    p.system = text(*v, "system"); p.systemMask = SystemMask(p.system);
    if (!p.systemMask) fail("system must be ps1, saturn, n64 or 3ds");
  }
  if (const Val *v = root.find("controls")) {
    p.controls = text(*v, "controls");
    if (p.controls != "gun" && p.controls != "pad" && p.controls != "wheel" && p.controls != "aim") fail("controls must be gun, pad, wheel or aim");
  }
  if (const Val *v = root.find("gun")) {
    p.gun = text(*v, "gun");
    if (p.gun != "guncon" && p.gun != "justifier" && p.gun != "lightgun" && p.gun != "stunner") fail("gun must be guncon, justifier, lightgun or stunner");
  }
  if (const Val *v = root.find("port")) { if (v->k != Val::K::num || !v->isInt || (v->n != 1 && v->n != 2)) fail("port must be 1 or 2"); p.port = (int)v->n; }
  if (const Val *v = root.find("default_view")) p.defaultView = text(*v, "default_view");
  readLayer(root, "", p.base);
  if (const Val *views = root.find("views")) {
    needTab(*views, "views");
    for (std::size_t i = 0; i < views->keys.size(); ++i) {
      const std::string where = "views." + views->keys[i] + ".";
      const Val &vt = views->vals[i];
      needTab(vt, "views." + views->keys[i]);
      allowed(vt, "views." + views->keys[i], {"label", "evidence", "notes", "settings", "core_options", "pad"});
      View view;
      if (const Val *v = vt.find("label")) view.label = text(*v, where + "label");
      if (const Val *v = vt.find("evidence")) view.evidence = text(*v, where + "evidence");
      if (const Val *v = vt.find("notes")) view.notes = text(*v, where + "notes");
      readLayer(vt, where, view.layer);
      p.views[views->keys[i]] = view;
    }
  }
  if (!p.defaultView.empty() && !p.views.count(p.defaultView)) fail("default_view '" + p.defaultView + "' is not one of its views");
  return p;
}

inline Profile LoadProfileText(const std::string &text, const std::string &name)
{
  return FromToml(Toml::Parse(text, name), name);
}
inline Profile LoadProfile(const std::string &path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read the profile " + path);
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
  return FromToml(Toml::Parse(text, path), path);
}

// ---- resolution
inline std::vector<std::string> EnvironmentNames()
{
  std::vector<std::string> out;
#ifdef _WIN32
  char *block = GetEnvironmentStringsA(); if (!block) return out;
  for (const char *e = block; *e; e += std::strlen(e) + 1) { const char *eq = std::strchr(e + 1, '='); if (eq) out.emplace_back(e, eq); }
  FreeEnvironmentStringsA(block);
#else
  extern char **environ;
  for (char **e = environ; e && *e; ++e) { const char *eq = std::strchr(*e, '='); if (eq) out.emplace_back(*e, eq); }
#endif
  return out;
}
inline const char *EnvValue(const char *name) { const char *v = std::getenv(name); return v && *v ? v : nullptr; }
inline void SetEnv(const std::string &name, const std::string &value)
{
#ifdef _WIN32
  _putenv((name + "=" + value).c_str());
#else
  setenv(name.c_str(), value.c_str(), 1);
#endif
}

struct Entry { const Setting *setting; std::string value, source; bool changed; };
struct Resolved {
  std::vector<Entry> entries;
  std::string view;
  std::vector<std::string> warnings;
  std::vector<std::string> optionLines;
  // The lines for the log (what differs from the defaults) and for --print-config (everything).
  std::vector<std::string> summary() const
  {
    std::vector<std::string> out; std::size_t changed = 0;
    for (const auto &e : entries) if (e.changed) ++changed;
    out.push_back("Config: " + std::to_string(entries.size()) + " settings known, " + std::to_string(changed) + " set (the rest at their defaults)");
    for (const auto &e : entries) if (e.changed) out.push_back("  " + std::string(e.setting->name) + " = " + e.value + "  (" + e.source + ")");
    for (const auto &l : optionLines) out.push_back("  " + l);
    for (const auto &w : warnings) out.push_back("Config warning: " + w);
    return out;
  }
  std::vector<std::string> printout() const
  {
    std::vector<std::string> out;
    for (const auto &e : entries) out.push_back(std::string(e.setting->name) + " = " + e.value + "  (" + e.source + ")");
    for (const auto &l : optionLines) out.push_back(l);
    for (const auto &w : warnings) out.push_back("warning: " + w);
    return out;
  }
};

// Resolves the settings: warns about ACVR_* names in the environment that nothing reads, reads each setting from
// the environment or (not set there) from the profile's base plus the chosen view, puts profile values into the
// process environment, and merges the profile's core options into `options` (a key already there, from --option,
// stays). Throws std::runtime_error when the view does not exist.
inline Resolved Resolve(const Profile *profile, const std::string &viewArg, std::map<std::string, std::string> &options)
{
  Resolved r;
  for (const auto &name : EnvironmentNames()) {
    const std::string up = Upper(name);
    if (up.compare(0, 5, "ACVR_") != 0 || Find(up) || OwnedElsewhere(up)) continue;
    const std::string maybe = Closest(up);
    r.warnings.push_back(name + " is set but acvr-ps1 reads no such setting" + (maybe.empty() ? "" : "; did you mean " + maybe + "?"));
  }
  std::map<std::string, std::pair<std::string, std::string>> fromProfile;   // name -> value, source
  std::map<std::string, std::pair<std::string, std::string>> optionFrom;
  if (profile) {
    std::string view = viewArg.empty() ? profile->defaultView : viewArg;
    const View *chosen = nullptr;
    if (!view.empty()) {
      const auto it = profile->views.find(view);
      if (it == profile->views.end()) {
        std::string have; for (const auto &v : profile->views) have += (have.empty() ? "" : ", ") + v.first;
        throw std::runtime_error("the profile has no view '" + view + "'" + (have.empty() ? " (it has no views)" : "; its views: " + have));
      }
      chosen = &it->second; r.view = view;
    }
    auto layer = [&](const Layer &l, const std::string &source) {
      for (const auto &kv : l.settings) { if (kv.second == "auto") fromProfile.erase(kv.first); else fromProfile[kv.first] = {kv.second, source}; }
      for (const auto &kv : l.options) optionFrom[kv.first] = {kv.second, source};
    };
    layer(profile->base, "profile");
    if (chosen) layer(chosen->layer, "profile view " + view);
    for (const auto &kv : optionFrom) {
      const auto have = options.find(kv.first);
      if (have != options.end()) r.optionLines.push_back("core option " + kv.first + " = " + have->second + "  (command line, over the profile's " + kv.second.first + ")");
      else { options[kv.first] = kv.second.first; r.optionLines.push_back("core option " + kv.first + " = " + kv.second.first + "  (" + kv.second.second + ")"); }
    }
    r.warnings.insert(r.warnings.end(), profile->warnings.begin(), profile->warnings.end());
  }
  for (const auto &s : Registry()) {
    Entry e{&s, s.def, "default", false};
    if (const char *env = EnvValue(s.name)) {
      e.value = env; e.changed = true; e.source = "environment";
      const auto p = fromProfile.find(s.name);
      if (p != fromProfile.end()) e.source += ", over the profile's " + p->second.first;
      const std::string issue = ValueIssue(s, env);
      if (!issue.empty()) r.warnings.push_back(std::string(s.name) + "=" + env + " " + issue);
    } else {
      const auto p = fromProfile.find(s.name);
      if (p != fromProfile.end()) { e.value = p->second.first; e.source = p->second.second; e.changed = true; SetEnv(s.name, e.value); }
    }
    r.entries.push_back(e);
  }
  return r;
}

}  // namespace hostcfg
