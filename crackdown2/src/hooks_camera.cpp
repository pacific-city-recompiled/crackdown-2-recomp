// Camera turn speed (--camera_sensitivity), the same setting as Crackdown 1's
// hooks_camera.cpp.
//
// The right stick reaches the camera unscaled (deadzone 8689 removed and
// rescaled in sub_82330B08, called from sub_82331180 at 0x82331250 /
// 0x82331288), so a faster camera has to scale the game's turn rates.
//
// The player-camera updates sub_8223F520 and sub_8224FF60 read their rates
// from a tuning object whose pointer is at 0x83800AD0 (published by
// sub_825587D0 at 0x82558834; the values are parsed from game data and
// unit-converted in sub_82558A48, which is why a scan for CD1's 0.75 / 3.0
// found nothing). Each rate is a triple indexed by camera mode m = 0..2, and
// there are two families (CD1 has one mode and two families):
//   +28/+52  base yaw    sub_82395448: lfsx f1 at 0x82395598 (B+4*(7|13+m))
//   +40/+64  base pitch  sub_82227DF0: lfsx f1 at 0x82227F40 (B+4*(10|16+m))
//   +76/+88  top yaw     sub_8223F520: lfsx f0 at 0x82240164 (B+4*(19|22+m));
//                        sub_8224FF60: lfs f0,76/88 at 0x822503AC / 0x822503B8
// The updates multiply these by the shaped stick and dt, as in CD1.
//
// Scaling the data rather than the stick: the hook keeps the original values
// and rescales them whenever the game writes them again (a reload).
// sub_82395448 (the yaw accessor) is called by both updates before any rate is
// read (0x8223FF64, 0x8224FFCC), so it is the tick.
//
// Not covered: the vehicle-associated camera sub_827229F0 has its own rate
// multipliers (B+360 / B+340, overrides 0x8309AA5C / 0x8309AA58).

#include "generated/default/crackdown2_init.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_DOUBLE(camera_sensitivity, 1.0, "Crackdown2",
                      "Camera turn speed multiplier for the right stick and mouse (0.25 - 4.0; 1 = the "
                      "game's own speed)")
    .range(0.25, 4.0)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

namespace {
constexpr uint32_t kCameraTuningPointer = 0x83800AD0;
constexpr std::array<uint32_t, 18> kRateOffsets = {
    28, 32, 36, 52, 56, 60,  // base yaw
    40, 44, 48, 64, 68, 72,  // base pitch
    76, 80, 84, 88, 92, 96,  // top yaw
};

struct State {
  uint32_t block = 0;
  std::array<uint32_t, kRateOffsets.size()> original{};
  std::array<uint32_t, kRateOffsets.size()> written{};
} g_state;

void ApplySensitivity(uint8_t* base) {
  const uint32_t block = REX_LOAD_U32(kCameraTuningPointer);
  if (!block) return;
  const double setting = REXCVAR_GET(camera_sensitivity);
  if (setting == 1.0 && g_state.block == 0) {
    // Default and never scaled: leave the game's data alone.
    return;
  }
  const float scale = float(std::clamp(setting, 0.25, 4.0));
  for (size_t i = 0; i < kRateOffsets.size(); ++i) {
    const uint32_t field = block + kRateOffsets[i];
    const uint32_t current = REX_LOAD_U32(field);
    // A new block, or the game wrote the field since we did: take it as the
    // original value.
    if (block != g_state.block || current != g_state.written[i]) {
      g_state.original[i] = current;
    }
    const uint32_t wanted = std::bit_cast<uint32_t>(std::bit_cast<float>(g_state.original[i]) * scale);
    if (current != wanted) REX_STORE_U32(field, wanted);
    g_state.written[i] = wanted;
  }
  if (block != g_state.block) {
    REXLOG_INFO("camera_sensitivity: {} (tuning block {:08X}; yaw {} {} {}, pitch {} {} {}, top yaw {} {} {})",
                scale, block, std::bit_cast<float>(g_state.original[0]),
                std::bit_cast<float>(g_state.original[1]), std::bit_cast<float>(g_state.original[2]),
                std::bit_cast<float>(g_state.original[6]), std::bit_cast<float>(g_state.original[7]),
                std::bit_cast<float>(g_state.original[8]), std::bit_cast<float>(g_state.original[12]),
                std::bit_cast<float>(g_state.original[13]), std::bit_cast<float>(g_state.original[14]));
    g_state.block = block;
  }
}
}  // namespace

REX_HOOK_RAW(sub_82395448) {
  ApplySensitivity(base);
  __imp__sub_82395448(ctx, base);
}
