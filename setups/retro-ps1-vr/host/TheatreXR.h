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
  bool sideBySide = false;         // the picture holds the left eye in its left half, the right eye in its right half
  bool aimDot = true;              // the orange dot where the right controller's ray meets the screen
  bool frame = true;               // the thin dark frame round the screen
  float edgeFade = 0;              // share of the picture's width and height over which its edges fade into the dark
  // The HUD panel: the game's 2D overlay (a separate picture, transparent elsewhere) on its own flat panel, placed
  // with the screen and fixed in the room, nearer than the screen.
  bool hud = false;
  float hudDistance = 1.8f, hudWidth = 1.2f, hudLift = -.1f, hudOpacity = 1.f, hudAspect = 16.f / 9.f;
  bool hudKeyBlack = false;        // opaque pure black in the HUD layer (a game's border bars) shows as transparent
};
struct Input {
  float u = .5f, v = .5f;
  bool active = false, offScreen = true, trigger = false, a = false, b = false, grip = false, quitHeld = false;
  // Left controller as a PlayStation pad: stick = d-pad, trigger = Cross, X (Index: A) = Circle,
  // grip = Square, menu (Index: stick click) = Start.
  float padX = 0, padY = 0;
  bool padCross = false, padCircle = false, padSquare = false, padStart = false;
  bool stickLeft = false, stickRight = false;   // thumbstick clicks
  // Analog values and the right controller's roll, for pad games (steering wheel, accelerate/brake).
  float triggerValue = 0, padTriggerValue = 0, gripValue = 0, padGripValue = 0;
  float rightX = 0, rightY = 0;   // right thumbstick
  float roll = 0;                 // right controller roll about its pointing axis, radians, positive turned clockwise (to the right)
  bool padYButton = false;             // left Y (Index: left B) pressed at all; held 3 s it still quits
  // Where the right controller's ray meets the HUD panel's plane (u, v may lie outside 0..1), when it does.
  bool hudHit = false; float hudU = .5f, hudV = .5f;
  // Filled by the host for games whose aim point moves by rate (Star Fox 64): where the game's own target marker
  // is on the picture now, from the game's memory (ACVR_PAD_FOLLOW_U/V), and how far it moved since last frame.
  bool hasCurrent = false; float curU = .5f, curV = .5f, velU = 0, velV = 0, sumU = 0, sumV = 0;   // sum: integral of the error
};
// All picture and graphics calls run on the thread whose OpenGL context is current
// in `owner` (the emulator thread). The headset runs on its own thread.
bool Start(Host_Window *owner, const Config &config);
void Publish(const std::uint32_t *xrgb, unsigned w, unsigned h);
// The same from a framebuffer of the owner context whose rows are bottom-up (a libretro hardware core).
void PublishFramebuffer(unsigned fbo, unsigned w, unsigned h);
// The HUD layer drawn over black and over white (rows bottom-up), for the HUD panel: alpha = 1 - (white - black).
void PublishHudFramebuffer(unsigned overBlack, unsigned overWhite, unsigned w, unsigned h);
Input ReadInput();          // stale (over 250 ms) input reads as inactive
void QueuePulse();
bool Stop();
bool RuntimeExit();         // the runtime ended the session (SteamVR "Exit game"): close the game
bool Focused();             // the headset shows this game and its controllers reach it (not the SteamVR dashboard)
bool PresenterGone();       // the headset picture stopped for good (see acvr-theatre.log): close the game
// Offline proof without any VR runtime: renders both eyes for a fixed head and hand
// pose into PNG files <prefix>-left.png and <prefix>-right.png.
bool RenderProof(const Config &config, const std::uint32_t *xrgb, unsigned w, unsigned h,
                 float aimU, float aimV, const std::string &prefix, std::string &error,
                 const std::uint32_t *hudArgb = nullptr, unsigned hudW = 0, unsigned hudH = 0);
}
