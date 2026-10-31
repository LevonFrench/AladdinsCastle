# Applies the ACVR stereo and VR changes to a pristine Supermodel tree (commit 8b4de23).
# Run from the Supermodel source root. Each edit asserts its anchor, so a changed upstream fails loudly.
import os, shutil

HERE = os.path.dirname(os.path.abspath(__file__))


def rd(p):
    return open(p, encoding='utf-8', errors='surrogateescape', newline='').read()


def wr(p, s):
    open(p, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(s)


def rep(s, old, new, count=1):
    n = s.count(old)
    assert n >= 1, 'missing anchor: ' + old[:70]
    assert count == 0 or n == count, 'anchor count %d: %s' % (n, old[:70])
    return s.replace(old, new)


def nl_of(s):
    return '\r\n' if '\r\n' in s else '\n'


# ---------------------------------------------------------------- new files
shutil.copyfile(os.path.join(HERE, 'ACVRXR.cpp'), 'Src/OSD/SDL/ACVRXR.cpp')
shutil.copyfile(os.path.join(HERE, 'ACVRStereo.h'), 'Src/OSD/ACVRStereo.h')
shutil.copyfile(os.path.join(HERE, 'ACVRChord.h'), 'Src/OSD/ACVRChord.h')
# The U4 hold ring (libacvr, MIT): one copy of the shared headers, so the ring looks and times the same as acvr-ps1.
LIBACVR = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'libacvr', 'src'))
os.makedirs('Src/OSD/acvr', exist_ok=True)
for name in ('stick_chord.hpp', 'hold_ring.hpp', 'hold_ring_gl.hpp', 'hold_ring_host.hpp'):
    if not os.path.isfile(os.path.join(LIBACVR, name)):
        raise SystemExit('libacvr header missing (run this script from a repository checkout): ' + name)
    shutil.copyfile(os.path.join(LIBACVR, name), os.path.join('Src/OSD/acvr', name))

# ---------------------------------------------------------------- New3D.cpp
p = 'Src/Graphics/New3D/New3D.cpp'
s = rd(p); nl = nl_of(s)
s = rep(s, '#include "Util/BitCast.h"', '#include "Util/BitCast.h"' + nl + '#include "OSD/ACVRStereo.h"')
s = rep(s,
        '\t{' + nl + '\t\tstd::lock_guard<std::mutex> guard(m_losMutex);',
        '\tif (!(g_acvrStereo.enabled && g_acvrStereo.eye == 1))\t// ACVR: the second eye must not swap the line-of-sight buffers again' + nl +
        '\t{' + nl + '\t\tstd::lock_guard<std::mutex> guard(m_losMutex);')
s = rep(s,
        '\tvp->projectionMatrix.LoadIdentity();\t// reset matrix' + nl,
        '\tvp->projectionMatrix.LoadIdentity();\t// reset matrix' + nl + nl +
        '\tconst bool acvrFullScreen = (vp->vpX == 0 && vp->vpWidth >= 495 && vp->vpY == 0 && vp->vpHeight >= 383);' + nl +
        '\t// A scene view is any full-width viewport, whatever its height: games letterbox their scenes (Star Wars Trilogy) and draw the sky in its own thin strip (Virtua Fighter 3)' + nl +
        '\tconst bool acvrMainView = (vp->vpWidth >= 400);' + nl +
        '\t// A full-width scene view of at least half the height (letterboxed ones included, thin sky strips not): its' + nl +
        '\t// frustum, carried on to the edges of the whole 496x384 picture, is the field of view of the picture' + nl +
        '\tconst bool acvrSceneView = (vp->vpX == 0 && vp->vpWidth >= 495 && vp->vpHeight >= 200);' + nl +
        '\tauto acvrRecordTan = [&](float L, float R) {' + nl +
        '\t\tif (g_acvrStereo.eye != 0) return;' + nl +
        '\t\tconst float perLine = (t - b) / vp->vpHeight;' + nl +
        '\t\tg_acvrStereo.tanL = L; g_acvrStereo.tanR = R;' + nl +
        '\t\tg_acvrStereo.tanT = t + vp->vpY * perLine;' + nl +
        '\t\tg_acvrStereo.tanB = b - (384.0f - vp->vpY - vp->vpHeight) * perLine;' + nl +
        '\t};' + nl + nl +
        '\tif (g_acvrStereo.debugViews && g_acvrStereo.eye == 0)' + nl +
        '\t\tprintf("[ACVR view] pri=%d x=%g y=%g w=%g h=%g  l=%.3f r=%.3f b=%.3f t=%.3f  scrollFog=%g\\n", vp->priority, vp->vpX, vp->vpY, vp->vpWidth, vp->vpHeight, l, r, b, t, vp->scrollFog);' + nl + nl +
        '\tif (g_acvrStereo.vr) {' + nl +
        '\t\t// ACVR head-tracked view: the main 3D viewports are drawn for the headset\'s own eye, over the' + nl +
        '\t\t// whole target. Small viewports (mirrors, HUD parts) are not drawn in the eye pictures yet.' + nl +
        '\t\tif (acvrSceneView) acvrRecordTan(l, r);' + nl +
        '\t\tif (acvrMainView) {' + nl +
        '\t\t\tconst float *e = g_acvrStereo.eyeTan[g_acvrStereo.eye];' + nl +
        '\t\t\tvp->x = 0; vp->y = 0; vp->width = m_totalXRes; vp->height = m_totalYRes;' + nl +
        '\t\t\tvp->projectionMatrix.FrustumRZ(e[0], e[1], e[2], e[3], NEAR_PLANE);' + nl +
        '\t\t\tvp->projectionMatrix.MultMatrix(g_acvrStereo.eyeView[g_acvrStereo.eye]);' + nl +
        '\t\t}' + nl +
        '\t\telse {' + nl +
        '\t\t\tvp->x = 0; vp->y = 0; vp->width = 0; vp->height = 0;' + nl +
        '\t\t\tvp->projectionMatrix.FrustumRZ(l, r, b, t, NEAR_PLANE);' + nl +
        '\t\t}' + nl +
        '\t\treturn;' + nl +
        '\t}' + nl + nl +
        '\tif (g_acvrStereo.fovScale != 1.0f && acvrFullScreen) {' + nl +
        '\t\tl *= g_acvrStereo.fovScale; r *= g_acvrStereo.fovScale; t *= g_acvrStereo.fovScale; b *= g_acvrStereo.fovScale;\t// ACVR: wider camera' + nl +
        '\t}' + nl + nl +
        '\t// ACVR screen-mode stereo: parallel cameras with off-axis frusta. The eye sits halfSep to one side; the' + nl +
        '\t// frustum is shifted so that the plane at the convergence distance has no disparity. l and r are tangents.' + nl +
        '\tfloat eyeMove = 0.0f, eyeShift = 0.0f;' + nl +
        '\tif (g_acvrStereo.enabled) {' + nl +
        '\t\teyeMove = (g_acvrStereo.eye == 0) ? g_acvrStereo.halfSep : -g_acvrStereo.halfSep;' + nl +
        '\t\tif (g_acvrStereo.convergence > 0.0f) eyeShift = eyeMove / g_acvrStereo.convergence;' + nl +
        '\t}' + nl)
s = rep(s,
        '\t\tvp->projectionMatrix.FrustumRZ(l*correction, r*correction, b, t, NEAR_PLANE);',
        '\t\tif (acvrSceneView) acvrRecordTan(l*correction, r*correction);' + nl +
        '\t\tvp->projectionMatrix.FrustumRZ(l*correction + eyeShift, r*correction + eyeShift, b, t, NEAR_PLANE);' + nl +
        '\t\tif (eyeMove != 0.0f) vp->projectionMatrix.Translate(eyeMove, 0.0f, 0.0f);')
s = rep(s,
        '\t\tvp->projectionMatrix.FrustumRZ(l, r, b, t, NEAR_PLANE);' + nl + '\t}' + nl + '}',
        '\t\tif (acvrSceneView) acvrRecordTan(l, r);' + nl +
        '\t\tvp->projectionMatrix.FrustumRZ(l + eyeShift, r + eyeShift, b, t, NEAR_PLANE);' + nl +
        '\t\tif (eyeMove != 0.0f) vp->projectionMatrix.Translate(eyeMove, 0.0f, 0.0f);' + nl + '\t}' + nl + '}')
s = rep(s,
        '\tif (m_wideScreen && (vp->vpX == 0) && (vp->vpWidth >= 495) && (vp->vpY == 0) && (vp->vpHeight >= 383)) {',
        '\t// ACVR: with ACVR_WIDE_ALL (on by default) letterboxed scenes and sky strips are widened as well' + nl +
        '\tif (m_wideScreen && (vp->vpX == 0) && (vp->vpWidth >= 495) && (((vp->vpY == 0) && (vp->vpHeight >= 383)) || g_acvrStereo.wideAll)) {')
s = rep(s,
        '\t\toutsideFrustum = true;' + nl + '\t}' + nl,
        '\t\toutsideFrustum = true;' + nl + '\t}' + nl +
        '\tif (g_acvrStereo.noCull) outsideFrustum = false;\t// ACVR: keep what the game would have dropped' + nl)
wr(p, s)

# ---------------------------------------------------------------- Render2D.cpp
p = 'Src/Graphics/Render2D.cpp'
s = rd(p); nl = nl_of(s)
s = rep(s, '#include "Render2D.h"', '#include "Render2D.h"' + nl + '#include "OSD/ACVRStereo.h"')
s = rep(s,
        '\t// Clear everything if requested or just overscan areas for wide screen mode' + nl + '\tif (isBottom)' + nl + '\t{' + nl,
        '\tif (isBottom && g_acvrStereo.vr && g_acvrStereo.backdrop)' + nl +
        '\t{' + nl +
        '\t\t// ACVR head-tracked: the bottom layer is a far backdrop. Clear the whole target to its top-edge colour' + nl +
        '\t\t// and draw the layer only where the arcade view lies in this eye picture.' + nl +
        '\t\tif (m_aaTarget) glBindFramebuffer(GL_FRAMEBUFFER, m_aaTarget);' + nl +
        '\t\tglClearColor(g_acvrStereo.backdropClear[0], g_acvrStereo.backdropClear[1], g_acvrStereo.backdropClear[2], 1.0f);' + nl +
        '\t\tglDisable(GL_SCISSOR_TEST);' + nl +
        '\t\tglClear(GL_COLOR_BUFFER_BIT);' + nl +
        '\t\tglEnable(GL_SCISSOR_TEST);' + nl +
        '\t\tif (m_aaTarget) glBindFramebuffer(GL_FRAMEBUFFER, 0);' + nl +
        '\t\tconst int *rect = g_acvrStereo.backdropRect[g_acvrStereo.eye];' + nl +
        '\t\tglViewport(rect[0], rect[1], rect[2], rect[3]);' + nl +
        '\t\treturn;' + nl +
        '\t}' + nl + nl +
        '\t// Clear everything if requested or just overscan areas for wide screen mode' + nl + '\tif (isBottom)' + nl + '\t{' + nl)
wr(p, s)

# ---------------------------------------------------------------- Model3.cpp
p = 'Src/Model3/Model3.cpp'
s = rd(p); nl = nl_of(s)
s = rep(s, '#include "OSD/Video.h"', '#include "OSD/Video.h"' + nl + '#include "OSD/ACVRStereo.h"')
old = ('    // Render frame' + nl +
       '    TileGen.BeginFrame();' + nl +
       '    GPU.BeginFrame();' + nl +
       '    TileGen.PreRenderFrame();' + nl +
       '    TileGen.RenderFrameBottom();' + nl +
       '    GPU.RenderFrame();' + nl +
       '    TileGen.RenderFrameTop();' + nl +
       '    GPU.EndFrame();' + nl +
       '    TileGen.EndFrame();' + nl +
       '    m_superAA->Draw();' + nl)
new = ('    // Render frame (ACVR: once per eye when stereo is on; in head-tracked mode the 2D layers are kept out' + nl +
       '    // of the eye pictures and drawn once into the HUD picture)' + nl +
       '    const int eyes = g_acvrStereo.enabled ? 2 : 1;' + nl +
       '    if (g_acvrStereo.enabled)' + nl +
       '      ACVR_FrameBegin();' + nl +
       '    if (g_acvrStereo.vr)' + nl +
       '    {' + nl +
       '      ACVR_BackdropBegin();' + nl +
       '      TileGen.BeginFrame();' + nl +
       '      TileGen.PreRenderFrame();' + nl +
       '      TileGen.RenderFrameBottom();' + nl +
       '      ACVR_BackdropEnd();' + nl +
       '    }' + nl +
       '    for (int eye = 0; eye < eyes; eye++)' + nl +
       '    {' + nl +
       '      g_acvrStereo.eye = eye;' + nl +
       '      TileGen.BeginFrame();' + nl +
       '      GPU.BeginFrame();' + nl +
       '      TileGen.PreRenderFrame();' + nl +
       '      if (!ACVR_SkipBottomLayer())' + nl +
       '        TileGen.RenderFrameBottom();' + nl +
       '      GPU.RenderFrame();' + nl +
       '      if (!g_acvrStereo.vr)' + nl +
       '        TileGen.RenderFrameTop();' + nl +
       '      GPU.EndFrame();' + nl +
       '      TileGen.EndFrame();' + nl +
       '      m_superAA->Draw();' + nl +
       '      if (g_acvrStereo.enabled)' + nl +
       '        ACVR_StereoEyeDone(eye);' + nl +
       '    }' + nl +
       '    g_acvrStereo.eye = 0;' + nl +
       '    if (g_acvrStereo.vr)' + nl +
       '    {' + nl +
       '      ACVR_HudBegin();' + nl +
       '      TileGen.RenderFrameTop();' + nl +
       '      ACVR_HudEnd();' + nl +
       '    }' + nl +
       '    if (g_acvrStereo.enabled)' + nl +
       '      ACVR_StereoPresent();' + nl)
s = rep(s, old, new)
wr(p, s)

# ---------------------------------------------------------------- Main.cpp
p = 'Src/OSD/SDL/Main.cpp'
s = rd(p); nl = nl_of(s)
s = rep(s, '#include "Gui.h"',
        '#include "Gui.h"' + nl + '#include "OSD/ACVRStereo.h"' + nl + '#include "OSD/ACVRChord.h"' + nl + '#include <cstdlib>' + nl + '#include <string>')
s = rep(s,
        'SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | (fullScreen ? SDL_WINDOW_FULLSCREEN : 0));',
        'SDL_WINDOW_OPENGL | (getenv("ACVR_HIDDEN") ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN) | (fullScreen ? SDL_WINDOW_FULLSCREEN : 0));')
s = rep(s, '  if (focusWindow)' + nl + '  {', '  if (focusWindow && !getenv("ACVR_HIDDEN"))' + nl + '  {')
block = open(os.path.join(HERE, 'acvr_main_block.cpp'), encoding='utf-8').read()
block = block.replace('\r\n', '\n').replace('\n', nl)
s = rep(s, 'static void SaveFrameBuffer(const std::string& file)', block + 'static void SaveFrameBuffer(const std::string& file)')
s = rep(s,
        '  // Swap the buffers' + nl + '  SDL_GL_SwapWindow(s_window);' + nl + '}',
        '  ACVR_EndFrame();' + nl + nl + '  // Swap the buffers' + nl + '  SDL_GL_SwapWindow(s_window);' + nl + '}')
s = rep(s,
        '  while (!quit)' + nl + '  {' + nl + '    // Poll the inputs',
        '  while (!quit && !s_acvrQuit)' + nl + '  {' + nl + '    // Poll the inputs')
s = rep(s,
        '    if (!Inputs->Poll(&game, xOffset, yOffset, xRes, yRes))' + nl + '      quit = true;' + nl,
        '    if (!Inputs->Poll(&game, xOffset, yOffset, xRes, yRes))' + nl + '      quit = true;' + nl +
        '    ACVR_ApplyInputScript(Inputs);' + nl +
        '    ACVR_ApplyVRInputs(Inputs, game.inputs);' + nl +
        '    const bool acvrHold = ACVR_XR_Hold();\t// headset: closed by the runtime, or not showing the game' + nl)
s = rep(s,
        '    if (paused)' + nl + '      Model3->RenderFrame();' + nl + '    else' + nl + '      Model3->RunFrame();' + nl,
        '    if (acvrHold) { }	// no game frame this pass' + nl + '    else if (paused)' + nl + '      Model3->RenderFrame();' + nl + '    else' + nl + '      Model3->RunFrame();' + nl +
        '    if (s_acvrWantSaveState) { SaveState(Model3); s_acvrWantSaveState = false; }' + nl)
s = rep(s, 'superAA->Init(totalXRes, totalYRes);', 'superAA->Init(totalXRes, totalYRes); s_acvrAATarget = superAA->GetTargetID();', count=0)
s = rep(s, '  auto cmd_line = ParseCommandLine(argc, argv);',
        '  auto cmd_line = ParseCommandLine(argc, argv);' + nl + '  ACVR_Init();')
s = rep(s,
        '  // Emulate!' + nl,
        '  if (getenv("ACVR_XR"))' + nl + '    ACVR_XR_Start(s_window);' + nl + nl + '  // Emulate!' + nl)
# The XR thread stops after NVRAM is saved, so a slow or hung XR join can never skip the save
s = rep(s,
        '  // Save NVRAM' + nl + '  SaveNVRAM(Model3);' + nl,
        '  // Save NVRAM' + nl + '  SaveNVRAM(Model3);' + nl + nl +
        '  // The XR thread stops only after NVRAM is safe (its join is bounded to 3 s)' + nl + '  ACVR_XR_Stop();' + nl)
wr(p, s)

# ---------------------------------------------------------------- project file
p = 'MSVC/Supermodel.vcxproj'
s = rd(p); nl = nl_of(s)
s = rep(s, '    <ClCompile Include="..\\Src\\OSD\\SDL\\Main.cpp" />',
        '    <ClCompile Include="..\\Src\\OSD\\SDL\\Main.cpp" />' + nl + '    <ClCompile Include="..\\Src\\OSD\\SDL\\ACVRXR.cpp" />')
s = rep(s, '..\\Libraries\\SDL2_net-2.4.0\\include;', '..\\Libraries\\SDL2_net-2.4.0\\include;..\\Libraries\\openxr\\include;', count=0)
wr(p, s)
print('acvr applied')
