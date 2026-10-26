# SPDX-License-Identifier: GPL-3.0-or-later
# Applies the AladdinsCastle hold-to-quit to a pristine Visual Pinball X tree at the pinned commit (upstream-pin.json).
# Run from the VPX source root: python <this folder>/apply_acvr.py
# Every edit asserts its anchor, so a moved upstream fails loudly instead of building something half patched.
# Line endings follow each file (the VPX repository stores CRLF; a Linux checkout may have LF).
import json
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
SETUP = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(SETUP))
PIN = json.load(open(os.path.join(SETUP, 'upstream-pin.json'), encoding='utf-8'))


def rd(p):
    return open(p, encoding='utf-8', errors='surrogateescape', newline='').read()


def wr(p, s):
    open(p, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(s)


def nl_of(s):
    return '\r\n' if '\r\n' in s else '\n'


def rep(s, old, new, nl):
    old, new = old.replace('\n', nl), new.replace('\n', nl)
    n = s.count(old)
    assert n == 1, 'anchor count %d: %s' % (n, old[:80])
    return s.replace(old, new)


def edit(path, pairs):
    s = rd(path)
    nl = nl_of(s)
    for old, new in pairs:
        s = rep(s, old, new, nl)
    wr(path, s)


for p in ('src/input/XRInputHandler.h', 'src/input/InputManager.cpp', 'src/renderer/VRDevice.cpp', 'src/core/git_version.h'):
    assert os.path.isfile(p), 'not a VPX source root (missing %s)' % p
assert 'ACVR' not in rd('src/input/XRInputHandler.h'), 'already patched'

# ---------------------------------------------------------------- new files
# libacvr's chord timing, shared with every other host, and the pinball hold-to-quit logic built on it.
os.makedirs('src/input/acvr', exist_ok=True)
shutil.copyfile(os.path.join(REPO, 'libacvr', 'src', 'stick_chord.hpp'), 'src/input/acvr/stick_chord.hpp')
shutil.copyfile(os.path.join(HERE, 'acvr', 'HoldToQuit.h'), 'src/input/acvr/HoldToQuit.h')
shutil.copyfile(os.path.join(HERE, 'acvr', 'TableFollow.h'), 'src/input/acvr/TableFollow.h')

# ---------------------------------------------------------------- InputManager.cpp
# "Align VR view using controllers" (the Hub binds it to both stick clicks) becomes hold-only: the table follows the
# hands while the binding is held and stays put on release (U3p), instead of toggling on every press.
edit('src/input/InputManager.cpp', [
    ('#include "ui/live/LiveUI.h"\n',
     '#include "ui/live/LiveUI.h"\n'
     '#include "input/acvr/TableFollow.h" // ACVR: hold-only table adjust\n'),
    ('         if (isPressed && m_player->IsVR())\n'
     '            m_player->m_vrDevice->EnableControllerViewCentering(!m_player->m_vrDevice->IsControllerViewCenteringEnabled());\n',
     '         // ACVR (AladdinsCastle): the table follows the hands only while the binding is held; letting go keeps it (U3p).\n'
     '         if (m_player->IsVR())\n'
     '            m_player->m_vrDevice->EnableControllerViewCentering(\n'
     '               acvr::table_follows_hands(wasPressed, isPressed, m_player->m_vrDevice->IsControllerViewCenteringEnabled()));\n'),
])

# ---------------------------------------------------------------- XRInputHandler.h
edit('src/input/XRInputHandler.h', [
    ('#include <openxr/openxr.h>\n',
     '#include <openxr/openxr.h>\n'
     '\n'
     '#include <atomic>\n'
     '#include "input/acvr/HoldToQuit.h" // ACVR: AladdinsCastle hold-to-quit\n'),
    ('      m_pininput.SetDeviceDefaultMapping(m_joyId, defaultMapping);\n   }\n',
     '      m_pininput.SetDeviceDefaultMapping(m_joyId, defaultMapping);\n'
     '\n'
     '      AcvrInit();\n'
     '   }\n'),
    ('   ~XRInputHandler() override\n   {\n',
     '   ~XRInputHandler() override\n'
     '   {\n'
     '      for (XrSpace& space : m_acvrHandSpace)\n'
     '         if (space != XR_NULL_HANDLE)\n'
     '            xrDestroySpace(space);\n'
     '      if (m_acvrBaseSpace != XR_NULL_HANDLE)\n'
     '         xrDestroySpace(m_acvrBaseSpace);\n'),
    ('      }\n   }\n\nprivate:\n   void SuggestBindings(',
     '      }\n'
     '\n'
     '      AcvrUpdate();\n'
     '   }\n'
     '\n'
     'private:\n'
     '   void SuggestBindings('),
    ('private:\n\n   InputManager& m_pininput;\n',
     '   // ACVR (AladdinsCastle) hold-to-quit: hold both stick clicks for 3 s and the table closes through VPX\'s own\n'
     '   // Exit Game action, so NVRAM and settings are saved as on any exit. If either hand travels more than 5 cm\n'
     '   // during the hold (the hands are scaling, turning or moving the table), that hold does not quit.\n'
     '   static constexpr int kAcvrQuitting = 10;\n'
     '   static constexpr int kAcvrCancelled = 11;\n'
     '   // The renderer passes each frame\'s predicted display time; hand positions are read at that time.\n'
     '   void AcvrSetDisplayTime(XrTime time) { m_acvrDisplayTime.store(time); }\n'
     '   // What the headset should show, once per change: 1..3 = seconds left, kAcvrQuitting, kAcvrCancelled, 0 = nothing new.\n'
     '   int AcvrTakeNotice() { return m_acvrNotice.exchange(0); }\n'
     '\n'
     'private:\n'
     '   void AcvrInit()\n'
     '   {\n'
     '      m_acvrClick[0] = GetAction("/user/hand/left/input/thumbstick/click"s);\n'
     '      m_acvrClick[1] = GetAction("/user/hand/right/input/thumbstick/click"s);\n'
     '      const std::string poses[2] = { "/user/hand/left/input/grip/pose"s, "/user/hand/right/input/grip/pose"s };\n'
     '      for (int i = 0; i < 2; i++)\n'
     '      {\n'
     '         const XrAction pose = GetAction(poses[i]);\n'
     '         if (pose == XR_NULL_HANDLE)\n'
     '            continue;\n'
     '         XrActionSpaceCreateInfo info { XR_TYPE_ACTION_SPACE_CREATE_INFO };\n'
     '         info.action = pose;\n'
     '         info.poseInActionSpace = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } };\n'
     '         if (xrCreateActionSpace(m_session, &info, &m_acvrHandSpace[i]) != XR_SUCCESS)\n'
     '            m_acvrHandSpace[i] = XR_NULL_HANDLE;\n'
     '      }\n'
     '      // LOCAL is fixed to the room, so hand travel is measured in the room and not relative to the head.\n'
     '      XrReferenceSpaceCreateInfo ref { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };\n'
     '      ref.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;\n'
     '      ref.poseInReferenceSpace = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } };\n'
     '      if (xrCreateReferenceSpace(m_session, &ref, &m_acvrBaseSpace) != XR_SUCCESS)\n'
     '         m_acvrBaseSpace = XR_NULL_HANDLE;\n'
     '      m_acvrExitSlot = m_pininput.GetInputActions()[m_pininput.GetExitGameActionId()]->NewDirectStateSlot();\n'
     '      PLOGI << "[ACVR] Hold-to-quit ready: hold both stick clicks for 3 s to quit and save; moving a hand more than 5 cm during the hold cancels it";\n'
     '   }\n'
     '\n'
     '   bool AcvrClick(XrAction action) const\n'
     '   {\n'
     '      if (action == XR_NULL_HANDLE)\n'
     '         return false;\n'
     '      XrActionStateGetInfo getInfo { XR_TYPE_ACTION_STATE_GET_INFO, nullptr, action };\n'
     '      XrActionStateBoolean state { XR_TYPE_ACTION_STATE_BOOLEAN };\n'
     '      return xrGetActionStateBoolean(m_session, &getInfo, &state) == XR_SUCCESS && state.isActive && state.currentState;\n'
     '   }\n'
     '\n'
     '   void AcvrNotice(int notice)\n'
     '   {\n'
     '      // Only a change is sent, and never a 0, so a pending notice is not wiped before the renderer shows it.\n'
     '      if (notice != 0 && notice != m_acvrLastNotice)\n'
     '         m_acvrNotice.store(notice);\n'
     '      m_acvrLastNotice = notice;\n'
     '   }\n'
     '\n'
     '   void AcvrUpdate()\n'
     '   {\n'
     '      InputAction& exitGame = *m_pininput.GetInputActions()[m_pininput.GetExitGameActionId()];\n'
     '      if (m_acvrExitPressed)\n'
     '      {\n'
     '         // The exit was pressed for one input step, like a key tap; release it.\n'
     '         exitGame.SetDirectState(m_acvrExitSlot, false);\n'
     '         m_acvrExitPressed = false;\n'
     '      }\n'
     '      const bool left = AcvrClick(m_acvrClick[0]), right = AcvrClick(m_acvrClick[1]);\n'
     '      acvr::HandSample hands[2];\n'
     '      const XrTime time = m_acvrDisplayTime.load();\n'
     '      if (time != 0 && m_acvrBaseSpace != XR_NULL_HANDLE)\n'
     '      {\n'
     '         for (int i = 0; i < 2; i++)\n'
     '         {\n'
     '            if (m_acvrHandSpace[i] == XR_NULL_HANDLE)\n'
     '               continue;\n'
     '            XrSpaceLocation location { XR_TYPE_SPACE_LOCATION };\n'
     '            if (xrLocateSpace(m_acvrHandSpace[i], m_acvrBaseSpace, time, &location) == XR_SUCCESS && (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT))\n'
     '            {\n'
     '               hands[i].valid = true;\n'
     '               hands[i].x = location.pose.position.x;\n'
     '               hands[i].y = location.pose.position.y;\n'
     '               hands[i].z = location.pose.position.z;\n'
     '            }\n'
     '         }\n'
     '      }\n'
     '      const acvr::HoldToQuitStatus status = m_acvrHold.step(usec() / 1000ull, left, right, hands[0], hands[1]);\n'
     '      if (status.cancelled_now)\n'
     '      {\n'
     '         PLOGI << "[ACVR] Hold-to-quit cancelled: a hand moved " << static_cast<int>(status.travel_m * 100.f + 0.5f) << " cm (adjusting the table)";\n'
     '         AcvrNotice(kAcvrCancelled);\n'
     '      }\n'
     '      if (status.quit)\n'
     '      {\n'
     '         PLOGI << "[ACVR] Hold-to-quit: both stick clicks held for 3 s; closing the table through Exit Game (NVRAM and settings are saved as on any exit)";\n'
     '         exitGame.SetDirectState(m_acvrExitSlot, true);\n'
     '         m_acvrExitPressed = true;\n'
     '         AcvrNotice(kAcvrQuitting);\n'
     '      }\n'
     '      else if (status.visible && status.remaining_ms > 0)\n'
     '      {\n'
     '         const int seconds = static_cast<int>((status.remaining_ms + 999u) / 1000u);\n'
     '         if (seconds != m_acvrLastNotice)\n'
     '            PLOGI << "[ACVR] Hold-to-quit: " << seconds << " s left";\n'
     '         AcvrNotice(seconds);\n'
     '      }\n'
     '      else if (!status.chord)\n'
     '         m_acvrLastNotice = 0;\n'
     '   }\n'
     '\n'
     '   acvr::HoldToQuit m_acvrHold;\n'
     '   XrAction m_acvrClick[2] { XR_NULL_HANDLE, XR_NULL_HANDLE };\n'
     '   XrSpace m_acvrHandSpace[2] { XR_NULL_HANDLE, XR_NULL_HANDLE };\n'
     '   XrSpace m_acvrBaseSpace = XR_NULL_HANDLE;\n'
     '   std::atomic<XrTime> m_acvrDisplayTime { 0 };\n'
     '   std::atomic<int> m_acvrNotice { 0 };\n'
     '   int m_acvrLastNotice = 0;\n'
     '   int m_acvrExitSlot = -1;\n'
     '   bool m_acvrExitPressed = false;\n'
     '\n'
     'private:\n'
     '\n'
     '   InputManager& m_pininput;\n'),
])

# ---------------------------------------------------------------- VRDevice.cpp
edit('src/renderer/VRDevice.cpp', [
    ('#include "core/vpversion.h"\n',
     '#include "core/vpversion.h"\n'
     '#include "ui/live/LiveUI.h" // ACVR: hold-to-quit countdown in the headset\n'),
    ('   renderLayerInfo.predictedDisplayTime = frameState.predictedDisplayTime;\n',
     '   renderLayerInfo.predictedDisplayTime = frameState.predictedDisplayTime;\n'
     '\n'
     '   // ACVR (AladdinsCastle) hold-to-quit: the input handler reads the hands at this frame\'s display time;\n'
     '   // its countdown is shown here as a notification, which VPX draws in the headset.\n'
     '   if (m_xrInputHandler)\n'
     '   {\n'
     '      m_xrInputHandler->AcvrSetDisplayTime(frameState.predictedDisplayTime);\n'
     '      static unsigned int acvrNoticeId = 0;\n'
     '      const int notice = m_xrInputHandler->AcvrTakeNotice();\n'
     '      if (notice != 0 && g_pplayer && g_pplayer->m_liveUI)\n'
     '      {\n'
     '         const std::string text = notice == XRInputHandler::kAcvrQuitting ? std::string("Quitting and saving...")\n'
     '            : notice == XRInputHandler::kAcvrCancelled                    ? std::string("Quit cancelled: your hands moved the table")\n'
     '                                                                          : "Keep holding both stick clicks to quit: " + std::to_string(notice) + " s";\n'
     '         acvrNoticeId = g_pplayer->m_liveUI->PushNotification(text, notice >= XRInputHandler::kAcvrQuitting ? 2500 : 1200, acvrNoticeId);\n'
     '      }\n'
     '   }\n'),
])

# ---------------------------------------------------------------- git_version.h
# The build reports the owner's version (10.8.1, revision 5436) with "-acvr" after the commit, so the program's
# own version information says which build it is (ProductVersion 10.8.1.5436.af26b2d93-acvr).
edit('src/core/git_version.h', [
    ('#define GIT_REVISION 9999\n', '#define GIT_REVISION %d\n' % PIN['revision']),
    ('#define GIT_SHA      "unknown"\n', '#define GIT_SHA      "%s-acvr"\n' % PIN['source_commit'][:9]),
])

print('acvr applied')
