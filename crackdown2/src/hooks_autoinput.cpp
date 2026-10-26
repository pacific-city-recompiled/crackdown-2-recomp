// crackdown2 - scripted pad input for unattended boot tests (--auto_input=START,A,RS_LEFT:800,...)
//
// Guest 0x829551D0 is a raw XamInputGetState(user r3, flags r4, state r5) thunk (import 0x83047EAC),
// and 0x829551D8 is XInputGetState(user r3, state r4) (sets r4=1 and r5=state). Same shape as Crackdown 1
// 0x82743798 (found by the XamInputGetState import call in the generated code, 2026-10-18).
// After --auto_input_delay_s, each token is held for its hold time (TOKEN:ms, default 300 ms), then the pad
// is neutral for --auto_input_gap_ms before the next token, on user --auto_input_user (default 0; 1 replays a second
// pad's input, like Austin's Xbox pad before --input_all_pads_user0). Without --auto_input nothing changes.
// Tokens: A B X Y START BACK UP DOWN LEFT RIGHT LB RB LS RS (buttons), LT RT (triggers),
// LS_UP/LS_DOWN/LS_LEFT/LS_RIGHT and RS_UP/RS_DOWN/RS_LEFT/RS_RIGHT (full stick deflection), WAIT (nothing).
// XINPUT_STATE: u32 packet; u16 buttons; u8 lt, rt; s16 lx, ly, rx, ry (big-endian in guest memory).

#include "generated/default/crackdown2_init.h"

#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <atomic>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_STRING(auto_input, "", "Crackdown2",
                      "Test only: comma list of pad inputs TOKEN[:hold_ms] played in order (see hooks_autoinput.cpp)");
REXCVAR_DEFINE_DOUBLE(auto_input_delay_s, 25.0, "Crackdown2", "Test only: seconds before --auto_input starts");
REXCVAR_DEFINE_INT32(auto_input_gap_ms, 2700, "Crackdown2", "Test only: neutral time after each --auto_input token");
REXCVAR_DEFINE_INT32(auto_input_user, 0, "Crackdown2", "Test only: guest user index --auto_input plays on");

namespace {
using Clock = std::chrono::steady_clock;

struct Step {
  uint16_t buttons = 0;
  uint8_t lt = 0, rt = 0;
  int16_t lx = 0, ly = 0, rx = 0, ry = 0;
  int64_t start_ms = 0, end_ms = 0;  // pressed during [start, end)
};

bool Parse(const std::string& name, Step& s) {
  static constexpr struct { const char* n; uint16_t b; } kButtons[] = {
      {"UP", 0x0001}, {"DOWN", 0x0002}, {"LEFT", 0x0004}, {"RIGHT", 0x0008}, {"START", 0x0010},
      {"BACK", 0x0020}, {"LS", 0x0040}, {"RS", 0x0080}, {"LB", 0x0100}, {"RB", 0x0200},
      {"A", 0x1000}, {"B", 0x2000}, {"X", 0x4000}, {"Y", 0x8000}};
  for (const auto& b : kButtons) {
    if (name == b.n) { s.buttons = b.b; return true; }
  }
  constexpr int16_t kMax = 32767, kMin = -32768;
  if (name == "LT") s.lt = 255;
  else if (name == "RT") s.rt = 255;
  else if (name == "LS_UP") s.ly = kMax;
  else if (name == "LS_DOWN") s.ly = kMin;
  else if (name == "LS_LEFT") s.lx = kMin;
  else if (name == "LS_RIGHT") s.lx = kMax;
  else if (name == "RS_UP") s.ry = kMax;
  else if (name == "RS_DOWN") s.ry = kMin;
  else if (name == "RS_LEFT") s.rx = kMin;
  else if (name == "RS_RIGHT") s.rx = kMax;
  else if (name != "WAIT") return false;
  return true;
}

const std::vector<Step>& Script() {
  static const std::vector<Step> steps = [] {
    std::vector<Step> v;
    std::stringstream ss(REXCVAR_GET(auto_input));
    int64_t t = 0;
    const int64_t gap = REXCVAR_GET(auto_input_gap_ms);
    for (std::string tok; std::getline(ss, tok, ',');) {
      if (tok.empty()) continue;
      Step s;
      int64_t hold = 300;
      if (auto colon = tok.find(':'); colon != std::string::npos) {
        hold = std::stoll(tok.substr(colon + 1));
        tok.resize(colon);
      }
      if (!Parse(tok, s)) {
        REXLOG_WARN("auto_input: unknown token '{}' skipped", tok);
        continue;
      }
      s.start_ms = t;
      s.end_ms = t + hold;
      t += hold + gap;
      v.push_back(s);
    }
    if (!v.empty()) REXLOG_INFO("auto_input: {} steps, {:.1f} s", v.size(), t / 1000.0);
    return v;
  }();
  return steps;
}

const Step* Current() {
  const auto& steps = Script();
  if (steps.empty()) return nullptr;
  static const auto start = Clock::now();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count() -
                  int64_t(REXCVAR_GET(auto_input_delay_s) * 1000.0);
  if (ms < 0) return nullptr;
  for (const Step& s : steps) {
    if (ms >= s.start_ms && ms < s.end_ms) return &s;
  }
  return nullptr;
}

std::atomic<uint16_t> g_test_buttons{0};
}  // namespace
namespace crackdown2 { uint16_t SweepWatchdogButtons(); }
namespace {

void Apply(PPCContext& ctx, uint8_t* base, uint32_t user, uint32_t state_ea) {
  if (!state_ea) return;
  if (user == 0) {
    if (const uint16_t extra = g_test_buttons.load(std::memory_order_relaxed) | crackdown2::SweepWatchdogButtons()) {
      REX_STORE_U16(state_ea + 4, uint16_t(REX_LOAD_U16(state_ea + 4) | extra));
      ctx.r3.u64 = 0;
    }
  }
  if (user != uint32_t(REXCVAR_GET(auto_input_user))) return;
  const Step* s = Current();
  if (!s) return;
  REX_STORE_U16(state_ea + 4, uint16_t(REX_LOAD_U16(state_ea + 4) | s->buttons));
  if (s->lt) REX_STORE_U8(state_ea + 6, s->lt);
  if (s->rt) REX_STORE_U8(state_ea + 7, s->rt);
  if (s->lx) REX_STORE_U16(state_ea + 8, uint16_t(s->lx));
  if (s->ly) REX_STORE_U16(state_ea + 10, uint16_t(s->ly));
  if (s->rx) REX_STORE_U16(state_ea + 12, uint16_t(s->rx));
  if (s->ry) REX_STORE_U16(state_ea + 14, uint16_t(s->ry));
  ctx.r3.u64 = 0;
}
}  // namespace

namespace crackdown2 {
// Test tools (ap_probe.cpp --ap_dev_sweep=jump): pad 0 buttons held in addition to the script, 0 = none.
void SetTestButtons(uint16_t buttons) { g_test_buttons.store(buttons, std::memory_order_relaxed); }
}  // namespace crackdown2

REX_HOOK_RAW(sub_829551D0) {
  const uint32_t user = ctx.r3.u32, state = ctx.r5.u32;
  __imp__sub_829551D0(ctx, base);
  Apply(ctx, base, user, state);
}

REX_HOOK_RAW(sub_829551D8) {
  const uint32_t user = ctx.r3.u32, state = ctx.r4.u32;
  __imp__sub_829551D8(ctx, base);
  Apply(ctx, base, user, state);
}
