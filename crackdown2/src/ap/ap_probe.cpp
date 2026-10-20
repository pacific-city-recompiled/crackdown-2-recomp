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
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <vector>
#include <fstream>
#include <set>
#include <sstream>
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
REXCVAR_DEFINE_DOUBLE(ap_dev_teleport_delay_s, 8.0, "Crackdown2",
                      "With --ap_dev_teleport_orb / --ap_dev_sweep: delay in seconds");
REXCVAR_DEFINE_STRING(ap_dev_sweep, "", "Crackdown2", "Test tool: pickups or jump (see ap_probe.cpp)");
REXCVAR_DEFINE_STRING(ap_dev_sweep_out, "", "Crackdown2", "Test tool: CSV for --ap_dev_sweep");
REXCVAR_DEFINE_INT32(ap_dev_sweep_from, 0, "Crackdown2", "Test tool: first registry ordinal for --ap_dev_sweep");
REXCVAR_DEFINE_INT32(ap_dev_sweep_to, 9999, "Crackdown2", "Test tool: last registry ordinal for --ap_dev_sweep");
REXCVAR_DEFINE_STRING(ap_dev_sweep_skip, "", "Crackdown2",
                      "Test tool: skip pickups already in these sweep CSVs (comma-separated paths), to resume");
REXCVAR_DEFINE_BOOL(ap_dev_sweep_ring, false, "Crackdown2", "Test tool: --ap_dev_sweep=pickups also drops around orbs");

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

namespace crackdown2 { void SetTestButtons(uint16_t buttons); }

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

// --ap_dev_teleport_orb / --ap_dev_sweep (Crackdown 1's --teleport, hooks_diag_physics.cpp, on Crackdown 2's
// controller). The character controller update sub_822498F8 (fp match 0.18 to Crackdown 1's sub_822A9E78) keeps
// the controller's position at +656 (Crackdown 1: +336) and the Havok character proxy at +560 (+660); setPosition
// is sub_82BFEC28 (identical 5-instruction body to Crackdown 1's sub_829D3560). +304 is the owning character, so
// only the agent's controller is moved: [[0x83800F88]+136]-28.
//
// --ap_dev_sweep=pickups: teleport onto every registry pickup in [--ap_dev_sweep_from, --ap_dev_sweep_to] (online
// orbs skipped), wait until the agent settles, and write a CSV row: whether the pickup was collected and where the
// agent landed (the surface under it). With --ap_dev_sweep_ring, pickups that are not ground-tier (agility type
// >= 1, or y >= 150) also get four drops from 2 m above the orb at 12 m around it: the surfaces a player climbs from.
// --ap_dev_sweep=jump: on the spot where the agent stands, set Agility 1..5 (sub_82753EB8) and hold A for 0.3 s and
// 1.5 s; each row is the jump's apex above the start.
namespace {
using Clock = std::chrono::steady_clock;
struct Target {
  std::string what;
  uint32_t ordinal = 0;
  int sample = 0;
  float pos[3]{};
  int level = 0, hold_ms = 0;
  int retries = 0;
};
struct Sweep {
  bool started = false, done = false, active = false;
  std::vector<Target> targets;
  size_t next = 0;
  Clock::time_point step_start{}, start{};
  float last_y = 0, base_y = 0, max_y = 0;
  int still = 0;
  Clock::time_point release_until{};  // a stuck agent (hanging from a ledge) lets go before the next teleport
  std::FILE* out = nullptr;
};
Sweep g_sweep;
uint32_t g_controller = 0;  // the agent's character controller (sub_822498F8 r3)
// Last time the sweep ran (ms since the steady epoch). The game update stops on the death screen, where only a pad
// press (A = Regenerate) continues, so the input hook pulses A when this goes stale (run crackdown2_015).
std::atomic<int64_t> g_sweep_alive_ms{0};
int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

uint32_t AgentCharacter(uint8_t* base) {
  const uint32_t player = REX_LOAD_U32(0x83800F88);
  const uint32_t component = player ? REX_LOAD_U32(player + 136) : 0;
  return component >= 28 ? component - 28 : 0;
}

void Teleport(PPCContext& ctx, uint8_t* base, uint32_t controller, const float (&v)[3]) {
  const uint32_t proxy = REX_LOAD_U32(controller + 560);
  const uint32_t pos = controller + 656;
  for (int i = 0; i < 3; ++i) REX_STORE_U32(pos + 4 * i, std::bit_cast<uint32_t>(v[i]));
  for (uint32_t i = 0; i < 16; i += 4) REX_STORE_U32(proxy + 16 + i, 0);
  const PPCContext saved = ctx;
  ctx.r3.u64 = proxy;
  ctx.r4.u64 = pos;
  sub_82BFEC28(ctx, base);
  ctx = saved;
}

void BuildTargets(uint8_t* base, uint32_t controller) {
  const std::string mode = REXCVAR_GET(ap_dev_sweep);
  auto& t = g_sweep.targets;
  const uint32_t stats = Stats(base);
  if (const int32_t ordinal = REXCVAR_GET(ap_dev_teleport_orb); ordinal >= 0 && mode.empty()) {
    if (stats && uint32_t(ordinal) < REX_LOAD_U32(stats + 4012)) {
      const uint32_t p = REX_LOAD_U32(stats + 12 + 4 * ordinal);
      Target x{"pickup", uint32_t(ordinal)};
      for (int i = 0; i < 3; ++i) x.pos[i] = F32(base, p + 144 + 4 * i);
      t.push_back(x);
    }
  } else if (mode == "pickups") {
    const uint32_t count = stats ? REX_LOAD_U32(stats + 4012) : 0;
    const uint32_t last = std::min<uint32_t>(count, uint32_t(REXCVAR_GET(ap_dev_sweep_to)) + 1);
    std::set<uint32_t> skip;  // resume: a sweep stops when the agent dies (it cannot regenerate it)
    std::stringstream files(REXCVAR_GET(ap_dev_sweep_skip));
    for (std::string file; std::getline(files, file, ',');) {
      std::ifstream in(file);
      for (std::string line; std::getline(in, line);)
        if (line.rfind("pickup,", 0) == 0) skip.insert(uint32_t(std::stoul(line.substr(7))));
    }
    for (uint32_t i = uint32_t(std::max(0, REXCVAR_GET(ap_dev_sweep_from))); i < last; ++i) {
      if (skip.contains(i)) continue;
      const uint32_t p = REX_LOAD_U32(stats + 12 + 4 * i);
      const uint32_t type = REX_LOAD_U32(p + 136);
      if (type == 10) continue;  // online orbs: Xbox Live co-op only
      const float orb[3] = {F32(base, p + 144), F32(base, p + 148), F32(base, p + 152)};
      Target x{"pickup", i};
      std::copy(orb, orb + 3, x.pos);
      t.push_back(x);
      if (REXCVAR_GET(ap_dev_sweep_ring) && ((type >= 1 && type <= 3) || orb[1] >= 150.0f)) {
        for (int k = 0; k < 4; ++k) {
          Target r{"ring", i, k};
          r.pos[0] = orb[0] + (k == 0 ? 12.f : k == 1 ? -12.f : 0.f);
          r.pos[1] = orb[1] + 2.f;
          r.pos[2] = orb[2] + (k == 2 ? 12.f : k == 3 ? -12.f : 0.f);
          t.push_back(r);
        }
      }
    }
    // Visit pickups nearest-first (each with its ring drops), so the world around the next target is mostly
    // streamed in already.
    std::vector<std::vector<Target>> groups;
    for (const auto& x : t) {
      if (x.what == "pickup") groups.emplace_back();
      groups.back().push_back(x);
    }
    std::vector<Target> ordered;
    std::vector<bool> used(groups.size());
    float at[3] = {F32(base, controller + 656), F32(base, controller + 660), F32(base, controller + 664)};
    for (size_t n = 0; n < groups.size(); ++n) {
      size_t best = 0;
      float best_d = 1e30f;
      for (size_t i = 0; i < groups.size(); ++i) {
        if (used[i]) continue;
        const float* q = groups[i][0].pos;
        const float d = (q[0] - at[0]) * (q[0] - at[0]) + (q[2] - at[2]) * (q[2] - at[2]);
        if (d < best_d) best_d = d, best = i;
      }
      used[best] = true;
      std::copy(groups[best][0].pos, groups[best][0].pos + 3, at);
      ordered.insert(ordered.end(), groups[best].begin(), groups[best].end());
    }
    t = std::move(ordered);
  } else if (mode == "jump") {
    const float here[3] = {F32(base, controller + 656), F32(base, controller + 660), F32(base, controller + 664)};
    for (int level = 1; level <= 5; ++level) {
      for (int hold : {300, 1500}) {
        Target x{"jump"};
        std::copy(here, here + 3, x.pos);
        x.level = level;
        x.hold_ms = hold;
        t.push_back(x);
      }
    }
  }
  const std::string& path = REXCVAR_GET(ap_dev_sweep_out);
  if (!path.empty() && (g_sweep.out = std::fopen(path.c_str(), "w")))
    std::fputs("what,ordinal,sample,type,tx,ty,tz,x,y,z,collected,level,hold_ms,apex,seconds,retries\n", g_sweep.out);
  REXLOG_INFO("ap_dev_sweep: {} targets ({})", t.size(), mode.empty() ? "teleport_orb" : mode);
}

// Caps the agent's fall speed at 8 m/s: the Havok proxy's velocity (proxy+16, y at +20) and the controller's
// own velocity vector at +672 (y at +676; Crackdown 1 zeroed the matching +352 for --teleport).
void ClampFall(uint8_t* base, uint32_t controller) {
  constexpr float kMax = -8.0f;
  const uint32_t proxy = REX_LOAD_U32(controller + 560);
  if (proxy && F32(base, proxy + 20) < kMax) REX_STORE_U32(proxy + 20, std::bit_cast<uint32_t>(kMax));
  if (F32(base, controller + 676) < kMax) REX_STORE_U32(controller + 676, std::bit_cast<uint32_t>(kMax));
}

void SetAgility(PPCContext& ctx, uint8_t* base, int level) {
  const uint32_t character = AgentCharacter(base);
  const uint32_t skills = character + 11460;
  if (!character || REX_LOAD_U32(skills + 4) != character) return;
  const PPCContext saved = ctx;
  ctx.r3.u64 = skills;
  ctx.r4.u64 = 0;
  ctx.r5.u64 = uint32_t(level);
  __imp__sub_82753EB8(ctx, base);
  ctx = saved;
}

void StepSweep(PPCContext& ctx, uint8_t* base, uint32_t controller) {
  auto& w = g_sweep;
  if (w.done) return;
  const uint32_t character = AgentCharacter(base);
  if (!character || REX_LOAD_U32(controller + 304) != character) return;
  // Only a live agent counts: a dead one (deep water, run crackdown2_016) keeps the update running but loses its
  // controller, and the watchdog's A press regenerates it.
  g_sweep_alive_ms.store(NowMs(), std::memory_order_relaxed);
  const auto now = Clock::now();
  if (!w.started) {
    if (!Stats(base) || REX_LOAD_U32(Stats(base) + 4012) == 0 || !REX_LOAD_U32(controller + 560)) return;
    if (w.start == Clock::time_point{}) w.start = now;
    if (now - w.start < std::chrono::duration<double>(REXCVAR_GET(ap_dev_teleport_delay_s))) return;
    w.started = true;
    BuildTargets(base, controller);
    // Pickup sweeps run at Agility 5: the ring drops fall up to ~60 m and killed an Agility 1 agent.
    if (REXCVAR_GET(ap_dev_sweep) == "pickups") SetAgility(ctx, base, 5);
  }
  const float y = F32(base, controller + 660);
  const double elapsed = std::chrono::duration<double>(now - w.step_start).count();
  if (w.active) {
    const Target& t = w.targets[w.next - 1];
    bool finished = false;
    if (t.what == "jump") {
      const bool hold = elapsed > 1.0 && elapsed * 1000.0 < 1000.0 + t.hold_ms;
      crackdown2::SetTestButtons(hold ? 0x1000 : 0);
      if (elapsed < 1.0) w.base_y = w.max_y = y;
      w.max_y = std::max(w.max_y, y);
      finished = elapsed > 1.0 + t.hold_ms / 1000.0 + 3.0;
    } else {
      // Long drops kill the agent even at Agility 5 (runs crackdown2_014/016/018): cap the fall speed.
      ClampFall(base, controller);
      // Hold the agent at the target for 0.8 s so the area's collision streams in first: falling through unloaded
      // ground after a long teleport killed it (run crackdown2_020, pickup 375).
      if (elapsed < 0.8) {
        Teleport(ctx, base, controller, t.pos);
        w.still = 0;
        w.last_y = y;
        return;
      }
      w.still = std::fabs(y - w.last_y) < 0.01f ? w.still + 1 : 0;
      finished = (elapsed > 1.3 && w.still >= 20) || elapsed > 20.0;
    }
    w.last_y = y;
    if (!finished) return;
    w.active = false;
    // Teleports don't move an agent that hangs from a ledge (sweep run crackdown2_012). If the agent is not where
    // it was sent, press B (let go), then send it again; up to three times.
    Target& cur = w.targets[w.next - 1];
    const float dx = F32(base, controller + 656) - cur.pos[0], dz = F32(base, controller + 664) - cur.pos[2];
    if (cur.what != "jump" && dx * dx + dz * dz > 9.0f && cur.retries < 3) {
      ++cur.retries;
      --w.next;
      w.release_until = now + std::chrono::milliseconds(500);
      // B lets go of a ledge; A also regenerates a dead agent (falls kill at low Agility, run crackdown2_014).
      crackdown2::SetTestButtons(cur.retries == 1 ? 0x2000 : 0x1000);
      return;
    }
    uint32_t type = 0, collected = 0;
    if (t.what != "jump") {
      const uint32_t p = REX_LOAD_U32(Stats(base) + 12 + 4 * t.ordinal);
      type = REX_LOAD_U32(p + 136);
      collected = REX_LOAD_U8(p + 160) == 0;
    }
    if (w.out) {
      std::fprintf(w.out, "%s,%u,%d,%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%u,%d,%d,%.2f,%.2f,%d\n", t.what.c_str(),
                   t.ordinal, t.sample, type, t.pos[0], t.pos[1], t.pos[2], F32(base, controller + 656), y,
                   F32(base, controller + 664), collected, t.level, t.hold_ms, w.max_y - w.base_y, elapsed, t.retries);
      std::fflush(w.out);
    }
  }
  if (w.next >= w.targets.size()) {
    w.done = true;
    crackdown2::SetTestButtons(0);
    REXLOG_INFO("ap_dev_sweep: done, {} targets", w.targets.size());
    return;
  }
  if (now < w.release_until) return;
  if (w.release_until != Clock::time_point{}) {
    w.release_until = {};
    crackdown2::SetTestButtons(0);
  }
  if (!REX_LOAD_U32(controller + 560)) return;  // no character proxy yet (crash at guest 0x10, run crackdown2_011)
  const Target& t = w.targets[w.next++];
  if (t.what == "jump") SetAgility(ctx, base, t.level);
  Teleport(ctx, base, controller, t.pos);
  REXLOG_INFO("ap_dev_sweep: {} {} sample {} -> ({:.1f} {:.1f} {:.1f})", t.what, t.ordinal, t.sample, t.pos[0],
              t.pos[1], t.pos[2]);
  w.active = true;
  w.step_start = now;
  w.still = 0;
  w.last_y = w.base_y = w.max_y = t.pos[1];
}
}  // namespace

namespace crackdown2 {
// Called from the pad hook (hooks_autoinput.cpp): extra buttons while a running sweep is stuck outside gameplay.
uint16_t SweepWatchdogButtons() {
  const int64_t alive = g_sweep_alive_ms.load(std::memory_order_relaxed);
  if (!alive || g_sweep.done) return 0;
  const int64_t now = NowMs();
  if (now - alive <= 4000) return 0;
  const int64_t phase = (now / 400) % 4;  // A (Regenerate), then Y (Regenerate at nearest drop point)
  return phase == 0 ? 0x1000 : phase == 2 ? 0x8000 : 0;
}
}  // namespace crackdown2

namespace crackdown2::ap {
// Runs the sweep from the game update (hooks_framerate.cpp), not from the controller update: the controller update
// stops while the agent hangs from a ledge (sweep run crackdown2_010 stalled there).
void ProbeUpdate(PPCContext& ctx, uint8_t* base) {
  static const bool on = REXCVAR_GET(ap_dev_teleport_orb) >= 0 || !REXCVAR_GET(ap_dev_sweep).empty();
  if (on && g_controller) StepSweep(ctx, base, g_controller);
}
}  // namespace crackdown2::ap

// Remembers the agent's controller for ProbeUpdate.
REX_HOOK_RAW(sub_822498F8) {
  static const bool on = REXCVAR_GET(ap_dev_teleport_orb) >= 0 || !REXCVAR_GET(ap_dev_sweep).empty();
  if (on) {
    const uint32_t character = AgentCharacter(base);
    if (character && REX_LOAD_U32(ctx.r3.u32 + 304) == character) {
      g_controller = ctx.r3.u32;
      if (g_sweep.active && g_sweep.next && g_sweep.targets[g_sweep.next - 1].what != "jump")
        ClampFall(base, ctx.r3.u32);
    }
  }
  __imp__sub_822498F8(ctx, base);
}
