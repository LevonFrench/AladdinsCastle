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
  float letterbox = 0;             // share of the picture's height cut from its top and from its bottom (3DS side by side: .25)
  bool swapEyes = false;           // side by side: the left half goes to the right eye and the other way round (ACVR_PS1_SWAP_EYES)
  bool aimDot = true;              // the orange dot where the right controller's ray meets the screen
  bool frame = true;               // the thin dark frame round the screen
  float edgeFade = 0;              // share of the picture's width and height over which its edges fade into the dark
  // The HUD panel: the game's 2D overlay (a separate picture, transparent elsewhere) on its own flat panel, placed
  // with the screen and fixed in the room, nearer than the screen.
  bool hud = false;
  float hudDistance = 1.8f, hudWidth = 1.2f, hudLift = -.1f, hudOpacity = 1.f, hudAspect = 16.f / 9.f;
  bool hudKeyBlack = false;        // opaque pure black in the HUD layer (a game's border bars) shows as transparent
  // The Nintendo 3DS touch panel: the core's frame holds the top screen (the stereo pair the screen shows) and the bottom
  // screen, each a rectangle given as shares of the frame (x, y, w, h from its top left). The bottom screen is a flat panel
  // in front of and below the screen, tilted to face the viewer; the right controller's ray on it is the stylus.
  bool touch = false;
  float touchTop[4] = { 0, 0, 1, 1 }, touchBottom[4] = { 0, 0, 1, 1 };
  float touchWidth = .8f, touchDistance = 1.f, touchLift = -.55f;   // panel width, distance ahead, height against the eyes (metres)
  float touchPanelAspect = 4.f / 3.f;   // the bottom screen's width over its height as drawn in the frame (the host works it out; the core sends new geometry later)
  bool proofAtPanel = false;       // the offline theatre proof aims the right hand at the panel instead of the screen
};
struct Input {
  float u = .5f, v = .5f;
  bool active = false, offScreen = true, trigger = false, a = false, b = false, grip = false;
  // Left controller as a PlayStation pad: stick = d-pad, trigger = Cross, X (Index: A) = Circle, grip = Square.
  // No menu button is read anywhere: the left menu button is the SteamVR dashboard under ALVR.
  float padX = 0, padY = 0;
  bool padCross = false, padCircle = false, padSquare = false;
  // The thumbstick clicks as they are pressed, and what the universal layer (StickChord.h, run by the presenter) makes of
  // them: Start (left click alone), Select (right click alone), and the 3 s two-click quit with its progress, 0..1 (latched
  // once it fires). The recentre chord is handled inside the presenter. Games never see the raw clicks.
  bool stickLeft = false, stickRight = false;
  bool uStart = false, uSelect = false, quitFired = false; float quitProgress = 0;
  // Analog values and the right controller's roll, for pad games (steering wheel, accelerate/brake).
  float triggerValue = 0, padTriggerValue = 0, gripValue = 0, padGripValue = 0;
  float rightX = 0, rightY = 0;   // right thumbstick
  float roll = 0;                 // right controller roll about its pointing axis, radians, positive turned clockwise (to the right)
  bool padYButton = false;             // left Y (Index: left B): a free button now (no quit or Select), for a game or a pad map
  // Where the right controller's ray meets the HUD panel's plane (u, v may lie outside 0..1), when it does.
  bool hudHit = false; float hudU = .5f, hudV = .5f;
  // Where the ray meets the 3DS touch panel (a little past its edge counts, clamped), when it does.
  bool touchHit = false; float touchU = .5f, touchV = .5f;
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
// The picture's shape changed (the core sent new geometry): the screen's aspect and the share of rows cropped top and bottom.
// touchAspect: the 3DS bottom screen's aspect on the panel (0 leaves it).
void SetFrameShape(float aspect, float letterbox, float touchAspect = 0);
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
