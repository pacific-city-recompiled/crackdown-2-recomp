// crackdown2 - Archipelago Phase 0 probe (--ap_probe, off by default; logs only, changes nothing)
//
// Static findings this checks at runtime (TU 0.0.5.1 image; docs/re-notes.md "Randomizer notes"):
// - Player stats object [0x83800E20] (lis -31872, lwz 3616). sub_8270BA38 registers every orb pickup at load:
//   stats+12+4*n = pickup (n = u32 stats+4012, stored in pickup+128), then counts it by pickup type (+136):
//   4 hidden (+4016 total), 0..3 agility (+4020), 10 online (+4024), plus per-district totals by +140.
// - sub_82709D18(pickup r3, active r4) is the pickup's SetActive. With +160 set and r4 = 0 (collected) it runs
//   the type's stat counter: sub_8270B8D0 agility (+4032, achievements 17/18), sub_8270B778 hidden (+4028),
//   then clears +160. Online orbs (sub_82711240) and audio logs (sub_8270F9B0) override it and call it.
// - cSkillSet: owner character +4, max level byte +8+skill, level byte +14+skill, XP floats +20+4s and +44+4s.
//   Skill order 0 Agility, 1 Driving, 2 Explosives, 3 Strength, 4 Firearms (stats screen sub_822B4EA0).
//   sub_82753CA0(skillset, skill, f1 xp) adds XP and levels up through sub_8232BE00; sub_82753EB8(skillset,
//   skill, level) sets a level (Crackdown 1's 82543BB0 / 82543DB0).

#include "generated/default/crackdown2_init.h"
#include "bridge.h"

#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_BOOL(ap_probe, false, "Crackdown2",
                    "Diagnostic: log orb pickups, XP and skill-level calls, and dump the orb registry");
REXCVAR_DEFINE_STRING(ap_probe_dump, "", "Crackdown2",
                      "Diagnostic: with --ap_probe, write the orb registry as JSON to this file");
REXCVAR_DEFINE_INT32(ap_dev_teleport_orb, -1, "Crackdown2",
                     "Test tool: move the agent onto this registry ordinal's pickup once, "
                     "--ap_dev_teleport_delay_s after the agent's controller first updates");
REXCVAR_DEFINE_DOUBLE(ap_dev_teleport_delay_s, 8.0, "Crackdown2", "With --ap_dev_teleport_orb: delay in seconds");

namespace {
constexpr uint32_t kStatsPtr = 0x83800E20;

float F32(uint8_t* base, uint32_t ea) { return std::bit_cast<float>(REX_LOAD_U32(ea)); }

uint32_t Stats(uint8_t* base) { return REX_LOAD_U32(kStatsPtr); }

// Writes the registry once it has been stable (same count) for 300 frames, and again if the count changes.
void DumpRegistry(uint8_t* base) {
  static uint32_t last_count = 0, stable = 0, dumped = ~0u;
  const uint32_t stats = Stats(base);
  if (!stats) return;
  const uint32_t count = REX_LOAD_U32(stats + 4012);
  if (count != last_count) {
    last_count = count;
    stable = 0;
    return;
  }
  if (++stable != 300 || count == dumped || count == 0 || count > 1000) return;
  dumped = count;
  REXLOG_INFO("ap_probe: registry count {} totals hidden {} agility {} online {} collected hidden {} agility {} "
              "online {}", count, REX_LOAD_U32(stats + 4016), REX_LOAD_U32(stats + 4020),
              REX_LOAD_U32(stats + 4024), REX_LOAD_U32(stats + 4028), REX_LOAD_U32(stats + 4032),
              REX_LOAD_U32(stats + 4036));
  const std::string& path = REXCVAR_GET(ap_probe_dump);
  if (path.empty()) return;
  std::FILE* f = std::fopen(path.c_str(), "w");
  if (!f) return;
  std::fputs("[\n", f);
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t p = REX_LOAD_U32(stats + 12 + 4 * i);
    std::fprintf(f, "%s{\"n\":%u,\"ptr\":\"%08X\",\"vt\":\"%08X\",\"reg\":%u,\"type\":%u,\"district\":%u,"
                 "\"pos\":[%.3f,%.3f,%.3f],\"active\":%u}\n", i ? "," : "", i, p, REX_LOAD_U32(p),
                 REX_LOAD_U32(p + 128), REX_LOAD_U32(p + 136), REX_LOAD_U32(p + 140), F32(base, p + 144),
                 F32(base, p + 148), F32(base, p + 152), REX_LOAD_U8(p + 160));
  }
  std::fputs("]\n", f);
  std::fclose(f);
  REXLOG_INFO("ap_probe: wrote {} pickups to {}", count, path);
}

void LogSkills(uint8_t* base, uint32_t set, const char* what) {
  REXLOG_INFO("ap_probe: {} skillset {:08X} owner {:08X} max {} {} {} {} {} level {} {} {} {} {} xp {:.1f} {:.1f} "
              "{:.1f} {:.1f} {:.1f}", what, set, REX_LOAD_U32(set + 4), (int8_t)REX_LOAD_U8(set + 8),
              (int8_t)REX_LOAD_U8(set + 9), (int8_t)REX_LOAD_U8(set + 10), (int8_t)REX_LOAD_U8(set + 11),
              (int8_t)REX_LOAD_U8(set + 12), (int8_t)REX_LOAD_U8(set + 14), (int8_t)REX_LOAD_U8(set + 15),
              (int8_t)REX_LOAD_U8(set + 16), (int8_t)REX_LOAD_U8(set + 17), (int8_t)REX_LOAD_U8(set + 18),
              F32(base, set + 44), F32(base, set + 48), F32(base, set + 52), F32(base, set + 56), F32(base, set + 60));
}
}  // namespace

namespace crackdown2::ap {
void ProbeFrame(uint8_t* base) {
  static const bool on = REXCVAR_GET(ap_probe);
  if (on) DumpRegistry(base);
}
}  // namespace crackdown2::ap

namespace crackdown2::ap {
// Called by bridge.cpp's SetActive hook before the native body.
void ProbeSetActive(PPCContext& ctx, uint8_t* base) {
  static const bool on = REXCVAR_GET(ap_probe);
  if (!on) return;
  const uint32_t p = ctx.r3.u32;
  REXLOG_INFO("ap_probe: SetActive pickup {:08X} vt {:08X} reg {} type {} district {} active {} -> {} pos {:.1f} "
              "{:.1f} {:.1f} lr {:08X}", p, REX_LOAD_U32(p), REX_LOAD_U32(p + 128), REX_LOAD_U32(p + 136),
              REX_LOAD_U32(p + 140), REX_LOAD_U8(p + 160), ctx.r4.u32 & 0xFF, F32(base, p + 144),
              F32(base, p + 148), F32(base, p + 152), (uint32_t)ctx.lr);
}

// Called by bridge.cpp's AddXP hook when the XP is not dropped: runs the native body.
void ProbeAddXP(PPCContext& ctx, uint8_t* base) {
  static const bool on = REXCVAR_GET(ap_probe);
  const uint32_t set = ctx.r3.u32, skill = ctx.r4.u32;
  const double xp = ctx.f1.f64;
  const uint32_t lr = (uint32_t)ctx.lr;
  __imp__sub_82753CA0(ctx, base);
  if (on) {
    REXLOG_INFO("ap_probe: AddXP skill {} xp {:.2f} lr {:08X}", skill, xp, lr);
    LogSkills(base, set, "after AddXP");
  }
}
}  // namespace crackdown2::ap

REX_HOOK_RAW(sub_82753EB8) {
  static const bool on = REXCVAR_GET(ap_probe);
  const uint32_t set = ctx.r3.u32, skill = ctx.r4.u32, level = ctx.r5.u32 & 0xFF;
  const uint32_t lr = (uint32_t)ctx.lr;
  __imp__sub_82753EB8(ctx, base);
  if (on) {
    REXLOG_INFO("ap_probe: SetLevel skill {} level {} lr {:08X}", skill, level, lr);
    LogSkills(base, set, "after SetLevel");
  }
}

// --ap_dev_teleport_orb (Crackdown 1's --teleport, hooks_diag_physics.cpp, on Crackdown 2's controller). The
// character controller update sub_822498F8 (fp match 0.18 to Crackdown 1's sub_822A9E78) keeps the controller's
// position at +656 (Crackdown 1: +336) and the Havok character proxy at +560 (+660); setPosition is
// sub_82BFEC28 (identical 5-instruction body to Crackdown 1's sub_829D3560). +304 is the owning character, so
// only the agent's controller is moved: [[0x83800F88]+136]-28.
namespace {
void MaybeTeleport(PPCContext& ctx, uint8_t* base, uint32_t controller) {
  static const int32_t ordinal = REXCVAR_GET(ap_dev_teleport_orb);
  static bool done = false;
  static std::chrono::steady_clock::time_point start{};
  if (ordinal < 0 || done) return;
  const uint32_t player = REX_LOAD_U32(0x83800F88);
  const uint32_t component = player ? REX_LOAD_U32(player + 136) : 0;
  if (component < 28 || REX_LOAD_U32(controller + 304) != component - 28) return;
  const auto now = std::chrono::steady_clock::now();
  if (start == std::chrono::steady_clock::time_point{}) start = now;
  if (now - start < std::chrono::duration<double>(REXCVAR_GET(ap_dev_teleport_delay_s))) return;
  done = true;
  const uint32_t stats = Stats(base);
  if (!stats || uint32_t(ordinal) >= REX_LOAD_U32(stats + 4012)) {
    REXLOG_ERROR("ap_dev_teleport: no registry entry {}", ordinal);
    return;
  }
  const uint32_t pickup = REX_LOAD_U32(stats + 12 + 4 * ordinal);
  const uint32_t proxy = REX_LOAD_U32(controller + 560);
  const uint32_t pos = controller + 656;
  for (int i = 0; i < 3; ++i) REX_STORE_U32(pos + 4 * i, REX_LOAD_U32(pickup + 144 + 4 * i));
  for (uint32_t i = 0; i < 16; i += 4) REX_STORE_U32(proxy + 16 + i, 0);
  const PPCContext saved = ctx;
  ctx.r3.u64 = proxy;
  ctx.r4.u64 = pos;
  sub_82BFEC28(ctx, base);
  ctx = saved;
  REXLOG_INFO("ap_dev_teleport: controller {:08X} proxy {:08X} -> pickup {} ({:.1f} {:.1f} {:.1f}) active {}",
              controller, proxy, ordinal, F32(base, pos), F32(base, pos + 4), F32(base, pos + 8),
              REX_LOAD_U8(pickup + 160));
}
}  // namespace

REX_HOOK_RAW(sub_822498F8) {
  MaybeTeleport(ctx, base, ctx.r3.u32);
  __imp__sub_822498F8(ctx, base);
}

