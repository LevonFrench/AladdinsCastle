// SPDX-License-Identifier: GPL-3.0-or-later
// The theatre presenter: the game picture on a big screen, the gun in your hand
// and the aim dot, drawn together into one OpenXR projection layer per eye.
#pragma once
#include "WinGL.h"
#include <cstdint>
#include <string>

namespace theatre {
struct Config {
  float distance = 2.5f;   // metres from the eyes to the screen centre
  float width = 2.4f;      // screen width in metres
  float aspect = 4.f / 3.f;
  float curve = 1.f;       // 1: curved around the viewer; 0: flat
  float lift = 0.f;        // screen centre height above the eyes, metres
  bool crt = false;        // scanlines
  std::string gunModel, gunMeta;   // GLB and its TOML; empty: no gun drawn
};
struct Input {
  float u = .5f, v = .5f;
  bool active = false, offScreen = true, trigger = false, a = false, b = false, grip = false, quitHeld = false;
};
// All picture and graphics calls run on the thread whose OpenGL context is current
// in `owner` (the emulator thread). The headset runs on its own thread.
bool Start(Host_Window *owner, const Config &config);
void Publish(const std::uint32_t *xrgb, unsigned w, unsigned h);
Input ReadInput();          // stale (over 250 ms) input reads as inactive
void QueuePulse();
bool Stop();
// Offline proof without any VR runtime: renders both eyes for a fixed head and hand
// pose into PNG files <prefix>-left.png and <prefix>-right.png.
bool RenderProof(const Config &config, const std::uint32_t *xrgb, unsigned w, unsigned h,
                 float aimU, float aimV, const std::string &prefix, std::string &error);
}
