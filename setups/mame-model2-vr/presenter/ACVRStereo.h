/*
 * ACVR stereo support for Supermodel (AladdinsCastle project).
 * Stereo state shared between the OSD layer, the Model 3 frame loop and the New3D renderer.
 * Licensed like Supermodel itself (GPL v3 or later).
 */
#ifndef INCLUDED_ACVRSTEREO_H
#define INCLUDED_ACVRSTEREO_H
#include "../../../libacvr/publish-presenter/Presenter.h"

struct ACVRStereoState
{
  bool  enabled     = false;  // draw every game frame once per eye
  int   eye         = 0;      // eye being drawn: 0 left, 1 right
  float halfSep     = 0.0f;   // half the eye separation, in game units
  float convergence = 0.0f;   // distance of the zero-parallax plane, in game units (0 = at infinity)

  // Field of view of the game's full-screen 3D viewport, as tangents (written by the renderer)
  float tanL = 0.0f, tanR = 0.0f, tanB = 0.0f, tanT = 0.0f;

  // Experiments for a head-tracked view: a wider camera than the arcade screen, and no frustum culling
  float fovScale = 1.0f;
  bool  noCull   = false;
};

extern ACVRStereoState g_acvrStereo;

// Called by the frame loop after one eye has been drawn to the back buffer
void ACVR_StereoEyeDone(int eye);

// Called once both eyes are stored: lays them side by side in the window
void ACVR_StereoPresent();

// OpenXR presenter (ACVRXR.cpp)
float ACVR_XR_DistanceMetres();
bool  ACVR_XR_Start(void *sdlWindow);
void  ACVR_XR_Publish(unsigned leftFbo, unsigned rightFbo, unsigned w, unsigned h);
// False retains resources for a later retry if their owning context is lost.
bool  ACVR_XR_Stop();
void  ACVR_XR_QueueShotPulse();
void  ACVR_XR_UpdateFov(float left, float right, float bottom, float top);

using ACVRControllerInput=acvr_publish::ControllerInput;
ACVRControllerInput ACVR_XR_ControllerInput();

#endif  // INCLUDED_ACVRSTEREO_H
