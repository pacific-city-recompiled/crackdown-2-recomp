// crackdown2 - scripted pad input for unattended boot tests (--auto_input=A,START,...)
//
// Guest 0x829551D0 is a raw XamInputGetState(user r3, flags r4, state r5) thunk (import 0x83047EAC),
// and 0x829551D8 is XInputGetState(user r3, state r4) (sets r4=1 and r5=state). Same shape as Crackdown 1
// 0x82743798 (found by the XamInputGetState import call in the generated code, 2026-10-18).
// Each poll after a startup delay presses the next button of the list for 300 ms, every 3 s.
// User 0 only. Without --auto_input nothing changes.

#include "generated/default/crackdown2_init.h"

#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/hook.h>

REXCVAR_DEFINE_STRING(auto_input, "", "Crackdown2",
                      "Test only: comma list of pad buttons (A,B,X,Y,START,BACK,UP,DOWN,LEFT,RIGHT) "
                      "pressed one after another, 300 ms each, every 3 s, after 25 s");

namespace {
using Clock = std::chrono::steady_clock;

uint16_t Button(const std::string& n) {
  if (n == "A") return 0x1000;
  if (n == "B") return 0x2000;
  if (n == "X") return 0x4000;
  if (n == "Y") return 0x8000;
  if (n == "START") return 0x0010;
  if (n == "BACK") return 0x0020;
  if (n == "UP") return 0x0001;
  if (n == "DOWN") return 0x0002;
  if (n == "LEFT") return 0x0004;
  if (n == "RIGHT") return 0x0008;
  return 0;
}

uint16_t Current() {
  static const std::vector<uint16_t> seq = [] {
    std::vector<uint16_t> v;
    std::stringstream ss(REXCVAR_GET(auto_input));
    for (std::string t; std::getline(ss, t, ',');)
      if (uint16_t b = Button(t)) v.push_back(b);
    return v;
  }();
  static const auto start = Clock::now();
  if (seq.empty()) return 0;
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
  if (ms < 25000) return 0;
  const auto t = ms - 25000;
  const size_t step = size_t(t / 3000);
  if (step >= seq.size() || t % 3000 >= 300) return 0;
  return seq[step];
}

void Apply(PPCContext& ctx, uint8_t* base, uint32_t user, uint32_t state_ea) {
  if (user != 0 || !state_ea) return;
  if (uint16_t b = Current()) {
    REX_STORE_U16(state_ea + 4, uint16_t(REX_LOAD_U16(state_ea + 4) | b));
    ctx.r3.u64 = 0;
  }
}
}  // namespace

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
