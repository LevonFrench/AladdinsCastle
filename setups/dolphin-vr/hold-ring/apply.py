#!/usr/bin/env python
"""Add (or remove) the U4 hold ring to a private DolphinXR source clone at the pin.

Usage: python -B apply.py <absolute-private-source-path> [--revert]

While both stick clicks are held, the headset shows the ring every AladdinsCastle host shows (libacvr
hold_ring.hpp, as in acvr-ps1): nothing for the first second, a ring that fills from 1 s to 3 s, one flash
at 3 s, gone as soon as a click lets go. It follows the same count as Dolphin's Exit hotkey
(Hotkeys.ini: General/Exit = hold(both stick clicks, 3.0)), read from the same OpenXR input, so the
ring flashes as the Exit fires. The quit itself stays Dolphin's own.

  - Copies the libacvr headers into Source/Core/VideoCommon/VR/ACVR/.
  - VideoCommon/VR/OpenXRManager.h: IOpenXRSwapchain::FillEyeRects (paint rectangles into an acquired
    eye image; backends without it draw nothing).
  - VideoBackends/D3D/D3DOpenXR: FillEyeRects with D3D11.1 ClearView (Direct3D 11, the backend this
    repository builds).
  - VideoCommon/Present.cpp: the eye blit paints the ring straight ahead in each eye (the per-eye path is
    used while the ring shows), the flat-screen blit in the middle of the picture.

Nothing is downloaded. Run after the other DolphinXR deltas (see compile-dolphin.yml).
"""
import argparse
import shutil
import subprocess
from pathlib import Path

PIN = "85c4266b29eb308d173fefa0f21f5cb77df3516a"
HERE = Path(__file__).resolve().parent
LIBACVR = HERE.parents[2] / "libacvr" / "src"
HEADERS = ("stick_chord.hpp", "hold_ring.hpp", "hold_ring_gl.hpp", "hold_ring_host.hpp")
HEADER_DIR = Path("Source/Core/VideoCommon/VR/ACVR")

MANAGER_H = Path("Source/Core/VideoCommon/VR/OpenXRManager.h")
D3D_H = Path("Source/Core/VideoBackends/D3D/D3DOpenXR.h")
D3D_CPP = Path("Source/Core/VideoBackends/D3D/D3DOpenXR.cpp")
PRESENT = Path("Source/Core/VideoCommon/Present.cpp")

FILL_DECL = """\
  virtual uint32_t GetEyeHeight() const = 0;

  // ACVR U4 hold ring (setups/dolphin-vr/hold-ring): paints solid rectangles into the acquired image of
  // one eye, in pixels from the top left, colours as display values. Backends without it draw nothing.
  struct FillRect
  {
    int left, top, right, bottom;
    float rgb[3];
  };
  virtual void FillEyeRects(uint32_t /*eye*/, const FillRect* /*rects*/, size_t /*count*/) {}
};
"""

D3D_FILL = """\
// ACVR U4 hold ring: one D3D11.1 ClearView per rectangle into the acquired eye image. Colours are display
// values, so on an sRGB view they are turned back to linear first (the view encodes them again).
void D3DOpenXR::FillEyeRects(uint32_t eye_index, const FillRect* rects, size_t count)
{
  if (eye_index >= 2 || !m_image_acquired[eye_index] || !rects || count == 0)
    return;
  const auto& sc = m_eye_swapchains[eye_index];
  const uint32_t image = m_acquired_image_index[eye_index];
  if (image >= sc.framebuffers.size() || !sc.framebuffers[image] ||
      sc.framebuffers[image]->GetNumRTVs() == 0)
    return;
  ID3D11RenderTargetView* rtv = sc.framebuffers[image]->GetRTVArray()[0];
  ComPtr<ID3D11DeviceContext1> context1;
  if (!rtv || FAILED(D3D::context.As(&context1)))
    return;
  D3D11_RENDER_TARGET_VIEW_DESC desc{};
  rtv->GetDesc(&desc);
  const bool srgb = desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
                    desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  for (size_t i = 0; i < count; ++i)
  {
    const FillRect& r = rects[i];
    if (r.right <= r.left || r.bottom <= r.top)
      continue;
    FLOAT color[4] = {r.rgb[0], r.rgb[1], r.rgb[2], 1.0f};
    if (srgb)
    {
      for (int c = 0; c < 3; ++c)
      {
        color[c] = color[c] <= 0.04045f ? color[c] / 12.92f :
                                          std::pow((color[c] + 0.055f) / 1.055f, 2.4f);
      }
    }
    const D3D11_RECT box{r.left, r.top, r.right, r.bottom};
    context1->ClearView(rtv, color, &box, 1);
  }
}

bool D3DOpenXR::SubmitFrame()
{
"""

PRESENT_HELPERS = """\
// ACVR U4 hold ring (setups/dolphin-vr/hold-ring, libacvr hold_ring.hpp): follows Dolphin's Exit hotkey,
// General/Exit = hold(both stick clicks, 3.0), from the same OpenXR input. Hidden for 1 s, fills to 3 s and
// flashes once as the Exit fires; gone as soon as a click lets go.
static acvr::HoldRingView ACVRHoldRingNow()
{
  static acvr::BothHeldProgress s_hold;
  static acvr::HoldRing s_ring;
  const Common::VR::OpenXRInputSnapshot input = Common::VR::OpenXRInputState::GetSnapshot();
  const bool both = input.runtime_active && input.session_focused &&
                    input.controllers[0].thumbstick_button && input.controllers[1].thumbstick_button;
  const u64 now = static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now().time_since_epoch())
                                       .count());
  return s_ring.update(now, s_hold.update(now, both));
}

// Straight ahead in the eye image the runtime gets (the field of view SubmitFrame sends with it), so the
// two rings meet far away.
static acvr::HoldRingSpot ACVREyeSpot(uint32_t eye, int width, int height)
{
  const XrFovf& fov = VR::g_openxr->GetPresentEyeViews()[eye].fov;
  return acvr::hold_ring_eye_spot(std::tan(fov.angleLeft), std::tan(fov.angleRight),
                                  std::tan(fov.angleDown), std::tan(fov.angleUp), width, height);
}

// Paints the ring into the acquired image of one eye: hold_ring_rects counts y up, the backends y down.
static void ACVRFillHoldRing(VR::IOpenXRSwapchain* sc, uint32_t eye, const acvr::HoldRingView& view,
                             const acvr::HoldRingSpot& at, int width, int height)
{
  const acvr::HoldRingRects rects = acvr::hold_ring_rects(view, at.cx, at.cy, at.radius);
  std::array<VR::IOpenXRSwapchain::FillRect, acvr::kHoldRingMaxRects> fill{};
  size_t count = 0;
  for (int i = 0; i < rects.n; ++i)
  {
    const acvr::HoldRingRect& q = rects.r[i];
    const int x0 = std::max(0, static_cast<int>(std::floor(q.x + 0.5f)));
    const int y0 = std::max(0, static_cast<int>(std::floor(q.y + 0.5f)));
    const int x1 = std::min(width, static_cast<int>(std::floor(q.x + q.w + 0.5f)));
    const int y1 = std::min(height, static_cast<int>(std::floor(q.y + q.h + 0.5f)));
    if (x1 <= x0 || y1 <= y0)
      continue;
    fill[count++] = {x0, height - y1, x1, height - y0, {q.rgb[0], q.rgb[1], q.rgb[2]}};
  }
  if (count != 0)
    sc->FillEyeRects(eye, fill.data(), count);
}

void Presenter::BlitCurrentSourceToOpenXREyes(const AbstractTexture* source_texture,
"""

EDITS = {
    MANAGER_H: [
        ("  virtual uint32_t GetEyeHeight() const = 0;\n};\n", FILL_DECL),
    ],
    D3D_H: [
        ("  void ReleaseEyeTexture(uint32_t eye_index) override;\n",
         "  void ReleaseEyeTexture(uint32_t eye_index) override;\n\n"
         "  // ACVR U4 hold ring: D3D11.1 ClearView rectangles into the acquired eye image.\n"
         "  void FillEyeRects(uint32_t eye_index, const FillRect* rects, size_t count) override;\n"),
    ],
    D3D_CPP: [
        ("#include <array>\n", "#include <array>\n#include <cmath>\n"),
        ("bool D3DOpenXR::SubmitFrame()\n{\n", D3D_FILL),
    ],
    PRESENT: [
        ('#include "VideoCommon/VR/OpenXRManager.h"\n#endif\n',
         '#include "VideoCommon/VR/OpenXRManager.h"\n'
         '#include "VideoCommon/VR/ACVR/hold_ring_host.hpp"  // the U4 hold ring (libacvr)\n'
         '#include <algorithm>\n#include <array>\n#include <cmath>\n#endif\n'),
        ("void Presenter::BlitCurrentSourceToOpenXREyes(const AbstractTexture* source_texture,\n",
         PRESENT_HELPERS),
        ("  const float vr_gamma = g_ActiveConfig.vr_gamma;\n",
         "  const acvr::HoldRingView acvr_ring = ACVRHoldRingNow();  // the U4 hold ring, if both clicks are held\n"
         "  const float vr_gamma = g_ActiveConfig.vr_gamma;\n"),
        ("  if (sc->SupportsLayeredRendering() && m_post_processor->CanBlitFromTextureLayered())\n",
         "  // While the ring shows, the per-eye path runs: each eye gets its own ring position.\n"
         "  if (!acvr_ring.visible && sc->SupportsLayeredRendering() &&\n"
         "      m_post_processor->CanBlitFromTextureLayered())\n"),
        ("    m_post_processor->BlitFromTexture(eye_rect, source_rc, source_texture, static_cast<int>(eye));\n"
         "    sc->ReleaseEyeTexture(eye);\n",
         "    m_post_processor->BlitFromTexture(eye_rect, source_rc, source_texture, static_cast<int>(eye));\n"
         "    if (acvr_ring.visible)\n"
         "    {\n"
         "      ACVRFillHoldRing(sc, eye, acvr_ring,\n"
         "                       ACVREyeSpot(eye, eye_rect.GetWidth(), eye_rect.GetHeight()),\n"
         "                       eye_rect.GetWidth(), eye_rect.GetHeight());\n"
         "    }\n"
         "    sc->ReleaseEyeTexture(eye);\n"),
        ("  m_post_processor->BlitFromTexture(flat_rect, source_rc, source_texture, 0);\n"
         "  sc->ReleaseFlatTexture();\n",
         "  m_post_processor->BlitFromTexture(flat_rect, source_rc, source_texture, 0);\n"
         "  if (const acvr::HoldRingView ring = ACVRHoldRingNow(); ring.visible)\n"
         "  {\n"
         "    // the flat panel: the middle of the picture, as in acvr-ps1\n"
         "    ACVRFillHoldRing(sc, 0, ring,\n"
         "                     acvr::hold_ring_screen_spot(flat_rect.GetWidth(), flat_rect.GetHeight()),\n"
         "                     flat_rect.GetWidth(), flat_rect.GetHeight());\n"
         "  }\n"
         "  sc->ReleaseFlatTexture();\n"),
    ],
}


def edit(path, pairs, revert):
    raw = path.read_bytes().decode("utf-8")
    crlf = "\r\n" in raw
    text = raw.replace("\r\n", "\n")
    for old, new in pairs:
        if revert:
            if new in text:
                text = text.replace(new, old)
            continue
        if new in text:
            continue  # already applied
        if text.count(old) != 1:
            raise SystemExit(f"{path.name}: anchor missing or ambiguous: {old.strip()[:80]}")
        text = text.replace(old, new)
    if crlf:
        text = text.replace("\n", "\r\n")
    path.write_bytes(text.encode("utf-8"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", type=Path)
    ap.add_argument("--revert", action="store_true")
    args = ap.parse_args()
    root = args.source.resolve(strict=True)
    if ".local" not in root.parts:
        raise SystemExit("Use a private source clone under .local")
    head = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    if head != PIN:
        raise SystemExit("Wrong source pin")
    for rel, pairs in EDITS.items():
        edit(root / rel, pairs, args.revert)
    target = root / HEADER_DIR
    if args.revert:
        shutil.rmtree(target, ignore_errors=True)
    else:
        target.mkdir(parents=True, exist_ok=True)
        for name in HEADERS:
            source = LIBACVR / name
            if not source.is_file():
                raise SystemExit(f"libacvr header missing (run this from a repository checkout): {name}")
            shutil.copyfile(source, target / name)
    print(("reverted " if args.revert else "applied ") + str(root))


if __name__ == "__main__":
    main()
