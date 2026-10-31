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
#include "PadControls.h"
#include "GlStereo.h"
#include "HostConfig.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <regex>
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
  void (*cheat_reset)() = nullptr;                       // optional
  void (*cheat_set)(unsigned, bool, const char *) = nullptr;   // optional
  std::size_t (*serialize_size)() = nullptr;                    // optional: offline proofs start from a saved state
  bool (*serialize)(void *, std::size_t) = nullptr;
  bool (*unserialize)(const void *, std::size_t) = nullptr;
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
  g_core.cheat_reset = reinterpret_cast<decltype(g_core.cheat_reset)>(GetProcAddress(g_core.dll, "retro_cheat_reset"));
  g_core.cheat_set = reinterpret_cast<decltype(g_core.cheat_set)>(GetProcAddress(g_core.dll, "retro_cheat_set"));
  g_core.serialize_size = reinterpret_cast<decltype(g_core.serialize_size)>(GetProcAddress(g_core.dll, "retro_serialize_size"));
  g_core.serialize = reinterpret_cast<decltype(g_core.serialize)>(GetProcAddress(g_core.dll, "retro_serialize"));
  g_core.unserialize = reinterpret_cast<decltype(g_core.unserialize)>(GetProcAddress(g_core.dll, "retro_unserialize"));
  return true;
}

// ---- host state seen by the core callbacks
std::string g_corePath, g_systemDir, g_saveDir;
std::map<std::string, std::string> g_optionOverrides, g_optionValues;
std::vector<std::pair<std::string, std::string>> g_optionList;   // key, "description; a|b|c"
retro_pixel_format g_pixelFormat = RETRO_PIXEL_FORMAT_0RGB1555;
retro_system_av_info g_av{};
std::vector<retro_controller_description> g_controllers[2];
std::deque<std::string> g_controllerNames[2];   // a deque keeps each name (and its desc pointer) in place
std::set<unsigned> g_unknownCommands;
bool g_shutdownRequested = false;

// A hardware-rendering core draws into our framebuffer; its picture is read back only when a
// CPU copy is needed (offline pictures), and copied on the GPU for the headset.
retro_hw_render_callback g_hw{};
bool g_hwOn = false, g_hwCpuStale = false;
GLuint g_hwFbo = 0, g_hwColor = 0, g_hwDepth = 0;
unsigned g_hwW = 0, g_hwH = 0, g_hwMaxW = 0, g_hwMaxH = 0;
Host_Window *g_glWindow = nullptr;
std::uintptr_t HwFramebuffer() { return g_hwFbo; }
retro_proc_address_t HwProc(const char *sym)
{
  auto p = reinterpret_cast<retro_proc_address_t>(wglGetProcAddress(sym));
  if (!p) p = reinterpret_cast<retro_proc_address_t>(GetProcAddress(GetModuleHandleA("opengl32.dll"), sym));
  return glstereo::Wrap(sym, p);   // unchanged unless GL tracing or stereo is on
}
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
// --controls pad|wheel|aim: port 1 is a RetroPad with analog sticks (Nintendo 64 games) instead of a light gun.
std::string g_controls = "gun";
pad::Config g_padCfg;
pad::RetroPad g_rpad;
bool PadMode() { return g_controls != "gun"; }
// ACVR_PAD_FOLLOW_U / _V = "c0,addr:k,addr:k...": the game's target marker on the picture (0..1) as c0 + sum of
// k * float at addr in the console's memory (fitted offline against the marker in snapshots).
struct FollowAxis { bool on = false; float c0 = 0; std::vector<std::pair<std::size_t, float>> terms; float prev = 0, sum = 0; bool seen = false; };
FollowAxis g_follow[2];
void LoadFollow()
{
  const char *names[2] = { "ACVR_PAD_FOLLOW_U", "ACVR_PAD_FOLLOW_V" };
  for (int i = 0; i < 2; ++i) {
    const char *e = std::getenv(names[i]); if (!e || !*e) continue;
    std::istringstream s(e); std::string item; bool first = true;
    while (std::getline(s, item, ',')) {
      if (first) { g_follow[i].c0 = (float)std::atof(item.c_str()); first = false; continue; }
      const auto colon = item.find(':'); if (colon == std::string::npos) continue;
      g_follow[i].terms.emplace_back((std::size_t)std::strtoul(item.substr(0, colon).c_str(), nullptr, 16) & 0x7ffffcu, (float)std::atof(item.substr(colon + 1).c_str()));
    }
    g_follow[i].on = !g_follow[i].terms.empty();
  }
}
// Fills where the marker is now (and how far it moved) into the controller state used for this frame's pad.
void FollowInto(theatre::Input &in)
{
  if (!g_follow[0].on && !g_follow[1].on) return;
  const auto *ram = static_cast<const unsigned char *>(g_core.get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
  const std::size_t size = g_core.get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
  if (!ram) return;
  float cur[2] = { .5f, .5f }, vel[2] = { 0, 0 };
  for (int i = 0; i < 2; ++i) {
    auto &f = g_follow[i]; if (!f.on) continue;
    float v = f.c0;
    for (const auto &t : f.terms) { float x = 0; if (t.first + 4 <= size) std::memcpy(&x, ram + t.first, 4); if (std::isfinite(x)) v += t.second * x; }
    vel[i] = f.seen ? v - f.prev : 0; f.prev = v; f.seen = true; cur[i] = v;
    // The error's integral, held so its share of the stick stays within full deflection.
    const float gain = g_padCfg.follow[4 + i], target = i ? in.v : in.u;
    const bool aiming = in.active && !in.offScreen;
    f.sum = aiming && gain != 0 ? std::clamp(f.sum + (target - v), -1.f / std::fabs(gain), 1.f / std::fabs(gain)) : 0.f;
  }
  in.hasCurrent = true; in.curU = cur[0]; in.curV = cur[1]; in.velU = vel[0]; in.velV = vel[1]; in.sumU = g_follow[0].sum; in.sumV = g_follow[1].sum;
}
// Pad games: the controller's rumble (an N64 Rumble Pak) becomes a pulse on the right controller, at most
// every 80 ms while the game keeps it on.
bool g_rumbleOn = false; std::uint64_t g_rumbleStarts = 0;
bool SetRumble(unsigned port, retro_rumble_effect, std::uint16_t strength)
{
  if (port != 0) return false;
  const bool on = strength > 0;
  if (on && !g_rumbleOn) ++g_rumbleStarts;
  g_rumbleOn = on; return true;
}

void CoreLog(retro_log_level level, const char *fmt, ...)
{
  static const char *names[] = { "debug", "info", "warn", "error" };
  char line[2048]; va_list ap; va_start(ap, fmt); std::vsnprintf(line, sizeof line, fmt, ap); va_end(ap);
  std::size_t n = std::strlen(line); while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
  if (level >= RETRO_LOG_INFO) Log("[core %s] %s", names[level & 3], line);
}

void FrameShapeChanged();   // below, with the theatre's other settings

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
  case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
    // Logged once for pad games: the core's own names for each RetroPad input (how the buttons map).
    static bool logged = false;
    if (PadMode() && !logged) {
      logged = true;
      for (auto *d = static_cast<const retro_input_descriptor *>(data); d && d->description; ++d)
        if (d->port == 0) Log("Input: device %u index %u id %u = %s", d->device, d->index, d->id, d->description);
    }
    return true;
  }
  case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
    if (!PadMode()) return false;
    static_cast<retro_rumble_interface *>(data)->set_rumble_state = SetRumble; return true;
  case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
  case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: return true;
  case RETRO_ENVIRONMENT_SHUTDOWN: g_shutdownRequested = true; return true;
  case RETRO_ENVIRONMENT_SET_MESSAGE: { auto *m = static_cast<const retro_message *>(data); if (m && m->msg) Log("[core message] %s", m->msg); return true; }
  case RETRO_ENVIRONMENT_SET_GEOMETRY: g_av.geometry = *static_cast<const retro_game_geometry *>(data); FrameShapeChanged(); return true;
  case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: g_av = *static_cast<const retro_system_av_info *>(data); FrameShapeChanged(); return true;
  case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER: *static_cast<unsigned *>(data) = RETRO_HW_CONTEXT_OPENGL_CORE; return true;
  case RETRO_ENVIRONMENT_SET_HW_RENDER: {
    // Desktop OpenGL only. The driver's compatibility context also serves core-profile requests.
    auto *cb = static_cast<retro_hw_render_callback *>(data);
    if (cb->context_type != RETRO_HW_CONTEXT_OPENGL && cb->context_type != RETRO_HW_CONTEXT_OPENGL_CORE) return false;
    if (!g_glWindow) {
      g_glWindow = Host_CreateWindow("ACVR PS1", 0, 0, 16, 16, Host_WINDOW_HIDDEN);
      if (!g_glWindow || !Host_GL_CreateContext(g_glWindow) || !Host_LoadGL()) { Log("No OpenGL context for the core"); return false; }
    }
    cb->get_current_framebuffer = HwFramebuffer; cb->get_proc_address = HwProc;
    g_hw = *cb; g_hwOn = true;
    Log("Core renders with OpenGL %u.%u (%s)", cb->version_major, cb->version_minor, glGetString(GL_VERSION));
    return true;
  }
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
    for (unsigned p = 0; p < 2; ++p) { g_controllers[p].clear(); g_controllerNames[p].clear(); }   // the latest call describes the ports
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
  if (data == RETRO_HW_FRAME_BUFFER_VALID) { g_hwW = w; g_hwH = h; g_hwCpuStale = true; ++g_frameSerial; return; }
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

// The 3DS touch panel: how often the core asked for the pointer, and how often with the stylus down (logged at the end).
std::uint64_t g_pointerReads = 0, g_pointerDownReads = 0;
std::int16_t InputState(unsigned port, unsigned device, unsigned index, unsigned id)
{
  if (PadMode()) {
    if (port != 0) return 0;
    const std::int16_t value = pad::State(g_rpad, device, index, id);
    if ((device & RETRO_DEVICE_MASK) == RETRO_DEVICE_POINTER) {
      if (!g_pointerReads++) Log("The core polls the pointer (RETRO_DEVICE_POINTER)");
      if (id == RETRO_DEVICE_ID_POINTER_PRESSED && value && !g_pointerDownReads++)
        Log("First pointer read with the stylus down: x %d, y %d (of -32767..32767 over the core's whole frame)",
            (int)pad::State(g_rpad, device, index, RETRO_DEVICE_ID_POINTER_X), (int)pad::State(g_rpad, device, index, RETRO_DEVICE_ID_POINTER_Y));
    }
    return value;
  }
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
  // Per-game aim trim, measured offline against where each game itself registers the shot:
  // ACVR_PS1_AIM = "x scale, x shift, y scale, y shift" around the screen centre (default 1,0,1,0 = unchanged).
  // Aim that lands outside after the trim is held at the edge, never read as off screen (that would reload).
  static const std::array<float, 4> trim = [] {
    std::array<float, 4> t{ 1, 0, 1, 0 }; const char *s = getenv("ACVR_PS1_AIM");
    if (s && *s) { float v[4]; if (std::sscanf(s, "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]) == 4 && v[0] > .5f && v[0] < 2 && v[2] > .5f && v[2] < 2) t = { v[0], v[1], v[2], v[3] }; }
    return t; }();
  const float gunU = std::clamp(.5f + (g_gun.u - .5f) * trim[0] + trim[1], .002f, .998f);
  const float gunV = std::clamp(.5f + (g_gun.v - .5f) * trim[2] + trim[3], .002f, .998f);
  if (base == RETRO_DEVICE_LIGHTGUN) {
    ++g_gunReads;
    switch (id) {
    case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X: return g_gun.offScreen ? -32768 : axis(gunU);
    case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y: return g_gun.offScreen ? -32768 : axis(gunV);
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
    case RETRO_DEVICE_ID_POINTER_X: return g_gun.offScreen ? -32768 : axis(gunU);
    case RETRO_DEVICE_ID_POINTER_Y: return g_gun.offScreen ? -32768 : axis(gunV);
    case RETRO_DEVICE_ID_POINTER_PRESSED: return g_gun.trigger;
    default: return 0;
    }
  }
  return 0;
}

// ---- per-game cheat codes, e.g. "No Gun Flash" (the CRT beam-sync flash a GunCon needs, pointless in VR).
// ACVR_PS1_CHEAT_FILE: a DuckStation-format .cht file, or a RetroArch-format one; ACVR_PS1_CHEATS: the [section]
// names (RetroArch: descriptions or cheatN) to enable, separated by ';'.
void ApplyCheats()
{
  const char *file = std::getenv("ACVR_PS1_CHEAT_FILE"), *names = std::getenv("ACVR_PS1_CHEATS");
  if (!file || !*file || !names || !*names) return;
  if (!g_core.cheat_set) { Log("Cheats: the core has no retro_cheat_set"); return; }
  std::ifstream in(file); if (!in) { Log("Cheats: cannot read %s", file); return; }
  std::vector<std::string> wanted; { std::string s(names), item; std::istringstream ss(s); while (std::getline(ss, item, ';')) if (!item.empty()) wanted.push_back(item); }
  std::map<std::string, std::string> codes; std::string section, line;
  const std::regex header(R"(^\s*\[(.+)\]\s*$)"), code(R"(^\s*([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{4,8})\s*$)");
  // RetroArch-format files (cheatN_desc = "...", cheatN_code = "..."): an entry is named by its description or "cheatN".
  const std::regex raDesc(R"re(^\s*cheat(\d+)_desc\s*=\s*"(.*)"\s*$)re"), raCode(R"re(^\s*cheat(\d+)_code\s*=\s*"(.*)"\s*$)re");
  std::map<std::string, std::string> raDescs;
  std::smatch m;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (std::regex_match(line, m, header)) section = m[1].str();
    else if (!section.empty() && std::regex_match(line, m, code)) { auto &c = codes[section]; if (!c.empty()) c += '+'; c += m[1].str() + " " + m[2].str(); }
    else if (std::regex_match(line, m, raDesc)) raDescs[m[1].str()] = m[2].str();
    else if (std::regex_match(line, m, raCode)) codes["cheat" + m[1].str()] = m[2].str();
  }
  for (const auto &d : raDescs) { const auto c = codes.find("cheat" + d.first); if (c != codes.end() && !codes.count(d.second)) codes[d.second] = c->second; }
  if (g_core.cheat_reset) g_core.cheat_reset();
  unsigned index = 0;
  for (const auto &n : wanted) {
    const auto it = codes.find(n);
    if (it == codes.end()) { Log("Cheats: no [%s] in %s", n.c_str(), file); continue; }
    g_core.cheat_set(index++, true, it->second.c_str());
    Log("Cheat on: %s (%s)", n.c_str(), it->second.c_str());
  }
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

// ---- emulator states for offline proofs ("state-save <file>" / "state-load <file>" script lines)
bool SaveState(const std::string &path)
{
  if (!g_core.serialize_size || !g_core.serialize) { Log("State: the core cannot save states"); return false; }
  std::vector<char> data(g_core.serialize_size());
  if (data.empty() || !g_core.serialize(data.data(), data.size())) { Log("State save failed"); return false; }
  std::ofstream out(path, std::ios::binary); out.write(data.data(), (std::streamsize)data.size());
  Log("State saved: %s (%zu bytes)", path.c_str(), data.size()); return (bool)out;
}
bool LoadState(const std::string &path)
{
  if (!g_core.unserialize) { Log("State: the core cannot load states"); return false; }
  std::ifstream in(path, std::ios::binary);
  std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const bool ok = !data.empty() && g_core.unserialize(data.data(), data.size());
  Log("State load %s: %s", path.c_str(), ok ? "done" : "FAILED"); return ok;
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

std::vector<std::uint32_t> g_hudFrame;   // the HUD layer's last picture (ACVR_GL_HUD)
// The hardware core's last picture as a CPU copy (its rows are bottom-up). With GL stereo on, both eyes
// side by side: left eye in the left half.
void EnsureCpuFrame()
{
  if (!g_hwOn || !g_hwCpuStale || !g_hwW || !g_hwH) return;
  const unsigned right = glstereo::RightFramebuffer(), eyes = right ? 2 : 1;
  g_frame.resize((std::size_t)g_hwW * eyes * g_hwH); g_frameW = g_hwW * eyes; g_frameH = g_hwH;
  std::vector<std::uint32_t> px((std::size_t)g_hwW * g_hwH);
  for (unsigned e = 0; e < eyes; ++e) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, e ? right : g_hwFbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, g_hwW, g_hwH, GL_BGRA, GL_UNSIGNED_BYTE, px.data());
    for (unsigned y = 0; y < g_hwH; ++y) for (unsigned x = 0; x < g_hwW; ++x)
      g_frame[(std::size_t)y * g_frameW + e * g_hwW + x] = px[(std::size_t)(g_hwH - 1 - y) * g_hwW + x] & 0xffffffu;
  }
  // The HUD layer, when on: 0xAARRGGBB, colour premultiplied; alpha from the layer over black and over white.
  if (const unsigned hud = glstereo::HudFramebuffer()) {
    g_hudFrame.resize((std::size_t)g_hwW * g_hwH);
    std::vector<std::uint32_t> white(px.size());
    const char *key = std::getenv("ACVR_HUD_KEY_BLACK"); const bool keyBlack = key && std::atoi(key) != 0;
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, hud); glReadPixels(0, 0, g_hwW, g_hwH, GL_BGRA, GL_UNSIGNED_BYTE, px.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, glstereo::HudFramebuffer(true)); glReadPixels(0, 0, g_hwW, g_hwH, GL_BGRA, GL_UNSIGNED_BYTE, white.data());
    for (unsigned y = 0; y < g_hwH; ++y) for (unsigned x = 0; x < g_hwW; ++x) {
      const std::size_t i = (std::size_t)(g_hwH - 1 - y) * g_hwW + x;
      const std::uint32_t b = px[i], w = white[i]; int d = 0;
      for (int sh : { 0, 8, 16 }) d = std::max(d, (int)((w >> sh) & 255) - (int)((b >> sh) & 255));
      std::uint32_t argb = ((std::uint32_t)(255 - std::clamp(d, 0, 255)) << 24) | (b & 0xffffffu);
      // ACVR_HUD_KEY_BLACK=1: a game's opaque black border bars in its 2D layer show as transparent.
      if (keyBlack && ((b >> 16) & 255) < 3 && ((b >> 8) & 255) < 3 && (b & 255) < 3) argb = 0;
      g_hudFrame[(std::size_t)y * g_hwW + x] = argb;
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  g_hwCpuStale = false;
}
// With GL stereo on, the headset gets both eyes side by side in one picture.
GLuint g_sbsFbo = 0, g_sbsTex = 0; unsigned g_sbsW = 0, g_sbsH = 0;
// The core's OpenGL state that publishing touches is put back afterwards: a core that caches its own state
// (GLideN64) would otherwise draw with a binding or scissor it no longer has.
struct KeepGlState {
  GLint draw = 0, read = 0, tex = 0; GLboolean scissor = GL_FALSE;
  KeepGlState() { glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read); glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex); scissor = glIsEnabled(GL_SCISSOR_TEST); }
  ~KeepGlState() { glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw); glBindFramebuffer(GL_READ_FRAMEBUFFER, read); glBindTexture(GL_TEXTURE_2D, tex); if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST); }
};
void PublishHardwareFrame()
{
  std::optional<KeepGlState> keep;   // pad cores (GLideN64) and stereo; the PS1 light-gun path stays as it was
  if (PadMode() || glstereo::StereoOn() || glstereo::HudOn()) keep.emplace();
  if (const unsigned hud = glstereo::HudFramebuffer()) theatre::PublishHudFramebuffer(hud, glstereo::HudFramebuffer(true), g_hwW, g_hwH);
  const unsigned right = glstereo::RightFramebuffer();
  if (!right) { theatre::PublishFramebuffer(g_hwFbo, g_hwW, g_hwH); return; }
  if (!g_sbsFbo || g_sbsW != g_hwW * 2 || g_sbsH != g_hwH) {
    if (!g_sbsFbo) { glGenFramebuffers(1, &g_sbsFbo); glGenTextures(1, &g_sbsTex); }
    g_sbsW = g_hwW * 2; g_sbsH = g_hwH;
    glBindTexture(GL_TEXTURE_2D, g_sbsTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_sbsW, g_sbsH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, g_sbsFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_sbsTex, 0);
  }
  glDisable(GL_SCISSOR_TEST);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_sbsFbo);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, g_hwFbo); glBlitFramebuffer(0, 0, g_hwW, g_hwH, 0, 0, g_hwW, g_hwH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, right); glBlitFramebuffer(0, 0, g_hwW, g_hwH, g_hwW, 0, 2 * g_hwW, g_hwH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  theatre::PublishFramebuffer(g_sbsFbo, g_sbsW, g_sbsH);
}

// A light-gun game blanks the screen white for a frame or two on each shot so a real GunCon can see the CRT
// beam. The emulated gun needs no flash, but some games only read the gun during it (Time Crisis misses
// without it), so the game keeps drawing it and the headset simply keeps the previous picture instead.
// Three lines across the whole picture (both eyes) are sampled, only in the frames just after a shot;
// nearly all bright white means a flash.
bool IsFlashFrame()
{
  const unsigned w = g_hwOn ? g_hwW : g_frameW, h = g_hwOn ? g_hwH : g_frameH;
  if (!w || !h) return false;
  unsigned bright = 0, total = 0;
  // ACVR_PS1_FLASH_CHANNELS: which colour channels must be bright (default "rgb", white). Area 51 on the Saturn
  // flashes blue: "b".
  static const unsigned channels = [] {
    const char *e = std::getenv("ACVR_PS1_FLASH_CHANNELS"); unsigned m = 0;
    for (const char *c = e ? e : ""; *c; ++c) m |= *c == 'r' ? 4u : *c == 'g' ? 2u : *c == 'b' ? 1u : 0u;
    return m ? m : 7u; }();
  std::vector<std::uint32_t> row(w);
  if (g_hwOn) { glBindFramebuffer(GL_READ_FRAMEBUFFER, g_hwFbo); glPixelStorei(GL_PACK_ALIGNMENT, 4); }
  for (const unsigned y : { h / 4, h / 2, (h * 3) / 4 }) {
    const std::uint32_t *px = nullptr;
    if (g_hwOn) { glReadPixels(0, (GLint)y, (GLsizei)w, 1, GL_BGRA, GL_UNSIGNED_BYTE, row.data()); px = row.data(); }
    else px = g_frame.data() + (std::size_t)y * w;
    for (unsigned x = 0; x < w; x += 2) {
      const std::uint32_t c = px[x];
      const unsigned r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
      ++total; if ((!(channels & 4) || r > 200) && (!(channels & 2) || g > 200) && (!(channels & 1) || b > 200)) ++bright;
    }
  }
  if (g_hwOn) glBindFramebuffer(GL_FRAMEBUFFER, 0);
  // ACVR_PS1_FLASH_SHARE: the share of bright samples (percent, default 85) that makes a flash. House of the Dead on
  // the Saturn whitens the picture over the scene rather than painting it white, so some of its flashes only reach 77%.
  static const unsigned share = [] { const char *e = std::getenv("ACVR_PS1_FLASH_SHARE"); const int v = e ? std::atoi(e) : 0; return v >= 50 && v <= 100 ? (unsigned)v : 85u; }();
  return total && bright * 100 >= total * share;
}
unsigned g_flashRun = 0, g_flashHidden = 0, g_flashLongest = 0;
bool g_hideFlash = true;   // ACVR_PS1_HIDE_FLASH, read in main once a profile has been applied
// The frame the gun's trigger was last down; a flash only follows a shot (autofire included), so white
// menus and fades with no shot always show, and no picture is read back while nobody is shooting.
unsigned g_triggerFrame = 0; bool g_triggerSeen = false;
const unsigned kFlashWindow = 20;   // frames (a third of a second) after the trigger in which a white frame counts as a gun flash
bool g_flashAudit = false;          // offline runs also log white frames outside that window (they stay visible)
unsigned g_flashOutside = 0;
// True when this frame should not be shown: a flash right after a shot, and not yet more than 4 in a row
// (a real fade to white shows).
bool HideThisFrame(unsigned frame)
{
  if (!g_hideFlash) return false;
  if (!g_triggerSeen || frame - g_triggerFrame > kFlashWindow) {
    if (g_flashAudit && IsFlashFrame() && ++g_flashOutside <= 40)
      Log("White frame %u shown: %s", frame, g_triggerSeen ? (std::to_string(frame - g_triggerFrame) + " frames after the trigger").c_str() : "no shot yet");
    g_flashRun = 0; return false;
  }
  if (!IsFlashFrame()) { g_flashRun = 0; return false; }
  ++g_flashRun; g_flashLongest = g_flashRun > g_flashLongest ? g_flashRun : g_flashLongest;
  if (g_flashRun > 4) return false;
  ++g_flashHidden; return true;
}

// Offline, 3DS touch panel on: the two rectangles the theatre cuts from the frame, saved on their own: <name>-top.png (the top
// screen, both eyes) and <name>-panel.png (the bottom screen as the panel shows it). A cross marks the stylus: yellow over
// the panel, magenta while it is down.
void WriteTouchCrops(const std::string &dir, const std::string &name)
{
  const auto &t = g_padCfg.touch;
  auto crop = [&](const float r[4], const char *suffix, bool cross) {
    const unsigned x0 = std::min(g_frameW - 1, (unsigned)std::lround(r[0] * g_frameW)), y0 = std::min(g_frameH - 1, (unsigned)std::lround(r[1] * g_frameH));
    const unsigned w = std::max(1u, std::min(g_frameW - x0, (unsigned)std::lround(r[2] * g_frameW))), h = std::max(1u, std::min(g_frameH - y0, (unsigned)std::lround(r[3] * g_frameH)));
    std::vector<std::uint32_t> px((std::size_t)w * h);
    for (unsigned y = 0; y < h; ++y) std::copy_n(g_frame.data() + (std::size_t)(y0 + y) * g_frameW + x0, w, px.data() + (std::size_t)y * w);
    if (cross && g_rpad.pointerOn) {
      const int cx = (int)std::lround((g_rpad.px + 1.f) / 2.f * g_frameW) - (int)x0, cy = (int)std::lround((g_rpad.py + 1.f) / 2.f * g_frameH) - (int)y0;
      const std::uint32_t colour = g_rpad.pointerDown ? 0xff00ffu : 0xffff00u;
      for (int d = -10; d <= 10; ++d) {
        if (cx + d >= 0 && cx + d < (int)w && cy >= 0 && cy < (int)h) px[(std::size_t)cy * w + cx + d] = colour;
        if (cy + d >= 0 && cy + d < (int)h && cx >= 0 && cx < (int)w) px[(std::size_t)(cy + d) * w + cx] = colour;
      }
    }
    const std::string path = dir + "\\" + name + suffix + ".png";
    Log("Snapshot %s %ux%u: %s", path.c_str(), w, h, acvr_png::write(path.c_str(), px.data(), w, h) ? "written" : "FAILED");
  };
  crop(t.top, "-top", false); crop(t.bottom, "-panel", true);
}

bool WriteSnapshot(const std::string &dir, const std::string &name)
{
  EnsureCpuFrame();
  if (g_frame.empty()) return false;
  const std::string path = dir + "\\" + name + ".png";
  const bool ok = acvr_png::write(path.c_str(), g_frame.data(), g_frameW, g_frameH);
  Log("Snapshot %s %ux%u: %s", path.c_str(), g_frameW, g_frameH, ok ? "written" : "FAILED");
  if (PadMode() && g_padCfg.touch.on) WriteTouchCrops(dir, name);
  if (!g_hudFrame.empty() && g_hudFrame.size() == (std::size_t)g_hwW * g_hwH) {
    // The HUD layer alone, over a grey checkerboard so its transparency shows.
    std::vector<std::uint32_t> view(g_hudFrame.size());
    for (unsigned y = 0; y < g_hwH; ++y) for (unsigned x = 0; x < g_hwW; ++x) {
      const std::uint32_t c = g_hudFrame[(std::size_t)y * g_hwW + x]; const unsigned a = c >> 24;
      const unsigned bg = ((x / 16 + y / 16) & 1) ? 96 : 160;
      auto ch = [&](int shift) { return std::min(255u, ((c >> shift) & 255) + bg * (255 - a) / 255); };
      view[(std::size_t)y * g_hwW + x] = (ch(16) << 16) | (ch(8) << 8) | ch(0);
    }
    const std::string hudPath = dir + "\\" + name + "-hud.png";
    Log("Snapshot %s (HUD layer): %s", hudPath.c_str(), acvr_png::write(hudPath.c_str(), view.data(), g_hwW, g_hwH) ? "written" : "FAILED");
  }
  return ok;
}

// ---- the hidden window whose OpenGL context the theatre shares
struct GlWindow {
  Host_Window *window = nullptr;
  bool create()
  {
    if (!window && g_glWindow) window = g_glWindow;   // the hardware core's context, shared with the theatre
    if (window) return true;
    window = Host_CreateWindow("ACVR PS1", 0, 0, 16, 16, Host_WINDOW_HIDDEN);
    if (!window || !Host_GL_CreateContext(window)) return false;
    Log("Hidden GL context: %s", glGetString(GL_VERSION));
    return true;
  }
  void destroy() { if (window) Host_DestroyWindow(window); window = nullptr; g_glWindow = nullptr; }
};

float EnvFloat(const char *name, float fallback)
{
  const char *v = std::getenv(name);
  if (!v || !*v) return fallback;
  const float f = (float)std::atof(v);
  return std::isfinite(f) ? f : fallback;
}

// The picture's shape from the core's frame geometry: the aspect the screen is drawn at and, for a core that puts
// both eyes side by side in a frame sized for one screen (Azahar's side-by-side-full: 1200x720 at 3x, each eye
// 600x360 in the middle rows), the share of rows cut top and bottom. The eye's content has the reported aspect, so
// the cut is (1 - (W/2) / aspect / H) / 2, and the screen's aspect is (W/2) / (H x (1 - 2 x cut)). A frame whose
// shape does not fit that reading is not cut. ACVR_PS1_LETTERBOX overrides the cut.
void FrameShape(float &aspect, float &letterbox, float *panelAspect = nullptr)
{
  const auto &g = g_av.geometry;
  const float reported = g.aspect_ratio > .1f ? g.aspect_ratio : 4.f / 3.f, w = (float)g.base_width, h = (float)g.base_height;
  // The 3DS touch panel: the top screen is a rectangle of the frame (each eye half of it, when the core draws them side by
  // side) and the bottom screen another; their aspects come from the rectangles and the frame's size.
  if (g_padCfg.touch.on && w > 0 && h > 0) {
    const auto &t = g_padCfg.touch;
    const bool sbs = EnvFloat("ACVR_PS1_SBS", 0.f) != 0.f && !glstereo::StereoOn();
    letterbox = 0;
    aspect = (t.top[2] * w / (sbs ? 2.f : 1.f)) / (t.top[3] * h);
    if (panelAspect) *panelAspect = (t.bottom[2] * w) / (t.bottom[3] * h);
    return;
  }
  // Only a core that draws the eyes itself: with GL stereo the host builds the side-by-side picture from full-size eyes.
  const bool coreSbs = EnvFloat("ACVR_PS1_SBS", 0.f) != 0.f && !glstereo::StereoOn(), known = w > 0 && h > 0;
  const char *forced = std::getenv("ACVR_PS1_LETTERBOX");
  letterbox = 0;
  if (forced && *forced) letterbox = std::clamp(EnvFloat("ACVR_PS1_LETTERBOX", 0.f), 0.f, .45f);
  else if (coreSbs && known && std::fabs(w / h / reported - 1.f) < .02f) letterbox = std::clamp((1.f - w / 2 / reported / h) / 2, 0.f, .45f);
  if (letterbox < .001f) letterbox = 0;
  aspect = coreSbs && known && letterbox > 0 ? w / 2 / (h * (1.f - 2 * letterbox)) : reported;
  // Wide view: the picture holds 1/kx of the game's width and 1/ky of its height.
  float kx = 1, ky = 1; glstereo::WideView(kx, ky);
  aspect *= ky / kx;
}
void FrameShapeChanged()
{
  static float lastAspect = 0, lastLetterbox = -1, lastPanel = -1;
  float aspect = 0, letterbox = 0, panel = 0; FrameShape(aspect, letterbox, &panel);
  theatre::SetFrameShape(aspect, letterbox, panel);
  if (aspect == lastAspect && letterbox == lastLetterbox && panel == lastPanel) return;
  lastAspect = aspect; lastLetterbox = letterbox; lastPanel = panel;
  Log("Frame shape: screen aspect %.4f, rows cut top and bottom %.3f (core frame %ux%u, reported aspect %.4f)", aspect, letterbox,
      g_av.geometry.base_width, g_av.geometry.base_height, g_av.geometry.aspect_ratio);
  if (panel > 0) Log("3DS touch panel: bottom screen aspect %.4f", panel);
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
  c.sideBySide = EnvFloat("ACVR_PS1_SBS", 0.f) != 0.f || glstereo::StereoOn();
  c.swapEyes = EnvFloat("ACVR_PS1_SWAP_EYES", 0.f) != 0.f;   // insurance if a core's eyes arrive the other way round
  // The 3DS core's side-by-side frames keep its 5:3 size and letterbox each eye in the middle half of the rows.
  FrameShapeChanged();
  float panelAspect = 0;
  FrameShape(c.aspect, c.letterbox, &panelAspect);
  if (g_padCfg.touch.on) {
    const auto &t = g_padCfg.touch;
    c.touch = true; c.touchPanelAspect = panelAspect > .1f ? panelAspect : 4.f / 3.f;
    for (int i = 0; i < 4; ++i) { c.touchTop[i] = t.top[i]; c.touchBottom[i] = t.bottom[i]; }
    c.touchWidth = t.panel[0]; c.touchDistance = t.panel[1]; c.touchLift = t.panel[2];
  }
  c.aimDot = !PadMode() || EnvFloat("ACVR_PAD_AIM_DOT", 0.f) != 0.f;   // pad games show their own crosshair, if any
  // Wide view: the picture holds 1/kx of the game's width and 1/ky of its height, so the surface is that much
  // wider (ACVR_PS1_SCREEN_W gives its width): no frame, edges fading into the dark.
  float kx = 1, ky = 1; glstereo::WideView(kx, ky);
  const bool wide = kx < 1 || ky < 1;
  c.frame = EnvFloat("ACVR_SCREEN_FRAME", wide ? 0.f : 1.f) != 0.f;
  c.edgeFade = EnvFloat("ACVR_SCREEN_FADE", wide ? .06f : 0.f);
  c.hud = glstereo::HudOn();
  if (c.hud) {
    // By default the panel covers the same view angle as the game's own picture on the world surface (so the
    // game's crosshair lines up with the world), nearer and a little lower.
    c.hudAspect = aspect > .1f ? aspect : 4.f / 3.f;
    c.hudDistance = EnvFloat("ACVR_HUD_DISTANCE", 1.8f);
    const float pictureArc = kx * c.width;   // the game's picture width on the surface
    const float angle = c.curve > .5f ? pictureArc / c.distance : 2 * std::atan(pictureArc / 2 / c.distance);
    c.hudWidth = EnvFloat("ACVR_HUD_WIDTH", 2 * c.hudDistance * std::tan(std::min(angle, 2.6f) / 2) * EnvFloat("ACVR_HUD_SCALE", 1.f));
    c.hudLift = EnvFloat("ACVR_HUD_LIFT", -.08f);
    c.hudOpacity = EnvFloat("ACVR_HUD_OPACITY", 1.f);
    c.hudKeyBlack = EnvFloat("ACVR_HUD_KEY_BLACK", 0.f) != 0.f;
  }
  if (const char *m = std::getenv("ACVR_PS1_GUN_MODEL")) c.gunModel = m;
  if (const char *m = std::getenv("ACVR_PS1_GUN_META")) c.gunMeta = m;
  return c;
}

void Usage()
{
  std::fprintf(stderr,
    "acvr-ps1 --core <libretro dll> --game <disc> --system <BIOS dir> --saves <dir>\n"
    "         [--gun guncon|justifier|lightgun|stunner] [--port 1|2] [--controls gun|pad|wheel|aim] [--option key=value]...\n"
    "         [--log <file>] [--no-xr] [--headless --frames N [--script file] [--snapshots dir]]\n"
    "         [--profile <file.toml> [--view <name>]] [--print-config]\n"
    "         [--theatre-proof <prefix> --test-aim U V | --test-touch U V] [--test-hold P: draw the U4 hold ring at progress P, 0.33 to 1, above 1 settled] [--state <saved state to start from>]\n"
    "Theatre: ACVR_XR_DISTANCE (m, default 2.5), ACVR_PS1_SCREEN_W (m, default 2.4), ACVR_PS1_CURVE (1 curved, 0 flat),\n"
    "         ACVR_PS1_LIFT (m), ACVR_PS1_CRT (1: scanlines), ACVR_PS1_GUN_MODEL (.glb), ACVR_PS1_GUN_META (.toml),\n"
    "         ACVR_PS1_AIM (per-game aim trim: x scale,x shift,y scale,y shift),\n"
    "         ACVR_PS1_SBS (1: side-by-side eyes), ACVR_PS1_SWAP_EYES (1: swap them),\n"
    "         ACVR_PS1_LETTERBOX (share of rows cut top and bottom; unset, it is worked out from the core's frame, 3DS: .25),\n"
    "         ACVR_PS1_3DS_TOUCH (1: the 3DS bottom screen as a touch panel; needs ACVR_PS1_3DS_TOP and _BOTTOM, rectangles of the frame; the right ray is the stylus).\n"
    "Profile: a per-game TOML file of ACVR_* settings, core options, a pad map and views (setups/retro-ps1-vr/profiles);\n"
    "         the environment and the command line override it. --print-config shows every setting and where it came from.\n"
    "Universal controls: left stick click Start, right stick click Select, both clicks together recentre, both held 3 s quit and save.\n"
    "Clean quit: both stick clicks held for 3 s, Escape, or create ACVR_PS1_STOP_FILE.\n");
}

}  // namespace

int main(int argc, char **argv)
{
  std::string game, gunName = "guncon", script, snapshots, logPath, profileNote;
  unsigned frames = 0, seconds = 0; bool headless = false, noXr = false, mute = false, testAim = false;
  float testU = .5f, testV = .5f, testHold = 0; bool testPanel = false; std::string theatreProof, startState, profilePath, viewName;
  bool printConfig = false, controlsGiven = false, gunGiven = false, portGiven = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string { if (i + 1 >= argc) { Usage(); std::exit(2); } return argv[++i]; };
    if (a == "--core") g_corePath = next();
    else if (a == "--game") game = next();
    else if (a == "--system") g_systemDir = next();
    else if (a == "--saves") g_saveDir = next();
    else if (a == "--gun") { gunName = next(); gunGiven = true; }
    else if (a == "--controls") { g_controls = next(); controlsGiven = true; }
    else if (a == "--port") { g_gunPort = (unsigned)std::atoi(next().c_str()) == 2 ? 1 : 0; portGiven = true; }
    else if (a == "--profile") profilePath = next();
    else if (a == "--view") viewName = next();
    else if (a == "--print-config") printConfig = true;
    else if (a == "--option") { const std::string kv = next(); const auto eq = kv.find('='); if (eq == std::string::npos) { Usage(); return 2; } g_optionOverrides[kv.substr(0, eq)] = kv.substr(eq + 1); }
    else if (a == "--log") logPath = next();
    else if (a == "--headless") headless = true;
    else if (a == "--no-xr") noXr = true;
    else if (a == "--no-pad") g_padOn = false;
    else if (a == "--mute") mute = true;
    else if (a == "--seconds") seconds = (unsigned)std::strtoul(next().c_str(), nullptr, 10);
    else if (a == "--theatre-proof") theatreProof = next();
    else if (a == "--state") startState = next();
    else if (a == "--test-aim") { testAim = true; testU = (float)std::atof(next().c_str()); testV = (float)std::atof(next().c_str()); }
    else if (a == "--test-touch") { testPanel = true; testU = (float)std::atof(next().c_str()); testV = (float)std::atof(next().c_str()); }   // the proof's hand aims at the 3DS panel
    else if (a == "--test-hold") testHold = (float)std::atof(next().c_str());   // the proof draws the U4 hold ring at this progress
    else if (a == "--frames") frames = (unsigned)std::strtoul(next().c_str(), nullptr, 10);
    else if (a == "--script") script = next();
    else if (a == "--snapshots") snapshots = next();
    else { Usage(); return 2; }
  }
  // The settings registry and the game's profile. A profile only fills in what the environment and the command
  // line leave open; with no profile nothing changes. This runs before anything reads an ACVR_* variable.
  hostcfg::Resolved config;
  try {
    if (!viewName.empty() && profilePath.empty()) throw std::runtime_error("--view needs --profile");
    std::optional<hostcfg::Profile> profile;
    if (!profilePath.empty()) profile = hostcfg::LoadProfile(profilePath);
    config = hostcfg::Resolve(profile ? &*profile : nullptr, viewName, g_optionOverrides);
    if (profile) {
      if (!controlsGiven && !profile->controls.empty()) g_controls = profile->controls;
      if (!gunGiven && !profile->gun.empty()) gunName = profile->gun;
      if (!portGiven && profile->port) g_gunPort = profile->port == 2 ? 1 : 0;
      profileNote = "Profile: " + (profile->name.empty() ? profilePath : profile->name) + " (" + profilePath + ")" +
                    (profile->system.empty() ? "" : ", system " + profile->system) + (config.view.empty() ? "" : ", view " + config.view);
    }
  } catch (const std::exception &e) { std::fprintf(stderr, "acvr-ps1: %s\n", e.what()); return 2; }
  g_hideFlash = [] { const char *e = std::getenv("ACVR_PS1_HIDE_FLASH"); return !e || std::atoi(e) != 0; }();
  if (printConfig) {
    if (!profileNote.empty()) std::printf("%s\n", profileNote.c_str());
    for (const auto &line : config.printout()) std::printf("%s\n", line.c_str());
    return 0;
  }
  if (g_corePath.empty() || game.empty() || g_systemDir.empty() || g_saveDir.empty() || (headless && !frames)) { Usage(); return 2; }
  if (gunName != "guncon" && gunName != "justifier" && gunName != "lightgun" && gunName != "stunner") { Usage(); return 2; }
  if (g_controls != "gun" && g_controls != "pad" && g_controls != "wheel" && g_controls != "aim") { Usage(); return 2; }
  if (!logPath.empty()) { g_log = std::fopen(logPath.c_str(), "w"); if (!g_log) g_log = stdout; }
  if (!profileNote.empty()) Log("%s", profileNote.c_str());
  for (const auto &line : config.summary()) Log("%s", line.c_str());
  // Offline runs stay out of the way; a game in the headset keeps normal priority so it is never starved.
  if (headless || noXr) SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
  CreateDirectoryA(g_saveDir.c_str(), nullptr);
  if (!snapshots.empty()) CreateDirectoryA(snapshots.c_str(), nullptr);

  g_flashAudit = headless;
  Script timeline;
  pad::Scripted scripted;
  if (!script.empty() && !timeline.load(script)) { Log("Cannot read script %s", script.c_str()); return 2; }
  // The core draws its own crosshair only in offline proofs, to show where it thinks the gun points.
  if (!PadMode() && !g_optionOverrides.count("beetle_psx_gun_cursor")) g_optionOverrides["beetle_psx_gun_cursor"] = headless ? "cross" : "off";
  if (!PadMode() && !g_optionOverrides.count("beetle_psx_gun_input_mode")) g_optionOverrides["beetle_psx_gun_input_mode"] = "lightgun";
  // Sega Saturn (Beetle Saturn): the Stunner / Virtua Gun reads light-gun coordinates; its crosshair is drawn offline only.
  if (gunName == "stunner") {
    if (!g_optionOverrides.count("beetle_saturn_virtuagun_input")) g_optionOverrides["beetle_saturn_virtuagun_input"] = "Lightgun";
    if (!g_optionOverrides.count("beetle_saturn_virtuagun_crosshair")) g_optionOverrides["beetle_saturn_virtuagun_crosshair"] = headless ? "Cross" : "Off";
  }
  if (PadMode()) {
    const std::string problems = g_padCfg.load(g_controls);
    LoadFollow();
    if (g_follow[0].on || g_follow[1].on) Log("Pad aim follows the game's marker from its memory (%zu + %zu terms), gains %.2f %.2f %.2f %.2f",
                                             g_follow[0].terms.size(), g_follow[1].terms.size(), g_padCfg.follow[0], g_padCfg.follow[1], g_padCfg.follow[2], g_padCfg.follow[3]);
    Log("Pad %s", g_padCfg.describe().c_str());
    if (!problems.empty()) Log("Pad settings not understood:%s", problems.c_str());
  }

  if (!LoadCore(g_corePath)) return 3;
  glstereo::Configure(Log);
  glstereo::InstallImports(g_core.dll);
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
  // Cores that do not read the file themselves (mupen64plus-next) get its bytes.
  std::vector<char> gameBytes;
  if (!sys.need_fullpath) {
    std::ifstream in(game, std::ios::binary);
    gameBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (gameBytes.empty()) { Log("Cannot read %s", game.c_str()); g_core.deinit(); return 4; }
    info.data = gameBytes.data(); info.size = gameBytes.size();
  }
  if (!g_core.load_game(&info)) { Log("The core could not load the disc"); g_core.deinit(); return 4; }
  g_core.get_system_av_info(&g_av);
  if (g_hwOn) {
    g_hwMaxW = std::max(g_av.geometry.max_width, 64u); g_hwMaxH = std::max(g_av.geometry.max_height, 64u);
    glGenFramebuffers(1, &g_hwFbo); glGenTextures(1, &g_hwColor);
    glBindTexture(GL_TEXTURE_2D, g_hwColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_hwMaxW, g_hwMaxH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, g_hwFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_hwColor, 0);
    if (g_hw.depth) {
      auto genRb = reinterpret_cast<PFNGLGENRENDERBUFFERSPROC>(wglGetProcAddress("glGenRenderbuffers"));
      auto bindRb = reinterpret_cast<PFNGLBINDRENDERBUFFERPROC>(wglGetProcAddress("glBindRenderbuffer"));
      auto storeRb = reinterpret_cast<PFNGLRENDERBUFFERSTORAGEPROC>(wglGetProcAddress("glRenderbufferStorage"));
      auto attachRb = reinterpret_cast<PFNGLFRAMEBUFFERRENDERBUFFERPROC>(wglGetProcAddress("glFramebufferRenderbuffer"));
      genRb(1, &g_hwDepth); bindRb(GL_RENDERBUFFER, g_hwDepth);
      storeRb(GL_RENDERBUFFER, g_hw.stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24, g_hwMaxW, g_hwMaxH);
      attachRb(GL_FRAMEBUFFER, g_hw.stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_hwDepth);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    Log("Core framebuffer %ux%u%s", g_hwMaxW, g_hwMaxH, g_hw.depth ? " with depth" : "");
    glstereo::SetHostFramebuffer(g_hwFbo, g_hwMaxW, g_hwMaxH, g_hw.depth, g_hw.stencil);
    if (g_hw.context_reset) g_hw.context_reset();
  }
  Log("Timing: %.4f fps, %.0f Hz audio; picture %ux%u, aspect %.4f", g_av.timing.fps, g_av.timing.sample_rate,
      g_av.geometry.base_width, g_av.geometry.base_height, g_av.geometry.aspect_ratio);

  if (PadMode()) {
    for (const auto &c : g_controllers[0]) Log("Port 1 device offered: %s (0x%X)", c.desc ? c.desc : "?", c.id);
    g_core.set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    Log("Port 1: pad (%s)", g_controls.c_str());
  } else {
  // Pick the gun by the name the core gives it; fall back to the usual light-gun subclasses.
  unsigned device = gunName == "justifier" ? RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 1)
                  : gunName == "lightgun" ? RETRO_DEVICE_LIGHTGUN : RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 0);
  for (const auto &c : g_controllers[g_gunPort]) {
    std::string d = c.desc ? c.desc : ""; std::transform(d.begin(), d.end(), d.begin(), ::tolower);
    if (d.find(gunName) != std::string::npos) { device = c.id; break; }
  }
  if (gunName == "stunner") for (const auto &c : g_controllers[g_gunPort]) Log("Port %u device offered: %s (0x%X)", g_gunPort + 1, c.desc ? c.desc : "?", c.id);
  g_core.set_controller_port_device(g_gunPort, device);
  if (g_padOn && gunName != "lightgun") g_core.set_controller_port_device(1 - g_gunPort, RETRO_DEVICE_JOYPAD);
  Log("Port %u: %s (device 0x%X)", g_gunPort + 1, gunName.c_str(), device);
  }

  {
    std::string base = game.substr(game.find_last_of("\\/") + 1);
    base = base.substr(0, base.find_last_of('.'));
    g_sramPath = g_saveDir + "\\" + base + ".srm";
  }
  LoadSram();
  ApplyCheats();
  // A saved moment to start from, e.g. a Saturn gun game's title screen just after its own gun calibration,
  // which Virtua Cop 2 did not restore from its save after a restart. The game boots normally if it cannot be loaded.
  if (!startState.empty() && !LoadState(startState)) Log("Start state not loaded: the game boots normally");
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
  const char *faceEnv = std::getenv("ACVR_PS1_FACE_BUTTONS");
  const bool faceButtons = faceEnv && std::atoi(faceEnv) == 1;
  if (faceButtons) Log("Face buttons: right A Cross, right B Circle, left X Square, left Y Triangle; gun buttons on the grips");
  Log("Universal controls: left stick click Start, right stick click Select, both clicks recentre, both held 3 s quit and save (no menu button is read)");
  if (const char *aim = std::getenv("ACVR_PS1_AIM")) if (*aim) Log("Aim trim (x scale, x shift, y scale, y shift): %s", aim);
  CleanQuit quit;
  bool quitCounting = false; ULONGLONG quitFiredAt = 0;
  timeBeginPeriod(1);   // 1 ms sleeps, so pacing and audio refill are not 15 ms steps
  const double frameMs = 1000.0 / (g_av.timing.fps > 1 ? g_av.timing.fps : 60.0);
  LARGE_INTEGER freq, t0; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&t0);
  unsigned frame = 0; std::uint64_t shownSerial = 0, shots = 0, countedSerial = 0;
  const ULONGLONG startedAt = GetTickCount64();
  bool lastTrigger = false, wasPaused = false; ULONGLONG lastSave = GetTickCount64();
  // After a pause or a lost audio device the clock pacing starts again from this frame.
  auto restartClock = [&] { QueryPerformanceCounter(&t0); t0.QuadPart -= (LONGLONG)(frame * frameMs * freq.QuadPart / 1000.0); };
  int result = 0;

  while (!g_shutdownRequested) {
    const ULONGLONG now = GetTickCount64();
    theatre::Input controller = theatreOn ? theatre::ReadInput() : theatre::Input{};
    if (testAim) { controller.active = true; controller.offScreen = false; controller.u = testU; controller.v = testV; }
    if (!headless) {
      const DWORD attr = stopFile.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesA(stopFile.c_str());
      quit.poll(now, false, attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
      // The universal quit (both stick clicks held 3 s) is resolved in the presenter. Logged, so a quit that does not
      // happen can be told apart from a hold that never arrived.
      if ((controller.quitProgress > .33f) != quitCounting) { quitCounting = controller.quitProgress > .33f; Log(quitCounting ? "Both stick clicks down (hold to 3 s to quit and save)" : "Stick clicks released"); }
      // The quit waits 0.7 s so the headset shows the hold ring flashing once; the game keeps running meanwhile.
      if (controller.quitFired) {
        if (!quitFiredAt) { quitFiredAt = now; Log("Both stick clicks held for 3 s: quit and save after the ring has flashed"); }
        if (now - quitFiredAt >= 700) { Log("Clean quit: both stick clicks held for 3 s"); break; }
      }
      if (GetAsyncKeyState(VK_ESCAPE) & 1) quit.request(CleanQuit::Reason::escape);
      if (seconds && now - startedAt >= seconds * 1000ull) quit.request(CleanQuit::Reason::time_limit);
      if (quit.requested()) { Log("Clean quit: %s", quit.reason()); break; }
      if (theatreOn && theatre::RuntimeExit()) { Log("Clean quit: SteamVR ended the game"); break; }
      if (theatreOn && theatre::PresenterGone()) { Log("Clean quit: the headset picture stopped (see acvr-theatre.log)"); result = 9; break; }
      // The game waits while the headset is not showing it: the SteamVR dashboard, standby, or still starting.
      const bool paused = theatreOn && !theatre::Focused();
      if (paused != wasPaused) {
        Log(paused ? "Paused: the headset is not showing the game (SteamVR dashboard, standby or start-up)" : "Resumed");
        wasPaused = paused; if (!paused) restartClock();
      }
      if (paused) {
        g_audio.hold();
        if (now - lastSave > 10000) { SaveSram(false); lastSave = now; }
        Sleep(20); continue;
      }
      Gun g; g.u = controller.u; g.v = controller.v; g.offScreen = !controller.active || controller.offScreen;
      // GunCon A is also on the grip, as the pedal (Time Crisis takes cover with it).
      g.trigger = controller.trigger; g.a = controller.a || controller.grip; g.b = controller.b;
      g.a = g.a || controller.padSquare;   // cover (Time Crisis) with either grip: the left grip is GunCon A too
      // Start and Select are the stick clicks. Right A and B stay as the gun hand's own aliases: the Justifier's side
      // button, and arcade Start and Coin.
      g.start = controller.a || controller.uStart;
      g.select = (gunName == "lightgun" && controller.b) || controller.uSelect;
      Pad p; p.left = controller.padX < -.5f; p.right = controller.padX > .5f; p.up = controller.padY > .5f; p.down = controller.padY < -.5f;
      p.cross = controller.padCross; p.circle = controller.padCircle; p.square = controller.padSquare;
      p.start = controller.uStart; p.select = controller.uSelect;
      if (faceButtons) {
        // The owner's layout: right A Cross, right B Circle, left X Square, left Y Triangle. The gun keeps the trigger;
        // its two buttons move to the right and left grips. Start and Select stay the stick clicks.
        p.cross = controller.a || controller.padCross; p.circle = controller.b; p.square = controller.padCircle;
        p.triangle = controller.padYButton;
        g.a = controller.grip; g.b = controller.padSquare;   // the gun's own two buttons on the grips
        g.start = controller.uStart; g.select = controller.uSelect;
      }
      g_pad = p;
      if (PadMode()) {
        // Aim at the game's own picture: the HUD panel when it is on (where the game's crosshair is drawn),
        // else the screen, undoing the wide view's squeeze.
        theatre::Input aimIn = controller;
        float kx = 1, ky = 1; glstereo::WideView(kx, ky);
        // ACVR_PAD_AIM_ON=world: the game's marker is in the 3D world (Star Fox 64), so aim at the world surface.
        const char *aimOn = std::getenv("ACVR_PAD_AIM_ON");
        const bool atWorld = aimOn && !std::strcmp(aimOn, "world");
        if (glstereo::HudOn() && !atWorld) { aimIn.u = controller.hudU; aimIn.v = controller.hudV; aimIn.offScreen = !controller.hudHit; }
        else if ((kx < 1 || ky < 1) && glstereo::WideSqueezed()) { aimIn.u = .5f + (controller.u - .5f) / kx; aimIn.v = .5f + (controller.v - .5f) / ky; }
        FollowInto(aimIn);
        g_rpad = pad::Map(aimIn, g_padCfg);
      }
      else g_gun = g;
    } else {
      if (frames && frame >= frames) break;
      while (timeline.next < timeline.events.size() && timeline.events[timeline.next].frame <= frame) {
        const auto &e = timeline.events[timeline.next++];
        const unsigned n = (unsigned)std::atoi(Arg(e.args, 0, "6").c_str());
        std::string padError;
        if (PadMode() && scripted.command(e.cmd, e.args, padError)) { if (!padError.empty()) Log("Script line at frame %u: %s", e.frame, padError.c_str()); }
        else if (e.cmd == "aim") { g_gun.u = (float)std::atof(Arg(e.args, 0, ".5").c_str()); g_gun.v = (float)std::atof(Arg(e.args, 1, ".5").c_str()); g_gun.offScreen = false; }
        else if (e.cmd == "trigger") timeline.holdTrigger = n;
        else if (e.cmd == "a") timeline.holdA = n;
        else if (e.cmd == "b") timeline.holdB = n;
        else if (e.cmd == "start") timeline.holdStart = n;
        else if (e.cmd == "select") timeline.holdSelect = n;
        else if (e.cmd == "offscreen") timeline.holdOff = n;
        else if (e.cmd.rfind("pad-", 0) == 0) timeline.pad[e.cmd.substr(4)] = n;
        else if (e.cmd == "reload") { timeline.holdOff = 8; timeline.holdTrigger = 6; }
        else if (e.cmd == "snap" && !snapshots.empty()) {
          WriteSnapshot(snapshots, Arg(e.args, 0, ("f" + std::to_string(frame)).c_str()));
          if (PadMode()) Log("  frame %u pad: buttons 0x%04X left %.3f %.3f right %.3f %.3f%s", frame, g_rpad.buttons, g_rpad.lx, g_rpad.ly, g_rpad.rx, g_rpad.ry,
                             scripted.vr.hasCurrent ? (" marker " + std::to_string(scripted.vr.curU) + " " + std::to_string(scripted.vr.curV) + " target " + std::to_string(scripted.vr.u) + " " + std::to_string(scripted.vr.v)).c_str() : "");
          if (PadMode() && g_padCfg.touch.on) Log("  frame %u stylus: %s, frame position %.4f %.4f; core pointer reads so far %llu, with the stylus down %llu", frame,
                                                  !g_rpad.pointerOn ? "off the panel" : g_rpad.pointerDown ? "down" : "over the panel", (g_rpad.px + 1) / 2, (g_rpad.py + 1) / 2,
                                                  (unsigned long long)g_pointerReads, (unsigned long long)g_pointerDownReads);
          // ACVR_RAM_WATCH: console memory words (hex addresses) logged as floats at each snapshot, to study a game.
          if (const char *watch = std::getenv("ACVR_RAM_WATCH")) {
            const auto *ram = static_cast<const unsigned char *>(g_core.get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
            const std::size_t size = g_core.get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
            std::string line; std::istringstream list(watch); std::string item;
            while (ram && std::getline(list, item, ',')) {
              const std::size_t addr = (std::size_t)std::strtoul(item.c_str(), nullptr, 16) & 0x7ffffcu; float f = 0;
              if (addr + 4 <= size) std::memcpy(&f, ram + addr, 4);
              char b[64]; std::snprintf(b, sizeof b, " %s=%.4f", item.c_str(), f); line += b;
            }
            Log("  frame %u ram:%s", frame, line.c_str());
          }
        }
        else if (e.cmd == "save") SaveSram(true);
        else if (e.cmd == "state-save") SaveState(Arg(e.args, 0, "state.bin"));
        else if (e.cmd == "ram-dump") {   // the console's main memory, for finding a game's variables offline
          const void *ram = g_core.get_memory_data(RETRO_MEMORY_SYSTEM_RAM); const std::size_t size = g_core.get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
          std::ofstream out(Arg(e.args, 0, "ram.bin"), std::ios::binary); if (ram && size) out.write(static_cast<const char *>(ram), (std::streamsize)size);
          Log("RAM dump %s: %zu bytes", Arg(e.args, 0, "ram.bin").c_str(), size);
        }
        else if (e.cmd == "state-load") { if (!LoadState(Arg(e.args, 0, "state.bin"))) frames = frame; }
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
      if (PadMode()) {
        FollowInto(scripted.vr); g_rpad = scripted.frame(g_padCfg);
        if (scripted.chordRecentre) { Log("Frame %u: both stick clicks together: recentre", frame); scripted.chordRecentre = false; }
        if (scripted.chordQuit) { Log("Clean quit: both stick clicks held for 3 s (frame %u)", frame); frames = frame; scripted.chordQuit = false; }
      }
      if (frames && frame >= frames) break;
    }
    if (g_gun.trigger && !lastTrigger) { ++shots; if (theatreOn) theatre::QueuePulse(); }
    { static ULONGLONG lastPulse = 0; if (g_rumbleOn && theatreOn && now - lastPulse >= 80) { theatre::QueuePulse(); lastPulse = now; } }
    if (g_gun.trigger) { g_triggerFrame = frame; g_triggerSeen = true; }
    lastTrigger = g_gun.trigger;

    g_core.run();
    ++frame;
    if (g_hwOn) glstereo::EndFrame(frame);
    // Offline runs count the flash frames the headset would skip (same test, nothing is skipped here).
    if (headless && g_hideFlash && g_frameSerial != countedSerial) { countedSerial = g_frameSerial; HideThisFrame(frame); }

    if (!headless && g_frameSerial != shownSerial && (g_hwOn ? g_hwW > 0 : !g_frame.empty())) {
      shownSerial = g_frameSerial;
      if (theatreOn && !HideThisFrame(frame)) { if (g_hwOn) PublishHardwareFrame(); else theatre::Publish(g_frame.data(), g_frameW, g_frameH); }
    }
    if (now - lastSave > 10000) { SaveSram(false); lastSave = now; }

    if (!headless) {
      if (g_audioWanted && g_audio.failed()) { Log("Audio device lost: running silent, paced by the clock"); g_audioWanted = false; restartClock(); }
      if (g_audioWanted) { for (int i = 0; i < 100 && g_audio.queued() > AudioOut::kTarget; ++i) Sleep(1); }
      else {
        LARGE_INTEGER t; QueryPerformanceCounter(&t);
        const double due = frame * frameMs, elapsed = (t.QuadPart - t0.QuadPart) * 1000.0 / freq.QuadPart;
        if (due > elapsed + 1) Sleep((DWORD)(due - elapsed));
      }
    }
  }
  if (g_shutdownRequested) Log("The core asked to shut down");
  if (headless && !snapshots.empty()) WriteSnapshot(snapshots, "final");
  EnsureCpuFrame();
  if (!theatreProof.empty() && !g_frame.empty()) {
    if (!view.create()) Log("Theatre proof: no OpenGL context");
    else {
      std::string note;
      const bool hud = !g_hudFrame.empty() && g_hudFrame.size() == (std::size_t)g_hwW * g_hwH;
      auto proofConfig = TheatreConfig(); proofConfig.proofAtPanel = testPanel; proofConfig.proofHold = testHold;
      const bool ok = theatre::RenderProof(proofConfig, g_frame.data(), g_frameW, g_frameH, testU, testV, theatreProof, note,
                                           hud ? g_hudFrame.data() : nullptr, hud ? g_hwW : 0, hud ? g_hwH : 0);
      Log("Theatre proof %s-left.png / -right.png: %s (%s)", theatreProof.c_str(), ok ? "written" : "FAILED", note.c_str());
      if (!ok) result = 8;
    }
  }
  Log("Ran %u frames; gun reads %llu; trigger presses %llu; last picture %ux%u", frame,
      (unsigned long long)g_gunReads, (unsigned long long)shots, g_frameW, g_frameH);
  if (PadMode()) Log("Rumble started %llu times", (unsigned long long)g_rumbleStarts);
  if (PadMode() && g_padCfg.touch.on) Log("3DS touch: the core read the pointer %llu times, %llu with the stylus down", (unsigned long long)g_pointerReads, (unsigned long long)g_pointerDownReads);
  if (g_hideFlash) Log("Gun flash frames kept out of the headset: %u (longest white run %u frames)%s", g_flashHidden, g_flashLongest,
                      g_flashAudit ? ("; white frames shown with no shot just before: " + std::to_string(g_flashOutside)).c_str() : "");
  timeEndPeriod(1);
  if (!headless) {
    const double secs = (GetTickCount64() - startedAt) / 1000.0;
    Log("Speed %.1f frames/s over %.0f s (game rate %.1f); audio gaps %u, dropped buffers %u", secs > 0 ? frame / secs : 0.0, secs,
        g_av.timing.fps, g_audio.underruns(), g_audio.dropped());
  }
  if (!SaveSram(false)) result = 6;
  // A hardware core frees its graphics objects in context_destroy while its emulator still exists.
  if (g_hwOn && g_hw.context_destroy) { Log("Releasing the core's graphics"); g_hw.context_destroy(); }
  Log("Unloading the disc"); g_core.unload_game();
  Log("Stopping the core"); g_core.deinit();
  if (theatreOn) theatre::Stop();
  g_audio.close();
  view.destroy();
  Log("Clean exit");
  if (g_log != stdout) std::fclose(g_log);
  return result;
}
