// crackdown2 - measured simulation clock for frame rates above 30 (--sim_game_clock)
//
// Same engine and design as Crackdown 1 (its hooks_framerate.cpp and docs/60fps-audit.md):
// - The main loop sub_82369AA0 calls the game update sub_82915C70 (its only caller) with
//   f1 = [0x832B60F8] * 1/30 (pooled literal 0x82114994): a fixed step per frame.
// - The step query sub_827671D0 (183 callers) returns [0x83205EF4] * 1/30 while the clock is locked.
// - Sixteen inlined timers add `locked ? 33 : ...` ms per frame (config/midasm_hooks.toml).
// - The clock object [0x82800B0C] is always locked (+13 = 1) in retail.
// At 60 fps all three run twice as fast. With --sim_game_clock the update dt and the step query are scaled by
// measured step / (1/30) and the 16 timers get the measured whole ms (remainder carried to the next frame;
// the float step itself is not rounded, which matters at 120/144 fps);
// the pooled 1/30 literal itself is left alone (Crackdown 1's --sim_step_literal: it has other readers, Havok's
// hkWorldCinfo among them). Measured = host time between presents (hooks_frametime.cpp), clamped to
// 1/240..1/20 s so loading stalls don't become huge steps. Flags are read once, at the first frame.
//
// --sim_frame_fixes (with --sim_game_clock): the frame-locked steps those three don't reach, the same classes as
// Crackdown 1's docs/60fps-audit.md (bit: what, config/midasm_hooks.toml has the disassembly):
//   0 the 30 Hz frame counter 0x8383A96C (38 readers; speech/line, respawn and UI deadlines in frames)
//   1 crowd animation clock sub_825E4AA0          2 sub_827338E8 timer += 1/30
//   3 sub_82334738 per-frame decay x -= x/30      4 0.033 dt passed by sub_826C9278
//   5 1/30 dt passed by sub_82549940              6 1/30 dt passed by sub_8256DCD0
//   7 stairs ground snap sub_82208998 (Crackdown 1 sub_822B4D40, below)
// --diag_sim_sites logs how often each site runs (every 5 s), to check they are per-frame.

#include "generated/default/crackdown2_init.h"
#include "midasm_hooks.h"
#include "ap/bridge.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_BOOL(sim_game_clock, false, "Crackdown2",
                    "Feed the measured frame time to the game update, the step query and the 16 fixed 33 ms "
                    "timers, so game speed doesn't follow frame rate (for --fps_cap above 30)");
REXCVAR_DEFINE_INT32(sim_game_clock_sites, 0xFFFF, "Crackdown2",
                     "Bitmask of the 33 ms timer sites --sim_game_clock rewrites (order as in "
                     "config/midasm_hooks.toml); for bisecting");
REXCVAR_DEFINE_INT32(sim_frame_fixes, 0xFF, "Crackdown2",
                     "With --sim_game_clock: bitmask of the frame-locked step fixes (frame counter, crowd "
                     "animation, timers, stairs ground snap; list in hooks_framerate.cpp); for bisecting");
REXCVAR_DEFINE_BOOL(diag_sim_sites, false, "Crackdown2",
                    "Diagnostic: log how often each --sim_frame_fixes site runs (every 5 s)");

namespace {
using Clock = std::chrono::steady_clock;
constexpr uint32_t kLockedFrameMs = 33;
constexpr double kMinStep = 1.0 / 240.0;
constexpr double kMaxStep = 1.0 / 20.0;

struct SimFlags {
  bool game_clock = REXCVAR_GET(sim_game_clock);
  int32_t sites = REXCVAR_GET(sim_game_clock_sites);
  int32_t fixes = REXCVAR_GET(sim_frame_fixes);
  bool diag = REXCVAR_GET(diag_sim_sites);
};
const SimFlags& Sim() {
  static const SimFlags flags = [] {
    SimFlags f;
    if (f.game_clock) REXLOG_INFO("sim_game_clock: on (sites mask {:04X}, fixes {:02X})", f.sites, f.fixes);
    return f;
  }();
  return flags;
}

Clock::time_point g_last_frame{};
double g_ms_carry = 0.0;
std::atomic<uint32_t> g_frame_ms{kLockedFrameMs};  // read by the mid-asm hooks (any thread)
std::atomic<float> g_step_scale{1.0f};             // measured step / (1/30)
double g_counter_carry = 0.0;                      // frame counter: fractional 30 Hz ticks

bool Fix(int bit) { return Sim().game_clock && (Sim().fixes & (1 << bit)); }
float StepScale() { return g_step_scale.load(std::memory_order_relaxed); }

// --diag_sim_sites: calls per site since the last report.
constexpr int kDiagSlots = 8;
std::atomic<uint32_t> g_diag_calls[kDiagSlots];
void Count(int bit) {
  if (Sim().diag) g_diag_calls[bit].fetch_add(1, std::memory_order_relaxed);
}
void ReportSites() {
  static Clock::time_point last = Clock::now();
  static uint32_t frames = 0;
  ++frames;
  const Clock::time_point now = Clock::now();
  if (now - last < std::chrono::seconds(5)) return;
  std::string line;
  for (int i = 0; i < kDiagSlots; ++i) line += " " + std::to_string(g_diag_calls[i].exchange(0));
  REXLOG_INFO("diag_sim_sites: {} frames, calls per fix bit:{}", frames, line);
  frames = 0;
  last = now;
}
}  // namespace

namespace crackdown2 {
// Called once per guest frame, right after the frame pacer (hooks_frametime.cpp).
void UpdateSimClock() {
  if (Sim().diag) ReportSites();
  if (!Sim().game_clock) return;
  const Clock::time_point now = Clock::now();
  if (g_last_frame != Clock::time_point{}) {
    const double dt = std::clamp(std::chrono::duration<double>(now - g_last_frame).count(), kMinStep, kMaxStep);
    g_ms_carry += dt * 1000.0;
    const uint32_t ms = uint32_t(std::floor(g_ms_carry));  // >= 4 since dt >= 1/240 s
    g_ms_carry -= ms;
    g_frame_ms.store(ms, std::memory_order_relaxed);
    // The float step uses the measured time, not the whole ms: at 120/144 fps a frame is 8.3/6.9 ms, and rounding
    // to 8/9 or 6/7 ms would make each step up to 15% off (judder). The ms timers keep the carry, so both add up
    // to the same (clamped) time.
    g_step_scale.store(float(dt * 30.0), std::memory_order_relaxed);
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
  // AP bridge on the game thread, after the update: queued network work, checks and skill grants (the present
  // hook runs on the render thread, where calling the skill setter crashed, run crackdown2_004).
  crackdown2::ap::OnFrame(ctx, base);
  crackdown2::ap::ProbeUpdate(ctx, base);
}

// Step query: returns the step in f1.
REX_HOOK_RAW(sub_827671D0) {
  __imp__sub_827671D0(ctx, base);
  if (Sim().game_clock) ctx.f1.f64 = double(float(ctx.f1.f64) * g_step_scale.load(std::memory_order_relaxed));
}

// Bits 1, 2, 4-6: a literal 30 Hz step (1/30 or 0.033) just loaded into f.
#define CD2_DEFINE_SCALE_STEP(bit, addr)                                         void Cd2ScaleStep_##addr(PPCRegister& f) {                                      Count(bit);                                                                    if (Fix(bit)) f.f64 = double(float(f.f64) * StepScale());                    }
CD2_SCALE_STEP_SITES(CD2_DEFINE_SCALE_STEP)
#undef CD2_DEFINE_SCALE_STEP

// Bit 3: f is the 1/30 in x * (1 - 1/30), applied once per frame. Per measured step that is
// (1 - 1/30)^(step * 30) to decay as much per second as at 30 fps.
void Cd2ScaleDecay_82334B2C(PPCRegister& f) {
  Count(3);
  if (Fix(3)) f.f64 = double(1.0f - std::pow(1.0f - float(f.f64), StepScale()));
}

// Bit 0: r is the frame counter + 1. Make it + whole 30 Hz ticks elapsed (fraction carried), so frame-count
// deadlines last as long as at 30 fps. Only the main loop runs this (sub_82338648, once per frame).
void Cd2FrameCounterTick(PPCRegister& r) {
  Count(0);
  if (!Fix(0)) return;
  g_counter_carry += StepScale();
  const uint32_t ticks = uint32_t(std::floor(g_counter_carry));
  g_counter_carry -= ticks;
  r.u64 = uint32_t(r.u32 - 1u + ticks);
}

// Bit 7: stairs speed bug (Crackdown 1 hooks_framerate.cpp, sub_822B4D40; same code here). The character
// controller sub_822498F8 calls the step-down snap sub_82208998(controller r3, dt f1, out r5), which ends with
// (0x8220911C..0x82209124) `fdivs f12,f13,f21; fneg f11,f12; stfs f11,4(r23)`: out.y = -drop * gain / dt
// (f21 = dt, r23 = out). A stair step is a fixed height, so at 60 fps the pull doubles and Havok's character
// proxy turns part of it into forward speed on stair edges (Crackdown 1: 24 m/s peaks against 12 at 30 fps).
// Scaling by dt * 30 gives it the 30 fps velocity. Frames of 1/32 s or longer (30 fps) are left alone.
REX_HOOK_RAW(sub_82208998) {
  const uint32_t out = ctx.r5.u32;
  const float dt = float(ctx.f1.f64);
  const uint32_t before = REX_LOAD_U32(out + 4);
  __imp__sub_82208998(ctx, base);
  const uint32_t after = REX_LOAD_U32(out + 4);
  if (after == before) return;  // no snap this step
  Count(7);
  if (Fix(7) && dt > 0.0f && dt < 1.0f / 32.0f) {
    REX_STORE_U32(out + 4, std::bit_cast<uint32_t>(std::bit_cast<float>(after) * dt * 30.0f));
  }
}
