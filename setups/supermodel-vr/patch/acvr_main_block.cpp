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
******************************************************************************/

ACVRStereoState g_acvrStereo;

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

// Called right after the inputs are polled: VR controllers as cabinet controls. The layout depends on the
// kind of cabinet. Only writes an input while its controller control is in use, so the keyboard and gamepad
// keep working.
//
//   Driving     hold both grips and turn your hands like a wheel (or left thumbstick); right trigger =
//               accelerator, left trigger = brake; right thumbstick up/down = shift up/down;
//               A = start, B = coin, X = view change / VR1, Y = VR2 / hand brake, stick clicks = VR3 / VR4
//   Pointing    (analog joystick, Star Wars Trilogy) where the right controller points = the stick;
//               right trigger = its trigger, right grip = its event button; A = start, B = coin
//   Stick games left thumbstick = joystick; A, B, X, Y = the four buttons;
//               menu button or right stick click = start (in Virtua Fighter 3 start also switches to the
//               first-person view during a fight); left stick click = coin
static void ACVR_ApplyVRInputs(CInputs *inputs, uint32_t gameInputs)
{
  ACVRControllers c;
  if (!ACVR_XR_GetControllers(c))
    return;
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

    press("Start1", c.a || c.menu);
    press("Coin1", c.b);
    press("ViewChange", c.x); press("VR1", c.x);
    press("VR2", c.y); press("HandBrake", c.y);
    press("VR3", c.stickClick[0]);
    press("VR4", c.stickClick[1]);
    press("GearShiftUp", c.stick[1][1] > 0.7f);
    press("GearShiftDown", c.stick[1][1] < -0.7f);
  }
  else if (gameInputs & Game::INPUT_ANALOG_JOYSTICK)
  {
    // The analog stick by pointing the right controller: 25 degrees to a side or 20 up or down is full deflection
    if (c.aimValid[1])
    {
      const float *q = c.aimRot[1];
      const float fx = -2.0f * (q[0] * q[2] + q[3] * q[1]);
      const float fy = -2.0f * (q[1] * q[2] - q[3] * q[0]);
      const float fz = -(1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]));
      const float rx = fx * ca + fz * sa, rz = -fx * sa + fz * ca;   // pointing direction in the player's frame
      const float side = atan2f(rx, -rz);                              // right is positive
      const float up = asinf(clampf(fy, -1.0f, 1.0f));
      set("AnalogJoyX", (int)clampf(128.0f + side / (25.0f * 0.01745329f) * 127.0f, 0.0f, 255.0f));
      set("AnalogJoyY", (int)clampf(128.0f - up / (20.0f * 0.01745329f) * 127.0f, 0.0f, 255.0f));
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

void ACVR_StereoEyeDone(int eye)
{
  if (eye < 0 || eye > 1) return;
  ACVR_EnsureTargets();
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

