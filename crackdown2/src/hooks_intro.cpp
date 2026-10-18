// crackdown2 - skip the boot logos (--skip_intro_videos, on by default)
//
// Guest 0x828AB9F0 is the boot-screen state machine, run once per frame (generated .45.cpp). Its state is the
// u32 at this+324: 0 (save notice) -> plays "Ruffian.usm" (0x82163988, from UI\splash.pack) and goes to 1; 1 -> shows the
// legal text (textLine1/textLine2.text) for 4500 ms and goes to 2; 2 -> plays "TitleFadeUp.usm" (0x821639B4)
// and goes to 3; 3 -> title. Each state moves on early when sub_828B60C8 returns nonzero; that function scans
// the four pads' button bits ("press a button to skip"), otherwise it waits for the movie object to report 7
// (finished) through the object at [0x828016FC] (lis -31872, lwz 5884; vtable slot +12).
//
// So while 0x828AB9F0 runs, sub_828B60C8 reports a press: each frame skips one boot screen, the same as a
// player tapping A. sub_828B60C8 has 5 other callers; outside the state machine it is unchanged.

#include "generated/default/crackdown2_init.h"

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_BOOL(skip_intro_videos, true, "Crackdown2",
                    "Skip the boot logos (Ruffian.usm, legal text, TitleFadeUp.usm) straight to Press Start");

namespace {
thread_local bool g_in_boot_screens = false;
uint32_t g_last_state = ~0u;
int g_state_frames = 0;
}  // namespace

REX_HOOK_RAW(sub_828AB9F0) {
  // State 0 is the save-notice screen (SaveWarning.gfx), which also waits for the byte at 0x82801667 (lis
  // -31872, lbz 5735) before it looks at the pads; forcing a skip there left the game stuck on the notice.
  // A state is skipped only after it has run for kSettleFrames frames, like a player pressing A once it shows
  // (a precaution; the stall seen in testing came from state 2, below).
  constexpr int kSettleFrames = 15;
  const uint32_t state = REX_LOAD_U32(ctx.r3.u32 + 324);
  if (state != g_last_state) {
    REXLOG_INFO("boot screens: state {} -> {}", g_last_state, state);
    g_last_state = state;
    g_state_frames = 0;
  }
  ++g_state_frames;
  const bool skip = REXCVAR_GET(skip_intro_videos) && g_state_frames > kSettleFrames;
  // State 2 (legal text) treats a press as "wait for release" and only moves on once the time since its start
  // (u32 ms at this+328, from sub_82954E48) reaches 4500, so there the start time is moved back instead.
  if (skip && state == 2 && g_state_frames == kSettleFrames + 1) {
    REX_STORE_U32(ctx.r3.u32 + 328, REX_LOAD_U32(ctx.r3.u32 + 328) - 4500);
  }
  g_in_boot_screens = skip && (state == 1 || state == 3);
  __imp__sub_828AB9F0(ctx, base);
  g_in_boot_screens = false;
}

REX_HOOK_RAW(sub_828B60C8) {
  if (g_in_boot_screens) {
    ctx.r3.u64 = 1;
    return;
  }
  __imp__sub_828B60C8(ctx, base);
}
