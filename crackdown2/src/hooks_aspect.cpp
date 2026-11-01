// crackdown2 - other aspect ratios (--aspect_ratio, default 16:9)
//
// Guest 0x830A3124 is the camera's aspect ratio (image value 16:9). The camera constructor sub_8252C1E0 writes it
// from the 16:9 float at 0x820F8D18 (lis r9,-32240; addi r7,r9,-29568; lfs f12,152(r7); lis r8,-31990;
// stfs f12,12580(r8) at 0x8252C2EC). The projection code reads it at 0x8224FB38, 0x82285AB8, 0x82291BD4, 0x825D367C
// and 0x825D3828. The only other writer is sub_8271C950, which toggles it between 16:9 (0x820F8D18) and 4:3
// (0x820F8D1C): Crackdown 1's leftover "widescreen" console command (sub_8250AC58 there), not reached in normal play.
// Same layout as Crackdown 1's 0x82BAA498 / sub_822846A0 (its hooks_aspect.cpp); found by scanning the
// title-update image (out/guest-image.bin) for lis/lfs/stfs pairs addressing 0x830A3124.
//
// So after the constructor runs, the chosen ratio replaces the 16:9. The guest frame buffer gets the same shape
// from ApplyGameDefaults (host_tuning.cpp), which sets the video mode to 720 lines times the ratio unless
// --video_mode_width/height were set.

#include "generated/default/crackdown2_init.h"

#include "host_tuning.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_STRING(aspect_ratio, "16:9", "Crackdown2",
                      "Game aspect ratio, as W:H (16:9, 16:10, 21:9, 32:9, 4:3) or a number such as 2.37. Hor+: "
                      "wider ratios show more to the sides");

namespace {
constexpr uint32_t kAspectAddress = 0x830A3124;  // f32, written by sub_8252C1E0
}

namespace crackdown2 {
double AspectRatio() {
  const std::string& text = REXCVAR_GET(aspect_ratio);
  double w = 0, h = 0;
  double ratio = 0;
  if (std::sscanf(text.c_str(), "%lf:%lf", &w, &h) == 2 && h > 0) {
    ratio = w / h;
  } else if (std::sscanf(text.c_str(), "%lf", &w) == 1) {
    ratio = w;
  }
  if (ratio < 1.0 || ratio > 4.0) {
    if (text != "16:9") REXLOG_WARN("aspect_ratio '{}' not understood (use W:H between 1 and 4); using 16:9", text);
    ratio = 16.0 / 9.0;
  }
  return ratio;
}

bool AspectRatioIsDefault() {
  const double ratio = AspectRatio();
  return ratio > 16.0 / 9.0 - 0.001 && ratio < 16.0 / 9.0 + 0.001;
}
}  // namespace crackdown2

namespace crackdown2 {
// Called once per guest frame (hooks_frametime.cpp) as well as after the constructor: in a PC run (log crackdown2_005)
// the first frame found the float at 4:3, before sub_8252C1E0 had run, so the menus and boot already use it.
void ApplyAspectRatio(uint8_t* base) {
  if (AspectRatioIsDefault()) return;
  const uint32_t wanted = std::bit_cast<uint32_t>(float(AspectRatio()));
  if (REX_LOAD_U32(kAspectAddress) == wanted) return;
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("aspect_ratio: camera aspect {:08X} {} -> {}", kAspectAddress,
                std::bit_cast<float>(REX_LOAD_U32(kAspectAddress)), AspectRatio());
  }
  REX_STORE_U32(kAspectAddress, wanted);
}
}  // namespace crackdown2

REX_HOOK_RAW(sub_8252C1E0) {
  __imp__sub_8252C1E0(ctx, base);
  crackdown2::ApplyAspectRatio(base);
}
