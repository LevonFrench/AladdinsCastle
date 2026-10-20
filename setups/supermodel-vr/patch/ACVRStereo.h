/*
 * ACVR stereo and VR support for Supermodel (AladdinsCastle project).
 * State shared between the OSD layer, the Model 3 frame loop, the New3D renderer and the OpenXR presenter.
 * Licensed like Supermodel itself (GPL v3 or later).
 */
#ifndef INCLUDED_ACVRSTEREO_H
#define INCLUDED_ACVRSTEREO_H

// One eye as the headset reports it: position in metres and orientation in the reference space,
// and the field of view as tangents (left, right, down, up)
struct ACVREyeView
{
  float pos[3];
  float rot[4];   // quaternion x, y, z, w
  float tan[4];
};

struct ACVRStereoState
{
  bool  enabled     = false;  // draw every game frame once per eye
  int   eye         = 0;      // eye being drawn: 0 left, 1 right
  float halfSep     = 0.0f;   // screen mode: half the eye separation, in game units
  float convergence = 0.0f;   // screen mode: distance of the zero-parallax plane, in game units (0 = infinity)

  // Field of view of the game's full-screen 3D viewport, as tangents (written by the renderer)
  float tanL = 0.0f, tanR = 0.0f, tanB = 0.0f, tanT = 0.0f;

  // A wider camera than the arcade screen, and no frustum culling
  float fovScale = 1.0f;
  bool  noCull   = false;

  // With the emulator's -wide-screen: also widen full-width scene views that are not full height (letterboxed
  // scenes such as Star Wars Trilogy's, sky strips such as Virtua Fighter 3's). ACVR_WIDE_ALL=0 turns it off.
  bool  wideAll  = true;

  // Head-tracked mode. When vr is true for a frame, the game's full-screen 3D viewports are drawn with the
  // headset's own per-eye frusta and with eyeView applied after the game's view matrix; the 2D layers are
  // left out of the eye pictures and drawn once into a separate HUD picture.
  bool  vr = false;
  float eyeTan[2][4]   = { { 0 } };  // left, right, down, up
  float eyeView[2][16] = { { 0 } };  // column-major

  // Head-tracked: the game's bottom 2D layer (often the sky) is kept as a far backdrop. It is drawn into the
  // rectangle of each eye picture where the arcade view lies, and the rest is cleared to its top-edge colour.
  bool  backdrop = false;                 // this frame has a backdrop to draw
  int   backdropRect[2][4] = { { 0 } };   // per eye: x, y, width, height in target pixels
  float backdropClear[3] = { 0, 0, 0 };

  bool  debugViews = false;               // print every viewport of this frame (ACVR_DEBUG=1, capture frames only)
};

extern ACVRStereoState g_acvrStereo;

// Frame loop hooks (OSD layer)
void ACVR_FrameBegin();             // start of a rendered frame: decides screen or head-tracked mode
bool ACVR_SkipBottomLayer();        // head-tracked: clears the target instead of drawing the bottom 2D layer
void ACVR_StereoEyeDone(int eye);   // one eye has been drawn to the back buffer
void ACVR_BackdropBegin();          // head-tracked: the bottom 2D layer is drawn once to read its sky colour
void ACVR_BackdropEnd();
void ACVR_HudBegin();               // head-tracked: the next 2D draws go to the HUD picture
void ACVR_HudEnd();
void ACVR_StereoPresent();          // both eyes are stored: side-by-side window, hand over to the headset

// OpenXR presenter (ACVRXR.cpp)
struct ACVRPublish
{
  unsigned    eyeFbo[2];
  unsigned    hudFbo;       // 0 = none
  unsigned    w, h;
  bool        vr;           // true: views[] are the eye poses these pictures were drawn for
  ACVREyeView views[2];
  float       originPos[3]; // where the player's head was at recentre
  float       originYaw;    // and which way it faced, radians
};

// The two VR controllers as the headset runtime reports them. Index 0 = left hand, 1 = right hand.
struct ACVRControllers
{
  bool  valid = false;          // false while the headset has no input focus
  float trigger[2] = { 0, 0 };  // 0 to 1
  float grip[2]    = { 0, 0 };  // 0 to 1
  float stick[2][2] = { { 0, 0 }, { 0, 0 } };   // x: right is positive, y: up is positive
  bool  stickClick[2] = { false, false };
  bool  a = false, b = false, x = false, y = false, menu = false;
  bool  gripValid[2] = { false, false };
  float gripPos[2][3] = { { 0 } };              // where each hand is, reference space, metres
  bool  aimValid[2] = { false, false };
  float aimRot[2][4] = { { 0 } };               // where each controller points, quaternion x, y, z, w
  bool  aimPosValid[2] = { false, false };
  float aimPos[2][3] = { { 0 } };               // where each pointing ray starts, reference space, metres

  // Screen mode: where the presenter put the screen (the whole eye picture), in the same space, so a pointing
  // ray can be met with the picture. Yaw only; size in metres.
  bool  screenValid = false;
  float screenPos[3] = { 0, 0, 0 };
  float screenYaw = 0.0f;
  float screenSize[2] = { 0, 0 };
};

bool  ACVR_XR_GetControllers(ACVRControllers &out);
float ACVR_XR_DistanceMetres();
bool  ACVR_XR_Start(void *sdlWindow);
bool  ACVR_XR_GetViews(ACVREyeView out[2]);   // false until the headset is tracked
void  ACVR_XR_Publish(const ACVRPublish &p);
void  ACVR_XR_Stop();

#endif  // INCLUDED_ACVRSTEREO_H
