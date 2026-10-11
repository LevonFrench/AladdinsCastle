#!/usr/bin/env python
"""Apply the facing/presence delta to a pinned, owned private source clone."""
import argparse
import hashlib
from pathlib import Path
import subprocess

PIN = '85c4266b29eb308d173fefa0f21f5cb77df3516a'


def replace(text, old, new):
    if text.count(old) != 1:
        raise SystemExit('Patch anchor missing or ambiguous: ' + old[:100])
    return text.replace(old, new)


def function(text, start, end, edit):
    first = text.index(start)
    last = text.index(end, first + len(start))
    return text[:first] + edit(text[first:last]) + text[last:]


def patch_header(text):
    text = replace(text, '#include "VideoCommon/AbstractFramebuffer.h"',
                   '#include "VideoCommon/AbstractFramebuffer.h"\n#include "VideoCommon/VR/ACFacing.h"')
    text = replace(text, '  void EnsureHomePositionFromCurrentViews() const;',
                   '  void EnsureHomePositionFromCurrentViews() const;\n'
                   '  struct HomeAnchor { XrVector3f position; float yaw; };\n'
                   '  HomeAnchor GetHomeAnchor() const;')
    return replace(text, '  mutable bool m_home_set{false};',
                   '  bool m_system_supports_user_presence = false;\n'
                   '  mutable std::mutex m_home_mutex;\n'
                   '  ACFacing::State m_facing;\n'
                   '  mutable bool m_home_set{false};')


def patch_manager(text):
    text = replace(text, '#include "Core/Config/GraphicsSettings.h"',
                   '#include "Core/Config/GraphicsSettings.h"\n#include "Core/System.h"\n'
                   '#include "VideoCommon/GeometryShaderManager.h"')
    text = replace(text, '  std::vector<const char*> enabled_extensions(extra_extensions);',
                   '  std::vector<const char*> enabled_extensions(extra_extensions);\n'
                   '  if (runtime_has(XR_EXT_USER_PRESENCE_EXTENSION_NAME))\n'
                   '    enabled_extensions.push_back(XR_EXT_USER_PRESENCE_EXTENSION_NAME);')
    text = replace(text, '  xrGetSystemProperties(m_instance, m_system_id, &props);',
                   '  XrSystemUserPresencePropertiesEXT presence{XR_TYPE_SYSTEM_USER_PRESENCE_PROPERTIES_EXT};\n'
                   '  const bool presence_enabled = IsExtensionEnabled(XR_EXT_USER_PRESENCE_EXTENSION_NAME);\n'
                   '  if (presence_enabled)\n  {\n    presence.next = props.next;\n'
                   '    props.next = &presence;\n  }\n'
                   '  const XrResult properties_result = xrGetSystemProperties(m_instance, m_system_id, &props);\n'
                   '  m_system_supports_user_presence = presence_enabled && XR_SUCCEEDED(properties_result) &&\n'
                   '      presence.supportsUserPresence == XR_TRUE;\n'
                   '  INFO_LOG_FMT(OPENXR, "OpenXR: User presence extension={} system_support={}; facing mode={}",\n'
                   '      presence_enabled ? "enabled" : "unavailable",\n'
                   '      m_system_supports_user_presence ? "yes" : "no",\n'
                   '      m_system_supports_user_presence ? "presence event then focused valid pose" :\n'
                   '      "controller-confirmed recenter required after wearing");')
    text = replace(text, '  m_session = session;\n',
                   '  m_session = session;\n  {\n    std::lock_guard lock(m_home_mutex);\n'
                   '    m_facing.ResetSession(m_system_supports_user_presence);\n  }\n')
    reset = '  m_home_set = false;\n  m_home_position = {0.f, 0.f, 0.f};'
    text = function(text, 'void OpenXRManager::DestroySession()',
                    'void OpenXRManager::PublishFrame', lambda body: replace(body, reset,
                    '  {\n    std::lock_guard lock(m_home_mutex);\n'
                    '    m_home_set = false;\n    m_home_position = {0.f, 0.f, 0.f};\n'
                    '    m_facing.ResetSession(m_system_supports_user_presence);\n  }'))
    text = function(text, 'bool OpenXRManager::CreateReferenceSpace()',
                    'bool OpenXRManager::PollEvents()', lambda body: replace(body, reset,
                    '  {\n    std::lock_guard lock(m_home_mutex);\n'
                    '    m_home_set = false;\n    m_home_position = {0.f, 0.f, 0.f};\n'
                    '    m_facing.ResetAnchor();\n  }'))
    text = replace(text, '    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:',
                   '    case XR_TYPE_EVENT_DATA_USER_PRESENCE_CHANGED_EXT:\n    {\n'
                   '      const auto& ev = *reinterpret_cast<const XrEventDataUserPresenceChangedEXT*>(&event);\n'
                   '      if (ev.session == m_session && m_system_supports_user_presence)\n      {\n'
                   '        std::lock_guard lock(m_home_mutex);\n'
                   '        m_facing.ObservePresence(ev.isUserPresent == XR_TRUE);\n'
                   '        INFO_LOG_FMT(OPENXR, "OpenXR: User presence={} facing_pending={}",\n'
                   '            ev.isUserPresent == XR_TRUE ? "present" : "absent", m_facing.Pending());\n'
                   '      }\n      break;\n    }\n'
                   '    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:')
    first = text.index('  if (m_recenter_requested.exchange(false, std::memory_order_acq_rel) && view_count >= 2)')
    last = text.index('\n  return true;', first)
    text = text[:first] + '''  bool facing_changed = false;
  {
    std::lock_guard lock(m_home_mutex);
    if (m_recenter_requested.exchange(false, std::memory_order_acq_rel))
      m_facing.Request();
    const XrVector3f center{
        0.5f * (m_eye_views[0].pose.position.x + m_eye_views[1].pose.position.x),
        0.5f * (m_eye_views[0].pose.position.y + m_eye_views[1].pose.position.y),
        0.5f * (m_eye_views[0].pose.position.z + m_eye_views[1].pose.position.z)};
    const auto& q = m_eye_views[0].pose.orientation;
    facing_changed = m_facing.Apply(m_session_focused.load(std::memory_order_acquire),
        (m_view_state_flags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0,
        (m_view_state_flags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0,
        view_count >= 2, {center.x, center.y, center.z}, {q.x, q.y, q.z, q.w});
    if (facing_changed)
    {
      m_home_position = center;
      m_home_set = true;
      m_flat_screen_pose_valid = false;
      INFO_LOG_FMT(OPENXR, "OpenXR: Recentered position=({:.4f},{:.4f},{:.4f}) yaw={:.4f}",
          center.x, center.y, center.z, m_facing.Yaw());
    }
  }
  if (facing_changed)
  {
    Core::System::GetInstance().GetGeometryShaderManager().InvalidateVRHeadPose();
    InvalidateControllerAnchorCache();
  }
''' + text[last:]
    text = replace(text, 'void OpenXRManager::EnsureHomePositionFromCurrentViews() const\n{',
                   'OpenXRManager::HomeAnchor OpenXRManager::GetHomeAnchor() const\n{\n'
                   '  std::lock_guard lock(m_home_mutex);\n'
                   '  return {m_home_position, m_facing.Yaw()};\n}\n\n'
                   'void OpenXRManager::EnsureHomePositionFromCurrentViews() const\n{\n'
                   '  std::lock_guard lock(m_home_mutex);')
    def tracking(body):
        body = replace(body, '  EnsureHomePositionFromCurrentViews();',
                       '  EnsureHomePositionFromCurrentViews();\n  const auto home = GetHomeAnchor();')
        return body.replace('m_home_position.', 'home.position.')
    text = function(text, 'std::array<XREyeView, 2> OpenXRManager::GetTrackingAdjustedEyeViews()',
                    '\nnamespace\n{\nconstexpr std::array<float, 9> CAMERA_ANCHOR_IDENTITY', tracking)
    def aim(body):
        body = replace(body, '  const float s = std::max(units_per_meter, 0.0001f);',
                       '  const auto home = GetHomeAnchor();\n'
                       '  const float s = std::max(units_per_meter, 0.0001f);')
        body = body.replace('m_home_position.', 'home.position.')
        body = replace(body, '  if (g_ActiveConfig.vr_enable_camera_height)',
                       '  const auto p = ACFacing::ToGame(ACFacing::Vector{px, py, pz}, home.yaw);\n'
                       '  px = p[0]; py = p[1]; pz = p[2];\n'
                       '  if (g_ActiveConfig.vr_enable_camera_height)')
        body = replace(body, '  if (m_camera_anchor_rotation_active)\n  {\n    const std::array<float, 9>& A = m_camera_anchor_rotation;\n    const std::array<float, 9> c = rot;',
                       '  rot = ACFacing::ToGame(rot, home.yaw);\n'
                       '  if (m_camera_anchor_rotation_active)\n  {\n    const std::array<float, 9>& A = m_camera_anchor_rotation;\n    const std::array<float, 9> c = rot;')
        return body
    text = function(text, 'bool OpenXRManager::MapAimPoseToGameView(',
                    'void OpenXRManager::ComputeVirtualScreenHit(', aim)
    def eyes(body):
        body = replace(body, '  const std::array<XREyeView, 2> eye_views = GetTrackingAdjustedEyeViews();',
                       '  const std::array<XREyeView, 2> eye_views = GetTrackingAdjustedEyeViews();\n'
                       '  const auto home = GetHomeAnchor();\n'
                       '  // None mode already synthesizes eyes in the fixed game basis.\n'
                       '  const float yaw = g_ActiveConfig.vr_tracking_mode == OpenXRTrackingMode::None ?\n'
                       '      0.0f : home.yaw;')
        body = replace(body, '    float m00 = r00, m01 = r01, m02 = r02;\n    float m10 = r10, m11 = r11, m12 = r12;\n    float m20 = r20, m21 = r21, m22 = r22;',
                       '    const auto rig = ACFacing::ToGame(ACFacing::Rotation{\n'
                       '        r00, r01, r02, r10, r11, r12, r20, r21, r22}, yaw);\n'
                       '    float m00 = rig[0], m01 = rig[1], m02 = rig[2];\n'
                       '    float m10 = rig[3], m11 = rig[4], m12 = rig[5];\n'
                       '    float m20 = rig[6], m21 = rig[7], m22 = rig[8];')
        first = body.index('    if (m_camera_anchor_rotation_active)')
        last = body.index('\n    }', first)
        body = body[:first] + '''    if (m_camera_anchor_rotation_active)
    {
      const auto anchored = ACFacing::Compose(m_camera_anchor_rotation, rig);
      m00 = anchored[0]; m01 = anchored[1]; m02 = anchored[2];
      m10 = anchored[3]; m11 = anchored[4]; m12 = anchored[5];
      m20 = anchored[6]; m21 = anchored[7]; m22 = anchored[8];''' + body[last:]
        body = body.replace('m_home_position.', 'home.position.')
        body = replace(body, '    if (camera_height_units != 0.0f)',
                       '    const auto p = ACFacing::ToGame(ACFacing::Vector{ex, ey, ez}, yaw);\n'
                       '    ex = p[0]; ey = p[1]; ez = p[2];\n'
                       '    if (camera_height_units != 0.0f)')
        return body
    text = function(text, 'void OpenXRManager::GetEyeProjectionRows(',
                    'void OpenXRManager::GetRawEyeProjectionRows(', eyes)
    return text


def patch_d3d(text):
    return replace(text, '      DX11::g_openxr_d3d = std::move(openxr);\n    }\n  }',
                   '      DX11::g_openxr_d3d = std::move(openxr);\n    }\n  }\n'
                   '  else\n  {\n'
                   '    INFO_LOG_FMT(OPENXR, "OpenXR disabled; no session initialized");\n  }')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    if '.local' not in source.parts:
        raise SystemExit('Private source under .local required')
    if subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'], text=True).strip()!=PIN:
        raise SystemExit('Wrong source pin')
    changes = {}
    for name, edit in [('VideoCommon/VR/OpenXRManager.h',patch_header),
                       ('VideoCommon/VR/OpenXRManager.cpp',patch_manager),
                       ('VideoBackends/D3D/D3DMain.cpp',patch_d3d)]:
        path = source/'Source/Core'/name
        original = subprocess.check_output(['git','-C',str(source),'show',PIN+':Source/Core/'+name]).decode('utf-8').replace('\r\n','\n')
        desired = edit(original)
        if path.read_text(encoding='utf-8') not in (original,desired):
            raise SystemExit('Existing edits differ: '+name)
        changes[path]=desired.encode('utf-8')
    header = Path(__file__).with_name('ACFacing.h').read_bytes()
    path = source/'Source/Core/VideoCommon/VR/ACFacing.h'
    if path.exists() and path.read_bytes()!=header:
        raise SystemExit('Existing helper differs')
    changes[path]=header
    for path,data in changes.items():
        if not path.exists() or path.read_bytes()!=data:
            path.write_bytes(data)
        print(path.name+' SHA256 '+hashlib.sha256(data).hexdigest())


if __name__=='__main__':
    main()
