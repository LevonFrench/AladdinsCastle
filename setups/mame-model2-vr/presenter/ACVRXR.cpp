/*
 * ACVR OpenXR presenter for Supermodel (AladdinsCastle project). GPL v3 or later, like Supermodel.
 *
 * Shows the two eye pictures in a VR headset as one stereoscopic screen: two quad layers, one visible to
 * each eye, placed in front of the player and sized to the game's own field of view, so the scene is seen
 * through a window at real scale.
 *
 * The emulator's frame loop is not touched: the eye pictures are copied into shared textures ("published")
 * and a separate thread, with its own OpenGL context, submits them at the headset's rate. The game keeps
 * its own 57.5 Hz pacing.
 *
 * Environment:
 *   ACVR_XR=1                turn the presenter on (also turns stereo on)
 *   ACVR_XR_DISTANCE=<m>     distance of the screen in metres (default 2.5)
 *   ACVR_XR_SIZE=<n>         multiplier on the screen size (default 1 = matches the game's field of view)
 *   ACVR_XR_HEIGHT=<m>       height of the screen centre relative to the eyes (default 0)
 *
 * The screen appears straight ahead of where the player is looking when the first picture arrives.
 * The Home key puts it there again.
 *
 * Everything it does is written to acvr-xr.log in the working folder.
 */
#ifdef _WIN32

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#include <windows.h>
#include "WinGL.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>


#include "ACVRStereo.h"
#include "AimMath.h"

namespace
{
  // ---- shared between the game thread and the XR thread
  std::mutex        s_mutex;                 // guards the published textures
  std::mutex        s_inputMutex;
  ACVRControllerInput s_input;
  ULONGLONG s_inputTime = 0;
  GLuint            s_pubTex[2] = { 0, 0 };  // shared textures holding the newest finished eye pictures
  GLuint            s_pubFboMain[2] = { 0, 0 }; // game-thread framebuffers on those textures
  unsigned          s_pubW = 0, s_pubH = 0;
  unsigned long     s_pubSerial = 0;
  std::atomic<bool> s_stop(false);
  std::atomic<bool> s_started(false);
  std::thread       s_thread;
  Host_Window       *s_xrWindow = nullptr;
  Host_GLContext     s_xrContext = nullptr;

  float s_distance = 2.5f, s_sizeScale = 1.0f, s_height = 0.0f;

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
  X(xrLocateSpace) X(xrGetOpenGLGraphicsRequirementsKHR) \
  X(xrStringToPath) X(xrCreateActionSet) X(xrDestroyActionSet) X(xrCreateAction) \
  X(xrSuggestInteractionProfileBindings) X(xrAttachSessionActionSets) X(xrCreateActionSpace) \
  X(xrSyncActions) X(xrGetActionStatePose) X(xrGetActionStateFloat) X(xrGetActionStateBoolean)
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
  XrSwapchain s_chain[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
  std::vector<XrSwapchainImageOpenGLKHR> s_images[2];
  unsigned    s_chainW = 0, s_chainH = 0;
  int64_t     s_format = 0;
  XrActionSet s_actions = XR_NULL_HANDLE;
  XrAction s_aim = XR_NULL_HANDLE, s_trigger = XR_NULL_HANDLE, s_start = XR_NULL_HANDLE, s_coin = XR_NULL_HANDLE;
  XrSpace s_aimSpace = XR_NULL_HANDLE;
  XrPath s_right = XR_NULL_PATH;

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

  bool InitActions()
  {
    if (!Check(p_xrStringToPath(s_instance, "/user/hand/right", &s_right), "right hand path")) return false;
    XrActionSetCreateInfo set = { XR_TYPE_ACTION_SET_CREATE_INFO };
    std::strcpy(set.actionSetName, "model2_game"); std::strcpy(set.localizedActionSetName, "Model 2 Game");
    if (!Check(p_xrCreateActionSet(s_instance, &set, &s_actions), "xrCreateActionSet")) return false;
    auto action = [](const char *name, const char *label, XrActionType type, XrAction &out) {
      XrActionCreateInfo info = { XR_TYPE_ACTION_CREATE_INFO };
      std::strcpy(info.actionName, name); std::strcpy(info.localizedActionName, label);
      info.actionType = type; info.countSubactionPaths = 1; info.subactionPaths = &s_right;
      return Check(p_xrCreateAction(s_actions, &info, &out), name);
    };
    if (!action("aim", "Right gun aim", XR_ACTION_TYPE_POSE_INPUT, s_aim) ||
        !action("trigger", "Shoot and offscreen reload", XR_ACTION_TYPE_FLOAT_INPUT, s_trigger) ||
        !action("start", "Start", XR_ACTION_TYPE_BOOLEAN_INPUT, s_start) ||
        !action("coin", "Coin", XR_ACTION_TYPE_BOOLEAN_INPUT, s_coin)) return false;
    const char *profiles[] = { "/interaction_profiles/oculus/touch_controller", "/interaction_profiles/valve/index_controller" };
    const char *paths[] = { "/user/hand/right/input/aim/pose", "/user/hand/right/input/trigger/value",
                           "/user/hand/right/input/a/click", "/user/hand/right/input/b/click" };
    const XrAction actions[] = { s_aim, s_trigger, s_start, s_coin };
    XrActionSuggestedBinding bindings[4]{};
    for (unsigned i=0; i<4; ++i) {
      bindings[i].action = actions[i];
      if (!Check(p_xrStringToPath(s_instance, paths[i], &bindings[i].binding), "controller binding path")) return false;
    }
    for (const char *profile : profiles) {
      XrInteractionProfileSuggestedBinding suggested = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
      if (!Check(p_xrStringToPath(s_instance, profile, &suggested.interactionProfile), "interaction profile")) return false;
      suggested.countSuggestedBindings = 4; suggested.suggestedBindings = bindings;
      Check(p_xrSuggestInteractionProfileBindings(s_instance, &suggested), profile);
    }
    XrSessionActionSetsAttachInfo attach = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
    attach.countActionSets = 1; attach.actionSets = &s_actions;
    if (!Check(p_xrAttachSessionActionSets(s_session, &attach), "xrAttachSessionActionSets")) return false;
    XrActionSpaceCreateInfo space = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
    space.action = s_aim; space.subactionPath = s_right; space.poseInActionSpace.orientation.w = 1;
    return Check(p_xrCreateActionSpace(s_session, &space, &s_aimSpace), "xrCreateActionSpace aim");
  }

  void ReadController(XrTime time, float width, float height)
  {
    ACVRControllerInput input;
    XrActiveActionSet active{ s_actions, s_right };
    XrActionsSyncInfo sync = { XR_TYPE_ACTIONS_SYNC_INFO };
    sync.countActiveActionSets = 1; sync.activeActionSets = &active;
    if (p_xrSyncActions(s_session, &sync) == XR_SUCCESS) {
      XrActionStateGetInfo get = { XR_TYPE_ACTION_STATE_GET_INFO }; get.subactionPath = s_right;
      XrActionStatePose aim = { XR_TYPE_ACTION_STATE_POSE };
      XrActionStateFloat trigger = { XR_TYPE_ACTION_STATE_FLOAT };
      XrActionStateBoolean start = { XR_TYPE_ACTION_STATE_BOOLEAN }, coin = { XR_TYPE_ACTION_STATE_BOOLEAN };
      get.action=s_aim; const bool aimOK=XR_SUCCEEDED(p_xrGetActionStatePose(s_session, &get, &aim));
      get.action=s_trigger; const bool triggerOK=XR_SUCCEEDED(p_xrGetActionStateFloat(s_session, &get, &trigger));
      get.action=s_start; const bool startOK=XR_SUCCEEDED(p_xrGetActionStateBoolean(s_session, &get, &start));
      get.action=s_coin; const bool coinOK=XR_SUCCEEDED(p_xrGetActionStateBoolean(s_session, &get, &coin));
      input.active = (aimOK && aim.isActive) || (startOK && start.isActive) || (coinOK && coin.isActive);
      input.start=startOK && start.isActive && start.currentState;
      input.coin=coinOK && coin.isActive && coin.currentState;
      XrSpaceLocation pose = { XR_TYPE_SPACE_LOCATION };
      const auto need=XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
      if (width>0 && height>0 && s_poseSet && aimOK && aim.isActive && XR_SUCCEEDED(p_xrLocateSpace(s_aimSpace, s_space, time, &pose)) && (pose.locationFlags & need)==need) {
        const auto &p=pose.pose.position, &c=s_quadPose.position;
        const auto &q=pose.pose.orientation, &s=s_quadPose.orientation;
        const auto hit=acvr_aim::intersect({p.x,p.y,p.z},{q.x,q.y,q.z,q.w},{c.x,c.y,c.z},{s.x,s.y,s.z,s.w},width,height);
        input.u=hit.u; input.v=hit.v; input.offScreen=!hit.on_screen;
        input.trigger=triggerOK && trigger.isActive && trigger.currentState>=.55f;
      }
    }
    std::lock_guard<std::mutex> guard(s_inputMutex); s_input=input; s_inputTime=GetTickCount64();
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
    strncpy(ci.applicationInfo.applicationName, "ACVR Model 2 Viewer", XR_MAX_APPLICATION_NAME_SIZE - 1);
    strncpy(ci.applicationInfo.engineName, "Model 2 Bitmap Viewer", XR_MAX_ENGINE_NAME_SIZE - 1);
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
    if (!InitActions()) return false;

    uint32_t n = 0;
    p_xrEnumerateSwapchainFormats(s_session, 0, &n, nullptr);
    std::vector<int64_t> formats(n);
    if (n == 0 || !Check(p_xrEnumerateSwapchainFormats(s_session, n, &n, formats.data()), "xrEnumerateSwapchainFormats")) return false;
    // The emulator's picture holds sRGB-encoded values, so an sRGB format shows it unchanged.
    const int64_t wanted[] = { GL_SRGB8_ALPHA8, GL_SRGB8, GL_RGBA8 };
    for (int64_t w : wanted)
    {
      for (int64_t f : formats) if (f == w) { s_format = w; break; }
      if (s_format) break;
    }
    if (!s_format) s_format = formats[0];
    Log("swapchain format 0x%llX%s", (long long)s_format, s_format == GL_RGBA8 ? " (not sRGB: colours may look washed out)" : "");
    return true;
  }

  void DestroyChains()
  {
    for (int i = 0; i < 2; i++)
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
    for (int i = 0; i < 2; i++)
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
    Log("swapchains %ux%u per eye, %u images", w, h, (unsigned)s_images[0].size());
    return true;
  }

  // Copies the newest published eye pictures into the swapchains. Returns false when nothing can be shown.
  bool CopyEyes(GLuint readFbo, GLuint drawFbo, unsigned long &shownSerial)
  {
    std::lock_guard<std::mutex> guard(s_mutex);
    if (s_pubSerial == 0 || !s_pubTex[0] || s_pubW == 0) return false;
    if (!EnsureChains(s_pubW, s_pubH)) return false;
    if (shownSerial == s_pubSerial) return true;      // the last released images are still right
    for (int i = 0; i < 2; i++)
    {
      uint32_t index = 0;
      XrSwapchainImageAcquireInfo ai = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
      if (!Check(p_xrAcquireSwapchainImage(s_chain[i], &ai, &index), "xrAcquireSwapchainImage")) return false;
      XrSwapchainImageWaitInfo wi = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
      wi.timeout = XR_INFINITE_DURATION;
      if (!Check(p_xrWaitSwapchainImage(s_chain[i], &wi), "xrWaitSwapchainImage")) return false;
      glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
      glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_pubTex[i], 0);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_images[i][index].image, 0);
      glBlitFramebuffer(0, 0, s_pubW, s_pubH, 0, 0, s_chainW, s_chainH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glFlush();
      XrSwapchainImageReleaseInfo ri = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
      if (!Check(p_xrReleaseSwapchainImage(s_chain[i], &ri), "xrReleaseSwapchainImage")) return false;
    }
    shownSerial = s_pubSerial;
    return true;
  }

  // Puts the screen straight ahead of the player's current gaze.
  // Returns false while the headset pose is not known yet.
  bool PlaceScreen(XrTime time)
  {
    s_quadPose.orientation.x = 0.0f; s_quadPose.orientation.y = 0.0f; s_quadPose.orientation.z = 0.0f; s_quadPose.orientation.w = 1.0f;
    s_quadPose.position.x = 0.0f; s_quadPose.position.y = s_height; s_quadPose.position.z = -s_distance;
    XrSpaceLocation loc = { XR_TYPE_SPACE_LOCATION };
    const XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if (XR_FAILED(p_xrLocateSpace(s_viewSpace, s_space, time, &loc)) || (loc.locationFlags & need) != need)
      return false;
    const XrQuaternionf q = loc.pose.orientation;
    float vx, vy;
    { std::lock_guard<std::mutex> guard(s_mutex);
      vx = s_distance * (g_acvrStereo.tanL + g_acvrStereo.tanR) * 0.5f;
      vy = s_height + s_distance * (g_acvrStereo.tanB + g_acvrStereo.tanT) * 0.5f; }
    const float vz = -s_distance;
    const float tx = 2 * (q.y * vz - q.z * vy), ty = 2 * (q.z * vx - q.x * vz), tz = 2 * (q.x * vy - q.y * vx);
    s_quadPose.orientation = q;
    s_quadPose.position.x = loc.pose.position.x + vx + q.w * tx + q.y * tz - q.z * ty;
    s_quadPose.position.y = loc.pose.position.y + vy + q.w * ty + q.z * tx - q.x * tz;
    s_quadPose.position.z = loc.pose.position.z + vz + q.w * tz + q.x * ty - q.y * tx;
    Log("screen placed %.2f m ahead of current gaze", s_distance);
    return true;
  }

  void ThreadMain()
  {
    if (Host_GL_MakeCurrent(s_xrWindow, s_xrContext) != 0) { Log("cannot use the XR OpenGL context: %s", Host_GetError()); return; }
    glDisable(GL_FRAMEBUFFER_SRGB);       // copy values unchanged
    glDisable(GL_SCISSOR_TEST);
    if (!InitXR())
    {
      Log("VR presenter off; the emulator keeps running on the monitor");
      Host_GL_MakeCurrent(s_xrWindow, nullptr);
      return;
    }
    GLuint readFbo = 0, drawFbo = 0;
    glGenFramebuffers(1, &readFbo);
    glGenFramebuffers(1, &drawFbo);

    bool running = false;
    unsigned long shownSerial = 0, frames = 0;
    while (!s_stop)
    {
      XrEventDataBuffer ev = { XR_TYPE_EVENT_DATA_BUFFER };
      while (p_xrPollEvent(s_instance, &ev) == XR_SUCCESS)
      {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
        {
          const XrEventDataSessionStateChanged *e = (const XrEventDataSessionStateChanged *)&ev;
          Log("session state %d", (int)e->state);
          if (e->state == XR_SESSION_STATE_READY)
          {
            XrSessionBeginInfo bi = { XR_TYPE_SESSION_BEGIN_INFO };
            bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            running = Check(p_xrBeginSession(s_session, &bi), "xrBeginSession");
          }
          else if (e->state == XR_SESSION_STATE_STOPPING)
          {
            p_xrEndSession(s_session);
            running = false;
          }
          else if (e->state == XR_SESSION_STATE_EXITING || e->state == XR_SESSION_STATE_LOSS_PENDING)
          {
            s_stop = true;
          }
        }
        else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
        {
          s_stop = true;
        }
        ev = { XR_TYPE_EVENT_DATA_BUFFER };
      }
      if (s_stop) break;
      if (!running) { Host_Delay(50); continue; }

      XrFrameWaitInfo wi = { XR_TYPE_FRAME_WAIT_INFO };
      XrFrameState fs = { XR_TYPE_FRAME_STATE };
      if (!Check(p_xrWaitFrame(s_session, &wi, &fs), "xrWaitFrame")) break;
      XrFrameBeginInfo bi = { XR_TYPE_FRAME_BEGIN_INFO };
      if (!Check(p_xrBeginFrame(s_session, &bi), "xrBeginFrame")) break;

      XrCompositionLayerQuad quads[2];
      const XrCompositionLayerBaseHeader *layers[2];
      uint32_t layerCount = 0;
      float width = 0, height = 0;
      if (fs.shouldRender && CopyEyes(readFbo, drawFbo, shownSerial))
      {
        // Screen sized to the game's own field of view at the chosen distance
        float tl, tr, tb, tt;
        { std::lock_guard<std::mutex> guard(s_mutex); tl = g_acvrStereo.tanL; tr = g_acvrStereo.tanR; tb = g_acvrStereo.tanB; tt = g_acvrStereo.tanT; }
        if (!(tr - tl > 0.05f) || !(tt - tb > 0.05f)) { tl = -0.6f; tr = 0.6f; tb = -0.45f; tt = 0.45f; }
        // Home recentres the screen in front of the player (the key state is kept by the emulator's own event pump)
        const Uint8 *keys = Host_GetKeyboardState(nullptr);
        if (!s_poseSet || (keys && keys[Host_SCANCODE_HOME]))
          s_poseSet = PlaceScreen(fs.predictedDisplayTime);
        width  = s_distance * (tr - tl) * s_sizeScale;
        height = s_distance * (tt - tb) * s_sizeScale;
        for (int i = 0; i < 2; i++)
        {
          XrCompositionLayerQuad q = { XR_TYPE_COMPOSITION_LAYER_QUAD };
          q.space = s_space;
          q.eyeVisibility = (i == 0) ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
          q.subImage.swapchain = s_chain[i];
          q.subImage.imageRect.offset.x = 0;
          q.subImage.imageRect.offset.y = 0;
          q.subImage.imageRect.extent.width = (int32_t)s_chainW;
          q.subImage.imageRect.extent.height = (int32_t)s_chainH;
          q.pose = s_quadPose;
          q.size.width = width;
          q.size.height = height;
          quads[i] = q;
          layers[i] = (const XrCompositionLayerBaseHeader *)&quads[i];
        }
        layerCount = 2;
        if (frames == 0) Log("first picture shown: screen %.2f x %.2f m at %.2f m", width, height, s_distance);
        frames++;
      }
      ReadController(fs.predictedDisplayTime, width, height);
      XrFrameEndInfo ei = { XR_TYPE_FRAME_END_INFO };
      ei.displayTime = fs.predictedDisplayTime;
      ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
      ei.layerCount = layerCount;
      ei.layers = layerCount ? layers : nullptr;
      if (!Check(p_xrEndFrame(s_session, &ei), "xrEndFrame")) break;
    }

    Log("presenter stopping after %lu shown frames", frames);
    DestroyChains();
    if (s_aimSpace != XR_NULL_HANDLE) p_xrDestroySpace(s_aimSpace);
    if (s_viewSpace != XR_NULL_HANDLE) p_xrDestroySpace(s_viewSpace);
    if (s_space != XR_NULL_HANDLE) p_xrDestroySpace(s_space);
    if (s_session != XR_NULL_HANDLE) p_xrDestroySession(s_session);
    if (s_actions != XR_NULL_HANDLE) p_xrDestroyActionSet(s_actions);
    if (s_instance != XR_NULL_HANDLE) p_xrDestroyInstance(s_instance);
    glDeleteFramebuffers(1, &readFbo);
    glDeleteFramebuffers(1, &drawFbo);
    Host_GL_MakeCurrent(s_xrWindow, nullptr);
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
  s_log = fopen("acvr-xr.log", "w");
  s_distance  = ACVR_XR_DistanceMetres();
  s_sizeScale = EnvFloat("ACVR_XR_SIZE", 1.0f);
  s_height    = EnvFloat("ACVR_XR_HEIGHT", 0.0f);
  Log("ACVR XR presenter: screen at %.2f m, size x%.2f, height %+.2f m", s_distance, s_sizeScale, s_height);

  Host_Window *window = (Host_Window *)mainWindow;
  Host_GLContext mainContext = Host_GL_GetCurrentContext();
  if (!window || !mainContext) { Log("no emulator OpenGL context"); return false; }
  Host_GL_SetAttribute(Host_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
  s_xrWindow = Host_CreateWindow("ACVR XR", Host_WINDOWPOS_UNDEFINED, Host_WINDOWPOS_UNDEFINED, 16, 16, Host_WINDOW_OPENGL | Host_WINDOW_HIDDEN);
  if (s_xrWindow) s_xrContext = Host_GL_CreateContext(s_xrWindow);
  Host_GL_SetAttribute(Host_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
  Host_GL_MakeCurrent(window, mainContext);     // creating a context makes it current: give the emulator its own back
  if (!s_xrWindow || !s_xrContext) { Log("cannot create the shared OpenGL context: %s", Host_GetError()); return false; }

  s_stop = false;
  s_started = true;
  s_thread = std::thread(ThreadMain);
  return true;
}

// Game thread: copy the finished eye pictures into the shared textures
void ACVR_XR_Publish(unsigned leftFbo, unsigned rightFbo, unsigned w, unsigned h)
{
  if (!s_started || w == 0 || h == 0) return;
  std::lock_guard<std::mutex> guard(s_mutex);
  if (!s_pubTex[0] || s_pubW != w || s_pubH != h)
  {
    if (s_pubTex[0]) { glDeleteFramebuffers(2, s_pubFboMain); glDeleteTextures(2, s_pubTex); }
    glGenTextures(2, s_pubTex);
    glGenFramebuffers(2, s_pubFboMain);
    for (int i = 0; i < 2; i++)
    {
      glBindTexture(GL_TEXTURE_2D, s_pubTex[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBindFramebuffer(GL_FRAMEBUFFER, s_pubFboMain[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_pubTex[i], 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    s_pubW = w; s_pubH = h;
  }
  const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
  glDisable(GL_SCISSOR_TEST);
  const unsigned src[2] = { leftFbo, rightFbo };
  for (int i = 0; i < 2; i++)
  {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src[i]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_pubFboMain[i]);
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (scissor) glEnable(GL_SCISSOR_TEST);
  glFinish();           // finish GPU copies before publishing to the other context
  s_pubSerial++;
}

void ACVR_XR_Stop()
{
  if (!s_started) return;
  s_stop = true;
  if (s_thread.joinable()) s_thread.join();
  s_started = false;
  glDeleteFramebuffers(2, s_pubFboMain);
  glDeleteTextures(2, s_pubTex);
  Host_DestroyWindow(s_xrWindow); s_xrWindow = nullptr; s_xrContext = nullptr;
  if (s_log) { fclose(s_log); s_log = nullptr; }
}

void ACVR_XR_UpdateFov(float left, float right, float bottom, float top)
{
  std::lock_guard<std::mutex> guard(s_mutex);
  g_acvrStereo.tanL = left; g_acvrStereo.tanR = right;
  g_acvrStereo.tanB = bottom; g_acvrStereo.tanT = top;
}

ACVRControllerInput ACVR_XR_ControllerInput()
{
  std::lock_guard<std::mutex> guard(s_inputMutex);
  return GetTickCount64()-s_inputTime < 250 ? s_input : ACVRControllerInput{};
}

#else   // not Windows: the presenter is not built

float ACVR_XR_DistanceMetres() { return 2.5f; }
bool ACVR_XR_Start(void *) { return false; }
void ACVR_XR_Publish(unsigned, unsigned, unsigned, unsigned) {}
void ACVR_XR_Stop() {}
ACVRControllerInput ACVR_XR_ControllerInput() { return {}; }

#endif

