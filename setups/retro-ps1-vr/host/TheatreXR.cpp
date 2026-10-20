// SPDX-License-Identifier: GPL-3.0-or-later
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#include "TheatreXR.h"
#include "TheatreRender.h"
#include "ImageWait.h"
#include "Png.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif
#ifndef GL_FRAMEBUFFER_SRGB
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#endif

namespace theatre {
namespace {
FILE *s_log = nullptr;
void Log(const char *fmt, ...)
{
  if (!s_log) return;
  va_list ap; va_start(ap, fmt); std::vfprintf(s_log, fmt, ap); va_end(ap); std::fputc('\n', s_log); std::fflush(s_log);
}

// ---- pictures: three sRGB textures in the shared context; the emulator never writes
// the one being shown or the one the headset is drawing.
struct Slot { GLuint tex = 0; int w = 0, h = 0; };
std::mutex s_slotMutex;
Slot s_slots[3];
int s_front = -1, s_inUse = -1;
Slot s_hudSlots[3], s_hudWhiteSlots[3];   // the HUD layer over black and over white, the same way (linear RGBA8)
int s_hudFront = -1, s_hudInUse = -1;

void EnsureSlot(Slot &slot, unsigned w, unsigned h)
{
  if (!slot.tex) glGenTextures(1, &slot.tex);
  glBindTexture(GL_TEXTURE_2D, slot.tex);
  if (slot.w != (int)w || slot.h != (int)h) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    slot.w = (int)w; slot.h = (int)h;
  }
}

void UploadInto(Slot &slot, const std::uint32_t *xrgb, unsigned w, unsigned h)
{
  EnsureSlot(slot, w, h);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, xrgb);
  tgl::GenerateMipmap(GL_TEXTURE_2D);
}

// ---- controller state shared with the emulator thread
std::mutex s_inputMutex;
Input s_input; ULONGLONG s_inputTime = 0;
std::atomic<unsigned> s_pulses{ 0 };
std::atomic<bool> s_stop{ false }, s_started{ false }, s_runtimeExit{ false }, s_gone{ false };
std::atomic<int> s_state{ 0 };   // the session's XrSessionState
std::thread s_thread;
Host_Window *s_owner = nullptr, *s_xrWindow = nullptr;
HGLRC s_ownerContext = nullptr;
Config s_config;
Gun s_gun;

PFN_xrGetInstanceProcAddr p_xrGetInstanceProcAddr = nullptr;
PFN_xrCreateInstance p_xrCreateInstance = nullptr;
#define XR_FUNCS(X) \
  X(xrDestroyInstance) X(xrGetSystem) X(xrCreateSession) X(xrDestroySession) X(xrCreateReferenceSpace) X(xrDestroySpace) \
  X(xrEnumerateSwapchainFormats) X(xrCreateSwapchain) X(xrDestroySwapchain) X(xrEnumerateSwapchainImages) \
  X(xrAcquireSwapchainImage) X(xrWaitSwapchainImage) X(xrReleaseSwapchainImage) X(xrPollEvent) X(xrBeginSession) \
  X(xrEndSession) X(xrWaitFrame) X(xrBeginFrame) X(xrEndFrame) X(xrResultToString) X(xrGetInstanceProperties) \
  X(xrLocateSpace) X(xrLocateViews) X(xrEnumerateViewConfigurationViews) X(xrGetOpenGLGraphicsRequirementsKHR) \
  X(xrStringToPath) X(xrCreateActionSet) X(xrDestroyActionSet) X(xrCreateAction) X(xrSuggestInteractionProfileBindings) \
  X(xrAttachSessionActionSets) X(xrCreateActionSpace) X(xrSyncActions) X(xrGetActionStatePose) X(xrGetActionStateFloat) \
  X(xrGetActionStateBoolean) X(xrGetActionStateVector2f) X(xrApplyHapticFeedback)
#define X(name) PFN_##name p_##name = nullptr;
XR_FUNCS(X)
#undef X

HMODULE s_loader = nullptr;
XrInstance s_instance = XR_NULL_HANDLE; XrSystemId s_system = XR_NULL_SYSTEM_ID; XrSession s_session = XR_NULL_HANDLE;
XrSpace s_local = XR_NULL_HANDLE, s_view = XR_NULL_HANDLE, s_aimSpace = XR_NULL_HANDLE, s_gripSpace = XR_NULL_HANDLE;
XrActionSet s_set = XR_NULL_HANDLE;
XrAction s_aim = XR_NULL_HANDLE, s_gripPose = XR_NULL_HANDLE, s_trigger = XR_NULL_HANDLE, s_a = XR_NULL_HANDLE, s_b = XR_NULL_HANDLE,
         s_squeeze = XR_NULL_HANDLE, s_quit = XR_NULL_HANDLE, s_haptic = XR_NULL_HANDLE,
         s_padStick = XR_NULL_HANDLE, s_padCross = XR_NULL_HANDLE, s_padCircle = XR_NULL_HANDLE, s_padSquare = XR_NULL_HANDLE, s_padStart = XR_NULL_HANDLE,
         s_stickLeft = XR_NULL_HANDLE, s_stickRight = XR_NULL_HANDLE, s_rightStick = XR_NULL_HANDLE;
XrPath s_right = XR_NULL_PATH, s_left = XR_NULL_PATH;
struct EyeChain { XrSwapchain chain = XR_NULL_HANDLE; std::vector<XrSwapchainImageOpenGLKHR> images; int w = 0, h = 0; GLuint fbo = 0, depth = 0; };
EyeChain s_eyes[2];
bool s_srgb = false;

bool Check(XrResult r, const char *what)
{
  if (XR_SUCCEEDED(r)) return true;
  char text[XR_MAX_RESULT_STRING_SIZE] = "";
  if (p_xrResultToString && s_instance) p_xrResultToString(s_instance, r, text);
  Log("FAILED %s: %s (%d)", what, text, (int)r); return false;
}

bool InitActions()
{
  Check(p_xrStringToPath(s_instance, "/user/hand/right", &s_right), "right path");
  Check(p_xrStringToPath(s_instance, "/user/hand/left", &s_left), "left path");
  XrActionSetCreateInfo set{ XR_TYPE_ACTION_SET_CREATE_INFO };
  std::strcpy(set.actionSetName, "ps1_gun"); std::strcpy(set.localizedActionSetName, "Light gun");
  if (!Check(p_xrCreateActionSet(s_instance, &set, &s_set), "xrCreateActionSet")) return false;
  auto make = [](const char *name, const char *label, XrActionType type, XrAction &out, XrPath hand) {
    XrActionCreateInfo info{ XR_TYPE_ACTION_CREATE_INFO };
    std::strcpy(info.actionName, name); std::strcpy(info.localizedActionName, label);
    info.actionType = type; info.countSubactionPaths = 1; info.subactionPaths = &hand;
    return Check(p_xrCreateAction(s_set, &info, &out), name);
  };
  if (!make("aim", "Gun aim", XR_ACTION_TYPE_POSE_INPUT, s_aim, s_right) ||
      !make("grip_pose", "Gun grip", XR_ACTION_TYPE_POSE_INPUT, s_gripPose, s_right) ||
      !make("trigger", "Trigger", XR_ACTION_TYPE_FLOAT_INPUT, s_trigger, s_right) ||
      !make("button_a", "Gun button A", XR_ACTION_TYPE_BOOLEAN_INPUT, s_a, s_right) ||
      !make("button_b", "Gun button B", XR_ACTION_TYPE_BOOLEAN_INPUT, s_b, s_right) ||
      !make("squeeze", "Pedal (grip)", XR_ACTION_TYPE_FLOAT_INPUT, s_squeeze, s_right) ||
      !make("quit", "Hold to quit", XR_ACTION_TYPE_BOOLEAN_INPUT, s_quit, s_left) ||
      !make("pad_stick", "Pad directions", XR_ACTION_TYPE_VECTOR2F_INPUT, s_padStick, s_left) ||
      !make("pad_cross", "Pad Cross", XR_ACTION_TYPE_FLOAT_INPUT, s_padCross, s_left) ||
      !make("pad_circle", "Pad Circle", XR_ACTION_TYPE_BOOLEAN_INPUT, s_padCircle, s_left) ||
      !make("pad_square", "Pad Square", XR_ACTION_TYPE_FLOAT_INPUT, s_padSquare, s_left) ||
      !make("pad_start", "Pad Start", XR_ACTION_TYPE_BOOLEAN_INPUT, s_padStart, s_left) ||
      !make("stick_left", "Left stick click (Select)", XR_ACTION_TYPE_BOOLEAN_INPUT, s_stickLeft, s_left) ||
      !make("stick_right", "Right stick click (Start)", XR_ACTION_TYPE_BOOLEAN_INPUT, s_stickRight, s_right) ||
      !make("right_stick", "Right stick", XR_ACTION_TYPE_VECTOR2F_INPUT, s_rightStick, s_right) ||
      !make("recoil", "Recoil", XR_ACTION_TYPE_VIBRATION_OUTPUT, s_haptic, s_right)) return false;
  struct Profile { const char *profile, *quit, *circle, *start; };
  const Profile profiles[] = { { "/interaction_profiles/oculus/touch_controller", "/user/hand/left/input/y/click", "/user/hand/left/input/x/click", "/user/hand/left/input/menu/click" },
                               { "/interaction_profiles/valve/index_controller", "/user/hand/left/input/b/click", "/user/hand/left/input/a/click", "/user/hand/left/input/thumbstick/click" } };
  for (const auto &pr : profiles) {
    const std::pair<XrAction, const char *> list[] = {
      { s_aim, "/user/hand/right/input/aim/pose" }, { s_gripPose, "/user/hand/right/input/grip/pose" },
      { s_trigger, "/user/hand/right/input/trigger/value" }, { s_a, "/user/hand/right/input/a/click" },
      { s_b, "/user/hand/right/input/b/click" }, { s_squeeze, "/user/hand/right/input/squeeze/value" },
      { s_quit, pr.quit }, { s_haptic, "/user/hand/right/output/haptic" },
      { s_padStick, "/user/hand/left/input/thumbstick" }, { s_padCross, "/user/hand/left/input/trigger/value" },
      { s_padCircle, pr.circle }, { s_padSquare, "/user/hand/left/input/squeeze/value" }, { s_padStart, pr.start },
      { s_stickLeft, "/user/hand/left/input/thumbstick/click" }, { s_stickRight, "/user/hand/right/input/thumbstick/click" },
      { s_rightStick, "/user/hand/right/input/thumbstick" } };
    std::vector<XrActionSuggestedBinding> b;
    for (const auto &item : list) { XrPath path; if (Check(p_xrStringToPath(s_instance, item.second, &path), item.second)) b.push_back({ item.first, path }); }
    XrInteractionProfileSuggestedBinding suggested{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
    if (!Check(p_xrStringToPath(s_instance, pr.profile, &suggested.interactionProfile), pr.profile)) continue;
    suggested.countSuggestedBindings = (uint32_t)b.size(); suggested.suggestedBindings = b.data();
    Check(p_xrSuggestInteractionProfileBindings(s_instance, &suggested), pr.profile);
  }
  XrSessionActionSetsAttachInfo attach{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO }; attach.countActionSets = 1; attach.actionSets = &s_set;
  if (!Check(p_xrAttachSessionActionSets(s_session, &attach), "xrAttachSessionActionSets")) return false;
  XrActionSpaceCreateInfo space{ XR_TYPE_ACTION_SPACE_CREATE_INFO }; space.subactionPath = s_right; space.poseInActionSpace.orientation.w = 1;
  space.action = s_aim; if (!Check(p_xrCreateActionSpace(s_session, &space, &s_aimSpace), "aim space")) return false;
  space.action = s_gripPose; return Check(p_xrCreateActionSpace(s_session, &space, &s_gripSpace), "grip space");
}

bool InitXR()
{
  s_loader = LoadLibraryA("openxr_loader.dll");
  if (!s_loader) { Log("openxr_loader.dll not found beside the program"); return false; }
  p_xrGetInstanceProcAddr = (PFN_xrGetInstanceProcAddr)GetProcAddress(s_loader, "xrGetInstanceProcAddr");
  p_xrCreateInstance = (PFN_xrCreateInstance)GetProcAddress(s_loader, "xrCreateInstance");
  if (!p_xrGetInstanceProcAddr || !p_xrCreateInstance) return false;
  const char *ext[] = { XR_KHR_OPENGL_ENABLE_EXTENSION_NAME };
  XrInstanceCreateInfo ci{ XR_TYPE_INSTANCE_CREATE_INFO };
  std::strncpy(ci.applicationInfo.applicationName, "ACVR PS1 Theatre", XR_MAX_APPLICATION_NAME_SIZE - 1);
  std::strncpy(ci.applicationInfo.engineName, "acvr-ps1", XR_MAX_ENGINE_NAME_SIZE - 1);
  ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0); ci.enabledExtensionCount = 1; ci.enabledExtensionNames = ext;
  if (XR_FAILED(p_xrCreateInstance(&ci, &s_instance))) { Log("No OpenXR runtime with OpenGL (is SteamVR running?)"); return false; }
#define X(name) if (XR_FAILED(p_xrGetInstanceProcAddr(s_instance, #name, (PFN_xrVoidFunction *)&p_##name)) || !p_##name) { Log("missing %s", #name); return false; }
  XR_FUNCS(X)
#undef X
  XrInstanceProperties ip{ XR_TYPE_INSTANCE_PROPERTIES };
  if (XR_SUCCEEDED(p_xrGetInstanceProperties(s_instance, &ip))) Log("runtime: %s", ip.runtimeName);
  XrSystemGetInfo si{ XR_TYPE_SYSTEM_GET_INFO }; si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  if (!Check(p_xrGetSystem(s_instance, &si, &s_system), "xrGetSystem (headset connected?)")) return false;
  XrGraphicsRequirementsOpenGLKHR req{ XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
  if (!Check(p_xrGetOpenGLGraphicsRequirementsKHR(s_instance, s_system, &req), "GL requirements")) return false;
  XrGraphicsBindingOpenGLWin32KHR binding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
  binding.hDC = wglGetCurrentDC(); binding.hGLRC = wglGetCurrentContext();
  XrSessionCreateInfo sci{ XR_TYPE_SESSION_CREATE_INFO }; sci.next = &binding; sci.systemId = s_system;
  if (!Check(p_xrCreateSession(s_instance, &sci, &s_session), "xrCreateSession")) return false;
  XrReferenceSpaceCreateInfo rs{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO }; rs.poseInReferenceSpace.orientation.w = 1;
  rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL; if (!Check(p_xrCreateReferenceSpace(s_session, &rs, &s_local), "local space")) return false;
  rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW; if (!Check(p_xrCreateReferenceSpace(s_session, &rs, &s_view), "view space")) return false;
  if (!InitActions()) return false;
  uint32_t n = 0; p_xrEnumerateSwapchainFormats(s_session, 0, &n, nullptr);
  std::vector<int64_t> formats(n); if (!n || !Check(p_xrEnumerateSwapchainFormats(s_session, n, &n, formats.data()), "formats")) return false;
  int64_t format = 0;
  for (int64_t want : { (int64_t)GL_SRGB8_ALPHA8, (int64_t)GL_RGBA8 }) { for (auto f : formats) if (f == want) { format = f; break; } if (format) break; }
  if (!format) format = formats[0];
  s_srgb = format == GL_SRGB8_ALPHA8;
  XrViewConfigurationView views[2]{ { XR_TYPE_VIEW_CONFIGURATION_VIEW }, { XR_TYPE_VIEW_CONFIGURATION_VIEW } };
  if (!Check(p_xrEnumerateViewConfigurationViews(s_instance, s_system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &n, views), "views")) return false;
  for (int e = 0; e < 2; ++e) {
    auto &eye = s_eyes[e]; eye.w = (int)views[e].recommendedImageRectWidth; eye.h = (int)views[e].recommendedImageRectHeight;
    XrSwapchainCreateInfo sc{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
    sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT; sc.format = format; sc.sampleCount = 1;
    sc.width = eye.w; sc.height = eye.h; sc.faceCount = 1; sc.arraySize = 1; sc.mipCount = 1;
    if (!Check(p_xrCreateSwapchain(s_session, &sc, &eye.chain), "xrCreateSwapchain")) return false;
    uint32_t count = 0; p_xrEnumerateSwapchainImages(eye.chain, 0, &count, nullptr);
    eye.images.assign(count, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
    if (!Check(p_xrEnumerateSwapchainImages(eye.chain, count, &count, (XrSwapchainImageBaseHeader *)eye.images.data()), "images")) return false;
    glGenFramebuffers(1, &eye.fbo); tgl::GenRenderbuffers(1, &eye.depth); tgl::BindRenderbuffer(GL_RENDERBUFFER, eye.depth);
    tgl::RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, eye.w, eye.h);
  }
  Log("eyes %dx%d, swapchain %s", s_eyes[0].w, s_eyes[0].h, s_srgb ? "sRGB" : "linear (colours encoded in the shader)");
  return true;
}

Mat4 PoseMatrix(const XrPosef &p)
{
  const float pos[3] = { p.position.x, p.position.y, p.position.z }, q[4] = { p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w };
  return Mat4::FromPose(pos, q);
}

// The screen goes straight ahead of the viewer, level (yaw only), at eye height plus lift.
ScreenGeometry PlaceScreen(const XrPosef &head)
{
  ScreenGeometry g; g.distance = s_config.distance; g.width = s_config.width; g.height = s_config.width / s_config.aspect;
  g.curve = s_config.curve; g.lift = s_config.lift;
  const auto m = PoseMatrix(head);
  const float fx = -m.m[8], fz = -m.m[10]; const float yaw = std::atan2(-fx, -fz);
  g.frame = Mat4::Translation(head.position.x, head.position.y, head.position.z) * Mat4::Rotation(0, 1, 0, yaw);
  Log("screen placed %.2f m ahead, %.2f x %.2f m, curve %.2f", g.distance, g.width, g.height, g.curve);
  return g;
}
// The HUD panel: flat, in the screen's frame, nearer and (by default) a little lower.
ScreenGeometry HudPanel(const ScreenGeometry &screen, const Config &c)
{
  ScreenGeometry h; h.frame = screen.frame; h.curve = 0; h.distance = c.hudDistance; h.width = c.hudWidth;
  h.height = c.hudWidth / (c.hudAspect > .1f ? c.hudAspect : 16.f / 9.f); h.lift = c.hudLift;
  return h;
}

void ThreadBody()
{
  if (Host_GL_MakeCurrent(s_xrWindow, s_xrWindow->context) != 0) { Log("cannot use the XR OpenGL context"); return; }
  if (!InitXR()) { Log("Theatre off: no headset session"); Host_GL_MakeCurrent(s_xrWindow, nullptr); return; }
  Renderer renderer; try { renderer.init(s_gun.loaded() ? &s_gun : nullptr); } catch (const std::exception &e) { Log("renderer: %s", e.what()); s_stop = true; }
  ScreenGeometry screen; bool placed = false, running = false; unsigned long frames = 0;
  bool lastTrigger = false; ULONGLONG kickAt = 0;
  while (!s_stop) {
    XrEventDataBuffer ev{ XR_TYPE_EVENT_DATA_BUFFER };
    while (p_xrPollEvent(s_instance, &ev) == XR_SUCCESS) {
      if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
        const auto state = reinterpret_cast<XrEventDataSessionStateChanged *>(&ev)->state;
        Log("session state %d", (int)state); s_state = (int)state;
        if (state == XR_SESSION_STATE_READY) { XrSessionBeginInfo bi{ XR_TYPE_SESSION_BEGIN_INFO }; bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO; running = Check(p_xrBeginSession(s_session, &bi), "xrBeginSession"); }
        // STOPPING alone (headset idle, runtime standby) only ends the session: the game waits, paused, for READY.
        else if (state == XR_SESSION_STATE_STOPPING) { Check(p_xrEndSession(s_session), "xrEndSession"); running = false; Log("session stopped: the game waits paused"); }
        // SteamVR's "Exit game" (or a lost runtime) ends with EXITING: the whole game closes, not just the picture.
        else if (state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING) { s_runtimeExit = true; s_stop = true; }
      } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) { s_runtimeExit = true; s_stop = true; }
      ev = { XR_TYPE_EVENT_DATA_BUFFER };
    }
    if (s_stop) break;
    if (!running) { Sleep(50); continue; }
    XrFrameWaitInfo wi{ XR_TYPE_FRAME_WAIT_INFO }; XrFrameState fs{ XR_TYPE_FRAME_STATE };
    if (!Check(p_xrWaitFrame(s_session, &wi, &fs), "xrWaitFrame")) break;
    XrFrameBeginInfo bi{ XR_TYPE_FRAME_BEGIN_INFO };
    if (!Check(p_xrBeginFrame(s_session, &bi), "xrBeginFrame")) break;

    // Controller first: aim ray against the screen, pose of the gun.
    Input in; Mat4 gripWorld = Mat4::Identity(), aimWorld = Mat4::Identity(); bool haveGun = false; float triggerValue = 0;
    XrActiveActionSet active{ s_set, XR_NULL_PATH }; XrActionsSyncInfo sync{ XR_TYPE_ACTIONS_SYNC_INFO }; sync.countActiveActionSets = 1; sync.activeActionSets = &active;
    if (p_xrSyncActions(s_session, &sync) == XR_SUCCESS) {
      XrActionStateGetInfo get{ XR_TYPE_ACTION_STATE_GET_INFO }; get.subactionPath = s_right;
      XrActionStatePose aim{ XR_TYPE_ACTION_STATE_POSE }; XrActionStateFloat trig{ XR_TYPE_ACTION_STATE_FLOAT }, sq{ XR_TYPE_ACTION_STATE_FLOAT };
      XrActionStateBoolean a{ XR_TYPE_ACTION_STATE_BOOLEAN }, b{ XR_TYPE_ACTION_STATE_BOOLEAN }, quit{ XR_TYPE_ACTION_STATE_BOOLEAN };
      get.action = s_aim; const bool aimOk = XR_SUCCEEDED(p_xrGetActionStatePose(s_session, &get, &aim)) && aim.isActive;
      get.action = s_trigger; const bool trigOk = XR_SUCCEEDED(p_xrGetActionStateFloat(s_session, &get, &trig)) && trig.isActive;
      get.action = s_squeeze; const bool sqOk = XR_SUCCEEDED(p_xrGetActionStateFloat(s_session, &get, &sq)) && sq.isActive;
      get.action = s_a; const bool aOk = XR_SUCCEEDED(p_xrGetActionStateBoolean(s_session, &get, &a)) && a.isActive;
      get.action = s_b; const bool bOk = XR_SUCCEEDED(p_xrGetActionStateBoolean(s_session, &get, &b)) && b.isActive;
      get.action = s_quit; get.subactionPath = s_left; in.quitHeld = XR_SUCCEEDED(p_xrGetActionStateBoolean(s_session, &get, &quit)) && quit.isActive && quit.currentState;
      {
        XrActionStateVector2f stick{ XR_TYPE_ACTION_STATE_VECTOR2F }; XrActionStateFloat cross{ XR_TYPE_ACTION_STATE_FLOAT }, square{ XR_TYPE_ACTION_STATE_FLOAT };
        XrActionStateBoolean circle{ XR_TYPE_ACTION_STATE_BOOLEAN }, start{ XR_TYPE_ACTION_STATE_BOOLEAN };
        get.action = s_padStick; if (XR_SUCCEEDED(p_xrGetActionStateVector2f(s_session, &get, &stick)) && stick.isActive) { in.padX = stick.currentState.x; in.padY = stick.currentState.y; }
        get.action = s_padCross; in.padCross = XR_SUCCEEDED(p_xrGetActionStateFloat(s_session, &get, &cross)) && cross.isActive && cross.currentState > .55f;
        if (cross.isActive) in.padTriggerValue = cross.currentState;
        get.action = s_padSquare; in.padSquare = XR_SUCCEEDED(p_xrGetActionStateFloat(s_session, &get, &square)) && square.isActive && square.currentState > .6f;
        if (square.isActive) in.padGripValue = square.currentState;
        in.padYButton = quit.isActive && quit.currentState;
        get.action = s_padCircle; in.padCircle = XR_SUCCEEDED(p_xrGetActionStateBoolean(s_session, &get, &circle)) && circle.isActive && circle.currentState;
        get.action = s_padStart; in.padStart = XR_SUCCEEDED(p_xrGetActionStateBoolean(s_session, &get, &start)) && start.isActive && start.currentState;
        XrActionStateBoolean sl{ XR_TYPE_ACTION_STATE_BOOLEAN }, sr{ XR_TYPE_ACTION_STATE_BOOLEAN };
        get.action = s_stickLeft; in.stickLeft = XR_SUCCEEDED(p_xrGetActionStateBoolean(s_session, &get, &sl)) && sl.isActive && sl.currentState;
        get.action = s_stickRight; get.subactionPath = s_right; in.stickRight = XR_SUCCEEDED(p_xrGetActionStateBoolean(s_session, &get, &sr)) && sr.isActive && sr.currentState;
        XrActionStateVector2f rstick{ XR_TYPE_ACTION_STATE_VECTOR2F };
        get.action = s_rightStick; if (XR_SUCCEEDED(p_xrGetActionStateVector2f(s_session, &get, &rstick)) && rstick.isActive) { in.rightX = rstick.currentState.x; in.rightY = rstick.currentState.y; }
        get.subactionPath = s_left;
      }
      in.active = aimOk || aOk || bOk; in.a = aOk && a.currentState; in.b = bOk && b.currentState; in.grip = sqOk && sq.currentState > .6f;
      triggerValue = trigOk ? trig.currentState : 0.f;
      in.triggerValue = triggerValue; in.gripValue = sqOk ? sq.currentState : 0.f;
      XrSpaceLocation pa{ XR_TYPE_SPACE_LOCATION }, pg{ XR_TYPE_SPACE_LOCATION };
      const auto need = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
      if (aimOk && XR_SUCCEEDED(p_xrLocateSpace(s_aimSpace, s_local, fs.predictedDisplayTime, &pa)) && (pa.locationFlags & need) == need) {
        aimWorld = PoseMatrix(pa.pose);
        gripWorld = XR_SUCCEEDED(p_xrLocateSpace(s_gripSpace, s_local, fs.predictedDisplayTime, &pg)) && (pg.locationFlags & need) == need ? PoseMatrix(pg.pose) : aimWorld;
        haveGun = true;
        in.trigger = triggerValue >= .55f;
        // Roll about the pointing axis: 0 with the controller's right side level, positive turned clockwise.
        in.roll = std::atan2(-aimWorld.m[1], aimWorld.m[5]);
        if (placed) {
          const float o[3] = { aimWorld.m[12], aimWorld.m[13], aimWorld.m[14] }, d[3] = { -aimWorld.m[8], -aimWorld.m[9], -aimWorld.m[10] };
          in.offScreen = !screen.hit(o, d, in.u, in.v);
          if (s_config.hud) { float hu = -9, hv = -9; HudPanel(screen, s_config).hit(o, d, hu, hv); in.hudHit = hu > -8; if (in.hudHit) { in.hudU = hu; in.hudV = hv; } }
        }
      }
    }
    if (in.trigger && !lastTrigger) kickAt = GetTickCount64();
    lastTrigger = in.trigger;
    { std::lock_guard<std::mutex> lock(s_inputMutex); s_input = in; s_inputTime = GetTickCount64(); }
    const unsigned pulses = s_pulses.exchange(0);
    if (in.active && pulses) {
      XrHapticActionInfo hi{ XR_TYPE_HAPTIC_ACTION_INFO }; hi.action = s_haptic; hi.subactionPath = s_right;
      XrHapticVibration v{ XR_TYPE_HAPTIC_VIBRATION }; v.duration = 40000000; v.amplitude = 1.f; v.frequency = XR_FREQUENCY_UNSPECIFIED;
      Check(p_xrApplyHapticFeedback(s_session, &hi, reinterpret_cast<const XrHapticBaseHeader *>(&v)), "recoil pulse");
    }

    XrCompositionLayerProjectionView projViews[2]{ { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW }, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } };
    XrCompositionLayerProjection layer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    const XrCompositionLayerBaseHeader *layers[1] = { reinterpret_cast<XrCompositionLayerBaseHeader *>(&layer) };
    uint32_t layerCount = 0;
    if (fs.shouldRender) {
      XrView views[2]{ { XR_TYPE_VIEW }, { XR_TYPE_VIEW } }; XrViewState vs{ XR_TYPE_VIEW_STATE };
      XrViewLocateInfo li{ XR_TYPE_VIEW_LOCATE_INFO }; li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO; li.displayTime = fs.predictedDisplayTime; li.space = s_local;
      uint32_t vc = 0;
      const bool located = XR_SUCCEEDED(p_xrLocateViews(s_session, &li, &vs, 2, &vc, views)) && vc == 2 &&
        (vs.viewStateFlags & (XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT)) == (XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT);
      if (located) {
        XrSpaceLocation head{ XR_TYPE_SPACE_LOCATION };
        if ((!placed || (GetAsyncKeyState(VK_HOME) & 0x8000)) && XR_SUCCEEDED(p_xrLocateSpace(s_view, s_local, fs.predictedDisplayTime, &head))) { screen = PlaceScreen(head.pose); placed = true; }
        EyeScene scene; scene.screen = screen; scene.crt = s_config.crt;
        if (s_config.sideBySide) scene.uw = .5f;
        scene.frame = s_config.frame; scene.fadeU = s_config.edgeFade; scene.fadeV = s_config.edgeFade * s_config.aspect;
        {
          std::lock_guard<std::mutex> lock(s_slotMutex); s_inUse = s_front; if (s_front >= 0) { scene.picture = s_slots[s_front].tex; scene.pictureW = s_slots[s_front].w; scene.pictureH = s_slots[s_front].h; }
          s_hudInUse = s_hudFront; if (s_config.hud && s_hudFront >= 0) { scene.hud = s_hudSlots[s_hudFront].tex; scene.hudOverWhite = s_hudWhiteSlots[s_hudFront].tex; scene.hudPanel = HudPanel(screen, s_config); scene.hudOpacity = s_config.hudOpacity; scene.hudKeyBlack = s_config.hudKeyBlack; }
        }
        scene.showDot = s_config.aimDot && in.active && !in.offScreen; scene.dotU = in.u; scene.dotV = in.v;
        if (haveGun && s_gun.loaded()) {
          const float since = (GetTickCount64() - kickAt) / 1000.f;
          scene.showGun = true; scene.trigger = triggerValue;
          { const float slideS = s_gun.recoilMs() / 1000.f; scene.recoil = kickAt && since < 2 * slideS ? (since < slideS ? 1.f : 2.f - since / slideS) : 0.f; }   // back, hold, return
          scene.gunRoot = s_gun.alignedRoot(gripWorld, aimWorld, kickAt && since < .1f ? 1.f - since / .1f : 0.f);
        }
        bool ok = true;
        // An image acquired for eye 0 must go back even when eye 1 fails.
        auto releaseFirst = [&](int e) { XrSwapchainImageReleaseInfo rel{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO }; if (e == 1) Check(p_xrReleaseSwapchainImage(s_eyes[0].chain, &rel), "release"); };
        for (int e = 0; e < 2 && ok; ++e) {
          auto &eye = s_eyes[e]; uint32_t index = 0;
          XrSwapchainImageAcquireInfo acq{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
          if (!Check(p_xrAcquireSwapchainImage(eye.chain, &acq, &index), "acquire")) { releaseFirst(e); ok = false; break; }
          // One slow compositor frame is a hitch, not the end: keep waiting up to 2 s before giving up.
          auto waited = acvr_wait::Result::deadline;
          for (int tries = 0; tries < 8 && waited == acvr_wait::Result::deadline; ++tries) {
            waited = acvr_wait::image([&](long long timeout) { XrSwapchainImageWaitInfo w{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO }; w.timeout = timeout; return p_xrWaitSwapchainImage(eye.chain, &w); }, [] { return s_stop.load(); });
            if (waited == acvr_wait::Result::deadline) Log("swapchain wait slow (%d ms so far)", (tries + 1) * 250);
          }
          if (waited != acvr_wait::Result::ready) {
            Log("swapchain wait did not finish"); releaseFirst(e); s_stop = true; ok = false; break;
          }
          glBindFramebuffer(GL_FRAMEBUFFER, eye.fbo);
          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eye.images[index].image, 0);
          tgl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, eye.depth);
          if (s_srgb) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
          const auto &f = views[e].fov;
          const Mat4 proj = Mat4::Projection(std::tan(f.angleLeft), std::tan(f.angleRight), std::tan(f.angleDown), std::tan(f.angleUp));
          scene.u0 = s_config.sideBySide && e ? .5f : 0.f;
          renderer.draw(scene, proj, PoseMatrix(views[e].pose).RigidInverse(), eye.w, eye.h, !s_srgb);
          glDisable(GL_FRAMEBUFFER_SRGB);
          glBindFramebuffer(GL_FRAMEBUFFER, 0);
          if (e == 1) glFinish();   // both eyes complete before either image goes back
          XrSwapchainImageReleaseInfo rel{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
          if (e == 1) { Check(p_xrReleaseSwapchainImage(s_eyes[0].chain, &rel), "release"); Check(p_xrReleaseSwapchainImage(s_eyes[1].chain, &rel), "release"); }
          projViews[e].pose = views[e].pose; projViews[e].fov = views[e].fov;
          projViews[e].subImage.swapchain = eye.chain; projViews[e].subImage.imageRect = { { 0, 0 }, { eye.w, eye.h } };
        }
        if (ok) {
          layer.space = s_local; layer.viewCount = 2; layer.views = projViews; layerCount = 1;
          if (!frames++) Log("first picture shown");
        }
      }
    }
    XrFrameEndInfo ei{ XR_TYPE_FRAME_END_INFO }; ei.displayTime = fs.predictedDisplayTime; ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    ei.layerCount = layerCount; ei.layers = layerCount ? layers : nullptr;
    if (!Check(p_xrEndFrame(s_session, &ei), "xrEndFrame")) break;
  }
  Log("presenter stopping after %lu frames", frames);
  glFinish();
  renderer.release();
  for (auto &eye : s_eyes) { if (eye.chain) p_xrDestroySwapchain(eye.chain); if (eye.fbo) glDeleteFramebuffers(1, &eye.fbo); if (eye.depth) tgl::DeleteRenderbuffers(1, &eye.depth); eye = {}; }
  for (XrSpace sp : { s_aimSpace, s_gripSpace, s_view, s_local }) if (sp) p_xrDestroySpace(sp);
  if (s_set) p_xrDestroyActionSet(s_set);
  if (s_session) p_xrDestroySession(s_session);
  if (s_instance) p_xrDestroyInstance(s_instance);
  s_aimSpace = s_gripSpace = s_view = s_local = XR_NULL_HANDLE; s_set = XR_NULL_HANDLE; s_session = XR_NULL_HANDLE; s_instance = XR_NULL_HANDLE;
  if (s_loader) { FreeLibrary(s_loader); s_loader = nullptr; }
  Host_GL_MakeCurrent(s_xrWindow, nullptr);
}

// The headset thread runs a little above normal so a busy emulator never makes it miss the compositor.
// However it ends, the game is told: a game with no picture must not keep running unseen.
void ThreadMain()
{
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
  ThreadBody();
  s_gone = true;
}

void LoadGun(const Config &c)
{
  if (s_gun.loaded() || c.gunModel.empty()) return;
  try { s_gun.load(c.gunModel, c.gunMeta); Log("gun model: %s", c.gunModel.c_str()); }
  catch (const std::exception &e) { Log("gun model not used: %s", e.what()); s_gun = Gun(); }
}
}  // namespace

bool Start(Host_Window *owner, const Config &config)
{
  if (s_started) return true;
  s_log = std::fopen("acvr-theatre.log", "w");
  s_config = config; s_owner = owner; s_ownerContext = wglGetCurrentContext();
  if (!tgl::Load()) { Log("OpenGL 3.3 entry points unavailable"); return false; }
  LoadGun(config);
  Host_GL_SetAttribute(Host_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
  s_xrWindow = Host_CreateWindow("ACVR theatre", 0, 0, 16, 16, Host_WINDOW_OPENGL | Host_WINDOW_HIDDEN);
  const bool made = s_xrWindow && Host_GL_CreateContext(s_xrWindow);
  Host_GL_SetAttribute(Host_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
  const bool restored = wglMakeCurrent(owner->dc, s_ownerContext);
  if (!made || !restored) { Log("cannot create the shared OpenGL context"); if (s_xrWindow) Host_DestroyWindow(s_xrWindow); s_xrWindow = nullptr; return false; }
  s_stop = false; s_gone = false; s_state = 0; s_started = true;
  s_thread = std::thread(ThreadMain);
  return true;
}

void Publish(const std::uint32_t *xrgb, unsigned w, unsigned h)
{
  if (!s_started || s_stop || !w || !h) return;
  int slot;
  { std::lock_guard<std::mutex> lock(s_slotMutex); slot = 0; while (slot == s_front || slot == s_inUse) ++slot; }
  UploadInto(s_slots[slot], xrgb, w, h);
  glFinish();   // complete before the headset thread may sample it
  std::lock_guard<std::mutex> lock(s_slotMutex); s_front = slot;
}

void PublishFramebuffer(unsigned fbo, unsigned w, unsigned h)
{
  if (!s_started || s_stop || !w || !h) return;
  int slot;
  { std::lock_guard<std::mutex> lock(s_slotMutex); slot = 0; while (slot == s_front || slot == s_inUse) ++slot; }
  EnsureSlot(s_slots[slot], w, h);
  static GLuint target = 0; if (!target) glGenFramebuffers(1, &target);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_slots[slot].tex, 0);
  glDisable(GL_SCISSOR_TEST); glDisable(GL_FRAMEBUFFER_SRGB);
  glBlitFramebuffer(0, 0, w, h, 0, h, w, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);   // bottom-up rows to top-down
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, s_slots[slot].tex); tgl::GenerateMipmap(GL_TEXTURE_2D);
  glFinish();
  std::lock_guard<std::mutex> lock(s_slotMutex); s_front = slot;
}

void EnsureLinearSlot(Slot &slot, unsigned w, unsigned h)
{
  if (!slot.tex) glGenTextures(1, &slot.tex);
  glBindTexture(GL_TEXTURE_2D, slot.tex);
  if (slot.w != (int)w || slot.h != (int)h) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    slot.w = (int)w; slot.h = (int)h;
  }
}
void PublishHudFramebuffer(unsigned overBlack, unsigned overWhite, unsigned w, unsigned h)
{
  if (!s_started || s_stop || !w || !h) return;
  int slot;
  { std::lock_guard<std::mutex> lock(s_slotMutex); slot = 0; while (slot == s_hudFront || slot == s_hudInUse) ++slot; }
  static GLuint target = 0; if (!target) glGenFramebuffers(1, &target);
  glDisable(GL_SCISSOR_TEST); glDisable(GL_FRAMEBUFFER_SRGB);
  for (int i = 0; i < 2; ++i) {
    Slot &s = i ? s_hudWhiteSlots[slot] : s_hudSlots[slot];
    EnsureLinearSlot(s, w, h);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, i ? overWhite : overBlack); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s.tex, 0);
    glBlitFramebuffer(0, 0, w, h, 0, h, w, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, s.tex); tgl::GenerateMipmap(GL_TEXTURE_2D);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glFinish();
  std::lock_guard<std::mutex> lock(s_slotMutex); s_hudFront = slot;
}

Input ReadInput()
{
  std::lock_guard<std::mutex> lock(s_inputMutex);
  return GetTickCount64() - s_inputTime < 250 ? s_input : Input{};
}

void QueuePulse() { if (s_started && !s_stop) ++s_pulses; }
bool RuntimeExit() { return s_runtimeExit.load(); }
bool Focused() { return s_state.load() == XR_SESSION_STATE_FOCUSED; }
bool PresenterGone() { return s_gone.load(); }

bool Stop()
{
  if (!s_started) return true;
  s_stop = true;
  if (s_thread.joinable()) s_thread.join();
  if (s_xrWindow) { Host_DestroyWindow(s_xrWindow); s_xrWindow = nullptr; }
  wglMakeCurrent(s_owner->dc, s_ownerContext);
  for (auto &slot : s_slots) { if (slot.tex) glDeleteTextures(1, &slot.tex); slot = {}; }
  for (auto &slot : s_hudSlots) { if (slot.tex) glDeleteTextures(1, &slot.tex); slot = {}; }
  for (auto &slot : s_hudWhiteSlots) { if (slot.tex) glDeleteTextures(1, &slot.tex); slot = {}; }
  s_front = s_inUse = -1; s_hudFront = s_hudInUse = -1; s_started = false;
  if (s_log) { std::fclose(s_log); s_log = nullptr; }
  return true;
}

bool RenderProof(const Config &config, const std::uint32_t *xrgb, unsigned w, unsigned h, float aimU, float aimV, const std::string &prefix, std::string &error,
                 const std::uint32_t *hudArgb, unsigned hudW, unsigned hudH)
{
  try {
    if (!tgl::Load()) throw std::runtime_error("OpenGL 3.3 entry points unavailable");
    LoadGun(config);
    Slot picture; UploadInto(picture, xrgb, w, h);
    Renderer renderer; renderer.init(s_gun.loaded() ? &s_gun : nullptr);
    ScreenGeometry screen; screen.distance = config.distance; screen.width = config.width; screen.height = config.width / config.aspect;
    screen.curve = config.curve; screen.lift = config.lift;
    // Head at the origin looking down -Z; right hand low and forward, aimed at (aimU, aimV).
    const auto target = screen.local(aimU, aimV);
    const float hand[3] = { .16f, -.28f, -.32f };
    float dir[3] = { target[0] - hand[0], target[1] - hand[1], target[2] - hand[2] };
    const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]); for (float &x : dir) x /= len;
    float right[3] = { -dir[2], 0, dir[0] }; const float rl = std::sqrt(right[0] * right[0] + right[2] * right[2]); right[0] /= rl; right[2] /= rl;
    const float back[3] = { -dir[0], -dir[1], -dir[2] };
    const float up[3] = { back[1] * right[2] - back[2] * right[1], back[2] * right[0] - back[0] * right[2], back[0] * right[1] - back[1] * right[0] };
    Mat4 aim = Mat4::Identity();
    for (int i = 0; i < 3; ++i) { aim.m[i] = right[i]; aim.m[4 + i] = up[i]; aim.m[8 + i] = -dir[i]; aim.m[12 + i] = hand[i]; }
    float u = -1, v = -1; const bool on = screen.hit(hand, dir, u, v);
    EyeScene scene; scene.screen = screen; scene.crt = config.crt; scene.picture = picture.tex; scene.pictureW = picture.w; scene.pictureH = picture.h;
    if (config.sideBySide) scene.uw = .5f;
    scene.frame = config.frame; scene.fadeU = config.edgeFade; scene.fadeV = config.edgeFade * config.aspect;
    Slot hud;
    if (config.hud && hudArgb && hudW && hudH) { UploadInto(hud, hudArgb, hudW, hudH); scene.hud = hud.tex; scene.hudPanel = HudPanel(screen, config); scene.hudOpacity = config.hudOpacity; scene.hudKeyBlack = config.hudKeyBlack; }
    scene.showDot = on && config.aimDot; scene.dotU = u; scene.dotV = v;
    if (s_gun.loaded()) { scene.showGun = true; scene.gunRoot = s_gun.alignedRoot(aim, aim, 0); }
    const int size = 1440; GLuint fbo = 0, color = 0, depth = 0;
    glGenFramebuffers(1, &fbo); glGenTextures(1, &color); tgl::GenRenderbuffers(1, &depth);
    glBindTexture(GL_TEXTURE_2D, color); glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    tgl::BindRenderbuffer(GL_RENDERBUFFER, depth); tgl::RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    tgl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (tgl::CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("proof framebuffer incomplete");
    const Mat4 proj = Mat4::Projection(-1.f, 1.f, -1.f, 1.f);   // 90 degrees, like a headset eye
    bool ok = true;
    for (int e = 0; e < 2; ++e) {
      const Mat4 eyePose = Mat4::Translation(e ? .032f : -.032f, 0, 0);
      scene.u0 = config.sideBySide && e ? .5f : 0.f;
      glEnable(GL_FRAMEBUFFER_SRGB);
      renderer.draw(scene, proj, eyePose.RigidInverse(), size, size, false);
      glDisable(GL_FRAMEBUFFER_SRGB);
      std::vector<std::uint32_t> px((std::size_t)size * size), top(px.size());
      glReadPixels(0, 0, size, size, GL_BGRA, GL_UNSIGNED_BYTE, px.data());
      for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) top[(std::size_t)y * size + x] = px[(std::size_t)(size - 1 - y) * size + x] & 0xffffffu;
      ok = acvr_png::write((prefix + (e ? "-right.png" : "-left.png")).c_str(), top.data(), size, size) && ok;
    }
    char note[256]; std::snprintf(note, sizeof note, "aim requested (%.4f, %.4f), ray hit %s (%.4f, %.4f), gun %s", aimU, aimV, on ? "on screen" : "off screen", u, v, s_gun.loaded() ? "drawn" : "not loaded");
    error = note;
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &color); tgl::DeleteRenderbuffers(1, &depth);
    glDeleteTextures(1, &picture.tex); if (hud.tex) glDeleteTextures(1, &hud.tex); renderer.release();
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL error during the proof render");
    return ok;
  } catch (const std::exception &e) { error = e.what(); return false; }
}
}
