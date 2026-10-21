"""Apply the AladdinsCastle offline stereo probe to COPIES of four DolphinXR source files.

Pinned source: iChris4/dolphinXR 85c4266b29eb308d173fefa0f21f5cb77df3516a.
Copy VideoCommon/GeometryShaderManager.cpp, VideoCommon/BPStructs.cpp, VideoCommon/FrameDumper.cpp
and VideoBackends/D3D/D3DMain.cpp into one private folder, then:

    python -I apply.py <that folder>

The shared source tree is never touched. Each replacement must match exactly once; the script
refuses otherwise. See README.md for building and running.
"""
import sys
from pathlib import Path

D = Path(sys.argv[1])


def sub(name, old, new, count=1):
    p = D / name
    t = p.read_text(encoding="utf-8")
    n = t.count(old)
    if n != count:
        raise SystemExit(f"{name}: expected {count} match(es), found {n}: {old[:60]!r}")
    p.write_text(t.replace(old, new), encoding="utf-8", newline="")


HELPER = r'''static constexpr int LINE_PT_TEX_OFFSETS[8] = {0, 16, 8, 4, 2, 1, 1, 1};

#ifdef ENABLE_VR
// ---- AladdinsCastle offline stereo probe (private, inert unless AC_STEREO_PROBE is set) ----
// With no OpenXR runtime at all, render the real VR stereo path from a fixed synthetic head:
// identity orientation at home, eyes at -/+ IPD/2 on X, symmetric FOV of +/- atan(tan) on both
// axes. Same math as OpenXRManager::GetEyeProjectionRows / GetRawEyeProjectionRows for those
// eye views (no camera/controller anchor, no tracking).
bool ACStereoProbeActive()
{
  static const bool on = std::getenv("AC_STEREO_PROBE") != nullptr;
  return on;
}
static float ACStereoProbeEnv(const char* name, float def)
{
  const char* v = std::getenv(name);
  if (!v || !*v)
    return def;
  const float f = static_cast<float>(std::atof(v));
  return std::isfinite(f) && f > 0.0f ? f : def;
}
static void ACStereoProbeRows(float units_per_meter, std::array<std::array<float, 4>, 4>& proj,
                              std::array<std::array<float, 4>, 2>& zrows,
                              std::array<std::array<float, 4>, 4>& head)
{
  const float s = std::max(units_per_meter, 0.0001f);
  const float ipd = ACStereoProbeEnv("AC_STEREO_PROBE_IPD", 0.064f);
  const float t = ACStereoProbeEnv("AC_STEREO_PROBE_TAN", 1.0f);
  constexpr float DEG_TO_RAD = 0.01745329252f;
  const float lean = g_ActiveConfig.vr_enable_lean_back_angle ?
                         g_ActiveConfig.vr_lean_back_angle * DEG_TO_RAD :
                         0.0f;
  const float cam_fwd =
      g_ActiveConfig.vr_enable_camera_forward ? -g_ActiveConfig.vr_camera_forward * s : 0.0f;
  const float cam_up =
      g_ActiveConfig.vr_enable_camera_height ? g_ActiveConfig.vr_camera_height * s : 0.0f;
  // Optional synthetic head yaw (degrees, positive = turn left) to show what is head-locked.
  const char* yaw_env = std::getenv("AC_STEREO_PROBE_YAW");
  const float yaw = yaw_env ? static_cast<float>(std::atof(yaw_env)) * DEG_TO_RAD : 0.0f;
  // q_xr = yaw about +Y; q = conj(q_xr) * lean_back(X), as in GetEyeProjectionRows.
  const float ay = -std::sin(0.5f * yaw), aw = std::cos(0.5f * yaw);
  const float bx = std::sin(0.5f * lean), bw = std::cos(0.5f * lean);
  const float qx = aw * bx, qy = ay * bw, qz = -ay * bx, qw = aw * bw;
  const float x2 = 2 * qx * qx, y2 = 2 * qy * qy, z2 = 2 * qz * qz;
  const float xy = 2 * qx * qy, xz = 2 * qx * qz, yz = 2 * qy * qz;
  const float wx = 2 * qw * qx, wy = 2 * qw * qy, wz = 2 * qw * qz;
  const float m00 = 1 - y2 - z2, m01 = xy + wz, m02 = xz - wy;
  const float m10 = xy - wz, m11 = 1 - x2 - z2, m12 = yz + wx;
  const float m20 = xz + wy, m21 = yz - wx, m22 = 1 - x2 - y2;
  const float p0x = 1.0f / t, p0z = 0.0f, p1y = 1.0f / t, p1z = 0.0f;
  for (int eye = 0; eye < 2; ++eye)
  {
    const float half = (eye == 0 ? -0.5f : 0.5f) * ipd * s;
    const float ex = half * std::cos(yaw);
    const float ey = cam_up;
    const float ez = cam_fwd - half * std::sin(yaw);
    const float c0x = m00 * p0x + m02 * p0z, c0y = m10 * p0x + m12 * p0z,
                c0z = m20 * p0x + m22 * p0z;
    const float c1x = m01 * p1y + m02 * p1z, c1y = m11 * p1y + m12 * p1z,
                c1z = m21 * p1y + m22 * p1z;
    proj[eye * 2 + 0] = {c0x, c0y, c0z, -(c0x * ex + c0y * ey + c0z * ez)};
    proj[eye * 2 + 1] = {c1x, c1y, c1z, -(c1x * ex + c1y * ey + c1z * ez)};
    zrows[eye] = {m02, m12, m22, -(m02 * ex + m12 * ey + m22 * ez)};
    head[eye * 2 + 0] = {p0x, 0.0f, p0z, -(p0x * half)};
    head[eye * 2 + 1] = {0.0f, p1y, p1z, 0.0f};
  }
}
#endif
'''

# --- GeometryShaderManager.cpp ---
g = "GeometryShaderManager.cpp"
sub(g, "#include <cmath>\n", "#include <cmath>\n#include <cstdlib>\n")
sub(g, "static constexpr int LINE_PT_TEX_OFFSETS[8] = {0, 16, 8, 4, 2, 1, 1, 1};\n", HELPER)
sub(g, "        if (VR::g_openxr && VR::g_openxr->IsSessionRunning())\n        {\n",
    "        if ((VR::g_openxr && VR::g_openxr->IsSessionRunning()) || ACStereoProbeActive())\n        {\n")
sub(g, "            VR::g_openxr->GetEyeProjectionRows(upm, eye_projection_rows, eye_z_rows);\n",
    "            std::array<std::array<float, 4>, 4> probe_head_rows{};\n"
    "            if (VR::g_openxr)\n"
    "              VR::g_openxr->GetEyeProjectionRows(upm, eye_projection_rows, eye_z_rows);\n"
    "            else\n"
    "              ACStereoProbeRows(upm, eye_projection_rows, eye_z_rows, probe_head_rows);\n")
sub(g, "            VR::g_openxr->GetRawEyeProjectionRows(upm, head_proj_rows);\n",
    "            if (VR::g_openxr)\n"
    "              VR::g_openxr->GetRawEyeProjectionRows(upm, head_proj_rows);\n"
    "            else\n"
    "              head_proj_rows = probe_head_rows;\n")
sub(g, "            VR::g_openxr->RecordRenderedEyeViews();\n",
    "            if (VR::g_openxr)\n"
    "              VR::g_openxr->RecordRenderedEyeViews();\n")
sub(g, "            perspective && VR::g_openxr && VR::g_openxr->IsSessionRunning() &&\n",
    "            perspective && ((VR::g_openxr && VR::g_openxr->IsSessionRunning()) || ACStereoProbeActive()) &&\n")

# --- BPStructs.cpp: queue the per-frame VR EFB clear offline too ---
b = "BPStructs.cpp"
sub(b, "      if (g_ActiveConfig.stereo_mode == StereoMode::OpenXR && VR::g_openxr &&\n"
       "          VR::g_openxr->IsSessionRunning())\n"
       "      {\n"
       "        VR::g_openxr->StampXFBPose(destAddr);\n",
    "      if (g_ActiveConfig.stereo_mode == StereoMode::OpenXR &&\n"
    "          ((VR::g_openxr && VR::g_openxr->IsSessionRunning()) || ACStereoProbeActive()))\n"
    "      {\n"
    "        if (VR::g_openxr)\n"
    "          VR::g_openxr->StampXFBPose(destAddr);\n")
# declare the probe flag once, after the includes
sub(b, "\n#include \"VideoCommon/BPStructs.h\"\n",
    "\n#include \"VideoCommon/BPStructs.h\"\n#ifdef ENABLE_VR\nbool ACStereoProbeActive();\n#endif\n")

# --- D3DMain.cpp: never touch the OpenXR loader under the probe ---
m = "D3DMain.cpp"
sub(m, "  if (g_ActiveConfig.VRSessionActive())\n  {\n    auto openxr",
    "  if (g_ActiveConfig.VRSessionActive() && std::getenv(\"AC_STEREO_PROBE\") != nullptr)\n"
    "  {\n"
    "    WARN_LOG_FMT(VIDEO, \"AC stereo probe: OpenXR not initialized; synthetic eyes.\");\n"
    "  }\n"
    "  else if (g_ActiveConfig.VRSessionActive())\n  {\n    auto openxr")
sub(m, "#include \"VideoBackends/D3D/D3DBase.h\"\n",
    "#include <cstdlib>\n\n#include \"VideoBackends/D3D/D3DBase.h\"\n")

# --- FrameDumper.cpp: screenshots carry both eye layers side by side, unscaled ---
f = "FrameDumper.cpp"
sub(f, "  int source_width = src_rect.GetWidth();\n  int source_height = src_rect.GetHeight();\n"
       "  int target_width = target_rect.GetWidth();\n  int target_height = target_rect.GetHeight();\n",
    "  int source_width = src_rect.GetWidth();\n  int source_height = src_rect.GetHeight();\n"
    "  int target_width = target_rect.GetWidth();\n  int target_height = target_rect.GetHeight();\n"
    "  if (std::getenv(\"AC_STEREO_PROBE\") != nullptr && src_texture->GetLayers() >= 2)\n"
    "  {\n"
    "    // Left eye (layer 0) on the left half, right eye (layer 1) on the right half.\n"
    "    if (!CheckFrameDumpReadbackTexture(source_width * 2, source_height))\n"
    "      return;\n"
    "    for (u32 eye = 0; eye < 2; ++eye)\n"
    "    {\n"
    "      m_frame_dump_readback_texture->CopyFromTexture(\n"
    "          src_texture, src_rect, eye, 0,\n"
    "          MathUtil::Rectangle<int>(static_cast<int>(eye) * source_width, 0,\n"
    "                                   (static_cast<int>(eye) + 1) * source_width, source_height));\n"
    "    }\n"
    "    m_last_frame_state = m_ffmpeg_dump.FetchState(ticks, frame_number);\n"
    "    m_frame_dump_needs_flush = true;\n"
    "    return;\n"
    "  }\n")
ft = (D / f).read_text(encoding="utf-8")
first_inc = ft.index("#include")
end = ft.index("\n", first_inc) + 1
(D / f).write_text(ft[:end] + "\n#include <cstdlib>\n" + ft[end:], encoding="utf-8", newline="")
print("patched", sorted(p.name for p in D.iterdir()))
