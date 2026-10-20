/******************************************************************************
 ACVR stereo and VR (AladdinsCastle): eye storage, head-tracked camera, HUD picture,
 side-by-side window, frame capture, scripted inputs

 Environment:
   ACVR_STEREO=1          draw each game frame once per eye (screen mode)
   ACVR_XR=1              also send the pictures to a VR headset (turns stereo on)
   ACVR_VR=1              head-tracked mode: the game camera follows the headset and the scene fills the view;
                          the 2D layers go on a separate HUD panel. Without a tracked headset it falls back
                          to screen mode.
   ACVR_VR_FAKE=1         head-tracked mode with a made-up headset (90 degree view), for checks on the monitor
   ACVR_VR_FAKE_YAW=<d>   turn that made-up head, degrees to the left
   ACVR_VR_BACKDROP=1     head-tracked: always treat the bottom 2D layer as a backdrop, even when its top edge is empty
   ACVR_EYE_SEP=<m>       screen mode: eye separation in metres (default 0.065)
   ACVR_WORLD_SCALE=<n>   game units per metre (default 1)
   ACVR_CONVERGENCE=<m>   screen mode: distance of the zero-parallax plane in metres
   ACVR_FOV_SCALE=<n>     screen mode: multiply the camera's field of view
   ACVR_NO_CULL=1         draw what the game's own frustum test would drop (always on in head-tracked mode)
   ACVR_CAPTURE_FRAME=<n>[,<n>...] save those frames as BMP files, quit after the last
   ACVR_CAPTURE_DIR=<dir> where the BMP files go (default: working folder)
   ACVR_SAVE_STATE_FRAME=<n> write a save state (current slot) at that frame
   ACVR_HIDDEN=1          keep the window hidden (for unattended captures)
   ACVR_INPUT_SCRIPT=<first>-<last>:<InputId>:<value>;...  hold an input during those frames
   ACVR_STICK_GRAB=1      Star Wars Trilogy: grab the flight stick with the right grip instead of pointing
   ACVR_STICK_INVERT=1    with ACVR_STICK_GRAB: pushing the stick forward aims up instead of down
   ACVR_TEST_STICK=<pitch>,<roll>  with ACVR_STICK_GRAB: a stick held at that tilt, degrees (pitch: pulled back,
                          roll: to the right), with no controllers, for unattended checks
   ACVR_WIDE_ALL=0        with -wide-screen: widen only full-screen views, as upstream (default: letterboxed scenes
                          and sky strips are widened too)
   ACVR_AIM_X, ACVR_AIM_Y analog-joystick games: where the game draws its crosshair for each stick value, as
                          "value:pixel,..." over its 496x384 picture (default: Star Wars Trilogy, measured)
   ACVR_AIM_DISTANCE=<m>  head-tracked: distance of the plane the arcade view is met on when pointing (default: the
                          HUD panel's distance, so the ray meets the panel where the game draws its crosshair)
   ACVR_HUD_MATCH=0|1     head-tracked: HUD panel over the arcade view exactly (default: on for analog-stick games)
   ACVR_LASER=0           head-tracked pointing games: no laser and aim dot from the right controller
   ACVR_STICK_MODEL=1     with ACVR_STICK_GRAB and ACVR_CONTROLS: draw the cabinet's flight stick (off by default)
   ACVR_FAKE_CONTROLLERS=<first>-<last>:<control>:<value>;...  made-up VR controllers for unattended checks, used
                          only while no real ones are read. Controls: trigger.l trigger.r grip.l grip.r a b x y menu
                          stick.lx stick.ly stick.rx stick.ry click.l click.r; wheel (degrees, both hands on a
                          wheel, positive turns right); aim.u aim.v (the right controller points at that place of
                          the game picture, 0..1 from the top left); aim.yaw aim.pitch (degrees, left and up
                          positive); hand.x hand.y hand.z (where the right controller is, metres from the head)
******************************************************************************/

ACVRStereoState g_acvrStereo;

static int      s_acvrCabinet = 0;          // 0 none, 1 wheel, 2 motorcycle bars, 3 flight stick (ACVR_STICK_GRAB)
static float    s_acvrSteer = 0.0f;         // -1 full left .. 1 full right
static int      s_acvrStickGrab = -1;       // ACVR_STICK_GRAB: -1 not read yet, 0 off, 1 on
static float    s_acvrStick[2] = { 0, 0 };  // the flight stick's tilt, -1 .. 1: x to the right, y pulled back
static GLuint   s_acvrEyeFbo[2] = { 0, 0 };
static GLuint   s_acvrEyeTex[2] = { 0, 0 };
static GLuint   s_acvrHudFbo = 0, s_acvrHudTex = 0;
static GLuint   s_acvrSkyFbo = 0, s_acvrSkyTex = 0;     // a 16 x 1 sample of the upper part of the last left-eye picture
static float    s_acvrSky[3] = { 0, 0, 0 };             // its colour, smoothed: fills what a game does not draw
static bool     s_acvrSkySampled = false;
static GLuint   s_acvrAATarget = 0;     // the emulator's supersampling framebuffer, 0 when supersampling is off
static unsigned s_acvrEyeW = 0, s_acvrEyeH = 0;
static bool     s_acvrHudDrawn = false;
static long     s_acvrFrame = 0;
static std::vector<long> s_acvrCaptureFrames;
static long     s_acvrSaveStateFrame = -1;
static bool     s_acvrWantSaveState = false;
static std::string s_acvrCaptureDir = ".";
static bool     s_acvrQuit = false;

static bool     s_acvrVRWanted = false, s_acvrVRFake = false, s_acvrVRBackdrop = false;
static float    s_acvrFakeYaw = 0.0f;
static float    s_acvrUnitsPerMetre = 1.0f;
static bool     s_acvrOriginSet = false;
static float    s_acvrOriginPos[3] = { 0, 0, 0 };
static float    s_acvrOriginYaw = 0.0f;
static ACVREyeView s_acvrViews[2];

struct ACVRScriptedInput { long first, last; std::string id; int value; };
static std::vector<ACVRScriptedInput> s_acvrInputScript;

static void ACVR_SaveBound(const std::string &file, unsigned w, unsigned h);

static float ACVR_EnvFloat(const char *name, float fallback)
{
  const char *v = getenv(name);
  if (!v || !*v) return fallback;
  float f = (float)atof(v);
  return std::isfinite(f) ? f : fallback;
}

static void ACVR_Init()
{
  const bool  xr         = getenv("ACVR_XR") != nullptr;
  const float sepMetres  = ACVR_EnvFloat("ACVR_EYE_SEP", 0.065f);
  s_acvrUnitsPerMetre    = ACVR_EnvFloat("ACVR_WORLD_SCALE", 1.0f);
  // In the headset the zero-parallax plane is the screen itself unless told otherwise
  const float convMetres = ACVR_EnvFloat("ACVR_CONVERGENCE", xr ? ACVR_XR_DistanceMetres() : 0.0f);
  s_acvrVRFake     = getenv("ACVR_VR_FAKE") != nullptr;
  s_acvrVRWanted   = s_acvrVRFake || getenv("ACVR_VR") != nullptr;
  s_acvrVRBackdrop = getenv("ACVR_VR_BACKDROP") != nullptr;
  s_acvrFakeYaw    = ACVR_EnvFloat("ACVR_VR_FAKE_YAW", 0.0f) * 0.01745329f;
  g_acvrStereo.enabled     = xr || s_acvrVRWanted || getenv("ACVR_STEREO") != nullptr;
  g_acvrStereo.halfSep     = 0.5f * sepMetres * s_acvrUnitsPerMetre;
  g_acvrStereo.convergence = convMetres * s_acvrUnitsPerMetre;
  g_acvrStereo.fovScale    = ACVR_EnvFloat("ACVR_FOV_SCALE", 1.0f);
  g_acvrStereo.noCull      = s_acvrVRWanted || getenv("ACVR_NO_CULL") != nullptr;
  if (const char *w = getenv("ACVR_WIDE_ALL")) g_acvrStereo.wideAll = (w[0] != '0');
  if (const char *f = getenv("ACVR_CAPTURE_FRAME"))
  {
    for (const char *c = f; *c; )
    {
      s_acvrCaptureFrames.push_back(atol(c));
      while (*c && *c != ',') c++;
      if (*c == ',') c++;
    }
  }
  if (const char *f = getenv("ACVR_SAVE_STATE_FRAME")) s_acvrSaveStateFrame = atol(f);
  if (const char *d = getenv("ACVR_CAPTURE_DIR"))   s_acvrCaptureDir = d;
  if (const char *script = getenv("ACVR_INPUT_SCRIPT"))
  {
    for (const char *c = script; *c; )
    {
      ACVRScriptedInput e; char id[64] = { 0 };
      if (sscanf(c, "%ld-%ld:%63[^:]:%d", &e.first, &e.last, id, &e.value) == 4) { e.id = id; s_acvrInputScript.push_back(e); }
      while (*c && *c != ';') c++;
      if (*c == ';') c++;
    }
  }
  if (g_acvrStereo.enabled)
    printf("ACVR stereo on: %s, %g units per metre\n", s_acvrVRWanted ? "head-tracked" : "screen", s_acvrUnitsPerMetre);
}

// Called right after the inputs are polled: holds the scripted inputs for their frames
static void ACVR_ApplyInputScript(CInputs *inputs)
{
  const long frame = s_acvrFrame + 1;
  for (const auto &e : s_acvrInputScript)
  {
    if (frame < e.first || frame > e.last) continue;
    auto input = (*inputs)[e.id.c_str()];
    if (input) input->value = e.value;
  }
}

static bool ACVR_StickGrabOn()
{
  if (s_acvrStickGrab < 0) { const char *e = getenv("ACVR_STICK_GRAB"); s_acvrStickGrab = (e && e[0] == '1') ? 1 : 0; }
  return s_acvrStickGrab == 1;
}

// A stick tilt in radians to -1 .. 1: a 2 degree dead zone, then full deflection at 25 degrees (the pointing scale)
static float ACVR_StickAxis(float angle)
{
  const float dead = 2.0f * 0.01745329f, full = 25.0f * 0.01745329f;
  const float m = fabsf(angle);
  if (!(m > dead)) return 0.0f;
  const float v = (m - dead) / (full - dead);
  return (angle < 0.0f ? -1.0f : 1.0f) * (v > 1.0f ? 1.0f : v);
}

// The flight stick to the game: x to the right, y pulled back. Pulled back aims up, the same hand movement
// (nose up) that aims up in the pointing mapping; ACVR_STICK_INVERT=1 swaps that.
static void ACVR_SetStick(CInputs *inputs, float x, float y)
{
  static int invert = -1;
  if (invert < 0) { const char *e = getenv("ACVR_STICK_INVERT"); invert = (e && e[0] == '1') ? 1 : 0; }
  s_acvrStick[0] = x; s_acvrStick[1] = y;
  const float gy = invert ? -y : y;
  if (auto in = (*inputs)["AnalogJoyX"]) in->value = (int)(128.0f + x * 127.0f + 0.5f);
  if (auto in = (*inputs)["AnalogJoyY"]) in->value = (int)(128.0f - gy * 127.0f + 0.5f);
}

static void ACVR_LogStick(CInputs *inputs, const char *how)
{
  static int frames = 0;
  if (++frames < 60) return;
  frames = 0;
  auto jx = (*inputs)["AnalogJoyX"], jy = (*inputs)["AnalogJoyY"];
  printf("ACVR stick (%s): x %.2f y %.2f -> AnalogJoyX %d AnalogJoyY %d\n", how, s_acvrStick[0], s_acvrStick[1],
         jx ? jx->value : -1, jy ? jy->value : -1);
}

// ---------------------------------------------------------------- ACVR: pointing at the picture
// Where an analog-joystick game (Star Wars Trilogy) draws its crosshair for each stick value, in pixels of its
// 496x384 picture, measured from the game's own crosshair (Yavin, 2026-10-19). The response is not linear: the
// crosshair moves half as far per step near the centre and at the ends, and never reaches the outer 34 pixels.
struct ACVRAimPoint { float value, pixel; };
static std::vector<ACVRAimPoint> s_acvrAimX, s_acvrAimY;

static void ACVR_ReadAimTable(std::vector<ACVRAimPoint> &t, const char *env, const char *fallback)
{
  const char *e = getenv(env);
  const char *src = (e && *e) ? e : fallback;
  t.clear();
  for (const char *c = src; *c; )
  {
    ACVRAimPoint pt;
    if (sscanf(c, "%f:%f", &pt.value, &pt.pixel) == 2) t.push_back(pt);
    while (*c && *c != ',') c++;
    if (*c == ',') c++;
  }
  std::sort(t.begin(), t.end(), [](const ACVRAimPoint &a, const ACVRAimPoint &b) { return a.pixel < b.pixel; });
}

static void ACVR_AimTablesRead()
{
  if (!s_acvrAimX.empty()) return;
  ACVR_ReadAimTable(s_acvrAimX, "ACVR_AIM_X", "0:34,16:48,32:76,48:108,64:140,80:172,96:203,112:230,128:245,160:288,176:319,192:352,208:384,224:418,240:446,255:461");
  ACVR_ReadAimTable(s_acvrAimY, "ACVR_AIM_Y", "0:36,16:46,32:67,48:91,64:115,80:140,96:163,112:183,128:195,144:206,160:227,176:252,192:277,208:301,224:325,240:346,255:357");
}

// The stick value that puts the crosshair at this pixel (clamped to the crosshair's reach)
static int ACVR_AimValue(const std::vector<ACVRAimPoint> &t, float pixel)
{
  if (t.size() < 2) return 128;
  if (pixel <= t.front().pixel) return (int)(t.front().value + 0.5f);
  if (pixel >= t.back().pixel) return (int)(t.back().value + 0.5f);
  for (size_t i = 1; i < t.size(); i++)
  {
    if (pixel > t[i].pixel) continue;
    const float f = (pixel - t[i - 1].pixel) / (t[i].pixel - t[i - 1].pixel);
    return (int)(t[i - 1].value + f * (t[i].value - t[i - 1].value) + 0.5f);
  }
  return 128;
}

// The surface a pointing ray is met on, in the controllers' space: the screen the presenter placed (screen mode),
// the same screen placed for a head at the origin when there is no presenter (unattended checks), or in the
// head-tracked mode a virtual screen that carries the arcade view, ACVR_AIM_DISTANCE metres ahead of the
// recentre point. g0 and g1 bound the game's own 496x384 picture on it, as fractions from the top left.
struct ACVRAimScreen { float centre[3], xaxis[3], normal[3], w, h, g0[2], g1[2]; };

static bool ACVR_GetAimScreen(const ACVRControllers &c, ACVRAimScreen &s)
{
  float tl = g_acvrStereo.tanL, tr = g_acvrStereo.tanR, tb = g_acvrStereo.tanB, tt = g_acvrStereo.tanT;
  if (!(tr - tl > 0.05f) || !(tt - tb > 0.05f)) { tl = -0.6f; tr = 0.6f; tb = -0.45f; tt = 0.45f; }
  float yaw = 0.0f;
  const bool whole = !g_acvrStereo.vr;      // screen mode: the screen shows the whole eye picture
  if (c.screenValid)
  {
    for (int k = 0; k < 3; k++) s.centre[k] = c.screenPos[k];
    yaw = c.screenYaw; s.w = c.screenSize[0]; s.h = c.screenSize[1];
  }
  else if (g_acvrStereo.vr)
  {
    // The arcade view on the plane of the HUD panel, which (with hudMatch) covers exactly this rectangle: the ray
    // meets the panel where the game's crosshair is drawn, and that is where the game's camera aims
    const float d = ACVR_EnvFloat("ACVR_AIM_DISTANCE", ACVR_EnvFloat("ACVR_HUD_DISTANCE", 1.6f));
    yaw = s_acvrOriginYaw;
    float cx, cy;
    ACVR_ArcadeRect(d, cx, cy, s.w, s.h);
    s.centre[0] = s_acvrOriginPos[0] + cx * cosf(yaw) - d * sinf(yaw);
    s.centre[1] = s_acvrOriginPos[1] + cy;
    s.centre[2] = s_acvrOriginPos[2] - cx * sinf(yaw) - d * cosf(yaw);
  }
  else
  {
    const float d = ACVR_XR_DistanceMetres(), size = ACVR_EnvFloat("ACVR_XR_SIZE", 1.0f);
    s.centre[0] = 0.0f; s.centre[1] = ACVR_EnvFloat("ACVR_XR_HEIGHT", 0.0f); s.centre[2] = -d;
    s.w = d * (tr - tl) * size; s.h = d * (tt - tb) * size;
  }
  s.xaxis[0] = cosf(yaw); s.xaxis[1] = 0.0f; s.xaxis[2] = -sinf(yaw);
  s.normal[0] = sinf(yaw); s.normal[1] = 0.0f; s.normal[2] = cosf(yaw);
  if (whole && totalXRes > 0 && totalYRes > 0)
  {
    s.g0[0] = (float)xOffset / totalXRes; s.g1[0] = (float)(xOffset + xRes) / totalXRes;
    s.g0[1] = (float)yOffset / totalYRes; s.g1[1] = (float)(yOffset + yRes) / totalYRes;
  }
  else { s.g0[0] = s.g0[1] = 0.0f; s.g1[0] = s.g1[1] = 1.0f; }
  return s.w > 0.0f && s.h > 0.0f;
}

// The last pointing ray that met the aim plane, reference space (drawn as a laser in the head-tracked mode)
static float s_acvrAimFrom[3] = { 0, 0, 0 }, s_acvrAimHit[3] = { 0, 0, 0 };
static long  s_acvrAimFrame = -10;

// Where the right controller's pointing ray meets the game picture: u, v from its top left (may be outside 0..1)
static bool ACVR_AimUV(const ACVRControllers &c, float &u, float &v)
{
  ACVRAimScreen s;
  if (!c.aimValid[1] || !ACVR_GetAimScreen(c, s)) return false;
  const float *q = c.aimRot[1];
  const float d[3] = { -2.0f * (q[0] * q[2] + q[3] * q[1]), -2.0f * (q[1] * q[2] - q[3] * q[0]), -(1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1])) };
  const float *o = c.aimPosValid[1] ? c.aimPos[1] : c.gripPos[1];
  const float dn = d[0] * s.normal[0] + d[1] * s.normal[1] + d[2] * s.normal[2];
  if (!(dn < -1e-4f)) return false;                       // pointing away from the screen
  const float t = ((s.centre[0] - o[0]) * s.normal[0] + (s.centre[1] - o[1]) * s.normal[1] + (s.centre[2] - o[2]) * s.normal[2]) / dn;
  if (!(t > 0.0f)) return false;
  const float hit[3] = { o[0] + t * d[0] - s.centre[0], o[1] + t * d[1] - s.centre[1], o[2] + t * d[2] - s.centre[2] };
  const float su = 0.5f + (hit[0] * s.xaxis[0] + hit[1] * s.xaxis[1] + hit[2] * s.xaxis[2]) / s.w;
  const float sv = 0.5f - hit[1] / s.h;
  for (int k = 0; k < 3; k++) { s_acvrAimFrom[k] = o[k]; s_acvrAimHit[k] = o[k] + t * d[k]; }
  s_acvrAimFrame = s_acvrFrame;
  u = (su - s.g0[0]) / (s.g1[0] - s.g0[0]);
  v = (sv - s.g0[1]) / (s.g1[1] - s.g0[1]);
  return true;
}

// Made-up controllers for unattended checks (ACVR_FAKE_CONTROLLERS), held for the given frames
struct ACVRFakeEntry { long first, last; std::string key; float value; };
static std::vector<ACVRFakeEntry> s_acvrFake;
static int s_acvrFakeRead = 0;

static void ACVR_FakeQuat(float yaw, float pitch, float q[4])      // pitch about x first, then yaw about y
{
  const float cy = cosf(yaw * 0.5f), sy = sinf(yaw * 0.5f), cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
  q[0] = cy * sp; q[1] = sy * cp; q[2] = -sy * sp; q[3] = cy * cp;
}

static bool ACVR_FakeControllers(ACVRControllers &c)
{
  if (!s_acvrFakeRead)
  {
    s_acvrFakeRead = 1;
    if (const char *e = getenv("ACVR_FAKE_CONTROLLERS"))
      for (const char *p = e; *p; )
      {
        ACVRFakeEntry f; char key[32] = { 0 };
        if (sscanf(p, "%ld-%ld:%31[^:]:%f", &f.first, &f.last, key, &f.value) == 4) { f.key = key; s_acvrFake.push_back(f); }
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
      }
  }
  if (s_acvrFake.empty()) return false;
  c = ACVRControllers();
  c.valid = true;
  const long frame = s_acvrFrame + 1;
  float hand[3] = { 0.2f, -0.3f, -0.3f };
  float aimU = -1.0f, aimV = -1.0f, aimYaw = 0.0f, aimPitch = 0.0f, wheel = 0.0f;
  bool aimAt = false, aimAngles = false, onWheel = false;
  for (const ACVRFakeEntry &f : s_acvrFake)
  {
    if (frame < f.first || frame > f.last) continue;
    const std::string &k = f.key; const float v = f.value;
    if (k == "trigger.l") c.trigger[0] = v; else if (k == "trigger.r") c.trigger[1] = v;
    else if (k == "grip.l") c.grip[0] = v; else if (k == "grip.r") c.grip[1] = v;
    else if (k == "a") c.a = v > 0.5f; else if (k == "b") c.b = v > 0.5f; else if (k == "x") c.x = v > 0.5f;
    else if (k == "y") c.y = v > 0.5f; else if (k == "menu") c.menu = v > 0.5f;
    else if (k == "stick.lx") c.stick[0][0] = v; else if (k == "stick.ly") c.stick[0][1] = v;
    else if (k == "stick.rx") c.stick[1][0] = v; else if (k == "stick.ry") c.stick[1][1] = v;
    else if (k == "click.l") c.stickClick[0] = v > 0.5f; else if (k == "click.r") c.stickClick[1] = v > 0.5f;
    else if (k == "wheel") { wheel = v; onWheel = true; }
    else if (k == "aim.u") { aimU = v; aimAt = true; } else if (k == "aim.v") { aimV = v; aimAt = true; }
    else if (k == "aim.yaw") { aimYaw = v; aimAngles = true; } else if (k == "aim.pitch") { aimPitch = v; aimAngles = true; }
    else if (k == "hand.x") hand[0] = v; else if (k == "hand.y") hand[1] = v; else if (k == "hand.z") hand[2] = v;
  }
  if (onWheel)
  {
    // Both hands on a 36 cm wheel 0.4 m ahead of and 0.35 m below the head, turned by the given angle
    const float a = wheel * 0.01745329f, rr = 0.18f, centre[3] = { 0.0f, -0.35f, -0.4f };
    c.grip[0] = c.grip[1] = 1.0f; c.gripValid[0] = c.gripValid[1] = true;
    c.gripPos[1][0] = centre[0] + rr * cosf(a); c.gripPos[1][1] = centre[1] - rr * sinf(a); c.gripPos[1][2] = centre[2];
    c.gripPos[0][0] = centre[0] - rr * cosf(a); c.gripPos[0][1] = centre[1] + rr * sinf(a); c.gripPos[0][2] = centre[2];
  }
  else
  {
    c.gripValid[1] = true;
    for (int k = 0; k < 3; k++) c.gripPos[1][k] = hand[k];
  }
  c.aimPosValid[1] = true;
  for (int k = 0; k < 3; k++) c.aimPos[1][k] = hand[k];
  if (aimAt && aimU >= -0.5f && aimV >= -0.5f)
  {
    // Point from the hand at that place of the game picture on the aim screen
    ACVRAimScreen s;
    if (ACVR_GetAimScreen(c, s))
    {
      const float su = s.g0[0] + aimU * (s.g1[0] - s.g0[0]), sv = s.g0[1] + aimV * (s.g1[1] - s.g0[1]);
      float d[3];
      for (int k = 0; k < 3; k++) d[k] = s.centre[k] + (su - 0.5f) * s.w * s.xaxis[k] - hand[k];
      d[1] += (0.5f - sv) * s.h;
      const float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      if (len > 1e-4f)
      {
        ACVR_FakeQuat(atan2f(-d[0], -d[2]), asinf(d[1] / len), c.aimRot[1]);
        c.aimValid[1] = true;
      }
    }
  }
  else if (aimAngles)
  {
    ACVR_FakeQuat(aimYaw * 0.01745329f, aimPitch * 0.01745329f, c.aimRot[1]);
    c.aimValid[1] = true;
  }
  return true;
}

// Called right after the inputs are polled: VR controllers as cabinet controls. The layout depends on the
// kind of cabinet. Only writes an input while its controller control is in use, so the keyboard and gamepad
// keep working.
//
//   Driving     hold both grips and turn your hands like a wheel (or left thumbstick); right trigger =
//               accelerator, left trigger = brake; right thumbstick up/down = shift up/down;
//               A = start, B = coin, X = view change / VR1, Y = VR2 / hand brake, stick clicks = VR3 / VR4
//   Pointing    (analog joystick, Star Wars Trilogy) where the right controller points = the stick;
//               right trigger = its trigger, right grip = its event button; A = start, B = coin
//   Flight stick (ACVR_STICK_GRAB=1, Star Wars Trilogy) hold the right grip to take the stick: tilt the hand
//               back or forward (pitch) = stick Y, tip it to a side (roll) = stick X, from its pose at the grab;
//               let go and it springs back to centre; right thumbstick when not holding it. Right trigger = its
//               trigger, right stick click = its event button (the grip is the hand on the stick); A = start, B = coin
//   Stick games left thumbstick = joystick; A, B, X, Y = the four buttons;
//               menu button or right stick click = start (in Virtua Fighter 3 start also switches to the
//               first-person view during a fight); left stick click = coin
static void ACVR_ApplyVRInputs(CInputs *inputs, uint32_t gameInputs)
{
  s_acvrCabinet = (gameInputs & Game::INPUT_HARLEY) ? 2 : ((gameInputs & Game::INPUT_VEHICLE) ? 1 : 0);
  {
    const char *m = getenv("ACVR_HUD_MATCH");
    g_acvrStereo.hudMatch = (m && *m) ? (m[0] == '1') : ((gameInputs & Game::INPUT_ANALOG_JOYSTICK) != 0);
  }
  if (auto st = (*inputs)["Steering"]) s_acvrSteer = (st->value - 128) / 127.0f;
  if ((gameInputs & Game::INPUT_ANALOG_JOYSTICK) && ACVR_StickGrabOn())
  {
    s_acvrCabinet = 3;
    if (const char *e = getenv("ACVR_TEST_STICK"))      // a stick held at a fixed tilt, no controllers needed
    {
      float pitch = 0.0f, roll = 0.0f;
      sscanf(e, "%f,%f", &pitch, &roll);
      ACVR_SetStick(inputs, ACVR_StickAxis(roll * 0.01745329f), ACVR_StickAxis(pitch * 0.01745329f));
      ACVR_LogStick(inputs, "test");
      return;
    }
  }
  // Hold Y for 3 seconds of wall-clock time to quit the game. Y keeps its game mapping while held (driving:
  // VR2 + HandBrake; stick games: Escape / Shift), so a 3 s hand-brake hold also quits.
  static Uint32 yDownSince = 0;       // SDL_GetTicks() when Y went down, 0 = not held
  ACVRControllers c;
  if (!ACVR_XR_GetControllers(c) && !ACVR_FakeControllers(c))
  {
    yDownSince = 0;                   // no input (focus lost, thread stopped, stale): the hold starts again
    return;
  }
  if (!c.y)
    yDownSince = 0;
  else
  {
    const Uint32 now = SDL_GetTicks();
    if (yDownSince == 0) yDownSince = now ? now : 1;
    else if (!s_acvrQuit && (Uint32)(now - yDownSince) >= 3000)
    {
      printf("ACVR: Y held for 3 seconds; quitting\n");
      s_acvrQuit = true;
    }
  }

  // Once a second, note whether the controllers' pointing poses arrive (acvr-input.log in the working folder)
  {
    static int logFrames = 0, logLines = 0;
    if (++logFrames >= 60 && logLines < 120)
    {
      logFrames = 0;
      if (FILE *lf = fopen("acvr-input.log", logLines == 0 ? "w" : "a"))
      {
        fprintf(lf, "valid %d aim L %d R %d grip L %d R %d trigger R %.2f\n", c.valid ? 1 : 0, c.aimValid[0] ? 1 : 0,
                c.aimValid[1] ? 1 : 0, c.gripValid[0] ? 1 : 0, c.gripValid[1] ? 1 : 0, c.trigger[1]);
        fclose(lf);
      }
      logLines++;
    }
  }
  auto set = [&](const char *id, int value) { auto input = (*inputs)[id]; if (input) input->value = value; };
  auto press = [&](const char *id, bool down) { if (down) set(id, 1); };
  auto clampf = [](float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); };
  const float yaw = s_acvrOriginSet ? s_acvrOriginYaw : 0.0f;
  const float ca = cosf(-yaw), sa = sinf(-yaw);

  if (gameInputs & (Game::INPUT_VEHICLE | Game::INPUT_HARLEY))
  {
    // Pedals
    if (c.trigger[1] > 0.02f) set("Accelerator", (int)(clampf(c.trigger[1], 0.0f, 1.0f) * 255.0f));
    if (c.trigger[0] > 0.02f) set("Brake", (int)(clampf(c.trigger[0], 0.0f, 1.0f) * 255.0f));

    // Steering: both grips held = hands on the wheel. The wheel angle is the tilt of the line from the left
    // hand to the right hand, seen from the driver's seat. 80 degrees of hand turn is full lock.
    bool steering = false;
    float steer = 0.0f;
    if (c.grip[0] > 0.5f && c.grip[1] > 0.5f && c.gripValid[0] && c.gripValid[1])
    {
      const float dx = c.gripPos[1][0] - c.gripPos[0][0];
      const float dy = c.gripPos[1][1] - c.gripPos[0][1];
      const float dz = c.gripPos[1][2] - c.gripPos[0][2];
      const float across = dx * ca + dz * sa;             // how far the right hand is to the driver's right
      const float angle = atan2f(dy, across);             // right hand higher = turning left
      steer = clampf(-angle / (80.0f * 0.01745329f), -1.0f, 1.0f);
      steering = true;
    }
    else if (fabsf(c.stick[0][0]) > 0.12f)
    {
      steer = clampf(c.stick[0][0], -1.0f, 1.0f);
      steering = true;
    }
    if (steering) set("Steering", (int)clampf(128.0f + steer * 127.0f, 0.0f, 255.0f));
    if (steering) s_acvrSteer = steer;

    press("Start1", c.a || c.menu);
    press("Coin1", c.b);
    press("ViewChange", c.x); press("VR1", c.x);
    press("VR2", c.y); press("HandBrake", c.y);
    press("VR3", c.stickClick[0]);
    press("VR4", c.stickClick[1]);
    press("GearShiftUp", c.stick[1][1] > 0.7f);
    press("GearShiftDown", c.stick[1][1] < -0.7f);
  }
  else if ((gameInputs & Game::INPUT_ANALOG_JOYSTICK) && ACVR_StickGrabOn())
  {
    // A centre flight stick. Its shaft follows the hand's turn since the grab, seen in the player's frame:
    // pitch moves it back and forward, roll to the sides, and a twist about the vertical does nothing.
    static bool grabbed = false;
    static float q0[4] = { 0, 0, 0, 1 };
    float x = 0.0f, y = 0.0f;
    if (c.grip[1] > 0.5f)
    {
      if (c.aimValid[1])
      {
        const float *q = c.aimRot[1];
        if (!grabbed) { for (int k = 0; k < 4; k++) q0[k] = q[k]; grabbed = true; }
        auto rot = [](const float *r, float sign, const float v[3], float o[3]) {   // v turned by r (sign -1: by its inverse)
          const float qx = sign * r[0], qy = sign * r[1], qz = sign * r[2], w = r[3];
          const float tx = 2.0f * (qy * v[2] - qz * v[1]), ty = 2.0f * (qz * v[0] - qx * v[2]), tz = 2.0f * (qx * v[1] - qy * v[0]);
          o[0] = v[0] + w * tx + (qy * tz - qz * ty);
          o[1] = v[1] + w * ty + (qz * tx - qx * tz);
          o[2] = v[2] + w * tz + (qx * ty - qy * tx);
        };
        const float up[3] = { 0.0f, 1.0f, 0.0f };
        float a[3], tip[3];
        rot(q0, -1.0f, up, a);
        rot(q, 1.0f, a, tip);                                              // the shaft: upright, turned as the hand turned
        const float px = tip[0] * ca + tip[2] * sa, pz = -tip[0] * sa + tip[2] * ca;   // in the player's frame
        x = ACVR_StickAxis(atan2f(px, tip[1]));                            // top to the right
        y = ACVR_StickAxis(atan2f(pz, tip[1]));                            // top towards the player: pulled back
      }
      else if (grabbed) { x = s_acvrStick[0]; y = s_acvrStick[1]; }      // tracking lost for a moment: hold still
    }
    int pointX = -1, pointY = -1;
    if (c.grip[1] <= 0.5f)
    {
      grabbed = false;                                                     // let go: back to centre
      // Not holding the stick: the right controller points, 1:1 with the game's crosshair, as without the grab
      float u, v;
      if (ACVR_AimUV(c, u, v))
      {
        ACVR_AimTablesRead();
        pointX = ACVR_AimValue(s_acvrAimX, u * 496.0f); pointY = ACVR_AimValue(s_acvrAimY, v * 384.0f);
        x = (pointX - 128) / 127.0f; y = (128 - pointY) / 127.0f;
      }
      else if (fabsf(c.stick[1][0]) > 0.12f || fabsf(c.stick[1][1]) > 0.12f)
      {
        x = clampf(c.stick[1][0], -1.0f, 1.0f);
        y = clampf(c.stick[1][1], -1.0f, 1.0f);                            // thumbstick up aims up
      }
    }
    if (pointX >= 0)
    {
      s_acvrStick[0] = x; s_acvrStick[1] = y;
      set("AnalogJoyX", pointX); set("AnalogJoyY", pointY);
    }
    else
      ACVR_SetStick(inputs, x, y);
    ACVR_LogStick(inputs, grabbed ? "grabbed" : "free");
    press("AnalogJoyTrigger", c.trigger[1] > 0.5f);
    press("AnalogJoyEvent", c.stickClick[1]);
    press("Start1", c.a || c.menu);
    press("Coin1", c.b);
  }
  else if (gameInputs & Game::INPUT_ANALOG_JOYSTICK)
  {
    // The analog stick by pointing the right controller: the stick value that puts the game's own crosshair
    // where the controller's ray meets the picture (the screen in screen mode; in head-tracked mode a virtual
    // screen carrying the arcade view). Off the picture, the crosshair stays at its edge.
    // While the controllers are connected the mouse never drives it: centred when the controller isn't pointing.
    ACVR_AimTablesRead();
    set("AnalogJoyX", 128);
    set("AnalogJoyY", 128);
    float u = 0.5f, v = 0.5f;
    if (ACVR_AimUV(c, u, v))
    {
      const int jx = ACVR_AimValue(s_acvrAimX, u * 496.0f), jy = ACVR_AimValue(s_acvrAimY, v * 384.0f);
      set("AnalogJoyX", jx);
      set("AnalogJoyY", jy);
      static int logFrames = 0;
      if (!s_acvrFake.empty() && ++logFrames >= 30)
      {
        logFrames = 0;
        printf("ACVR aim: frame %ld u %.4f v %.4f -> AnalogJoyX %d AnalogJoyY %d\n", s_acvrFrame + 1, u, v, jx, jy);
      }
    }
    press("AnalogJoyTrigger", c.trigger[1] > 0.5f);
    press("AnalogJoyEvent", c.grip[1] > 0.5f);
    press("Start1", c.a || c.menu);
    press("Coin1", c.b);
  }
  else
  {
    // Stick-and-buttons cabinets. Start is kept off the four face buttons.
    press("JoyLeft", c.stick[0][0] < -0.5f); press("JoyRight", c.stick[0][0] > 0.5f);
    press("JoyUp", c.stick[0][1] > 0.5f);    press("JoyDown", c.stick[0][1] < -0.5f);
    press("Punch", c.a); press("Kick", c.b); press("Guard", c.x); press("Escape", c.y);
    press("Beat", c.a);  press("Charge", c.b); press("Jump", c.x); press("Shift", c.y);
    press("ShortPass", c.a); press("LongPass", c.b); press("Shoot", c.x);
    press("Start1", c.menu || c.stickClick[1]);
    press("Coin1", c.stickClick[0]);
  }
}

static void ACVR_MakeTarget(GLuint &fbo, GLuint &tex, unsigned w, unsigned h)
{
  glGenFramebuffers(1, &fbo);
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
}

static void ACVR_EnsureTargets()
{
  // With supersampling on, the eye pictures keep the full supersampled size: the headset gets every pixel
  const unsigned wantW = totalXRes * (unsigned)aaValue, wantH = totalYRes * (unsigned)aaValue;
  if (s_acvrEyeTex[0] && s_acvrEyeW == wantW && s_acvrEyeH == wantH)
    return;
  if (s_acvrEyeTex[0])
  {
    glDeleteFramebuffers(2, s_acvrEyeFbo);
    glDeleteTextures(2, s_acvrEyeTex);
    glDeleteFramebuffers(1, &s_acvrHudFbo);
    glDeleteTextures(1, &s_acvrHudTex);
  }
  s_acvrEyeW = wantW;
  s_acvrEyeH = wantH;
  for (int i = 0; i < 2; i++)
    ACVR_MakeTarget(s_acvrEyeFbo[i], s_acvrEyeTex[i], s_acvrEyeW, s_acvrEyeH);
  ACVR_MakeTarget(s_acvrHudFbo, s_acvrHudTex, s_acvrEyeW, s_acvrEyeH);
  if (!s_acvrSkyTex)
    ACVR_MakeTarget(s_acvrSkyFbo, s_acvrSkyTex, 16, 1);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Clears the whole of the bound target, including outside the scissor box
static void ACVR_ClearBound(float alpha)
{
  const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
  glDisable(GL_SCISSOR_TEST);
  glClearColor(0.0f, 0.0f, 0.0f, alpha);
  glClear(GL_COLOR_BUFFER_BIT);
  if (scissor) glEnable(GL_SCISSOR_TEST);
}

static void ACVR_QuatMul(const float a[4], const float b[4], float out[4])
{
  out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
  out[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
  out[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
  out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

// Start of a rendered frame: in head-tracked mode, fetch the newest eye poses and build the per-eye
// frusta and view matrices, relative to where the head was at recentre
void ACVR_FrameBegin()
{
  g_acvrStereo.vr = false;
  s_acvrHudDrawn = false;
  if (s_acvrSkySampled)
  {
    unsigned char px[16 * 4] = { 0 };
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_acvrSkyFbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 16, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    float sum[3] = { 0, 0, 0 };
    for (int i = 0; i < 16; i++) for (int k = 0; k < 3; k++) sum[k] += px[i * 4 + k];
    for (int k = 0; k < 3; k++) s_acvrSky[k] += 0.08f * (sum[k] / (16.0f * 255.0f) - s_acvrSky[k]);   // eased, so it does not flicker
    s_acvrSkySampled = false;
  }
  g_acvrStereo.debugViews = getenv("ACVR_DEBUG") != nullptr &&
    std::find(s_acvrCaptureFrames.begin(), s_acvrCaptureFrames.end(), s_acvrFrame + 1) != s_acvrCaptureFrames.end();
  if (!s_acvrVRWanted)
    return;

  ACVREyeView v[2];
  if (s_acvrVRFake)
  {
    const float c = cosf(s_acvrFakeYaw), s = sinf(s_acvrFakeYaw);
    for (int i = 0; i < 2; i++)
    {
      const float x = (i == 0) ? -0.032f : 0.032f;
      v[i].pos[0] = x * c; v[i].pos[1] = 0.0f; v[i].pos[2] = -x * s;
      v[i].rot[0] = 0.0f; v[i].rot[1] = sinf(s_acvrFakeYaw * 0.5f); v[i].rot[2] = 0.0f; v[i].rot[3] = cosf(s_acvrFakeYaw * 0.5f);
      v[i].tan[0] = -1.0f; v[i].tan[1] = 1.0f; v[i].tan[2] = -1.0f; v[i].tan[3] = 1.0f;
    }
  }
  else if (!ACVR_XR_GetViews(v))
  {
    return;     // headset not tracked yet: this frame is drawn in screen mode
  }

  // Where the head is and which way it faces (level, yaw only)
  const float *q = v[0].rot;
  const float fx = -2.0f * (q[0] * q[2] + q[3] * q[1]);
  const float fz = -(1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]));
  const float yaw = atan2f(-fx, -fz);
  const float centre[3] = { 0.5f * (v[0].pos[0] + v[1].pos[0]), 0.5f * (v[0].pos[1] + v[1].pos[1]), 0.5f * (v[0].pos[2] + v[1].pos[2]) };
  const Uint8 *keys = SDL_GetKeyboardState(nullptr);
  if (!s_acvrOriginSet || (keys && keys[SDL_SCANCODE_HOME]))
  {
    if (s_acvrVRFake) { s_acvrOriginPos[0] = s_acvrOriginPos[1] = s_acvrOriginPos[2] = 0.0f; s_acvrOriginYaw = 0.0f; }
    else { s_acvrOriginPos[0] = centre[0]; s_acvrOriginPos[1] = centre[1]; s_acvrOriginPos[2] = centre[2]; s_acvrOriginYaw = yaw; }
    s_acvrOriginSet = true;
  }

  // Undo the recentre yaw: a rotation by -originYaw about the up axis
  const float a = -s_acvrOriginYaw;
  const float ca = cosf(a), sa = sinf(a);
  const float unyaw[4] = { 0.0f, sinf(a * 0.5f), 0.0f, cosf(a * 0.5f) };
  for (int i = 0; i < 2; i++)
  {
    const float dx = v[i].pos[0] - s_acvrOriginPos[0];
    const float dy = v[i].pos[1] - s_acvrOriginPos[1];
    const float dz = v[i].pos[2] - s_acvrOriginPos[2];
    const float t[3] = { (dx * ca + dz * sa) * s_acvrUnitsPerMetre, dy * s_acvrUnitsPerMetre, (-dx * sa + dz * ca) * s_acvrUnitsPerMetre };
    float r[4];
    ACVR_QuatMul(unyaw, v[i].rot, r);
    const float x = r[0], y = r[1], z = r[2], w = r[3];
    const float R[3][3] = {
      { 1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w)     },
      { 2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w)     },
      { 2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y) } };
    // View matrix = inverse of the eye's pose: transpose of R, then minus that times the position
    float *m = g_acvrStereo.eyeView[i];
    for (int row = 0; row < 3; row++)
    {
      for (int col = 0; col < 3; col++)
        m[col * 4 + row] = R[col][row];
      m[12 + row] = -(R[0][row] * t[0] + R[1][row] * t[1] + R[2][row] * t[2]);
    }
    m[3] = m[7] = m[11] = 0.0f; m[15] = 1.0f;
    for (int k = 0; k < 4; k++) g_acvrStereo.eyeTan[i][k] = v[i].tan[k];
    s_acvrViews[i] = v[i];

    // Where the arcade view lies in this eye's picture: the game camera's forward axis, seen from the eye,
    // gives its centre; its own field of view gives its size. Rotation only, so the backdrop sits at infinity.
    float tl = g_acvrStereo.tanL, tr = g_acvrStereo.tanR, tb = g_acvrStereo.tanB, tt = g_acvrStereo.tanT;
    if (!(tr - tl > 0.05f) || !(tt - tb > 0.05f)) { tl = -0.6f; tr = 0.6f; tb = -0.45f; tt = 0.45f; }
    const float ex = -m[8], ey = -m[9], ez = -m[10];      // the view matrix applied to (0, 0, -1)
    int *rect = g_acvrStereo.backdropRect[i];
    rect[0] = rect[1] = rect[2] = rect[3] = 0;
    if (ez < -0.2f)
    {
      const float cx = ex / -ez, cy = ey / -ez;
      const float *e = v[i].tan;
      const float W = (float)(totalXRes * (unsigned)aaValue), H = (float)(totalYRes * (unsigned)aaValue);
      const float x0 = (cx + tl - e[0]) / (e[1] - e[0]) * W, x1 = (cx + tr - e[0]) / (e[1] - e[0]) * W;
      const float y0 = (cy + tb - e[2]) / (e[3] - e[2]) * H, y1 = (cy + tt - e[2]) / (e[3] - e[2]) * H;
      rect[0] = (int)x0; rect[1] = (int)y0; rect[2] = (int)(x1 - x0); rect[3] = (int)(y1 - y0);
    }
  }
  g_acvrStereo.vr = true;
}

// Head-tracked: draw the bottom 2D layer once on its own to see whether it holds a backdrop, and read the
// colour along its top edge. That colour fills whatever the backdrop does not cover.
void ACVR_BackdropBegin()
{
  g_acvrStereo.backdrop = false;
  ACVR_EnsureTargets();
  if (!s_acvrAATarget)
    glBindFramebuffer(GL_FRAMEBUFFER, s_acvrHudFbo);    // the 2D renderer draws to whatever is bound
}

void ACVR_BackdropEnd()
{
  glBindFramebuffer(GL_READ_FRAMEBUFFER, s_acvrAATarget ? s_acvrAATarget : s_acvrHudFbo);
  unsigned char px[16 * 4] = { 0 };
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  // A sixteenth of the way down from the top, clear of the border the emulator leaves around the layer
  glReadPixels((GLint)(s_acvrEyeW / 2) - 8, (GLint)(s_acvrEyeH - s_acvrEyeH / 16), 16, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
  if (g_acvrStereo.debugViews)
    ACVR_SaveBound(s_acvrCaptureDir + "/bottom-layer.bmp", s_acvrEyeW, s_acvrEyeH);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  float sum[4] = { 0, 0, 0, 0 };
  for (int i = 0; i < 16; i++) for (int k = 0; k < 4; k++) sum[k] += px[i * 4 + k];
  if (!s_acvrVRBackdrop && sum[3] / 16.0f < 128.0f)
    return;                                             // nothing there: no backdrop this frame
  for (int k = 0; k < 3; k++) g_acvrStereo.backdropClear[k] = sum[k] / (16.0f * 255.0f);
  g_acvrStereo.backdrop = true;
}

bool ACVR_SkipBottomLayer()
{
  if (!g_acvrStereo.vr || g_acvrStereo.backdrop)
    return false;       // screen mode, or a backdrop to draw: the 2D renderer handles the bottom layer
  // No backdrop from the game: fill with the colour of the upper part of the last picture, so that what
  // the game never draws (straight up, far to the sides) reads as more sky instead of a black hole
  glBindFramebuffer(GL_FRAMEBUFFER, s_acvrAATarget);
  const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
  glDisable(GL_SCISSOR_TEST);
  glClearColor(s_acvrSky[0], s_acvrSky[1], s_acvrSky[2], 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (scissor) glEnable(GL_SCISSOR_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return true;
}

// ---------------------------------------------------------------- ACVR: cabinet controls in the headset
// Head-tracked mode draws the cabinet's own control (the wheel, Harley's bars, or with ACVR_STICK_GRAB=1
// Star Wars Trilogy's flight stick, tilting with the stick the game receives) in front of the
// player, turning with the steering the game receives. Models come from assets/controls, baked by
// bake_control.py into Assets/acvr/*.acm. The control sits where the cabinet data puts it relative to a
// seated player whose eyes were at the recentre point (ACVR_CONTROL_POS overrides, metres x,y,z).
// Off unless ACVR_CONTROLS=1; ACVR_TEST_STEER=-1..1 fixes the steering for unattended pictures.

struct ACVRControlPart { bool moving; float colour[3]; GLsizei count; GLsizei first; };
static int      s_acvrControlLoaded = 0;    // which cabinet's model is in the buffers (0: none)
static bool     s_acvrControlFailed = false;
static GLuint   s_acvrControlProg = 0, s_acvrControlVao = 0, s_acvrControlVbo = 0, s_acvrControlIbo = 0;
static GLint    s_acvrCtlMvp = -1, s_acvrCtlModel = -1, s_acvrCtlAngle = -1, s_acvrCtlMoving = -1, s_acvrCtlColour = -1, s_acvrCtlAxis = -1;
static GLint    s_acvrCtlAngle2 = -1, s_acvrCtlAxis2 = -1, s_acvrCtlPivot = -1;
static float    s_acvrControlPivot[3] = { 0, 0, 0 };   // the loaded model's pivot origin, from the baked file
static std::vector<ACVRControlPart> s_acvrControlParts;

static GLuint ACVR_CompileControlShader()
{
  static const char *vs =
    "#version 410 core\n"
    "layout(location = 0) in vec3 inPos;\n"
    "layout(location = 1) in vec3 inNormal;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 model;\n"
    "uniform float angle;\n"
    "uniform int moving;\n"
    "uniform vec3 axis;\n"
    "uniform float angle2;\n"      // a first turn, before the one above (the flight stick's second gimbal axis)
    "uniform vec3 axis2;\n"
    "uniform vec3 pivot;\n"        // what the moving parts turn about (the flight stick's gimbal; 0 for the others)
    "out vec3 vNormal;\n"
    "vec3 rot(vec3 v, vec3 ax, float a) {\n"
    "  float c = cos(a), s = sin(a);\n"
    "  return v * c + cross(ax, v) * s + ax * dot(ax, v) * (1.0 - c);\n"
    "}\n"
    "vec3 turn(vec3 v) {\n"
    "  if (moving == 0) return v;\n"
    "  return rot(rot(v, axis2, angle2), axis, angle);\n"
    "}\n"
    "void main() {\n"
    "  vec3 p = moving == 0 ? inPos : turn(inPos - pivot) + pivot;\n"
    "  vNormal = mat3(model) * turn(inNormal);\n"
    "  gl_Position = mvp * vec4(p, 1.0);\n"
    "}\n";
  static const char *fs =
    "#version 410 core\n"
    "in vec3 vNormal;\n"
    "uniform vec3 colour;\n"
    "out vec4 outColour;\n"
    "void main() {\n"
    "  vec3 n = normalize(vNormal);\n"
    "  float key = max(dot(n, normalize(vec3(0.3, 0.9, 0.4))), 0.0);\n"
    "  float fill = max(dot(n, normalize(vec3(-0.5, 0.2, 0.8))), 0.0);\n"
    "  vec3 c = colour * (0.35 + 0.75 * key + 0.25 * fill) + vec3(0.05);\n"
    "  outColour = vec4(pow(c, vec3(1.0 / 2.2)), 1.0);\n"
    "}\n";
  auto compile = [](GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(s, sizeof(log), nullptr, log); printf("ACVR controls shader: %s\n", log); }
    return s;
  };
  GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
  GLuint p = glCreateProgram();
  glAttachShader(p, v); glAttachShader(p, f);
  glLinkProgram(p);
  glDeleteShader(v); glDeleteShader(f);
  GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) { glDeleteProgram(p); return 0; }
  return p;
}

static bool ACVR_EnsureControlProgram()
{
  if (s_acvrControlProg) return true;
  s_acvrControlProg = ACVR_CompileControlShader();
  if (!s_acvrControlProg) return false;
  s_acvrCtlMvp = glGetUniformLocation(s_acvrControlProg, "mvp");
  s_acvrCtlModel = glGetUniformLocation(s_acvrControlProg, "model");
  s_acvrCtlAngle = glGetUniformLocation(s_acvrControlProg, "angle");
  s_acvrCtlMoving = glGetUniformLocation(s_acvrControlProg, "moving");
  s_acvrCtlColour = glGetUniformLocation(s_acvrControlProg, "colour");
  s_acvrCtlAxis = glGetUniformLocation(s_acvrControlProg, "axis");
  s_acvrCtlAngle2 = glGetUniformLocation(s_acvrControlProg, "angle2");
  s_acvrCtlAxis2 = glGetUniformLocation(s_acvrControlProg, "axis2");
  s_acvrCtlPivot = glGetUniformLocation(s_acvrControlProg, "pivot");
  return true;
}

// The supersampling target is a colour texture only, with no depth buffer: a model drawn into it had every
// triangle painted in file order, so its far side showed through its near side (it looked hollow). A depth
// buffer of our own is attached while a model is drawn over the picture and taken off again afterwards, so the
// emulator's own drawing into that target is unchanged.
static GLuint   s_acvrDepthRb = 0;
static unsigned s_acvrDepthW = 0, s_acvrDepthH = 0;
static void ACVR_OverlayDepth(bool on)
{
  if (!s_acvrAATarget) return;         // no supersampling: the window's own buffer has depth
  if (!on) { glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0); return; }
  if (!s_acvrDepthRb || s_acvrDepthW != s_acvrEyeW || s_acvrDepthH != s_acvrEyeH)
  {
    GLint bound = 0;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &bound);
    if (!s_acvrDepthRb) glGenRenderbuffers(1, &s_acvrDepthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, s_acvrDepthRb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, (GLsizei)s_acvrEyeW, (GLsizei)s_acvrEyeH);
    glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)bound);
    s_acvrDepthW = s_acvrEyeW; s_acvrDepthH = s_acvrEyeH;
  }
  glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_acvrDepthRb);
}

static bool ACVR_LoadControl(int cabinet)
{
  const char *file = cabinet == 3 ? "Assets/acvr/stick-flight.acm" :
                     (cabinet == 2 ? "Assets/acvr/bars-motorcycle.acm" : "Assets/acvr/wheel-solid.acm");
  FILE *f = fopen(file, "rb");
  if (!f && cabinet == 1) { file = "Assets/acvr/wheel-three-spoke.acm"; f = fopen(file, "rb"); }   // the older baked wheel
  if (!f) { printf("ACVR controls: %s not found\n", file); return false; }
  char magic[4]; uint32_t parts = 0; float origin[3];
  bool ok = fread(magic, 1, 4, f) == 4 && memcmp(magic, "ACM1", 4) == 0 && fread(&parts, 4, 1, f) == 1 && fread(origin, 4, 3, f) == 3;
  std::vector<float> verts;
  std::vector<uint32_t> indices;
  s_acvrControlParts.clear();
  for (uint32_t i = 0; ok && i < parts; i++)
  {
    uint32_t moving = 0, nv = 0, ni = 0; float colour[3];
    ok = fread(&moving, 4, 1, f) == 1 && fread(colour, 4, 3, f) == 3 && fread(&nv, 4, 1, f) == 1 && fread(&ni, 4, 1, f) == 1;
    if (!ok) break;
    const uint32_t base = (uint32_t)(verts.size() / 6);
    const size_t vstart = verts.size(), istart = indices.size();
    verts.resize(vstart + (size_t)nv * 6);
    indices.resize(istart + ni);
    ok = fread(verts.data() + vstart, 4, (size_t)nv * 6, f) == (size_t)nv * 6 && fread(indices.data() + istart, 4, ni, f) == ni;
    for (size_t k = istart; k < indices.size(); k++) indices[k] += base;
    ACVRControlPart part = { moving != 0, { colour[0], colour[1], colour[2] }, (GLsizei)ni, (GLsizei)istart };
    s_acvrControlParts.push_back(part);
  }
  fclose(f);
  if (!ok || verts.empty()) { printf("ACVR controls: %s unreadable\n", file); return false; }
  for (int k = 0; k < 3; k++) s_acvrControlPivot[k] = origin[k];
  if (!ACVR_EnsureControlProgram()) return false;
  if (!s_acvrControlVao)
  {
    glGenVertexArrays(1, &s_acvrControlVao);
    glGenBuffers(1, &s_acvrControlVbo);
    glGenBuffers(1, &s_acvrControlIbo);
  }
  GLint vao = 0, ab = 0;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &ab);
  glBindVertexArray(s_acvrControlVao);
  glBindBuffer(GL_ARRAY_BUFFER, s_acvrControlVbo);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_acvrControlIbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const void *)(3 * sizeof(float)));
  glBindVertexArray((GLuint)vao);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)ab);
  printf("ACVR controls: %s, %u parts\n", file, (unsigned)s_acvrControlParts.size());
  return true;
}

// Column-major 4x4 helpers
static void ACVR_MatMul(const float *a, const float *b, float *out)
{
  float r[16];
  for (int c = 0; c < 4; c++)
    for (int row = 0; row < 4; row++)
      r[c * 4 + row] = a[0 * 4 + row] * b[c * 4 + 0] + a[1 * 4 + row] * b[c * 4 + 1] + a[2 * 4 + row] * b[c * 4 + 2] + a[3 * 4 + row] * b[c * 4 + 3];
  memcpy(out, r, sizeof(r));
}

static void ACVR_DrawControls(int eye)
{
  if (s_acvrCabinet == 0 || s_acvrControlFailed) return;
  static int stickModel = -1;
  if (stickModel < 0) { const char *e = getenv("ACVR_STICK_MODEL"); stickModel = e && e[0] == '1'; }
  if (s_acvrCabinet == 3 && !stickModel) return;          // the flight stick model: off unless asked for
  static int enabled = -1;
  if (enabled < 0) { const char *e = getenv("ACVR_CONTROLS"); enabled = e && e[0] == '1'; }   // off unless asked for
  if (!enabled) return;
  if (s_acvrControlLoaded != s_acvrCabinet)
  {
    if (!ACVR_LoadControl(s_acvrCabinet)) { s_acvrControlFailed = true; return; }
    s_acvrControlLoaded = s_acvrCabinet;
  }

  // Placement in the recentred room (metres): the cabinet's seat at 0.55 m with the wheel at 0.78 m and 0.42 m
  // ahead puts it roughly 0.4 m below and 0.45 m ahead of a seated player's eyes; Harley's bars a little higher.
  float pos[3] = { 0.0f, -0.38f, -0.45f };
  float tiltDeg = 25.0f, range = 2.35619449f;
  float axis[3] = { 0.0f, 0.0f, -1.0f };
  if (s_acvrCabinet == 2) { pos[1] = -0.32f; pos[2] = -0.42f; tiltDeg = 0.0f; range = 0.523599f; axis[0] = 0.0f; axis[1] = -1.0f; axis[2] = 0.0f; }
  // The flight stick: upright, its base 0.6 m below the eyes and 0.35 m ahead, a little right, so the grip is
  // where a seated player's right hand rests. It tilts 25 degrees at full deflection, the same as the hand.
  float angle2 = 0.0f, axis2[3] = { 1.0f, 0.0f, 0.0f }, pivot[3] = { 0.0f, 0.0f, 0.0f };
  if (s_acvrCabinet == 3)
  {
    pos[0] = 0.12f; pos[1] = -0.6f; pos[2] = -0.35f; tiltDeg = 0.0f; range = 25.0f * 0.01745329f;
    axis[0] = 0.0f; axis[1] = 0.0f; axis[2] = -1.0f;                 // stick X: top to the right
    angle2 = s_acvrStick[1] * range;                                  // stick Y: top towards the player
    for (int k = 0; k < 3; k++) pivot[k] = s_acvrControlPivot[k];
  }
  if (const char *e = getenv("ACVR_CONTROL_POS")) sscanf(e, "%f,%f,%f", &pos[0], &pos[1], &pos[2]);
  float steer = s_acvrCabinet == 3 ? s_acvrStick[0] : s_acvrSteer;
  if (const char *e = getenv("ACVR_TEST_STEER")) steer = (float)atof(e);
  steer = steer < -1.0f ? -1.0f : (steer > 1.0f ? 1.0f : steer);

  // model = scale(units per metre) * translate(pos) * rotateX(-tilt)
  const float u = s_acvrUnitsPerMetre;
  const float t = -tiltDeg * 0.01745329f, ct = cosf(t), st = sinf(t);
  const float model[16] = {
    u, 0, 0, 0,
    0, u * ct, u * st, 0,
    0, -u * st, u * ct, 0,
    pos[0] * u, pos[1] * u, pos[2] * u, 1 };
  // projection from this eye's tangents, near 5 cm, far 100 m
  const float *tn = g_acvrStereo.eyeTan[eye];
  const float n = 0.05f * u, fa = 100.0f * u;
  const float l = tn[0] * n, r = tn[1] * n, b = tn[2] * n, tp = tn[3] * n;
  const float proj[16] = {
    2 * n / (r - l), 0, 0, 0,
    0, 2 * n / (tp - b), 0, 0,
    (r + l) / (r - l), (tp + b) / (tp - b), -(fa + n) / (fa - n), -1,
    0, 0, -2 * fa * n / (fa - n), 0 };
  float vm[16], mvp[16];
  ACVR_MatMul(g_acvrStereo.eyeView[eye], model, vm);
  ACVR_MatMul(proj, vm, mvp);

  // keep the emulator's GL state
  GLint prog, vao, fb, vp[4]; GLboolean depth, cull, blend, scissor, depthMask;
  glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb);
  glGetIntegerv(GL_VIEWPORT, vp);
  depth = glIsEnabled(GL_DEPTH_TEST); cull = glIsEnabled(GL_CULL_FACE); blend = glIsEnabled(GL_BLEND); scissor = glIsEnabled(GL_SCISSOR_TEST);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
  GLint depthFunc; glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
  GLfloat clearDepth; glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
  GLint elementBuffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementBuffer);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_acvrAATarget);
  ACVR_OverlayDepth(true);
  glViewport(0, 0, (GLsizei)s_acvrEyeW, (GLsizei)s_acvrEyeH);
  glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthFunc(GL_LESS); glClearDepth(1.0);
  glClear(GL_DEPTH_BUFFER_BIT);      // the game's picture is finished; the control is nearest anyway
  glUseProgram(s_acvrControlProg);
  glUniformMatrix4fv(s_acvrCtlMvp, 1, GL_FALSE, mvp);
  glUniformMatrix4fv(s_acvrCtlModel, 1, GL_FALSE, model);
  glUniform1f(s_acvrCtlAngle, steer * range);
  glUniform3f(s_acvrCtlAxis, axis[0], axis[1], axis[2]);
  glUniform1f(s_acvrCtlAngle2, angle2);
  glUniform3f(s_acvrCtlAxis2, axis2[0], axis2[1], axis2[2]);
  glUniform3f(s_acvrCtlPivot, pivot[0], pivot[1], pivot[2]);
  glBindVertexArray(s_acvrControlVao);
  for (const ACVRControlPart &p : s_acvrControlParts)
  {
    glUniform1i(s_acvrCtlMoving, p.moving ? 1 : 0);
    glUniform3f(s_acvrCtlColour, p.colour[0], p.colour[1], p.colour[2]);
    glDrawElements(GL_TRIANGLES, p.count, GL_UNSIGNED_INT, (const void *)(size_t)(p.first * sizeof(uint32_t)));
  }
  ACVR_OverlayDepth(false);

  glBindVertexArray((GLuint)vao);
  glUseProgram((GLuint)prog);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)fb);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  glDepthMask(depthMask);
  glDepthFunc((GLenum)depthFunc);
  glClearDepth(clearDepth);
  if (!depth) glDisable(GL_DEPTH_TEST);
  if (cull) glEnable(GL_CULL_FACE);
  if (blend) glEnable(GL_BLEND);
  if (scissor) glEnable(GL_SCISSOR_TEST);
}

// ---------------------------------------------------------------- ACVR: laser and aim dot (head-tracked pointing)
// A thin beam from the right controller to where its ray meets the arcade view, and a dot there, drawn into each
// eye's picture with the same shader as the cabinet controls. Closed meshes: a capped cylinder and a sphere.
static GLuint  s_acvrPtrVao = 0, s_acvrPtrVbo = 0, s_acvrPtrIbo = 0;
static GLsizei s_acvrPtrCyl = 0, s_acvrPtrSph = 0;      // index counts: cylinder first, then the sphere

static bool ACVR_MakePointerMeshes()
{
  if (s_acvrPtrVao) return true;
  if (!ACVR_EnsureControlProgram()) return false;
  std::vector<float> v; std::vector<uint32_t> ix;
  auto vert = [&](float x, float y, float z, float nx, float ny, float nz) { v.insert(v.end(), { x, y, z, nx, ny, nz }); return (uint32_t)(v.size() / 6 - 1); };
  const int N = 16;
  // cylinder, radius 1, from z = 0 to z = 1, with both ends capped
  for (int i = 0; i < N; i++)
  {
    const float a0 = 6.2831853f * i / N, a1 = 6.2831853f * (i + 1) / N;
    const float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
    const uint32_t a = vert(c0, s0, 0, c0, s0, 0), b = vert(c1, s1, 0, c1, s1, 0), c = vert(c1, s1, 1, c1, s1, 0), d = vert(c0, s0, 1, c0, s0, 0);
    ix.insert(ix.end(), { a, b, c, a, c, d });
    const uint32_t m0 = vert(0, 0, 0, 0, 0, -1), e0 = vert(c1, s1, 0, 0, 0, -1), f0 = vert(c0, s0, 0, 0, 0, -1);
    ix.insert(ix.end(), { m0, e0, f0 });
    const uint32_t m1 = vert(0, 0, 1, 0, 0, 1), e1 = vert(c0, s0, 1, 0, 0, 1), f1 = vert(c1, s1, 1, 0, 0, 1);
    ix.insert(ix.end(), { m1, e1, f1 });
  }
  s_acvrPtrCyl = (GLsizei)ix.size();
  // sphere, radius 1
  const int R = 10;
  const uint32_t base = (uint32_t)(v.size() / 6);
  for (int i = 0; i <= R; i++)
    for (int j = 0; j <= N; j++)
    {
      const float th = 3.1415927f * i / R, ph = 6.2831853f * j / N;
      const float x = sinf(th) * cosf(ph), y = cosf(th), z = sinf(th) * sinf(ph);
      vert(x, y, z, x, y, z);
    }
  for (int i = 0; i < R; i++)
    for (int j = 0; j < N; j++)
    {
      const uint32_t a = base + i * (N + 1) + j, b = a + 1, c = a + (N + 1), d = c + 1;
      ix.insert(ix.end(), { a, c, b, b, c, d });
    }
  s_acvrPtrSph = (GLsizei)ix.size() - s_acvrPtrCyl;
  GLint vao = 0, ab = 0;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &ab);
  glGenVertexArrays(1, &s_acvrPtrVao); glGenBuffers(1, &s_acvrPtrVbo); glGenBuffers(1, &s_acvrPtrIbo);
  glBindVertexArray(s_acvrPtrVao);
  glBindBuffer(GL_ARRAY_BUFFER, s_acvrPtrVbo);
  glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_acvrPtrIbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, ix.size() * sizeof(uint32_t), ix.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const void *)(3 * sizeof(float)));
  glBindVertexArray((GLuint)vao);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)ab);
  return true;
}

static void ACVR_DrawPointer(int eye)
{
  static int laser = -1;
  if (laser < 0) { const char *e = getenv("ACVR_LASER"); laser = !(e && e[0] == '0'); }
  if (!laser || !g_acvrStereo.hudMatch || s_acvrAimFrame < s_acvrFrame - 1) return;
  if (!ACVR_MakePointerMeshes()) return;
  // reference space -> the recentred player frame, in game units (as the eye views are)
  const float ya = -s_acvrOriginYaw, ca = cosf(ya), sa = sinf(ya), u = s_acvrUnitsPerMetre;
  auto toPlayer = [&](const float p[3], float o[3]) {
    const float dx = p[0] - s_acvrOriginPos[0], dy = p[1] - s_acvrOriginPos[1], dz = p[2] - s_acvrOriginPos[2];
    o[0] = (dx * ca + dz * sa) * u; o[1] = dy * u; o[2] = (-dx * sa + dz * ca) * u;
  };
  float from[3], hit[3];
  toPlayer(s_acvrAimFrom, from); toPlayer(s_acvrAimHit, hit);
  float dir[3] = { hit[0] - from[0], hit[1] - from[1], hit[2] - from[2] };
  const float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
  if (!(len > 1e-3f)) return;
  for (int k = 0; k < 3; k++) dir[k] /= len;
  float side[3] = { dir[2], 0.0f, -dir[0] };                   // up x dir
  float sl = sqrtf(side[0] * side[0] + side[2] * side[2]);
  if (sl < 1e-4f) { side[0] = 1.0f; side[2] = 0.0f; sl = 1.0f; }
  side[0] /= sl; side[2] /= sl;
  const float up2[3] = { dir[1] * side[2] - dir[2] * side[1], dir[2] * side[0] - dir[0] * side[2], dir[0] * side[1] - dir[1] * side[0] };
  const float rb = 0.0015f * u, rd = 0.012f * u;               // beam and dot radius
  const float beam[16] = { side[0] * rb, side[1] * rb, side[2] * rb, 0, up2[0] * rb, up2[1] * rb, up2[2] * rb, 0,
                           dir[0] * len, dir[1] * len, dir[2] * len, 0, from[0], from[1], from[2], 1 };
  const float dot[16] = { rd, 0, 0, 0, 0, rd, 0, 0, 0, 0, rd, 0, hit[0], hit[1], hit[2], 1 };
  const float *tn = g_acvrStereo.eyeTan[eye];
  const float n = 0.05f * u, fa = 100.0f * u;
  const float l = tn[0] * n, rr = tn[1] * n, b = tn[2] * n, tp = tn[3] * n;
  const float proj[16] = { 2 * n / (rr - l), 0, 0, 0, 0, 2 * n / (tp - b), 0, 0,
                           (rr + l) / (rr - l), (tp + b) / (tp - b), -(fa + n) / (fa - n), -1, 0, 0, -2 * fa * n / (fa - n), 0 };
  GLint prog, vao, fb, vp[4]; GLboolean depth, cull, blend, scissor, depthMask; GLint depthFunc; GLfloat clearDepth;
  glGetIntegerv(GL_CURRENT_PROGRAM, &prog); glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao); glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb);
  glGetIntegerv(GL_VIEWPORT, vp);
  depth = glIsEnabled(GL_DEPTH_TEST); cull = glIsEnabled(GL_CULL_FACE); blend = glIsEnabled(GL_BLEND); scissor = glIsEnabled(GL_SCISSOR_TEST);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask); glGetIntegerv(GL_DEPTH_FUNC, &depthFunc); glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_acvrAATarget);
  ACVR_OverlayDepth(true);
  glViewport(0, 0, (GLsizei)s_acvrEyeW, (GLsizei)s_acvrEyeH);
  glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthFunc(GL_LESS); glClearDepth(1.0);
  glClear(GL_DEPTH_BUFFER_BIT);
  glUseProgram(s_acvrControlProg);
  glUniform1i(s_acvrCtlMoving, 0);
  glBindVertexArray(s_acvrPtrVao);
  float vm[16], mvp[16];
  ACVR_MatMul(g_acvrStereo.eyeView[eye], beam, vm); ACVR_MatMul(proj, vm, mvp);
  glUniformMatrix4fv(s_acvrCtlMvp, 1, GL_FALSE, mvp); glUniformMatrix4fv(s_acvrCtlModel, 1, GL_FALSE, beam);
  glUniform3f(s_acvrCtlColour, 1.0f, 0.12f, 0.08f);
  glDrawElements(GL_TRIANGLES, s_acvrPtrCyl, GL_UNSIGNED_INT, (const void *)0);
  ACVR_MatMul(g_acvrStereo.eyeView[eye], dot, vm); ACVR_MatMul(proj, vm, mvp);
  glUniformMatrix4fv(s_acvrCtlMvp, 1, GL_FALSE, mvp); glUniformMatrix4fv(s_acvrCtlModel, 1, GL_FALSE, dot);
  glUniform3f(s_acvrCtlColour, 1.0f, 0.95f, 0.2f);
  glDrawElements(GL_TRIANGLES, s_acvrPtrSph, GL_UNSIGNED_INT, (const void *)(size_t)(s_acvrPtrCyl * sizeof(uint32_t)));
  ACVR_OverlayDepth(false);
  glBindVertexArray((GLuint)vao); glUseProgram((GLuint)prog);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)fb); glViewport(vp[0], vp[1], vp[2], vp[3]);
  glDepthMask(depthMask); glDepthFunc((GLenum)depthFunc); glClearDepth(clearDepth);
  if (!depth) glDisable(GL_DEPTH_TEST);
  if (cull) glEnable(GL_CULL_FACE);
  if (blend) glEnable(GL_BLEND);
  if (scissor) glEnable(GL_SCISSOR_TEST);
}

void ACVR_StereoEyeDone(int eye)
{
  if (eye < 0 || eye > 1) return;
  ACVR_EnsureTargets();
  if (g_acvrStereo.vr) { ACVR_DrawControls(eye); ACVR_DrawPointer(eye); }
  const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
  glDisable(GL_SCISSOR_TEST);       // blits obey the scissor box
  glBindFramebuffer(GL_READ_FRAMEBUFFER, s_acvrAATarget);
  if (!s_acvrAATarget) glReadBuffer(GL_BACK);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_acvrEyeFbo[eye]);
  glBlitFramebuffer(0, 0, s_acvrEyeW, s_acvrEyeH, 0, 0, s_acvrEyeW, s_acvrEyeH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  if (eye == 0 && g_acvrStereo.vr)
  {
    // A strip from the upper middle of the picture, read back at the start of the next frame
    const GLint y = (GLint)(s_acvrEyeH * 0.72f), x = (GLint)(s_acvrEyeW / 2) - 64;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_acvrEyeFbo[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_acvrSkyFbo);
    glBlitFramebuffer(x, y, x + 128, y + 8, 0, 0, 16, 1, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    s_acvrSkySampled = true;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (scissor) glEnable(GL_SCISSOR_TEST);
}

// The 2D layers drawn between these two calls land in the HUD picture, on a transparent background.
// (The 2D renderer draws to whatever framebuffer is bound when supersampling is off.)
void ACVR_HudBegin()
{
  ACVR_EnsureTargets();
  // With supersampling on, the 2D renderer binds the supersampling target itself: draw there and copy after
  glBindFramebuffer(GL_FRAMEBUFFER, s_acvrAATarget ? s_acvrAATarget : s_acvrHudFbo);
  ACVR_ClearBound(0.0f);
}

void ACVR_HudEnd()
{
  if (s_acvrAATarget)
  {
    const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_acvrAATarget);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_acvrHudFbo);
    glBlitFramebuffer(0, 0, s_acvrEyeW, s_acvrEyeH, 0, 0, s_acvrEyeW, s_acvrEyeH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    if (scissor) glEnable(GL_SCISSOR_TEST);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  s_acvrHudDrawn = true;
}

void ACVR_StereoPresent()
{
  if (!s_acvrEyeTex[0]) return;
  const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
  glDisable(GL_SCISSOR_TEST);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glDrawBuffer(GL_BACK);
  const GLint half = (GLint)(totalXRes / 2);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, s_acvrEyeFbo[0]);
  glBlitFramebuffer(0, 0, s_acvrEyeW, s_acvrEyeH, 0, 0, half, totalYRes, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, s_acvrEyeFbo[1]);
  glBlitFramebuffer(0, 0, s_acvrEyeW, s_acvrEyeH, half, 0, totalXRes, totalYRes, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (scissor) glEnable(GL_SCISSOR_TEST);

  ACVRPublish p;
  p.eyeFbo[0] = s_acvrEyeFbo[0];
  p.eyeFbo[1] = s_acvrEyeFbo[1];
  p.hudFbo = s_acvrHudDrawn ? s_acvrHudFbo : 0;
  p.w = s_acvrEyeW;
  p.h = s_acvrEyeH;
  p.vr = g_acvrStereo.vr;
  p.views[0] = s_acvrViews[0];
  p.views[1] = s_acvrViews[1];
  p.originPos[0] = s_acvrOriginPos[0]; p.originPos[1] = s_acvrOriginPos[1]; p.originPos[2] = s_acvrOriginPos[2];
  p.originYaw = s_acvrOriginYaw;
  ACVR_XR_Publish(p);
}

static void ACVR_SaveBound(const std::string &file, unsigned w, unsigned h)
{
  std::shared_ptr<uint8_t> pixels(new uint8_t[w * h * 4], std::default_delete<uint8_t[]>());
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
  Util::WriteSurfaceToBMP<Util::RGBA8>(file, pixels.get(), w, h, true);
}

// Headset runs (ACVR_XR): called once per loop pass, after the inputs. Returns true when this pass must not run
// a game frame. When the runtime ended the game (SteamVR "Exit game", a lost runtime) or the presenter is gone
// (it could not start, or its thread ended), the game closes through the normal exit, which saves NVRAM. While
// the session is not running (starting up, or stopped by the runtime) the game waits, paused.
static bool ACVR_XR_Hold()
{
  static int last = 0;
  const int state = ACVR_XR_State();
  if (state != last)
  {
    static const char *what[] = { "off", "headset showing the game: running", "headset not showing the game: paused",
                                  "the headset runtime ended the game: closing", "the headset picture stopped: closing" };
    printf("ACVR XR: %s\n", what[state < 0 || state > 4 ? 0 : state]);
    fflush(stdout);
    last = state;
  }
  if (state == 3 || state == 4) { s_acvrQuit = true; return true; }
  if (state == 2) { SDL_Delay(20); return true; }
  return false;
}

// Called once per presented frame, before the buffer swap
static void ACVR_EndFrame()
{
  s_acvrFrame++;
  if (s_acvrSaveStateFrame >= 0 && s_acvrFrame == s_acvrSaveStateFrame)
    s_acvrWantSaveState = true;
  if (s_acvrCaptureFrames.empty() || std::find(s_acvrCaptureFrames.begin(), s_acvrCaptureFrames.end(), s_acvrFrame) == s_acvrCaptureFrames.end())
    return;
  const std::string tag = s_acvrCaptureFrames.size() > 1 ? ("-f" + std::to_string(s_acvrFrame)) : std::string();
  if (g_acvrStereo.enabled && s_acvrEyeTex[0])
  {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_acvrEyeFbo[0]);
    ACVR_SaveBound(s_acvrCaptureDir + "/eye-left" + tag + ".bmp", s_acvrEyeW, s_acvrEyeH);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_acvrEyeFbo[1]);
    ACVR_SaveBound(s_acvrCaptureDir + "/eye-right" + tag + ".bmp", s_acvrEyeW, s_acvrEyeH);
    if (s_acvrHudDrawn)
    {
      glBindFramebuffer(GL_READ_FRAMEBUFFER, s_acvrHudFbo);
      ACVR_SaveBound(s_acvrCaptureDir + "/hud" + tag + ".bmp", s_acvrEyeW, s_acvrEyeH);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  else
  {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    ACVR_SaveBound(s_acvrCaptureDir + "/mono" + tag + ".bmp", totalXRes, totalYRes);
  }
  if (s_acvrFrame >= *std::max_element(s_acvrCaptureFrames.begin(), s_acvrCaptureFrames.end()))
  {
    printf("ACVR capture written at frame %ld; quitting\n", s_acvrFrame);
    s_acvrQuit = true;
  }
}

