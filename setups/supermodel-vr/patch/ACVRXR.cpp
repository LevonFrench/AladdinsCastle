/*
 * ACVR OpenXR presenter for Supermodel (AladdinsCastle project). GPL v3 or later, like Supermodel.
 *
 * Two ways of showing the eye pictures in a VR headset:
 *
 *   Screen mode        two quad layers, one visible to each eye, placed in front of the player and sized to
 *                      the game's own field of view: a window onto the scene at real scale.
 *   Head-tracked mode  a projection layer: the pictures were drawn for the headset's own eye poses and fill
 *                      the view. The 2D layers arrive as a separate picture and are shown on a HUD panel.
 *
 * The emulator's frame loop is not touched: the pictures are copied into shared textures ("published") and
 * a separate thread, with its own OpenGL context, submits them at the headset's rate. The game keeps its own
 * 57.5 Hz pacing. In head-tracked mode each published frame carries the eye poses it was drawn for, so the
 * runtime can re-aim it for head turns between game frames.
 *
 * Environment:
 *   ACVR_XR=1                turn the presenter on
 *   ACVR_XR_DISTANCE=<m>     screen mode: distance of the screen in metres (default 2.5)
 *   ACVR_XR_SIZE=<n>         screen mode: multiplier on the screen size (default 1)
 *   ACVR_XR_HEIGHT=<m>       height of the screen or HUD centre relative to the eyes (default 0)
 *   ACVR_HUD_DISTANCE=<m>    head-tracked: distance of the HUD panel (default 1.6)
 *   ACVR_HUD_WIDTH=<m>       head-tracked: width of the HUD panel, 4:3 (default 1.6)
 *
 * Controllers: triggers, grips, thumbsticks, face buttons and hand poses are read every headset frame and
 * handed to the game thread, which turns them into cabinet inputs.
 *
 * The Home key recentres. Everything the presenter does is written to acvr-xr.log in the working folder.
 */
#ifdef _WIN32

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#include <windows.h>
#include <GL/glew.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "SDLIncludes.h"
#include "OSD/ACVRStereo.h"

namespace
{
  enum { LEFT = 0, RIGHT = 1, HUD = 2, CHAINS = 3 };

  // ---- shared between the game thread and the XR thread
  std::mutex        s_mutex;                    // guards everything published and the newest eye views
  GLuint            s_pubTex[CHAINS] = { 0, 0, 0 };     // shared textures holding the newest finished pictures
  GLuint            s_pubFboMain[CHAINS] = { 0, 0, 0 }; // game-thread framebuffers on those textures
  unsigned          s_pubW = 0, s_pubH = 0;
  unsigned long     s_pubSerial = 0;
  ACVRPublish       s_pubInfo;                  // mode, poses and origin of the published frame
  bool              s_pubHasHud = false;
  // GPU hand-off between the two contexts (sync objects are shared like the textures):
  // s_pubReady is signalled when the game thread's copies into s_pubTex are complete,
  // s_readDone when the XR thread has finished reading them. Each side waits on the other's.
  GLsync            s_pubReady = nullptr;
  GLsync            s_readDone = nullptr;
  ACVREyeView       s_latestViews[2];           // newest eye poses from the headset, for the game thread
  bool              s_latestValid = false;
  std::atomic<bool> s_stop(false);
  std::atomic<bool> s_started(false);
  std::thread       s_thread;
  std::atomic<bool> s_threadDone(false);        // set by the XR thread as its very last step
  std::atomic<bool> s_wanted(false);            // ACVR_XR_Start was called
  std::atomic<bool> s_runtimeExit(false);       // the runtime ended the session or the instance
  std::atomic<bool> s_sessionRunning(false);    // between xrBeginSession and STOPPING
  SDL_Window       *s_xrWindow = nullptr;
  SDL_GLContext     s_xrContext = nullptr;

  float s_distance = 2.5f, s_sizeScale = 1.0f, s_height = 0.0f, s_hudDistance = 1.6f, s_hudWidth = 1.6f;

  FILE *s_log = nullptr;
  void Log(const char *fmt, ...)
  {
    if (!s_log) return;
    va_list ap; va_start(ap, fmt); vfprintf(s_log, fmt, ap); va_end(ap);
    fputc('\n', s_log); fflush(s_log);
  }

  float EnvFloat(const char *name, float fallback)
  {
    const char *v = getenv(name);
    if (!v || !*v) return fallback;
    float f = (float)atof(v);
    return std::isfinite(f) ? f : fallback;
  }

  // ---- OpenXR entry points, resolved at run time so the emulator still starts without the loader
  PFN_xrGetInstanceProcAddr p_xrGetInstanceProcAddr = nullptr;
  PFN_xrCreateInstance      p_xrCreateInstance = nullptr;
#define ACVR_XR_FUNCS(X) \
  X(xrDestroyInstance) X(xrGetSystem) X(xrCreateSession) X(xrDestroySession) X(xrCreateReferenceSpace) \
  X(xrDestroySpace) X(xrEnumerateSwapchainFormats) X(xrCreateSwapchain) X(xrDestroySwapchain) \
  X(xrEnumerateSwapchainImages) X(xrAcquireSwapchainImage) X(xrWaitSwapchainImage) X(xrReleaseSwapchainImage) \
  X(xrPollEvent) X(xrBeginSession) X(xrEndSession) X(xrRequestExitSession) X(xrWaitFrame) X(xrBeginFrame) \
  X(xrEndFrame) X(xrResultToString) X(xrGetInstanceProperties) X(xrGetSystemProperties) \
  X(xrLocateSpace) X(xrLocateViews) X(xrGetOpenGLGraphicsRequirementsKHR) \
  X(xrStringToPath) X(xrCreateActionSet) X(xrCreateAction) X(xrSuggestInteractionProfileBindings) \
  X(xrAttachSessionActionSets) X(xrSyncActions) X(xrGetActionStateFloat) X(xrGetActionStateBoolean) \
  X(xrGetActionStateVector2f) X(xrCreateActionSpace)
#define X(name) PFN_##name p_##name = nullptr;
  ACVR_XR_FUNCS(X)
#undef X

  XrInstance  s_instance = XR_NULL_HANDLE;
  XrSystemId  s_system = XR_NULL_SYSTEM_ID;
  XrSession   s_session = XR_NULL_HANDLE;
  XrSpace     s_space = XR_NULL_HANDLE;
  XrSpace     s_viewSpace = XR_NULL_HANDLE;
  XrPosef     s_quadPose;
  bool        s_poseSet = false;
  float       s_quadW = 0.0f, s_quadH = 0.0f;   // the screen's size as last submitted (screen mode)
  XrSwapchain s_chain[CHAINS] = { XR_NULL_HANDLE, XR_NULL_HANDLE, XR_NULL_HANDLE };
  std::vector<XrSwapchainImageOpenGLKHR> s_images[CHAINS];
  unsigned    s_chainW = 0, s_chainH = 0;
  int64_t     s_format = 0;

  const char *Res(XrResult r)
  {
    static char buf[XR_MAX_RESULT_STRING_SIZE];
    if (p_xrResultToString && s_instance != XR_NULL_HANDLE && XR_SUCCEEDED(p_xrResultToString(s_instance, r, buf)))
      return buf;
    snprintf(buf, sizeof buf, "XrResult %d", (int)r);
    return buf;
  }

  bool Check(XrResult r, const char *what)
  {
    if (XR_SUCCEEDED(r)) return true;
    Log("FAILED %s: %s", what, Res(r));
    return false;
  }

  bool InitXR()
  {
    HMODULE loader = LoadLibraryA("openxr_loader.dll");
    if (!loader) { Log("openxr_loader.dll not found beside the program (error %lu)", GetLastError()); return false; }
    p_xrGetInstanceProcAddr = (PFN_xrGetInstanceProcAddr)GetProcAddress(loader, "xrGetInstanceProcAddr");
    p_xrCreateInstance      = (PFN_xrCreateInstance)GetProcAddress(loader, "xrCreateInstance");
    if (!p_xrGetInstanceProcAddr || !p_xrCreateInstance) { Log("loader lacks its entry points"); return false; }

    const char *extensions[] = { XR_KHR_OPENGL_ENABLE_EXTENSION_NAME };
    XrInstanceCreateInfo ci = { XR_TYPE_INSTANCE_CREATE_INFO };
    strncpy(ci.applicationInfo.applicationName, "ACVR Supermodel", XR_MAX_APPLICATION_NAME_SIZE - 1);
    strncpy(ci.applicationInfo.engineName, "Supermodel", XR_MAX_ENGINE_NAME_SIZE - 1);
    ci.applicationInfo.applicationVersion = 1;
    ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    ci.enabledExtensionCount = 1;
    ci.enabledExtensionNames = extensions;
    XrResult r = p_xrCreateInstance(&ci, &s_instance);
    if (XR_FAILED(r)) { Log("FAILED xrCreateInstance: XrResult %d (is an OpenXR runtime with OpenGL support active, e.g. SteamVR?)", (int)r); return false; }

#define X(name) if (XR_FAILED(p_xrGetInstanceProcAddr(s_instance, #name, (PFN_xrVoidFunction *)&p_##name)) || !p_##name) { Log("missing OpenXR function %s", #name); return false; }
    ACVR_XR_FUNCS(X)
#undef X

    XrInstanceProperties ip = { XR_TYPE_INSTANCE_PROPERTIES };
    if (XR_SUCCEEDED(p_xrGetInstanceProperties(s_instance, &ip)))
      Log("runtime: %s %u.%u.%u", ip.runtimeName, XR_VERSION_MAJOR(ip.runtimeVersion), XR_VERSION_MINOR(ip.runtimeVersion), XR_VERSION_PATCH(ip.runtimeVersion));

    XrSystemGetInfo si = { XR_TYPE_SYSTEM_GET_INFO };
    si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (!Check(p_xrGetSystem(s_instance, &si, &s_system), "xrGetSystem (is the headset connected?)")) return false;
    XrSystemProperties sp = { XR_TYPE_SYSTEM_PROPERTIES };
    if (XR_SUCCEEDED(p_xrGetSystemProperties(s_instance, s_system, &sp)))
      Log("system: %s", sp.systemName);

    XrGraphicsRequirementsOpenGLKHR req = { XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
    if (!Check(p_xrGetOpenGLGraphicsRequirementsKHR(s_instance, s_system, &req), "xrGetOpenGLGraphicsRequirementsKHR")) return false;

    XrGraphicsBindingOpenGLWin32KHR binding = { XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
    binding.hDC = wglGetCurrentDC();
    binding.hGLRC = wglGetCurrentContext();
    if (!binding.hDC || !binding.hGLRC) { Log("no OpenGL context on the XR thread"); return false; }
    XrSessionCreateInfo sci = { XR_TYPE_SESSION_CREATE_INFO };
    sci.next = &binding;
    sci.systemId = s_system;
    if (!Check(p_xrCreateSession(s_instance, &sci, &s_session), "xrCreateSession")) return false;

    XrReferenceSpaceCreateInfo rs = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rs.poseInReferenceSpace.orientation.w = 1.0f;
    if (!Check(p_xrCreateReferenceSpace(s_session, &rs, &s_space), "xrCreateReferenceSpace")) return false;
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    if (!Check(p_xrCreateReferenceSpace(s_session, &rs, &s_viewSpace), "xrCreateReferenceSpace (view)")) return false;

    uint32_t n = 0;
    p_xrEnumerateSwapchainFormats(s_session, 0, &n, nullptr);
    std::vector<int64_t> formats(n);
    if (n == 0 || !Check(p_xrEnumerateSwapchainFormats(s_session, n, &n, formats.data()), "xrEnumerateSwapchainFormats")) return false;
    // The emulator's picture holds sRGB-encoded values, so an sRGB format shows it unchanged.
    const int64_t wanted[] = { GL_SRGB8_ALPHA8, GL_RGBA8 };
    for (int64_t w : wanted)
    {
      for (int64_t f : formats) if (f == w) { s_format = w; break; }
      if (s_format) break;
    }
    if (!s_format) s_format = formats[0];
    Log("swapchain format 0x%llX%s", (long long)s_format, s_format == GL_RGBA8 ? " (not sRGB: colours may look washed out)" : "");
    return true;
  }

  void InitInput();

  // ---- controllers
  ACVRControllers s_controllers;              // newest state, guarded by s_mutex
  Uint32          s_controllersTicks = 0;     // SDL_GetTicks() when s_controllers was written, guarded by s_mutex
  const Uint32    CONTROLLERS_MAX_AGE_MS = 250;

  // No input: nothing held, nothing pressed. Used when the session loses focus, the sync fails or the thread ends.
  void ClearControllers()
  {
    std::lock_guard<std::mutex> guard(s_mutex);
    s_controllers = ACVRControllers();
    s_controllersTicks = SDL_GetTicks();
  }
  bool s_focused = false;                     // XR thread only: the session is FOCUSED, so input is ours
  XrActionSet s_actionSet = XR_NULL_HANDLE;
  XrAction    s_actTrigger = XR_NULL_HANDLE, s_actGrip = XR_NULL_HANDLE, s_actStick = XR_NULL_HANDLE, s_actStickClick = XR_NULL_HANDLE;
  XrAction    s_actGripPose = XR_NULL_HANDLE, s_actAimPose = XR_NULL_HANDLE;
  XrAction    s_actA = XR_NULL_HANDLE, s_actB = XR_NULL_HANDLE, s_actX = XR_NULL_HANDLE, s_actY = XR_NULL_HANDLE, s_actMenu = XR_NULL_HANDLE;
  XrPath      s_handPath[2] = { XR_NULL_PATH, XR_NULL_PATH };
  XrSpace     s_gripSpace[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE }, s_aimSpace[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
  bool        s_inputReady = false;

  XrPath Path(const char *s)
  {
    XrPath p = XR_NULL_PATH;
    p_xrStringToPath(s_instance, s, &p);
    return p;
  }

  XrAction MakeAction(const char *name, XrActionType type, bool perHand)
  {
    XrActionCreateInfo ci = { XR_TYPE_ACTION_CREATE_INFO };
    strncpy(ci.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
    strncpy(ci.localizedActionName, name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    ci.actionType = type;
    if (perHand) { ci.countSubactionPaths = 2; ci.subactionPaths = s_handPath; }
    XrAction a = XR_NULL_HANDLE;
    Check(p_xrCreateAction(s_actionSet, &ci, &a), name);
    return a;
  }

  void Suggest(const char *profile, const std::vector<XrActionSuggestedBinding> &bindings)
  {
    XrInteractionProfileSuggestedBinding sb = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
    sb.interactionProfile = Path(profile);
    sb.countSuggestedBindings = (uint32_t)bindings.size();
    sb.suggestedBindings = bindings.data();
    const XrResult r = p_xrSuggestInteractionProfileBindings(s_instance, &sb);
    Log("controller bindings %s: %s", profile, XR_SUCCEEDED(r) ? "ok" : Res(r));
  }

  // Controller input is optional: if any of this fails the pictures still work
  void InitInput()
  {
    s_handPath[0] = Path("/user/hand/left");
    s_handPath[1] = Path("/user/hand/right");
    XrActionSetCreateInfo si = { XR_TYPE_ACTION_SET_CREATE_INFO };
    strcpy(si.actionSetName, "acvr");
    strcpy(si.localizedActionSetName, "ACVR cabinet controls");
    if (!Check(p_xrCreateActionSet(s_instance, &si, &s_actionSet), "xrCreateActionSet")) return;
    s_actTrigger    = MakeAction("trigger", XR_ACTION_TYPE_FLOAT_INPUT, true);
    s_actGrip       = MakeAction("grip", XR_ACTION_TYPE_FLOAT_INPUT, true);
    s_actStick      = MakeAction("stick", XR_ACTION_TYPE_VECTOR2F_INPUT, true);
    s_actStickClick = MakeAction("stick_click", XR_ACTION_TYPE_BOOLEAN_INPUT, true);
    s_actGripPose   = MakeAction("grip_pose", XR_ACTION_TYPE_POSE_INPUT, true);
    s_actAimPose    = MakeAction("aim_pose", XR_ACTION_TYPE_POSE_INPUT, true);
    s_actA    = MakeAction("button_a", XR_ACTION_TYPE_BOOLEAN_INPUT, false);
    s_actB    = MakeAction("button_b", XR_ACTION_TYPE_BOOLEAN_INPUT, false);
    s_actX    = MakeAction("button_x", XR_ACTION_TYPE_BOOLEAN_INPUT, false);
    s_actY    = MakeAction("button_y", XR_ACTION_TYPE_BOOLEAN_INPUT, false);
    s_actMenu = MakeAction("button_menu", XR_ACTION_TYPE_BOOLEAN_INPUT, false);

    std::vector<XrActionSuggestedBinding> common;
    const char *hands[2] = { "/user/hand/left", "/user/hand/right" };
    for (int h = 0; h < 2; h++)
    {
      const std::string base = hands[h];
      common.push_back({ s_actTrigger,    Path((base + "/input/trigger/value").c_str()) });
      common.push_back({ s_actGrip,       Path((base + "/input/squeeze/value").c_str()) });
      common.push_back({ s_actStick,      Path((base + "/input/thumbstick").c_str()) });
      common.push_back({ s_actStickClick, Path((base + "/input/thumbstick/click").c_str()) });
      common.push_back({ s_actGripPose,   Path((base + "/input/grip/pose").c_str()) });
      common.push_back({ s_actAimPose,    Path((base + "/input/aim/pose").c_str()) });
    }
    std::vector<XrActionSuggestedBinding> touch = common;
    touch.push_back({ s_actA,    Path("/user/hand/right/input/a/click") });
    touch.push_back({ s_actB,    Path("/user/hand/right/input/b/click") });
    touch.push_back({ s_actX,    Path("/user/hand/left/input/x/click") });
    touch.push_back({ s_actY,    Path("/user/hand/left/input/y/click") });
    touch.push_back({ s_actMenu, Path("/user/hand/left/input/menu/click") });
    Suggest("/interaction_profiles/oculus/touch_controller", touch);
    std::vector<XrActionSuggestedBinding> index = common;
    index.push_back({ s_actA, Path("/user/hand/right/input/a/click") });
    index.push_back({ s_actB, Path("/user/hand/right/input/b/click") });
    index.push_back({ s_actX, Path("/user/hand/left/input/a/click") });
    index.push_back({ s_actY, Path("/user/hand/left/input/b/click") });
    Suggest("/interaction_profiles/valve/index_controller", index);

    XrSessionActionSetsAttachInfo ai = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
    ai.countActionSets = 1;
    ai.actionSets = &s_actionSet;
    if (!Check(p_xrAttachSessionActionSets(s_session, &ai), "xrAttachSessionActionSets")) return;
    for (int h = 0; h < 2; h++)
    {
      XrActionSpaceCreateInfo sc = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
      sc.subactionPath = s_handPath[h];
      sc.poseInActionSpace.orientation.w = 1.0f;
      sc.action = s_actGripPose;
      p_xrCreateActionSpace(s_session, &sc, &s_gripSpace[h]);
      sc.action = s_actAimPose;
      p_xrCreateActionSpace(s_session, &sc, &s_aimSpace[h]);
    }
    s_inputReady = true;
    Log("controller input ready");
  }

  float ReadFloat(XrAction a, int hand)
  {
    XrActionStateGetInfo gi = { XR_TYPE_ACTION_STATE_GET_INFO };
    gi.action = a; gi.subactionPath = s_handPath[hand];
    XrActionStateFloat st = { XR_TYPE_ACTION_STATE_FLOAT };
    return (XR_SUCCEEDED(p_xrGetActionStateFloat(s_session, &gi, &st)) && st.isActive) ? st.currentState : 0.0f;
  }

  bool ReadBool(XrAction a, XrPath sub)
  {
    XrActionStateGetInfo gi = { XR_TYPE_ACTION_STATE_GET_INFO };
    gi.action = a; gi.subactionPath = sub;
    XrActionStateBoolean st = { XR_TYPE_ACTION_STATE_BOOLEAN };
    return XR_SUCCEEDED(p_xrGetActionStateBoolean(s_session, &gi, &st)) && st.isActive && st.currentState;
  }

  // Reads the controllers and hands the result to the game thread
  void UpdateControllers(XrTime time)
  {
    ACVRControllers c;
    if (s_inputReady && s_focused)
    {
      XrActiveActionSet active = { s_actionSet, XR_NULL_PATH };
      XrActionsSyncInfo si = { XR_TYPE_ACTIONS_SYNC_INFO };
      si.countActiveActionSets = 1;
      si.activeActionSets = &active;
      if (p_xrSyncActions(s_session, &si) == XR_SUCCESS)    // XR_SESSION_NOT_FOCUSED also "succeeds": then there is no input
      {
        c.valid = true;
        const XrSpaceLocationFlags posOk = XR_SPACE_LOCATION_POSITION_VALID_BIT;
        const XrSpaceLocationFlags rotOk = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        for (int h = 0; h < 2; h++)
        {
          c.trigger[h] = ReadFloat(s_actTrigger, h);
          c.grip[h] = ReadFloat(s_actGrip, h);
          XrActionStateGetInfo gi = { XR_TYPE_ACTION_STATE_GET_INFO };
          gi.action = s_actStick; gi.subactionPath = s_handPath[h];
          XrActionStateVector2f v = { XR_TYPE_ACTION_STATE_VECTOR2F };
          if (XR_SUCCEEDED(p_xrGetActionStateVector2f(s_session, &gi, &v)) && v.isActive) { c.stick[h][0] = v.currentState.x; c.stick[h][1] = v.currentState.y; }
          c.stickClick[h] = ReadBool(s_actStickClick, s_handPath[h]);
          XrSpaceLocation loc = { XR_TYPE_SPACE_LOCATION };
          if (s_gripSpace[h] != XR_NULL_HANDLE && XR_SUCCEEDED(p_xrLocateSpace(s_gripSpace[h], s_space, time, &loc)) && (loc.locationFlags & posOk))
          {
            c.gripValid[h] = true;
            c.gripPos[h][0] = loc.pose.position.x; c.gripPos[h][1] = loc.pose.position.y; c.gripPos[h][2] = loc.pose.position.z;
          }
          loc = { XR_TYPE_SPACE_LOCATION };
          if (s_aimSpace[h] != XR_NULL_HANDLE && XR_SUCCEEDED(p_xrLocateSpace(s_aimSpace[h], s_space, time, &loc)) && (loc.locationFlags & rotOk))
          {
            c.aimValid[h] = true;
            c.aimRot[h][0] = loc.pose.orientation.x; c.aimRot[h][1] = loc.pose.orientation.y;
            c.aimRot[h][2] = loc.pose.orientation.z; c.aimRot[h][3] = loc.pose.orientation.w;
            if (loc.locationFlags & posOk)
            {
              c.aimPosValid[h] = true;
              c.aimPos[h][0] = loc.pose.position.x; c.aimPos[h][1] = loc.pose.position.y; c.aimPos[h][2] = loc.pose.position.z;
            }
          }
        }
        if (s_poseSet && s_quadW > 0.0f)
        {
          c.screenValid = true;
          c.screenPos[0] = s_quadPose.position.x; c.screenPos[1] = s_quadPose.position.y; c.screenPos[2] = s_quadPose.position.z;
          c.screenYaw = 2.0f * atan2f(s_quadPose.orientation.y, s_quadPose.orientation.w);
          c.screenSize[0] = s_quadW; c.screenSize[1] = s_quadH;
        }
        c.a = ReadBool(s_actA, XR_NULL_PATH);
        c.b = ReadBool(s_actB, XR_NULL_PATH);
        c.x = ReadBool(s_actX, XR_NULL_PATH);
        c.y = ReadBool(s_actY, XR_NULL_PATH);
        c.menu = ReadBool(s_actMenu, XR_NULL_PATH);
      }
    }
    // Not focused, or the sync failed: c is still the empty, invalid state, so nothing stays held
    std::lock_guard<std::mutex> guard(s_mutex);
    s_controllers = c;
    s_controllersTicks = SDL_GetTicks();
  }

  void DestroyChains()
  {
    for (int i = 0; i < CHAINS; i++)
    {
      if (s_chain[i] != XR_NULL_HANDLE) p_xrDestroySwapchain(s_chain[i]);
      s_chain[i] = XR_NULL_HANDLE;
      s_images[i].clear();
    }
    s_chainW = s_chainH = 0;
  }

  bool EnsureChains(unsigned w, unsigned h)
  {
    if (s_chain[0] != XR_NULL_HANDLE && s_chainW == w && s_chainH == h) return true;
    DestroyChains();
    for (int i = 0; i < CHAINS; i++)
    {
      XrSwapchainCreateInfo ci = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
      ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
      ci.format = s_format;
      ci.sampleCount = 1;
      ci.width = w; ci.height = h;
      ci.faceCount = 1; ci.arraySize = 1; ci.mipCount = 1;
      if (!Check(p_xrCreateSwapchain(s_session, &ci, &s_chain[i]), "xrCreateSwapchain")) return false;
      uint32_t n = 0;
      p_xrEnumerateSwapchainImages(s_chain[i], 0, &n, nullptr);
      XrSwapchainImageOpenGLKHR blank = { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR };
      s_images[i].assign(n, blank);
      if (!Check(p_xrEnumerateSwapchainImages(s_chain[i], n, &n, (XrSwapchainImageBaseHeader *)s_images[i].data()), "xrEnumerateSwapchainImages")) return false;
    }
    s_chainW = w; s_chainH = h;
    Log("swapchains %ux%u, %u images each", w, h, (unsigned)s_images[0].size());
    return true;
  }

  // What the XR thread is currently showing
  struct Shown
  {
    unsigned long serial = 0;
    ACVRPublish   info;
    bool          hasHud = false;
  };

  // Copies the newest published pictures into the swapchains. Returns false when nothing can be shown.
  // The swapchain images are acquired and waited for without holding the publication lock (a slow
  // runtime cannot stall the game thread), in bounded waits that give up when the presenter stops.
  bool CopyPictures(GLuint readFbo, GLuint drawFbo, Shown &shown)
  {
    unsigned w, h;
    unsigned long serial;
    bool hasHud;
    {
      std::lock_guard<std::mutex> guard(s_mutex);
      if (s_pubSerial == 0 || !s_pubTex[0] || s_pubW == 0) return false;
      w = s_pubW; h = s_pubH; serial = s_pubSerial; hasHud = s_pubHasHud;
    }
    if (!EnsureChains(w, h)) return false;
    if (shown.serial == serial) return true;      // the last released images are still right

    uint32_t index[CHAINS] = { 0, 0, 0 };
    bool acquired[CHAINS] = { false, false, false };
    bool ok = true;
    for (int i = 0; i < CHAINS && ok; i++)
    {
      if (i == HUD && !hasHud) continue;
      XrSwapchainImageAcquireInfo ai = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
      if (!Check(p_xrAcquireSwapchainImage(s_chain[i], &ai, &index[i]), "xrAcquireSwapchainImage")) { ok = false; break; }
      XrSwapchainImageWaitInfo wi = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
      wi.timeout = 100000000;     // 100 ms, repeated until the image is ready or the presenter stops
      XrResult r;
      while ((r = p_xrWaitSwapchainImage(s_chain[i], &wi)) == XR_TIMEOUT_EXPIRED && !s_stop) {}
      if (r != XR_SUCCESS)
      {
        // (an image that was acquired but never waited for cannot be released: the presenter is
        // stopping or the runtime failed, and the swapchains are destroyed on the way out)
        if (r != XR_TIMEOUT_EXPIRED) Check(r, "xrWaitSwapchainImage");
        return false;
      }
      acquired[i] = true;
    }

    if (ok)
    {
      std::lock_guard<std::mutex> guard(s_mutex);
      if (s_pubW != w || s_pubH != h)
        ok = false;                               // resized meanwhile: the next frame picks it up
      else
      {
        if (s_pubReady) glWaitSync(s_pubReady, 0, GL_TIMEOUT_IGNORED);   // the game thread's copies first
        for (int i = 0; i < CHAINS; i++)
        {
          if (!acquired[i]) continue;
          glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
          glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_pubTex[i], 0);
          glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
          glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_images[i][index[i]].image, 0);
          glBlitFramebuffer(0, 0, w, h, 0, 0, s_chainW, s_chainH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (s_readDone) glDeleteSync(s_readDone);
        s_readDone = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);     // the game thread waits on this
        glFlush();
        shown.serial = s_pubSerial;
        shown.info = s_pubInfo;
        shown.hasHud = acquired[HUD];
      }
    }
    for (int i = 0; i < CHAINS; i++)
    {
      if (!acquired[i]) continue;
      XrSwapchainImageReleaseInfo ri = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
      if (!Check(p_xrReleaseSwapchainImage(s_chain[i], &ri), "xrReleaseSwapchainImage")) ok = false;
    }
    return ok;
  }

  XrPosef PoseAhead(const float origin[3], float yaw, float up, float distance)
  {
    XrPosef p;
    p.orientation.x = 0.0f; p.orientation.y = sinf(yaw * 0.5f); p.orientation.z = 0.0f; p.orientation.w = cosf(yaw * 0.5f);
    p.position.x = origin[0] - sinf(yaw) * distance;
    p.position.y = origin[1] + up;
    p.position.z = origin[2] - cosf(yaw) * distance;
    return p;
  }

  // Screen mode: puts the screen straight ahead of where the player is looking now (level, yaw only).
  // Returns false while the headset pose is not known yet.
  bool PlaceScreen(XrTime time)
  {
    const float zero[3] = { 0.0f, 0.0f, 0.0f };
    s_quadPose = PoseAhead(zero, 0.0f, s_height, s_distance);
    XrSpaceLocation loc = { XR_TYPE_SPACE_LOCATION };
    const XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if (XR_FAILED(p_xrLocateSpace(s_viewSpace, s_space, time, &loc)) || (loc.locationFlags & need) != need)
      return false;
    const XrQuaternionf q = loc.pose.orientation;
    const float fx = -2.0f * (q.x * q.z + q.w * q.y);          // where the head's forward axis (0,0,-1) points
    const float fz = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));
    const float yaw = atan2f(-fx, -fz);
    const float head[3] = { loc.pose.position.x, loc.pose.position.y, loc.pose.position.z };
    s_quadPose = PoseAhead(head, yaw, s_height, s_distance);
    Log("screen placed %.2f m ahead of the head (yaw %.0f deg, eye height %.2f m)", s_distance, yaw * 57.29578f, loc.pose.position.y);
    return true;
  }

  // Asks the headset where the eyes will be and hands that to the game thread
  void UpdateViews(XrTime time)
  {
    XrViewLocateInfo li = { XR_TYPE_VIEW_LOCATE_INFO };
    li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    li.displayTime = time;
    li.space = s_space;
    XrViewState vs = { XR_TYPE_VIEW_STATE };
    XrView views[2] = { { XR_TYPE_VIEW }, { XR_TYPE_VIEW } };
    uint32_t n = 0;
    const XrViewStateFlags need = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
    const bool ok = XR_SUCCEEDED(p_xrLocateViews(s_session, &li, &vs, 2, &n, views)) && n == 2 && (vs.viewStateFlags & need) == need;
    std::lock_guard<std::mutex> guard(s_mutex);
    s_latestValid = ok;
    if (!ok) return;
    for (int i = 0; i < 2; i++)
    {
      ACVREyeView &e = s_latestViews[i];
      e.pos[0] = views[i].pose.position.x; e.pos[1] = views[i].pose.position.y; e.pos[2] = views[i].pose.position.z;
      e.rot[0] = views[i].pose.orientation.x; e.rot[1] = views[i].pose.orientation.y;
      e.rot[2] = views[i].pose.orientation.z; e.rot[3] = views[i].pose.orientation.w;
      e.tan[0] = tanf(views[i].fov.angleLeft); e.tan[1] = tanf(views[i].fov.angleRight);
      e.tan[2] = tanf(views[i].fov.angleDown); e.tan[3] = tanf(views[i].fov.angleUp);
    }
  }

  void FillSubImage(XrSwapchainSubImage &s, int chain)
  {
    s.swapchain = s_chain[chain];
    s.imageRect.offset.x = 0;
    s.imageRect.offset.y = 0;
    s.imageRect.extent.width = (int32_t)s_chainW;
    s.imageRect.extent.height = (int32_t)s_chainH;
    s.imageArrayIndex = 0;
  }

  void ThreadBody()
  {
    if (SDL_GL_MakeCurrent(s_xrWindow, s_xrContext) != 0) { Log("cannot use the XR OpenGL context: %s", SDL_GetError()); return; }
    glDisable(GL_FRAMEBUFFER_SRGB);       // copy values unchanged
    glDisable(GL_SCISSOR_TEST);
    if (!InitXR())
    {
      Log("VR presenter off; the emulator keeps running on the monitor");
      SDL_GL_MakeCurrent(s_xrWindow, nullptr);
      return;
    }
    InitInput();
    GLuint readFbo = 0, drawFbo = 0;
    glGenFramebuffers(1, &readFbo);
    glGenFramebuffers(1, &drawFbo);

    bool running = false;
    unsigned long frames = 0;
    int lastMode = -1;
    Shown shown;
    while (!s_stop)
    {
      XrEventDataBuffer ev = { XR_TYPE_EVENT_DATA_BUFFER };
      while (p_xrPollEvent(s_instance, &ev) == XR_SUCCESS)
      {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
        {
          const XrEventDataSessionStateChanged *e = (const XrEventDataSessionStateChanged *)&ev;
          Log("session state %d", (int)e->state);
          const bool wasFocused = s_focused;
          s_focused = (e->state == XR_SESSION_STATE_FOCUSED);
          if (wasFocused && !s_focused) ClearControllers();   // left FOCUSED: drop every held control now
          if (e->state == XR_SESSION_STATE_READY)
          {
            XrSessionBeginInfo bi = { XR_TYPE_SESSION_BEGIN_INFO };
            bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            running = Check(p_xrBeginSession(s_session, &bi), "xrBeginSession");
            s_sessionRunning = running;
          }
          else if (e->state == XR_SESSION_STATE_STOPPING)
          {
            // Headset idle or runtime standby: end the session; the game waits, paused, for READY
            Check(p_xrEndSession(s_session), "xrEndSession");
            running = false;
            s_sessionRunning = false;
            Log("session stopped: the game waits, paused");
          }
          else if (e->state == XR_SESSION_STATE_EXITING || e->state == XR_SESSION_STATE_LOSS_PENDING)
          {
            // SteamVR's "Exit game", or a lost runtime: the whole game closes, not just the picture
            Log(e->state == XR_SESSION_STATE_EXITING ? "session exiting: the game closes" : "session lost: the game closes");
            s_runtimeExit = true;
            s_stop = true;
          }
        }
        else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
        {
          Log("instance loss pending: the game closes");
          s_runtimeExit = true;
          s_stop = true;
        }
        ev = { XR_TYPE_EVENT_DATA_BUFFER };
      }
      if (s_stop) break;
      if (!running) { SDL_Delay(50); continue; }

      XrFrameWaitInfo wi = { XR_TYPE_FRAME_WAIT_INFO };
      XrFrameState fs = { XR_TYPE_FRAME_STATE };
      if (!Check(p_xrWaitFrame(s_session, &wi, &fs), "xrWaitFrame")) break;
      XrFrameBeginInfo bi = { XR_TYPE_FRAME_BEGIN_INFO };
      if (!Check(p_xrBeginFrame(s_session, &bi), "xrBeginFrame")) break;
      UpdateViews(fs.predictedDisplayTime);
      UpdateControllers(fs.predictedDisplayTime);

      XrCompositionLayerQuad quads[2];
      XrCompositionLayerProjection projection = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
      XrCompositionLayerProjectionView projViews[2];
      const XrCompositionLayerBaseHeader *layers[3];
      uint32_t layerCount = 0;
      if (fs.shouldRender && CopyPictures(readFbo, drawFbo, shown))
      {
        if (shown.info.vr)
        {
          // Head-tracked: the pictures fill the view, with the eye poses they were drawn for
          for (int i = 0; i < 2; i++)
          {
            XrCompositionLayerProjectionView v = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
            const ACVREyeView &e = shown.info.views[i];
            v.pose.position.x = e.pos[0]; v.pose.position.y = e.pos[1]; v.pose.position.z = e.pos[2];
            v.pose.orientation.x = e.rot[0]; v.pose.orientation.y = e.rot[1]; v.pose.orientation.z = e.rot[2]; v.pose.orientation.w = e.rot[3];
            v.fov.angleLeft = atanf(e.tan[0]); v.fov.angleRight = atanf(e.tan[1]);
            v.fov.angleDown = atanf(e.tan[2]); v.fov.angleUp = atanf(e.tan[3]);
            FillSubImage(v.subImage, i);
            projViews[i] = v;
          }
          projection.space = s_space;
          projection.viewCount = 2;
          projection.views = projViews;
          layers[layerCount++] = (const XrCompositionLayerBaseHeader *)&projection;

          if (shown.hasHud)
          {
            // The game's 2D layers on a panel ahead of where the player faced at recentre
            XrCompositionLayerQuad q = { XR_TYPE_COMPOSITION_LAYER_QUAD };
            q.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            q.space = s_space;
            q.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            FillSubImage(q.subImage, HUD);
            q.pose = PoseAhead(shown.info.originPos, shown.info.originYaw, s_height, s_hudDistance);
            q.size.width = s_hudWidth;
            q.size.height = s_hudWidth * 0.75f;
            quads[0] = q;
            layers[layerCount++] = (const XrCompositionLayerBaseHeader *)&quads[0];
          }
          s_quadW = 0.0f;                                   // no screen to point at: aiming uses the arcade view
          if (lastMode != 1) { Log("head-tracked picture shown (HUD panel %s)", shown.hasHud ? "on" : "off"); lastMode = 1; }
        }
        else
        {
          // Screen sized to the game's own field of view at the chosen distance
          float tl = g_acvrStereo.tanL, tr = g_acvrStereo.tanR, tb = g_acvrStereo.tanB, tt = g_acvrStereo.tanT;
          if (!(tr - tl > 0.05f) || !(tt - tb > 0.05f)) { tl = -0.6f; tr = 0.6f; tb = -0.45f; tt = 0.45f; }
          // Home recentres the screen (the key state is kept by the emulator's own event pump)
          const Uint8 *keys = SDL_GetKeyboardState(nullptr);
          if (!s_poseSet || (keys && keys[SDL_SCANCODE_HOME]))
            s_poseSet = PlaceScreen(fs.predictedDisplayTime);
          const float width  = s_distance * (tr - tl) * s_sizeScale;
          const float height = s_distance * (tt - tb) * s_sizeScale;
          s_quadW = width; s_quadH = height;
          for (int i = 0; i < 2; i++)
          {
            XrCompositionLayerQuad q = { XR_TYPE_COMPOSITION_LAYER_QUAD };
            q.space = s_space;
            q.eyeVisibility = (i == 0) ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
            FillSubImage(q.subImage, i);
            q.pose = s_quadPose;
            q.size.width = width;
            q.size.height = height;
            quads[i] = q;
            layers[layerCount++] = (const XrCompositionLayerBaseHeader *)&quads[i];
          }
          if (lastMode != 0) { Log("screen picture shown: %.2f x %.2f m at %.2f m", width, height, s_distance); lastMode = 0; }
        }
        frames++;
      }
      XrFrameEndInfo ei = { XR_TYPE_FRAME_END_INFO };
      ei.displayTime = fs.predictedDisplayTime;
      ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
      ei.layerCount = layerCount;
      ei.layers = layerCount ? layers : nullptr;
      if (!Check(p_xrEndFrame(s_session, &ei), "xrEndFrame")) break;
    }

    Log("presenter stopping after %lu shown frames", frames);
    s_sessionRunning = false;
    s_focused = false;
    ClearControllers();
    DestroyChains();
    if (s_viewSpace != XR_NULL_HANDLE) p_xrDestroySpace(s_viewSpace);
    if (s_space != XR_NULL_HANDLE) p_xrDestroySpace(s_space);
    if (s_session != XR_NULL_HANDLE) p_xrDestroySession(s_session);
    if (s_instance != XR_NULL_HANDLE) p_xrDestroyInstance(s_instance);
    glDeleteFramebuffers(1, &readFbo);
    glDeleteFramebuffers(1, &drawFbo);
    SDL_GL_MakeCurrent(s_xrWindow, nullptr);
  }

  // Every way out of the XR thread leaves the controllers empty and says it is done; the game loop then closes
  // the game (ACVR_XR_State), so it never keeps running with no picture
  void ThreadMain()
  {
    ThreadBody();
    s_sessionRunning = false;
    s_focused = false;
    ClearControllers();
    s_threadDone = true;
  }
}

float ACVR_XR_DistanceMetres()
{
  return EnvFloat("ACVR_XR_DISTANCE", 2.5f);
}

// Game thread, with the emulator's OpenGL context current
bool ACVR_XR_Start(void *mainWindow)
{
  if (s_started) return true;
  s_wanted = true;
  s_runtimeExit = false;
  s_sessionRunning = false;
  s_log = fopen("acvr-xr.log", "w");
  s_distance    = ACVR_XR_DistanceMetres();
  s_sizeScale   = EnvFloat("ACVR_XR_SIZE", 1.0f);
  s_height      = EnvFloat("ACVR_XR_HEIGHT", 0.0f);
  s_hudDistance = EnvFloat("ACVR_HUD_DISTANCE", 1.6f);
  s_hudWidth    = EnvFloat("ACVR_HUD_WIDTH", 1.6f);
  Log("ACVR XR presenter: screen at %.2f m, size x%.2f, height %+.2f m; HUD panel %.2f m wide at %.2f m", s_distance, s_sizeScale, s_height, s_hudWidth, s_hudDistance);

  SDL_Window *window = (SDL_Window *)mainWindow;
  SDL_GLContext mainContext = SDL_GL_GetCurrentContext();
  if (!window || !mainContext) { Log("no emulator OpenGL context"); return false; }
  SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
  s_xrWindow = SDL_CreateWindow("ACVR XR", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  if (s_xrWindow) s_xrContext = SDL_GL_CreateContext(s_xrWindow);
  SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
  SDL_GL_MakeCurrent(window, mainContext);     // creating a context makes it current: give the emulator its own back
  if (!s_xrWindow || !s_xrContext) { Log("cannot create the shared OpenGL context: %s", SDL_GetError()); return false; }

  s_stop = false;
  s_threadDone = false;
  ClearControllers();
  s_started = true;
  s_thread = std::thread(ThreadMain);
  return true;
}

// Game thread: the newest eye poses from the headset
bool ACVR_XR_GetViews(ACVREyeView out[2])
{
  if (!s_started) return false;
  std::lock_guard<std::mutex> guard(s_mutex);
  if (!s_latestValid) return false;
  out[0] = s_latestViews[0];
  out[1] = s_latestViews[1];
  return true;
}

// Game thread: the newest controller state
bool ACVR_XR_GetControllers(ACVRControllers &out)
{
  if (!s_started) return false;
  std::lock_guard<std::mutex> guard(s_mutex);
  // Older than CONTROLLERS_MAX_AGE_MS (the XR thread stalled, stopped rendering or ended): no input at all
  if (s_threadDone || (Uint32)(SDL_GetTicks() - s_controllersTicks) > CONTROLLERS_MAX_AGE_MS)
  {
    out = ACVRControllers();
    return false;
  }
  out = s_controllers;
  return out.valid;
}

// Game thread: copy the finished pictures into the shared textures
void ACVR_XR_Publish(const ACVRPublish &p)
{
  if (!s_started || p.w == 0 || p.h == 0) return;
  std::lock_guard<std::mutex> guard(s_mutex);
  // The XR thread must have finished reading the textures before they are overwritten or deleted
  if (s_readDone)
  {
    if (!s_pubTex[0] || s_pubW != p.w || s_pubH != p.h)
      glClientWaitSync(s_readDone, GL_SYNC_FLUSH_COMMANDS_BIT, 50000000);   // deleting: wait here, up to 50 ms
    else
      glWaitSync(s_readDone, 0, GL_TIMEOUT_IGNORED);                       // overwriting: the GPU waits
  }
  if (!s_pubTex[0] || s_pubW != p.w || s_pubH != p.h)
  {
    if (s_pubTex[0]) { glDeleteFramebuffers(CHAINS, s_pubFboMain); glDeleteTextures(CHAINS, s_pubTex); }
    glGenTextures(CHAINS, s_pubTex);
    glGenFramebuffers(CHAINS, s_pubFboMain);
    for (int i = 0; i < CHAINS; i++)
    {
      glBindTexture(GL_TEXTURE_2D, s_pubTex[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, p.w, p.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBindFramebuffer(GL_FRAMEBUFFER, s_pubFboMain[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_pubTex[i], 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    s_pubW = p.w; s_pubH = p.h;
  }
  const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
  glDisable(GL_SCISSOR_TEST);
  const unsigned src[CHAINS] = { p.eyeFbo[0], p.eyeFbo[1], p.hudFbo };
  for (int i = 0; i < CHAINS; i++)
  {
    if (!src[i]) continue;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src[i]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_pubFboMain[i]);
    glBlitFramebuffer(0, 0, p.w, p.h, 0, 0, p.w, p.h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (scissor) glEnable(GL_SCISSOR_TEST);
  if (s_pubReady) glDeleteSync(s_pubReady);
  s_pubReady = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);    // the XR thread waits on this before reading
  glFlush();            // make the copies and the fence visible to the other context
  s_pubInfo = p;
  s_pubHasHud = p.hudFbo != 0;
  s_pubSerial++;
}

int ACVR_XR_State()
{
  if (!s_wanted) return 0;
  if (s_runtimeExit) return 3;
  if (!s_started || s_threadDone) return 4;
  return s_sessionRunning ? 1 : 2;
}

void ACVR_XR_Stop()
{
  if (!s_started) return;
  s_stop = true;
  // A hung OpenXR runtime (xrWaitFrame that never returns) must not hang the exit: wait up to 3 s, then let it go
  const Uint32 waitStart = SDL_GetTicks();
  while (!s_threadDone && (Uint32)(SDL_GetTicks() - waitStart) < 3000) SDL_Delay(5);
  if (!s_threadDone)
  {
    Log("XR thread did not stop within 3 s; leaving it behind");
    if (s_thread.joinable()) s_thread.detach();
    s_started = false;
    return;             // its context, window and log stay alive: the thread may still be using them
  }
  if (s_thread.joinable()) s_thread.join();
  s_started = false;
  // Leave nothing behind for a later start: the fences, the XR thread's context and its window
  if (s_pubReady) { glDeleteSync(s_pubReady); s_pubReady = nullptr; }
  if (s_readDone) { glDeleteSync(s_readDone); s_readDone = nullptr; }
  if (s_xrContext) { SDL_GL_DeleteContext(s_xrContext); s_xrContext = nullptr; }
  if (s_xrWindow) { SDL_DestroyWindow(s_xrWindow); s_xrWindow = nullptr; }
  s_poseSet = false;
  s_latestValid = false;
  if (s_log) { fclose(s_log); s_log = nullptr; }
}

#else   // not Windows: the presenter is not built

float ACVR_XR_DistanceMetres() { return 2.5f; }
bool ACVR_XR_Start(void *) { return false; }
bool ACVR_XR_GetViews(ACVREyeView *) { return false; }
bool ACVR_XR_GetControllers(ACVRControllers &) { return false; }
void ACVR_XR_Publish(const ACVRPublish &) {}
void ACVR_XR_Stop() {}
int ACVR_XR_State() { return 0; }

#endif
