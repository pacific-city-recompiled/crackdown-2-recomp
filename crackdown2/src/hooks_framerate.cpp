// crackdown2 - measured simulation clock for frame rates above 30 (--sim_game_clock)
//
// Same engine and design as Crackdown 1 (its hooks_framerate.cpp and docs/60fps-audit.md):
// - The main loop sub_82369AA0 calls the game update sub_82915C70 (its only caller) with
//   f1 = [0x832B60F8] * 1/30 (pooled literal 0x82114994): a fixed step per frame.
// - The step query sub_827671D0 (183 callers) returns [0x83205EF4] * 1/30 while the clock is locked.
// - Sixteen inlined timers add `locked ? 33 : ...` ms per frame (config/midasm_hooks.toml).
// - The clock object [0x82800B0C] is always locked (+13 = 1) in retail.
// At 60 fps all three run twice as fast. With --sim_game_clock the update dt and the step query are scaled by
// measured step / (1/30) and the 16 timers get the measured whole ms (remainder carried to the next frame);
// the pooled 1/30 literal itself is left alone (Crackdown 1's --sim_step_literal: it has other readers, Havok's
// hkWorldCinfo among them). Measured = host time between presents (hooks_frametime.cpp), clamped to
// 1/240..1/20 s so loading stalls don't become huge steps. Flags are read once, at the first frame.

#include "generated/default/crackdown2_init.h"
#include "midasm_hooks.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_BOOL(sim_game_clock, false, "Crackdown2",
                    "Feed the measured frame time to the game update, the step query and the 16 fixed 33 ms "
                    "timers, so game speed doesn't follow frame rate (for --fps_cap above 30)");
REXCVAR_DEFINE_INT32(sim_game_clock_sites, 0xFFFF, "Crackdown2",
                     "Bitmask of the 33 ms timer sites --sim_game_clock rewrites (order as in "
                     "config/midasm_hooks.toml); for bisecting");

namespace {
using Clock = std::chrono::steady_clock;
constexpr uint32_t kLockedFrameMs = 33;
constexpr double kMinStep = 1.0 / 240.0;
constexpr double kMaxStep = 1.0 / 20.0;

struct SimFlags {
  bool game_clock = REXCVAR_GET(sim_game_clock);
  int32_t sites = REXCVAR_GET(sim_game_clock_sites);
};
const SimFlags& Sim() {
  static const SimFlags flags = [] {
    SimFlags f;
    if (f.game_clock) REXLOG_INFO("sim_game_clock: on (sites mask {:04X})", f.sites);
    return f;
  }();
  return flags;
}

Clock::time_point g_last_frame{};
double g_ms_carry = 0.0;
std::atomic<uint32_t> g_frame_ms{kLockedFrameMs};  // read by the mid-asm hooks (any thread)
std::atomic<float> g_step_scale{1.0f};             // measured step / (1/30)
}  // namespace

namespace crackdown2 {
// Called once per guest frame, right after the frame pacer (hooks_frametime.cpp).
void UpdateSimClock() {
  if (!Sim().game_clock) return;
  const Clock::time_point now = Clock::now();
  if (g_last_frame != Clock::time_point{}) {
    const double dt = std::clamp(std::chrono::duration<double>(now - g_last_frame).count(), kMinStep, kMaxStep);
    g_ms_carry += dt * 1000.0;
    const uint32_t ms = uint32_t(std::floor(g_ms_carry));  // >= 4 since dt >= 1/240 s
    g_ms_carry -= ms;
    g_frame_ms.store(ms, std::memory_order_relaxed);
    g_step_scale.store(float(ms) / 1000.0f * 30.0f, std::memory_order_relaxed);
  }
  g_last_frame = now;
}
}  // namespace crackdown2

#define CD2_DEFINE_FRAME_MS(bit, addr)                    \
  void Cd2FrameMs_##addr(PPCRegister& r) {                \
    if (Sim().game_clock && (Sim().sites & (1 << (bit)))) \
      r.u64 = g_frame_ms.load(std::memory_order_relaxed); \
  }
CD2_FRAME_MS_SITES(CD2_DEFINE_FRAME_MS)
#undef CD2_DEFINE_FRAME_MS

// Game update: f1 = dt.
REX_HOOK_RAW(sub_82915C70) {
  if (Sim().game_clock) ctx.f1.f64 = double(float(ctx.f1.f64) * g_step_scale.load(std::memory_order_relaxed));
  __imp__sub_82915C70(ctx, base);
}

// Step query: returns the step in f1.
REX_HOOK_RAW(sub_827671D0) {
  __imp__sub_827671D0(ctx, base);
  if (Sim().game_clock) ctx.f1.f64 = double(float(ctx.f1.f64) * g_step_scale.load(std::memory_order_relaxed));
}
