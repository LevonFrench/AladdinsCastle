// SPDX-License-Identifier: GPL-3.0-or-later
// ACVR PS1 host: runs a user-supplied PlayStation libretro core (Beetle PSX),
// shows its picture in the headset as a theatre screen through the shared
// ACVR theatre presenter, and feeds the VR controller to the core as a GunCon
// or Justifier light gun. A headless scripted mode makes offline proofs.
#include "WinGL.h"
#include "TheatreXR.h"
#include "CleanQuit.h"
#include "retro_abi.h"
#include "AudioOut.h"
#include "Png.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

FILE *g_log = stdout;
void Log(const char *fmt, ...)
{
  va_list ap; va_start(ap, fmt); std::vfprintf(g_log, fmt, ap); va_end(ap);
  std::fputc('\n', g_log); std::fflush(g_log);
}

// ---- core entry points
struct Core {
  HMODULE dll = nullptr;
  void (*set_environment)(retro_environment_t) = nullptr;
  void (*set_video_refresh)(retro_video_refresh_t) = nullptr;
  void (*set_audio_sample)(retro_audio_sample_t) = nullptr;
  void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = nullptr;
  void (*set_input_poll)(retro_input_poll_t) = nullptr;
  void (*set_input_state)(retro_input_state_t) = nullptr;
  void (*init)() = nullptr;
  void (*deinit)() = nullptr;
  unsigned (*api_version)() = nullptr;
  void (*get_system_info)(retro_system_info *) = nullptr;
  void (*get_system_av_info)(retro_system_av_info *) = nullptr;
  void (*set_controller_port_device)(unsigned, unsigned) = nullptr;
  void (*run)() = nullptr;
  bool (*load_game)(const retro_game_info *) = nullptr;
  void (*unload_game)() = nullptr;
  void *(*get_memory_data)(unsigned) = nullptr;
  std::size_t (*get_memory_size)(unsigned) = nullptr;
} g_core;

bool LoadCore(const std::string &path)
{
  g_core.dll = LoadLibraryA(path.c_str());
  if (!g_core.dll) { Log("Cannot load core %s (Win32 %lu)", path.c_str(), GetLastError()); return false; }
#define SYM(field, name) g_core.field = reinterpret_cast<decltype(g_core.field)>(GetProcAddress(g_core.dll, name)); \
  if (!g_core.field) { Log("Core lacks %s", name); return false; }
  SYM(set_environment, "retro_set_environment") SYM(set_video_refresh, "retro_set_video_refresh")
  SYM(set_audio_sample, "retro_set_audio_sample") SYM(set_audio_sample_batch, "retro_set_audio_sample_batch")
  SYM(set_input_poll, "retro_set_input_poll") SYM(set_input_state, "retro_set_input_state")
  SYM(init, "retro_init") SYM(deinit, "retro_deinit") SYM(api_version, "retro_api_version")
  SYM(get_system_info, "retro_get_system_info") SYM(get_system_av_info, "retro_get_system_av_info")
  SYM(set_controller_port_device, "retro_set_controller_port_device") SYM(run, "retro_run")
  SYM(load_game, "retro_load_game") SYM(unload_game, "retro_unload_game")
  SYM(get_memory_data, "retro_get_memory_data") SYM(get_memory_size, "retro_get_memory_size")
#undef SYM
  return true;
}

// ---- host state seen by the core callbacks
std::string g_corePath, g_systemDir, g_saveDir;
std::map<std::string, std::string> g_optionOverrides, g_optionValues;
std::vector<std::pair<std::string, std::string>> g_optionList;   // key, "description; a|b|c"
retro_pixel_format g_pixelFormat = RETRO_PIXEL_FORMAT_0RGB1555;
retro_system_av_info g_av{};
std::vector<retro_controller_description> g_controllers[2];
std::vector<std::string> g_controllerNames[2];
std::set<unsigned> g_unknownCommands;
bool g_shutdownRequested = false;

std::vector<std::uint32_t> g_frame;   // 0x00RRGGBB, top row first
unsigned g_frameW = 0, g_frameH = 0;
std::uint64_t g_frameSerial = 0;

AudioOut g_audio;
bool g_audioWanted = false;

struct Gun {
  float u = .5f, v = .5f;
  bool offScreen = true, trigger = false, a = false, b = false, start = false, select = false;
} g_gun;
unsigned g_gunPort = 0;
// The other port holds a standard pad, played with the left controller (or the offline script).
struct Pad { bool up = false, down = false, left = false, right = false, cross = false, circle = false, square = false, triangle = false, start = false, select = false; } g_pad;
bool g_padOn = true;
std::uint64_t g_gunReads = 0;

void CoreLog(retro_log_level level, const char *fmt, ...)
{
  static const char *names[] = { "debug", "info", "warn", "error" };
  char line[2048]; va_list ap; va_start(ap, fmt); std::vsnprintf(line, sizeof line, fmt, ap); va_end(ap);
  std::size_t n = std::strlen(line); while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
  if (level >= RETRO_LOG_INFO) Log("[core %s] %s", names[level & 3], line);
}

bool Environment(unsigned cmd, void *data)
{
  switch (cmd) {
  case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: *static_cast<const char **>(data) = g_systemDir.c_str(); return true;
  case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *static_cast<const char **>(data) = g_saveDir.c_str(); return true;
  case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY: *static_cast<const char **>(data) = g_saveDir.c_str(); return true;
  case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH: *static_cast<const char **>(data) = g_corePath.c_str(); return true;
  case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
    const auto f = *static_cast<const retro_pixel_format *>(data);
    if (f != RETRO_PIXEL_FORMAT_XRGB8888 && f != RETRO_PIXEL_FORMAT_RGB565 && f != RETRO_PIXEL_FORMAT_0RGB1555) return false;
    g_pixelFormat = f; return true;
  }
  case RETRO_ENVIRONMENT_GET_CAN_DUPE: *static_cast<bool *>(data) = true; return true;
  case RETRO_ENVIRONMENT_GET_OVERSCAN: *static_cast<bool *>(data) = false; return true;
  case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: static_cast<retro_log_callback *>(data)->log = CoreLog; return true;
  case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *static_cast<unsigned *>(data) = 0; return true;
  case RETRO_ENVIRONMENT_GET_LANGUAGE: *static_cast<unsigned *>(data) = 0; return true;
  case RETRO_ENVIRONMENT_GET_USERNAME: *static_cast<const char **>(data) = "Player"; return true;
  case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: if (data) *static_cast<int *>(data) = 3; return true;
  case RETRO_ENVIRONMENT_GET_FASTFORWARDING: if (data) *static_cast<bool *>(data) = false; return true;
  case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
  case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
  case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: return true;
  case RETRO_ENVIRONMENT_SHUTDOWN: g_shutdownRequested = true; return true;
  case RETRO_ENVIRONMENT_SET_MESSAGE: { auto *m = static_cast<const retro_message *>(data); if (m && m->msg) Log("[core message] %s", m->msg); return true; }
  case RETRO_ENVIRONMENT_SET_GEOMETRY: g_av.geometry = *static_cast<const retro_game_geometry *>(data); return true;
  case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: g_av = *static_cast<const retro_system_av_info *>(data); return true;
  case RETRO_ENVIRONMENT_SET_VARIABLES: {
    for (auto *v = static_cast<const retro_variable *>(data); v && v->key; ++v) {
      std::string spec = v->value ? v->value : "";
      g_optionList.emplace_back(v->key, spec);
      const auto semi = spec.find("; ");
      std::string choices = semi == std::string::npos ? spec : spec.substr(semi + 2);
      const std::string first = choices.substr(0, choices.find('|'));
      const auto over = g_optionOverrides.find(v->key);
      g_optionValues[v->key] = over != g_optionOverrides.end() ? over->second : first;
    }
    return true;
  }
  case RETRO_ENVIRONMENT_GET_VARIABLE: {
    auto *v = static_cast<retro_variable *>(data);
    if (!v || !v->key) return false;
    auto it = g_optionValues.find(v->key);
    if (it == g_optionValues.end()) { auto o = g_optionOverrides.find(v->key); if (o == g_optionOverrides.end()) { v->value = nullptr; return false; } it = g_optionValues.emplace(v->key, o->second).first; }
    v->value = it->second.c_str(); return true;
  }
  case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *static_cast<bool *>(data) = false; return true;
  case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: {
    unsigned port = 0;
    for (auto *info = static_cast<const retro_controller_info *>(data); info && info->types && port < 2; ++info, ++port)
      for (unsigned i = 0; i < info->num_types; ++i) {
        g_controllers[port].push_back(info->types[i]);
        g_controllerNames[port].push_back(info->types[i].desc ? info->types[i].desc : "");
        g_controllers[port].back().desc = g_controllerNames[port].back().c_str();
      }
    return true;
  }
  default:
    if (g_unknownCommands.insert(cmd).second) Log("(core asked for unsupported environment call %u)", cmd);
    return false;
  }
}

void VideoRefresh(const void *data, unsigned w, unsigned h, std::size_t pitch)
{
  if (!data || !w || !h) return;   // duplicate frame: keep the last picture
  g_frame.resize((std::size_t)w * h); g_frameW = w; g_frameH = h;
  for (unsigned y = 0; y < h; ++y) {
    const auto *row = static_cast<const unsigned char *>(data) + y * pitch;
    std::uint32_t *out = g_frame.data() + (std::size_t)y * w;
    if (g_pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888) {
      const auto *p = reinterpret_cast<const std::uint32_t *>(row);
      for (unsigned x = 0; x < w; ++x) out[x] = p[x] & 0xffffffu;
    } else {
      const auto *p = reinterpret_cast<const std::uint16_t *>(row);
      for (unsigned x = 0; x < w; ++x) {
        unsigned r, g, b; const unsigned c = p[x];
        if (g_pixelFormat == RETRO_PIXEL_FORMAT_RGB565) { r = (c >> 11) & 31; g = (c >> 5) & 63; b = c & 31; g = (g << 2) | (g >> 4); }
        else { r = (c >> 10) & 31; g = (c >> 5) & 31; b = c & 31; g = (g << 3) | (g >> 2); }
        r = (r << 3) | (r >> 2); b = (b << 3) | (b >> 2);
        out[x] = (r << 16) | (g << 8) | b;
      }
    }
  }
  ++g_frameSerial;
}

void AudioSample(std::int16_t l, std::int16_t r) { const std::int16_t f[2] = { l, r }; if (g_audioWanted) g_audio.write(f, 1); }
std::size_t AudioBatch(const std::int16_t *d, std::size_t frames) { if (g_audioWanted) g_audio.write(d, frames); return frames; }
void InputPoll() {}

std::int16_t InputState(unsigned port, unsigned device, unsigned, unsigned id)
{
  if (port != g_gunPort) {
    if (!g_padOn || port > 1 || (device & RETRO_DEVICE_MASK) != RETRO_DEVICE_JOYPAD) return 0;
    switch (id) {
    case RETRO_DEVICE_ID_JOYPAD_UP: return g_pad.up;
    case RETRO_DEVICE_ID_JOYPAD_DOWN: return g_pad.down;
    case RETRO_DEVICE_ID_JOYPAD_LEFT: return g_pad.left;
    case RETRO_DEVICE_ID_JOYPAD_RIGHT: return g_pad.right;
    case RETRO_DEVICE_ID_JOYPAD_B: return g_pad.cross;      // libretro B is the PlayStation Cross
    case RETRO_DEVICE_ID_JOYPAD_A: return g_pad.circle;
    case RETRO_DEVICE_ID_JOYPAD_Y: return g_pad.square;
    case RETRO_DEVICE_ID_JOYPAD_X: return g_pad.triangle;
    case RETRO_DEVICE_ID_JOYPAD_START: return g_pad.start;
    case RETRO_DEVICE_ID_JOYPAD_SELECT: return g_pad.select;
    default: return 0;
    }
  }
  const unsigned base = device & RETRO_DEVICE_MASK;
  auto axis = [](float t) { return (std::int16_t)std::lround(std::clamp(t * 2.f - 1.f, -1.f, 1.f) * 32767.f); };
  if (base == RETRO_DEVICE_LIGHTGUN) {
    ++g_gunReads;
    switch (id) {
    case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X: return g_gun.offScreen ? -32768 : axis(g_gun.u);
    case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y: return g_gun.offScreen ? -32768 : axis(g_gun.v);
    case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN: return g_gun.offScreen;
    case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER: return g_gun.trigger && !g_gun.offScreen;
    case RETRO_DEVICE_ID_LIGHTGUN_RELOAD: return g_gun.trigger && g_gun.offScreen;
    case RETRO_DEVICE_ID_LIGHTGUN_AUX_A: return g_gun.a;
    case RETRO_DEVICE_ID_LIGHTGUN_AUX_B: return g_gun.b;
    case RETRO_DEVICE_ID_LIGHTGUN_START: return g_gun.start;
    case RETRO_DEVICE_ID_LIGHTGUN_SELECT: return g_gun.select;
    default: return 0;
    }
  }
  if (base == RETRO_DEVICE_JOYPAD) {   // arcade sets read coin and start from the pad
    switch (id) {
    case RETRO_DEVICE_ID_JOYPAD_START: return g_gun.start;
    case RETRO_DEVICE_ID_JOYPAD_SELECT: return g_gun.select;
    default: return 0;
    }
  }
  if (base == RETRO_DEVICE_POINTER) {
    switch (id) {
    case RETRO_DEVICE_ID_POINTER_X: return g_gun.offScreen ? -32768 : axis(g_gun.u);
    case RETRO_DEVICE_ID_POINTER_Y: return g_gun.offScreen ? -32768 : axis(g_gun.v);
    case RETRO_DEVICE_ID_POINTER_PRESSED: return g_gun.trigger;
    default: return 0;
    }
  }
  return 0;
}

// ---- memory card 0 lives in the core's save RAM; the host persists it
std::string g_sramPath;
std::vector<unsigned char> g_sramSaved;

void LoadSram()
{
  void *mem = g_core.get_memory_data(RETRO_MEMORY_SAVE_RAM);
  const std::size_t size = g_core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
  if (!mem || !size) { Log("Core exposes no save RAM"); return; }
  std::ifstream in(g_sramPath, std::ios::binary);
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (bytes.size() == size) { std::memcpy(mem, bytes.data(), size); Log("Memory card loaded: %s", g_sramPath.c_str()); }
  else if (!bytes.empty()) Log("Memory card file has %zu bytes, core expects %zu: left unused", bytes.size(), size);
  g_sramSaved.assign(static_cast<unsigned char *>(mem), static_cast<unsigned char *>(mem) + size);
}

bool SaveSram(bool force)
{
  void *mem = g_core.get_memory_data(RETRO_MEMORY_SAVE_RAM);
  const std::size_t size = g_core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
  if (!mem || !size) return true;
  if (!force && g_sramSaved.size() == size && !std::memcmp(g_sramSaved.data(), mem, size)) return true;
  const std::string temp = g_sramPath + ".tmp";
  FILE *f = std::fopen(temp.c_str(), "wb");
  if (!f) { Log("Cannot write %s", temp.c_str()); return false; }
  const bool ok = std::fwrite(mem, 1, size, f) == size && std::fclose(f) == 0;
  if (!ok || !MoveFileExA(temp.c_str(), g_sramPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { Log("Memory card save failed"); return false; }
  g_sramSaved.assign(static_cast<unsigned char *>(mem), static_cast<unsigned char *>(mem) + size);
  Log("Memory card saved: %s", g_sramPath.c_str());
  return true;
}

// ---- offline script: "<frame> <command> [arguments]" per line, # comments
struct ScriptEvent { unsigned frame; std::string cmd; std::vector<std::string> args; };
struct Script {
  std::vector<ScriptEvent> events; std::size_t next = 0;
  unsigned holdTrigger = 0, holdA = 0, holdB = 0, holdStart = 0, holdSelect = 0, holdOff = 0;
  std::map<std::string, unsigned> pad;   // pad-up, pad-cross, pad-start ... frames left
  bool load(const std::string &path)
  {
    std::ifstream in(path); if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
      const auto hash = line.find('#'); if (hash != std::string::npos) line.resize(hash);
      std::istringstream s(line); ScriptEvent e;
      if (!(s >> e.frame >> e.cmd)) continue;
      for (std::string a; s >> a;) e.args.push_back(a);
      events.push_back(e);
    }
    std::stable_sort(events.begin(), events.end(), [](const ScriptEvent &a, const ScriptEvent &b) { return a.frame < b.frame; });
    return true;
  }
};

std::string Arg(const std::vector<std::string> &a, std::size_t i, const char *fallback) { return i < a.size() ? a[i] : fallback; }

bool WriteSnapshot(const std::string &dir, const std::string &name)
{
  if (g_frame.empty()) return false;
  const std::string path = dir + "\\" + name + ".png";
  const bool ok = acvr_png::write(path.c_str(), g_frame.data(), g_frameW, g_frameH);
  Log("Snapshot %s %ux%u: %s", path.c_str(), g_frameW, g_frameH, ok ? "written" : "FAILED");
  return ok;
}

// ---- the hidden window whose OpenGL context the theatre shares
struct GlWindow {
  Host_Window *window = nullptr;
  bool create()
  {
    if (window) return true;
    window = Host_CreateWindow("ACVR PS1", 0, 0, 16, 16, Host_WINDOW_HIDDEN);
    if (!window || !Host_GL_CreateContext(window)) return false;
    Log("Hidden GL context: %s", glGetString(GL_VERSION));
    return true;
  }
  void destroy() { if (window) Host_DestroyWindow(window); window = nullptr; }
};

float EnvFloat(const char *name, float fallback)
{
  const char *v = std::getenv(name);
  if (!v || !*v) return fallback;
  const float f = (float)std::atof(v);
  return std::isfinite(f) ? f : fallback;
}

theatre::Config TheatreConfig()
{
  theatre::Config c;
  c.distance = EnvFloat("ACVR_XR_DISTANCE", 2.5f);
  c.width = EnvFloat("ACVR_PS1_SCREEN_W", 2.4f);
  c.curve = EnvFloat("ACVR_PS1_CURVE", 1.f);
  c.lift = EnvFloat("ACVR_PS1_LIFT", 0.f);
  c.crt = EnvFloat("ACVR_PS1_CRT", 0.f) != 0.f;
  const float aspect = g_av.geometry.aspect_ratio;
  c.aspect = aspect > .1f ? aspect : 4.f / 3.f;
  if (const char *m = std::getenv("ACVR_PS1_GUN_MODEL")) c.gunModel = m;
  if (const char *m = std::getenv("ACVR_PS1_GUN_META")) c.gunMeta = m;
  return c;
}

void Usage()
{
  std::fprintf(stderr,
    "acvr-ps1 --core <libretro dll> --game <disc> --system <BIOS dir> --saves <dir>\n"
    "         [--gun guncon|justifier|lightgun] [--port 1|2] [--option key=value]...\n"
    "         [--log <file>] [--no-xr] [--headless --frames N [--script file] [--snapshots dir]]\n"
    "         [--theatre-proof <prefix> --test-aim U V]\n"
    "Theatre: ACVR_XR_DISTANCE (m, default 2.5), ACVR_PS1_SCREEN_W (m, default 2.4), ACVR_PS1_CURVE (1 curved, 0 flat),\n"
    "         ACVR_PS1_LIFT (m), ACVR_PS1_CRT (1: scanlines), ACVR_PS1_GUN_MODEL (.glb), ACVR_PS1_GUN_META (.toml).\n"
    "Clean quit: hold left Y (Index: left B) for 3 s, Escape, or create ACVR_PS1_STOP_FILE.\n");
}

}  // namespace

int main(int argc, char **argv)
{
  std::string game, gunName = "guncon", script, snapshots, logPath;
  unsigned frames = 0, seconds = 0; bool headless = false, noXr = false, mute = false, testAim = false;
  float testU = .5f, testV = .5f; std::string theatreProof;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string { if (i + 1 >= argc) { Usage(); std::exit(2); } return argv[++i]; };
    if (a == "--core") g_corePath = next();
    else if (a == "--game") game = next();
    else if (a == "--system") g_systemDir = next();
    else if (a == "--saves") g_saveDir = next();
    else if (a == "--gun") gunName = next();
    else if (a == "--port") g_gunPort = (unsigned)std::atoi(next().c_str()) == 2 ? 1 : 0;
    else if (a == "--option") { const std::string kv = next(); const auto eq = kv.find('='); if (eq == std::string::npos) { Usage(); return 2; } g_optionOverrides[kv.substr(0, eq)] = kv.substr(eq + 1); }
    else if (a == "--log") logPath = next();
    else if (a == "--headless") headless = true;
    else if (a == "--no-xr") noXr = true;
    else if (a == "--no-pad") g_padOn = false;
    else if (a == "--mute") mute = true;
    else if (a == "--seconds") seconds = (unsigned)std::strtoul(next().c_str(), nullptr, 10);
    else if (a == "--theatre-proof") theatreProof = next();
    else if (a == "--test-aim") { testAim = true; testU = (float)std::atof(next().c_str()); testV = (float)std::atof(next().c_str()); }
    else if (a == "--frames") frames = (unsigned)std::strtoul(next().c_str(), nullptr, 10);
    else if (a == "--script") script = next();
    else if (a == "--snapshots") snapshots = next();
    else { Usage(); return 2; }
  }
  if (g_corePath.empty() || game.empty() || g_systemDir.empty() || g_saveDir.empty() || (headless && !frames)) { Usage(); return 2; }
  if (gunName != "guncon" && gunName != "justifier" && gunName != "lightgun") { Usage(); return 2; }
  if (!logPath.empty()) { g_log = std::fopen(logPath.c_str(), "w"); if (!g_log) g_log = stdout; }
  SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
  CreateDirectoryA(g_saveDir.c_str(), nullptr);
  if (!snapshots.empty()) CreateDirectoryA(snapshots.c_str(), nullptr);

  Script timeline;
  if (!script.empty() && !timeline.load(script)) { Log("Cannot read script %s", script.c_str()); return 2; }
  // The core draws its own crosshair only in offline proofs, to show where it thinks the gun points.
  if (!g_optionOverrides.count("beetle_psx_gun_cursor")) g_optionOverrides["beetle_psx_gun_cursor"] = headless ? "cross" : "off";
  if (!g_optionOverrides.count("beetle_psx_gun_input_mode")) g_optionOverrides["beetle_psx_gun_input_mode"] = "lightgun";

  if (!LoadCore(g_corePath)) return 3;
  if (g_core.api_version() != RETRO_API_VERSION) { Log("Core API version %u is not supported", g_core.api_version()); return 3; }
  g_core.set_environment(Environment);
  g_core.set_video_refresh(VideoRefresh);
  g_core.set_audio_sample(AudioSample);
  g_core.set_audio_sample_batch(AudioBatch);
  g_core.set_input_poll(InputPoll);
  g_core.set_input_state(InputState);
  g_core.init();
  retro_system_info sys{}; g_core.get_system_info(&sys);
  Log("Core: %s %s", sys.library_name ? sys.library_name : "?", sys.library_version ? sys.library_version : "?");

  retro_game_info info{}; info.path = game.c_str();
  if (!g_core.load_game(&info)) { Log("The core could not load the disc"); g_core.deinit(); return 4; }
  g_core.get_system_av_info(&g_av);
  Log("Timing: %.4f fps, %.0f Hz audio; picture %ux%u, aspect %.4f", g_av.timing.fps, g_av.timing.sample_rate,
      g_av.geometry.base_width, g_av.geometry.base_height, g_av.geometry.aspect_ratio);

  // Pick the gun by the name the core gives it; fall back to the usual light-gun subclasses.
  unsigned device = gunName == "justifier" ? RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 1)
                  : gunName == "lightgun" ? RETRO_DEVICE_LIGHTGUN : RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 0);
  for (const auto &c : g_controllers[g_gunPort]) {
    std::string d = c.desc ? c.desc : ""; std::transform(d.begin(), d.end(), d.begin(), ::tolower);
    if (d.find(gunName) != std::string::npos) { device = c.id; break; }
  }
  g_core.set_controller_port_device(g_gunPort, device);
  if (g_padOn && gunName != "lightgun") g_core.set_controller_port_device(1 - g_gunPort, RETRO_DEVICE_JOYPAD);
  Log("Port %u: %s (device 0x%X)", g_gunPort + 1, gunName.c_str(), device);

  {
    std::string base = game.substr(game.find_last_of("\\/") + 1);
    base = base.substr(0, base.find_last_of('.'));
    g_sramPath = g_saveDir + "\\" + base + ".srm";
  }
  LoadSram();
  {
    std::ofstream options(g_saveDir + "\\core-options.txt");
    for (const auto &o : g_optionList) options << o.first << " = " << g_optionValues[o.first] << "    (" << o.second << ")\n";
  }

  GlWindow view; bool theatreOn = false;
  if (!headless) {
    if (!view.create()) { Log("Hidden OpenGL context unavailable"); return 5; }
    g_audioWanted = !mute && g_audio.open((unsigned)std::lround(g_av.timing.sample_rate));
    if (!g_audioWanted) Log(mute ? "Muted" : "Audio device unavailable: running silent");
    if (!noXr) {
      const auto config = TheatreConfig();
      theatreOn = theatre::Start(view.window, config);
      Log("Theatre: screen %.2f m wide at %.2f m, curve %.2f%s; gun model %s: %s", config.width, config.distance, config.curve,
          config.crt ? ", scanlines" : "", config.gunModel.empty() ? "none" : config.gunModel.c_str(), theatreOn ? "presenter started" : "presenter unavailable");
    }
  }

  const char *stopEnv = std::getenv("ACVR_PS1_STOP_FILE");
  const std::string stopFile = stopEnv && *stopEnv ? stopEnv : "";
  if (!stopFile.empty()) Log("Clean stop file: %s", stopFile.c_str());
  CleanQuit quit;
  const double frameMs = 1000.0 / (g_av.timing.fps > 1 ? g_av.timing.fps : 60.0);
  LARGE_INTEGER freq, t0; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&t0);
  unsigned frame = 0; std::uint64_t shownSerial = 0, shots = 0;
  const ULONGLONG startedAt = GetTickCount64();
  bool lastTrigger = false; ULONGLONG lastSave = GetTickCount64();
  int result = 0;

  while (!g_shutdownRequested) {
    const ULONGLONG now = GetTickCount64();
    theatre::Input controller = theatreOn ? theatre::ReadInput() : theatre::Input{};
    if (testAim) { controller.active = true; controller.offScreen = false; controller.u = testU; controller.v = testV; }
    if (!headless) {
      const DWORD attr = stopFile.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesA(stopFile.c_str());
      quit.poll(now, controller.quitHeld, attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
      if (GetAsyncKeyState(VK_ESCAPE) & 1) quit.request(CleanQuit::Reason::escape);
      if (seconds && now - startedAt >= seconds * 1000ull) quit.request(CleanQuit::Reason::time_limit);
      if (quit.requested()) { Log("Clean quit: %s", quit.reason()); break; }
      Gun g; g.u = controller.u; g.v = controller.v; g.offScreen = !controller.active || controller.offScreen;
      // GunCon A is also on the grip, as the pedal (Time Crisis takes cover with it).
      g.trigger = controller.trigger; g.a = controller.a || controller.grip; g.b = controller.b;
      g.start = controller.a;   // the Justifier's side button; arcade Start
      g.select = gunName == "lightgun" && controller.b;   // arcade Coin
      Pad p; p.left = controller.padX < -.5f; p.right = controller.padX > .5f; p.up = controller.padY > .5f; p.down = controller.padY < -.5f;
      p.cross = controller.padCross; p.circle = controller.padCircle; p.square = controller.padSquare; p.start = controller.padStart;
      g_pad = p;
      g_gun = g;
    } else {
      if (frames && frame >= frames) break;
      while (timeline.next < timeline.events.size() && timeline.events[timeline.next].frame <= frame) {
        const auto &e = timeline.events[timeline.next++];
        const unsigned n = (unsigned)std::atoi(Arg(e.args, 0, "6").c_str());
        if (e.cmd == "aim") { g_gun.u = (float)std::atof(Arg(e.args, 0, ".5").c_str()); g_gun.v = (float)std::atof(Arg(e.args, 1, ".5").c_str()); g_gun.offScreen = false; }
        else if (e.cmd == "trigger") timeline.holdTrigger = n;
        else if (e.cmd == "a") timeline.holdA = n;
        else if (e.cmd == "b") timeline.holdB = n;
        else if (e.cmd == "start") timeline.holdStart = n;
        else if (e.cmd == "select") timeline.holdSelect = n;
        else if (e.cmd == "offscreen") timeline.holdOff = n;
        else if (e.cmd.rfind("pad-", 0) == 0) timeline.pad[e.cmd.substr(4)] = n;
        else if (e.cmd == "reload") { timeline.holdOff = 8; timeline.holdTrigger = 6; }
        else if (e.cmd == "snap" && !snapshots.empty()) WriteSnapshot(snapshots, Arg(e.args, 0, ("f" + std::to_string(frame)).c_str()));
        else if (e.cmd == "save") SaveSram(true);
        else if (e.cmd == "quit") { frames = frame; }
        else Log("Unknown script command '%s' at frame %u", e.cmd.c_str(), e.frame);
      }
      g_gun.trigger = timeline.holdTrigger > 0; g_gun.a = timeline.holdA > 0; g_gun.b = timeline.holdB > 0;
      g_gun.start = timeline.holdStart > 0; g_gun.select = timeline.holdSelect > 0;
      auto held = [&](const char *k) { auto it = timeline.pad.find(k); return it != timeline.pad.end() && it->second > 0; };
      g_pad.up = held("up"); g_pad.down = held("down"); g_pad.left = held("left"); g_pad.right = held("right");
      g_pad.cross = held("cross"); g_pad.circle = held("circle"); g_pad.square = held("square"); g_pad.triangle = held("triangle");
      g_pad.start = held("start"); g_pad.select = held("select");
      for (auto &kv : timeline.pad) if (kv.second) --kv.second;
      const bool off = timeline.holdOff > 0;
      static bool aimed = false; aimed = aimed || !g_gun.offScreen;
      g_gun.offScreen = off || !aimed;
      for (unsigned *h : { &timeline.holdTrigger, &timeline.holdA, &timeline.holdB, &timeline.holdStart, &timeline.holdSelect, &timeline.holdOff }) if (*h) --*h;
      if (frames && frame >= frames) break;
    }
    if (g_gun.trigger && !lastTrigger) { ++shots; if (theatreOn) theatre::QueuePulse(); }
    lastTrigger = g_gun.trigger;

    g_core.run();
    ++frame;

    if (!headless && g_frameSerial != shownSerial && !g_frame.empty()) {
      shownSerial = g_frameSerial;
      if (theatreOn) theatre::Publish(g_frame.data(), g_frameW, g_frameH);
    }
    if (now - lastSave > 10000) { SaveSram(false); lastSave = now; }

    if (!headless) {
      if (g_audioWanted) { for (int i = 0; i < 100 && g_audio.queued() > 6; ++i) Sleep(1); }
      else {
        LARGE_INTEGER t; QueryPerformanceCounter(&t);
        const double due = frame * frameMs, elapsed = (t.QuadPart - t0.QuadPart) * 1000.0 / freq.QuadPart;
        if (due > elapsed + 1) Sleep((DWORD)(due - elapsed));
      }
    }
  }
  if (g_shutdownRequested) Log("The core asked to shut down");
  if (headless && !snapshots.empty()) WriteSnapshot(snapshots, "final");
  if (!theatreProof.empty() && !g_frame.empty()) {
    if (!view.create()) Log("Theatre proof: no OpenGL context");
    else {
      std::string note;
      const bool ok = theatre::RenderProof(TheatreConfig(), g_frame.data(), g_frameW, g_frameH, testU, testV, theatreProof, note);
      Log("Theatre proof %s-left.png / -right.png: %s (%s)", theatreProof.c_str(), ok ? "written" : "FAILED", note.c_str());
      if (!ok) result = 8;
    }
  }
  Log("Ran %u frames; gun reads %llu; trigger presses %llu; last picture %ux%u", frame,
      (unsigned long long)g_gunReads, (unsigned long long)shots, g_frameW, g_frameH);
  if (!SaveSram(false)) result = 6;
  g_core.unload_game();
  g_core.deinit();
  if (theatreOn) theatre::Stop();
  g_audio.close();
  view.destroy();
  Log("Clean exit");
  if (g_log != stdout) std::fclose(g_log);
  return result;
}
