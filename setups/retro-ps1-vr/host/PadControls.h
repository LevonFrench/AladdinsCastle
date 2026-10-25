// SPDX-License-Identifier: GPL-3.0-or-later
// Pad games (Nintendo 64 through mupen64plus-next, or any libretro pad core): the VR controllers become a
// standard RetroPad with analog sticks. Modes (--controls):
//   pad   the left stick is the analog stick; buttons come from the map
//   wheel racing: the left stick steers (and gives the analog Y, nose up/down), the triggers are gas and brake;
//         rolling the right controller like a steering wheel also steers only when ACVR_N64_ROLL_STEER=1 (off by default)
//   aim   while aiming (right grip held, or always), the right controller's point on the screen sets the analog
//         stick so the game's own crosshair follows it, with the game's aim button held; the left stick then
//         goes to the right analog (the N64 C buttons). Not aiming, the left stick is the analog stick.
// The same mapping serves the headset and the offline scripts (a scripted virtual controller), so offline
// proofs exercise exactly what the headset sends.
#pragma once
#include "TheatreXR.h"
#include "retro_abi.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace pad {

struct RetroPad {
  std::uint16_t buttons = 0;          // bit = RETRO_DEVICE_ID_JOYPAD_*
  float lx = 0, ly = 0, rx = 0, ry = 0;   // -1..1, libretro convention: +x right, +y down
  // RETRO_DEVICE_POINTER (the 3DS touch screen): where the stylus is over the core's whole frame, -1..1 from its top left, and whether it is down.
  bool pointerOn = false, pointerDown = false; float px = 0, py = 0;
  bool on(unsigned id) const { return (buttons >> id) & 1; }
  void press(unsigned id)
  {
    if (id < 16) buttons |= (std::uint16_t)(1u << id);
    else if (id == 16) ry = -1; else if (id == 17) ry = 1; else if (id == 18) rx = -1; else if (id == 19) rx = 1;
  }
};

constexpr int kRightUp = 16, kRightDown = 17, kRightLeft = 18, kRightRight = 19;
inline int RetroButton(const std::string &name)
{
  static const std::map<std::string, int> names = {
    { "b", RETRO_DEVICE_ID_JOYPAD_B }, { "y", RETRO_DEVICE_ID_JOYPAD_Y }, { "select", RETRO_DEVICE_ID_JOYPAD_SELECT },
    { "start", RETRO_DEVICE_ID_JOYPAD_START }, { "up", RETRO_DEVICE_ID_JOYPAD_UP }, { "down", RETRO_DEVICE_ID_JOYPAD_DOWN },
    { "left", RETRO_DEVICE_ID_JOYPAD_LEFT }, { "right", RETRO_DEVICE_ID_JOYPAD_RIGHT }, { "a", RETRO_DEVICE_ID_JOYPAD_A },
    { "x", RETRO_DEVICE_ID_JOYPAD_X }, { "l", RETRO_DEVICE_ID_JOYPAD_L }, { "r", RETRO_DEVICE_ID_JOYPAD_R },
    { "l2", RETRO_DEVICE_ID_JOYPAD_L2 }, { "r2", RETRO_DEVICE_ID_JOYPAD_R2 }, { "l3", RETRO_DEVICE_ID_JOYPAD_L3 },
    { "r3", RETRO_DEVICE_ID_JOYPAD_R3 },
    // The right analog stick pushed fully one way (mupen64plus-next: the N64 C buttons).
    { "rs-up", kRightUp }, { "rs-down", kRightDown }, { "rs-left", kRightLeft }, { "rs-right", kRightRight } };
  const auto it = names.find(name);
  return it == names.end() ? -1 : it->second;
}

// The VR controls a map may name.
inline bool VrControl(const theatre::Input &in, const std::string &name)
{
  if (name == "rtrig") return in.triggerValue > .35f;
  if (name == "ltrig") return in.padTriggerValue > .35f;
  if (name == "rgrip") return in.gripValue > .5f;
  if (name == "lgrip") return in.padGripValue > .5f;
  if (name == "ra") return in.a;
  if (name == "rb") return in.b;
  if (name == "lx") return in.padCircle;          // left X (Index: left A)
  if (name == "ly") return in.padYButton;              // left Y (Index: left B); held 3 s it quits
  if (name == "lmenu") return in.padStart;        // left menu (Index: left stick click)
  if (name == "rclick") return in.stickRight;
  if (name == "lclick") return in.stickLeft;
  if (name == "rup") return in.rightY > .6f;
  if (name == "rdown") return in.rightY < -.6f;
  if (name == "rleft") return in.rightX < -.6f;
  if (name == "rright") return in.rightX > .6f;
  return false;
}
// The VR controls that can put the 3DS stylus down (ACVR_PS1_3DS_STYLUS).
inline bool KnownStylusControl(const std::string &name)
{
  static const char *known[] = { "rtrig", "ltrig", "rgrip", "lgrip", "ra", "rb", "lx", "ly", "lmenu", "rclick", "lclick" };
  for (const char *k : known) if (name == k) return true;
  return false;
}
// The same control, held: the offline scripts' way of pressing the control a setting names.
inline void ForceVrControl(theatre::Input &in, const std::string &name)
{
  if (name == "rtrig") in.triggerValue = 1;
  else if (name == "ltrig") in.padTriggerValue = 1;
  else if (name == "rgrip") in.gripValue = 1;
  else if (name == "lgrip") in.padGripValue = 1;
  else if (name == "ra") in.a = true;
  else if (name == "rb") in.b = true;
  else if (name == "lx") in.padCircle = true;
  else if (name == "ly") in.padYButton = true;
  else if (name == "lmenu") in.padStart = true;
  else if (name == "rclick") in.stickRight = true;
  else if (name == "lclick") in.stickLeft = true;
}

// The Nintendo 3DS bottom-screen touch panel (ACVR_PS1_3DS_TOUCH). Azahar draws both screens into one frame: the top
// screen (a side-by-side stereo pair) and the bottom screen are two rectangles of it, given as shares of the frame's
// width and height ("x,y,w,h" from its top left). The theatre draws the top rectangle as the screen and the bottom
// rectangle as a flat panel; the right controller's ray on the panel is the stylus, sent to the core as
// RETRO_DEVICE_POINTER over the core's whole frame (the core finds the bottom screen in its own layout).
struct TouchLayout {
  bool on = false;
  float top[4] = { 0, 0, 1, 1 }, bottom[4] = { 0, 0, 1, 1 };
  float pointer[4] = { 0, 0, 1, 1 };          // where the core takes the bottom screen to be when it reads the stylus (ACVR_PS1_3DS_POINTER; default: the bottom rectangle)
  std::string button = "rtrig";               // the VR control that puts the stylus down
  float panel[3] = { .8f, 1.f, -.55f };       // panel width, distance ahead and height against the eyes, metres
  static bool Rect(const char *text, float out[4])
  {
    float v[4]; char extra = 0;
    if (!text || std::sscanf(text, "%f,%f,%f,%f%c", &v[0], &v[1], &v[2], &v[3], &extra) != 4) return false;
    if (!(v[0] >= 0 && v[1] >= 0 && v[2] > .02f && v[3] > .02f && v[0] + v[2] <= 1.001f && v[1] + v[3] <= 1.001f)) return false;
    for (int i = 0; i < 4; ++i) out[i] = v[i];
    return true;
  }
  // Environment: ACVR_PS1_3DS_TOUCH (1), ACVR_PS1_3DS_TOP, ACVR_PS1_3DS_BOTTOM, ACVR_PS1_3DS_STYLUS, ACVR_PS1_3DS_PANEL.
  // Returns what is wrong, empty when the panel is usable or not asked for.
  std::string load()
  {
    const char *e = std::getenv("ACVR_PS1_3DS_TOUCH");
    on = false;
    if (!e || std::atoi(e) == 0) return "";
    std::string problems;
    if (!Rect(std::getenv("ACVR_PS1_3DS_TOP"), top)) problems += " ACVR_PS1_3DS_TOP is missing or not x,y,w,h as shares of the frame;";
    if (!Rect(std::getenv("ACVR_PS1_3DS_BOTTOM"), bottom)) problems += " ACVR_PS1_3DS_BOTTOM is missing or not x,y,w,h as shares of the frame;";
    const char *pointerText = std::getenv("ACVR_PS1_3DS_POINTER");
    if (pointerText && *pointerText) { if (!Rect(pointerText, pointer)) problems += " ACVR_PS1_3DS_POINTER is not x,y,w,h as shares of the frame;"; }
    else for (int i = 0; i < 4; ++i) pointer[i] = bottom[i];
    if ((e = std::getenv("ACVR_PS1_3DS_STYLUS")) && *e) {
      if (KnownStylusControl(e)) button = e; else problems += std::string(" ACVR_PS1_3DS_STYLUS '") + e + "' is not a VR control;";
    }
    if ((e = std::getenv("ACVR_PS1_3DS_PANEL")) && *e) {
      float v[3]; char extra = 0;
      if (std::sscanf(e, "%f,%f,%f%c", &v[0], &v[1], &v[2], &extra) == 3 && v[0] > .1f && v[0] < 3 && v[1] > .3f && v[1] < 4 && v[2] > -2 && v[2] < 2) { for (int i = 0; i < 3; ++i) panel[i] = v[i]; }
      else problems += " ACVR_PS1_3DS_PANEL is not width,distance,height in metres;";
    }
    on = problems.empty();
    return problems;
  }
};

struct Config {
  std::string mode = "pad";                       // pad | wheel | aim
  std::vector<std::pair<std::string, int>> map;   // VR control -> RetroPad button
  float wheelDegrees = 75, wheelDead = 3;         // roll for full lock, and the dead zone, degrees
  bool rollSteer = false;                         // roll steering is opt-in (ACVR_N64_ROLL_STEER=1); else the left stick steers
  std::string aimHold = "rgrip";                  // VR control that enters aim mode; "always" keeps aiming
  int aimButton = RETRO_DEVICE_ID_JOYPAD_R;       // the game's aim-mode button (GoldenEye: N64 R)
  float aimTrim[4] = { 1, 0, 1, 0 };              // stick = clamp((offset - shift) * scale), offset = (u - .5) * 2 per axis
  float aimCurve = 1;                             // stick = sign * |stick| ^ curve (measured per game)
  bool rightStickToAnalog = true;                 // right thumbstick to the right analog (N64 C buttons)
  // Rate-driven aim (the stick moves the game's marker rather than placing it): stick = p * error - d * velocity
  // + i * integral of the error, per axis, from where the marker is now (Input::curU/curV).
  // ACVR_PAD_FOLLOW_GAIN = "px,dx,py,dy,ix,iy".
  float follow[6] = { 6, 40, 6, 40, 0, 0 };
  TouchLayout touch;                              // the 3DS touch panel (ACVR_PS1_3DS_TOUCH and its companions), off unless asked for

  // "rtrig=a,ltrig=b,rgrip=r" ; unknown names are reported in `problems`.
  void parseMap(const std::string &text, std::string &problems)
  {
    std::istringstream s(text); std::string item;
    while (std::getline(s, item, ',')) {
      const auto eq = item.find('='); if (eq == std::string::npos) { if (!item.empty()) problems += " '" + item + "'"; continue; }
      const std::string vr = item.substr(0, eq), retro = item.substr(eq + 1);
      const int id = RetroButton(retro);
      if (id < 0) { problems += " '" + item + "'"; continue; }
      map.emplace_back(vr, id);
    }
  }
  static std::string DefaultMap(const std::string &mode)
  {
    // libretro names; mupen64plus-next reads RetroPad B as N64 A, Y as N64 B, L2 as Z, R as R, L as L, Start as Start.
    // Start: left stick click everywhere (the left menu button is the SteamVR dashboard under ALVR); the left menu
    // button stays as an alias, and so does the right stick click where the right stick is unused (racing, aiming).
    // The pad map has no rclick alias: the right stick is the C buttons there. Racing and aiming moved L from the
    // left click to left X, which had only duplicated Z (wheel) or A (aim).
    if (mode == "wheel") return "rtrig=b,ltrig=y,rgrip=r,lgrip=l2,ra=b,rb=y,lx=l,lclick=start,lmenu=start,rclick=start";
    if (mode == "aim") return "rtrig=l2,ra=b,rb=y,lgrip=l2,lx=l,lclick=start,lmenu=start,rclick=start,ltrig=r";
    return "ra=b,rb=y,rtrig=l2,rgrip=r,ltrig=l2,lgrip=l,lx=b,lclick=start,lmenu=start";
  }
  // True when the active map (or the aim-hold control) uses either grip. The both-grips recentre gesture is only
  // safe when it does not: a game that holds both grips (Star Fox 64 3D boost and brake) must not move the screen.
  bool usesGrip() const
  {
    for (const auto &m : map) if (m.first == "rgrip" || m.first == "lgrip") return true;
    if (touch.on && (touch.button == "rgrip" || touch.button == "lgrip")) return true;
    return mode == "aim" && (aimHold == "rgrip" || aimHold == "lgrip");
  }
  // Environment: ACVR_PAD_MAP, ACVR_PAD_WHEEL_DEG, ACVR_PAD_AIM_HOLD, ACVR_PAD_AIM_BUTTON, ACVR_PAD_AIM, ACVR_PAD_AIM_CURVE.
  std::string load(const std::string &m)
  {
    mode = m; std::string problems;
    const char *e = std::getenv("ACVR_PAD_MAP");
    parseMap(e && *e ? e : DefaultMap(mode), problems);
    if ((e = std::getenv("ACVR_PAD_WHEEL_DEG")) && std::atof(e) >= 10) wheelDegrees = (float)std::atof(e);
    // The owner's choice: rolling the controller to steer is an option, off unless asked for.
    rollSteer = ((e = std::getenv("ACVR_N64_ROLL_STEER")) && std::atoi(e) == 1) || ((e = std::getenv("ACVR_PAD_ROLL_STEER")) && std::atoi(e) == 1);
    if ((e = std::getenv("ACVR_PAD_AIM_HOLD")) && *e) aimHold = e;
    if ((e = std::getenv("ACVR_PAD_AIM_BUTTON")) && *e) { const int id = std::string(e) == "none" ? -1 : RetroButton(e); if (id >= 0 || std::string(e) == "none") aimButton = id; else problems += std::string(" aim button '") + e + "'"; }
    if ((e = std::getenv("ACVR_PAD_FOLLOW_GAIN")) && *e) std::sscanf(e, "%f,%f,%f,%f,%f,%f", &follow[0], &follow[1], &follow[2], &follow[3], &follow[4], &follow[5]);
    if ((e = std::getenv("ACVR_PAD_AIM")) && *e) std::sscanf(e, "%f,%f,%f,%f", &aimTrim[0], &aimTrim[1], &aimTrim[2], &aimTrim[3]);
    if ((e = std::getenv("ACVR_PAD_AIM_CURVE")) && std::atof(e) > .2) aimCurve = (float)std::atof(e);
    const std::string touchProblems = touch.load();
    if (!touchProblems.empty()) problems += " 3DS touch panel not used:" + touchProblems;
    return problems;
  }
  std::string describe() const
  {
    std::string d = "controls " + mode + ":";
    for (const auto &m : map) d += " " + m.first + "->" + std::to_string(m.second);
    char extra[200];
    if (mode == "wheel" && rollSteer) { std::snprintf(extra, sizeof extra, "; roll steering on: full lock at %.0f deg roll, dead zone %.0f deg", wheelDegrees, wheelDead); d += extra; }
    if (mode == "wheel" && !rollSteer) d += "; the left stick steers (roll steering off; ACVR_N64_ROLL_STEER=1 turns it on)";
    if (touch.on) { std::snprintf(extra, sizeof extra, "; 3DS touch panel: stylus down with %s, bottom screen at %.3f,%.3f,%.3f,%.3f of the frame", touch.button.c_str(), touch.bottom[0], touch.bottom[1], touch.bottom[2], touch.bottom[3]); d += extra; }
    if (mode == "aim") { std::snprintf(extra, sizeof extra, "; aim while %s, aim button %d, trim %.3f,%.3f,%.3f,%.3f, curve %.2f", aimHold.c_str(), aimButton, aimTrim[0], aimTrim[1], aimTrim[2], aimTrim[3], aimCurve); d += extra; }
    return d;
  }
};

inline float Clamp1(float v) { return std::clamp(v, -1.f, 1.f); }

// Steering from roll: linear from the dead zone to full lock.
inline float WheelSteer(float rollRadians, const Config &c)
{
  const float deg = rollRadians * 57.29578f, mag = std::fabs(deg);
  if (mag <= c.wheelDead) return 0;
  return Clamp1((deg > 0 ? 1.f : -1.f) * (mag - c.wheelDead) / (c.wheelDegrees - c.wheelDead));
}

// The analog deflection that puts the game's crosshair at screen point (u, v), from the per-game trim.
inline void AimStick(float u, float v, const Config &c, float &x, float &y)
{
  auto axis = [&](float t, float scale, float shift) {
    const float s = Clamp1(((t - .5f) * 2.f - shift) * scale);
    return (s < 0 ? -1.f : 1.f) * std::pow(std::fabs(s), c.aimCurve);
  };
  x = axis(u, c.aimTrim[0], c.aimTrim[1]); y = axis(v, c.aimTrim[2], c.aimTrim[3]);
}

inline bool Aiming(const theatre::Input &in, const Config &c)
{
  if (c.mode != "aim" || !in.active || in.offScreen) return false;
  return c.aimHold == "always" || VrControl(in, c.aimHold);
}

inline RetroPad Map(const theatre::Input &in, const Config &c)
{
  RetroPad p;
  const float sx = in.padX, sy = -in.padY;   // left thumbstick, +y down
  if (c.rightStickToAnalog) { p.rx = in.rightX; p.ry = -in.rightY; }
  if (c.mode == "wheel") {
    const float steer = in.active && c.rollSteer ? WheelSteer(in.roll, c) : 0.f;
    p.lx = std::fabs(sx) > std::fabs(steer) ? sx : steer; p.ly = sy;
  } else if (c.mode == "aim" && Aiming(in, c)) {
    if (in.hasCurrent) {   // closed loop on the marker's position
      p.lx = Clamp1(c.follow[0] * (in.u - in.curU) - c.follow[1] * in.velU + c.follow[4] * in.sumU);
      p.ly = Clamp1(c.follow[2] * (in.v - in.curV) - c.follow[3] * in.velV + c.follow[5] * in.sumV);
    } else AimStick(in.u, in.v, c, p.lx, p.ly);
    if (c.aimButton >= 0) p.press((unsigned)c.aimButton);
    if (std::fabs(sx) > .3f || std::fabs(sy) > .3f) { p.rx = sx; p.ry = sy; }   // walk/strafe on the C buttons while aiming
  } else { p.lx = sx; p.ly = sy; }
  for (const auto &m : c.map) if (VrControl(in, m.first)) p.press((unsigned)m.second);   // buttons (and C directions) last
  if (c.touch.on && in.touchHit) {   // the stylus: over the panel, down while the chosen control is held
    const auto &b = c.touch.pointer;
    p.pointerOn = true; p.pointerDown = VrControl(in, c.touch.button);
    // The panel's 0..1 is the bottom screen's share of the frame (as the core takes it to be); the core's pointer range -1..1 is the whole frame.
    p.px = (b[0] + std::clamp(in.touchU, 0.f, 1.f) * b[2]) * 2.f - 1.f;
    p.py = (b[1] + std::clamp(in.touchV, 0.f, 1.f) * b[3]) * 2.f - 1.f;
  }
  return p;
}

inline std::int16_t Axis(float v) { return (std::int16_t)std::lround(Clamp1(v) * 32767.f); }

// What the core reads through retro_input_state for port 0.
inline std::int16_t State(const RetroPad &p, unsigned device, unsigned index, unsigned id)
{
  switch (device & RETRO_DEVICE_MASK) {
  case RETRO_DEVICE_JOYPAD:
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (std::int16_t)p.buttons;
    return id < 16 ? p.on(id) : 0;
  case RETRO_DEVICE_ANALOG:
    if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) return Axis(id == RETRO_DEVICE_ID_ANALOG_X ? p.lx : p.ly);
    if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT) return Axis(id == RETRO_DEVICE_ID_ANALOG_X ? p.rx : p.ry);
    if (index == RETRO_DEVICE_INDEX_ANALOG_BUTTON) return id < 16 && p.on(id) ? 32767 : 0;
    return 0;
  case RETRO_DEVICE_POINTER:   // the 3DS stylus; one pointer, none unless the touch panel is on
    if (index != 0 || !p.pointerOn) return 0;
    switch (id) {
    case RETRO_DEVICE_ID_POINTER_X: return Axis(p.px);
    case RETRO_DEVICE_ID_POINTER_Y: return Axis(p.py);
    case RETRO_DEVICE_ID_POINTER_PRESSED: return p.pointerDown;
    case RETRO_DEVICE_ID_POINTER_COUNT: return p.pointerDown ? 1 : 0;
    default: return 0;
    }
  default: return 0;
  }
}

// Offline: a scripted virtual controller (the same fields the headset fills) plus raw pad overrides for menus.
struct Scripted {
  theatre::Input vr;
  std::map<std::string, unsigned> holdVr;   // ra, rb, rgrip, lgrip, lx, ly, lmenu, rclick, lclick: frames left
  std::map<int, unsigned> holdRaw;          // RetroPad button: frames left
  bool rawStick = false; float rawX = 0, rawY = 0;
  bool rawRight = false; float rawRX = 0, rawRY = 0;

  // Returns false when the command is not a pad command.
  bool command(const std::string &cmd, const std::vector<std::string> &args, std::string &error)
  {
    auto f = [&](std::size_t i, float fallback) { return i < args.size() ? (float)std::atof(args[i].c_str()) : fallback; };
    auto n = [&](std::size_t i) { return i < args.size() ? (unsigned)std::atoi(args[i].c_str()) : 6u; };
    if (cmd == "roll") { vr.roll = f(0, 0) / 57.29578f; vr.active = true; }
    else if (cmd == "point") { if (!args.empty() && args[0] == "off") vr.offScreen = true; else { vr.u = f(0, .5f); vr.v = f(1, .5f); vr.offScreen = false; } vr.active = true; }
    else if (cmd == "rtrig") vr.triggerValue = f(0, 1);
    else if (cmd == "ltrig") vr.padTriggerValue = f(0, 1);
    else if (cmd == "rgrip") vr.gripValue = f(0, 1);
    else if (cmd == "lgrip") vr.padGripValue = f(0, 1);
    else if (cmd == "lstick") { vr.padX = f(0, 0); vr.padY = f(1, 0); }
    else if (cmd == "rstick") { vr.rightX = f(0, 0); vr.rightY = f(1, 0); }
    else if (cmd == "touch") {   // touch <u> <v> [frames]: the stylus over the 3DS panel (0..1 from its top left), down for the frames; "touch off" lifts it away
      if (!args.empty() && args[0] == "off") { vr.touchHit = false; holdVr["stylus"] = 0; }
      else { vr.touchHit = true; vr.touchU = f(0, .5f); vr.touchV = f(1, .5f); vr.active = true; holdVr["stylus"] = n(2); }
    }
    else if (cmd == "vr") { if (args.empty()) { error = "vr needs a control"; return true; } holdVr[args[0]] = n(1); }
    else if (cmd == "btn") {
      const int id = args.empty() ? -1 : RetroButton(args[0]);
      if (id < 0) { error = "btn needs a RetroPad button name"; return true; }
      holdRaw[id] = n(1);
    }
    else if (cmd == "stick") { if (!args.empty() && args[0] == "off") rawStick = false; else { rawStick = true; rawX = f(0, 0); rawY = f(1, 0); } }
    else if (cmd == "cstick") { if (!args.empty() && args[0] == "off") rawRight = false; else { rawRight = true; rawRX = f(0, 0); rawRY = f(1, 0); } }
    else return false;
    return true;
  }
  // One frame: the pad the core sees. Hold counters count down after use.
  RetroPad frame(const Config &c)
  {
    theatre::Input in = vr;
    auto held = [&](const char *k) { auto it = holdVr.find(k); return it != holdVr.end() && it->second > 0; };
    in.a = held("ra"); in.b = held("rb"); in.padCircle = held("lx"); in.padYButton = held("ly"); in.padStart = held("lmenu");
    in.stickRight = held("rclick"); in.stickLeft = held("lclick");
    if (held("rgrip")) in.gripValue = 1;
    if (held("lgrip")) in.padGripValue = 1;
    if (held("rtrig")) in.triggerValue = 1;
    if (held("ltrig")) in.padTriggerValue = 1;
    if (c.touch.on && held("stylus")) ForceVrControl(in, c.touch.button);   // after the lines above, which set a, b and the clicks outright
    RetroPad p = Map(in, c);
    for (const auto &h : holdRaw) if (h.second) p.press((unsigned)h.first);
    if (rawStick) { p.lx = rawX; p.ly = rawY; }
    if (rawRight) { p.rx = rawRX; p.ry = rawRY; }
    for (auto &h : holdVr) if (h.second) --h.second;
    for (auto &h : holdRaw) if (h.second) --h.second;
    return p;
  }
};

}  // namespace pad
