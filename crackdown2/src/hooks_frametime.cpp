// crackdown2 - guest frame timing: --frame_log=<csv> and the F3 overlay's frame rate
//
// Guest 0x82385DA8 is the game's end-of-frame / present function: the only caller of the VdSwap import in the
// generated code (crackdown2_recomp.222.cpp), and the frame the loader stalls sat in (docs/re-notes.md "Load
// time"). Each call is one guest frame, so frame time = host time between calls. Same output format as
// Crackdown 1 (frame_time_us per row), so its tools/frametimes.py reads it.

#include "generated/default/crackdown2_init.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/ui/overlay/debug_overlay.h>

#include "host_tuning.h"

REXCVAR_DEFINE_STRING(frame_log, "", "Crackdown2", "Write one row per guest frame (frame_time_us) to this CSV");

namespace {
using Clock = std::chrono::steady_clock;
std::FILE* g_log = nullptr;
std::atomic<uint64_t> g_frames{0};
std::atomic<double> g_fps{0.0};

void OnFrame() {
  static const bool opened = [] {
    const std::string& path = REXCVAR_GET(frame_log);
    if (!path.empty() && (g_log = std::fopen(path.c_str(), "w"))) {
      std::setvbuf(g_log, nullptr, _IOFBF, 1 << 16);
      std::fputs("frame_time_us\n", g_log);
    }
    return true;
  }();
  (void)opened;
  static Clock::time_point last{}, window_start{};
  static uint64_t window_frames = 0;
  const Clock::time_point now = Clock::now();
  if (g_log && last != Clock::time_point{}) {
    std::fprintf(g_log, "%lld\n",
                 static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(now - last).count()));
    static uint32_t rows = 0;
    if (++rows % 60 == 0) std::fflush(g_log);  // test runs end with a kill, not a window close
  }
  last = now;
  g_frames.fetch_add(1, std::memory_order_relaxed);
  if (window_start == Clock::time_point{}) window_start = now;
  ++window_frames;
  const double seconds = std::chrono::duration<double>(now - window_start).count();
  if (seconds >= 0.5) {
    g_fps.store(window_frames / seconds, std::memory_order_relaxed);
    window_start = now;
    window_frames = 0;
  }
}
}  // namespace

namespace crackdown2 {
rex::ui::FrameStats GuestFrameStats() {
  rex::ui::FrameStats stats;
  stats.frame_count = g_frames.load(std::memory_order_relaxed);
  stats.fps = g_fps.load(std::memory_order_relaxed);
  stats.frame_time_ms = stats.fps > 0 ? 1000.0 / stats.fps : 0;
  return stats;
}
void FlushFrameLog() {
  if (g_log) std::fflush(g_log);
}
}  // namespace crackdown2

REX_HOOK_RAW(sub_82385DA8) {
  OnFrame();
  __imp__sub_82385DA8(ctx, base);
}
