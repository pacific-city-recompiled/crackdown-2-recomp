// In-process Archipelago bridge for Crackdown 2 (same design as Crackdown 1's crackdown/src/ap/bridge.cpp).
//
// One coordinator, guarded by g_mutex, owns SessionState. The network thread (client.cpp) only queues packets;
// they are applied here on the game thread after each game update (hooks_framerate.cpp, sub_82915C70).
//
// Guest contracts (TU 0.0.5.1; docs/re-notes.md "Randomizer notes", probe run crackdown2_003):
// - Player stats [0x83800E20]: pickup registry at +12 + 4 * ordinal, count +4012 (932 = 500 agility, 80 online,
//   300 hidden orbs, 52 audio logs, in that order). Pickup +136 type, +140 district, +144 position, +160 active.
// - sub_8270B448 resets the stats object (totals/counts to 0) before a world (re)load: a transition marker.
// - sub_8226B2F0 (stats save interface, vtable 0x82139950) reads the active flags of a native load and sets them
//   through the pickup's SetActive; sub_822708A8 (vtable 0x82139954) writes them for a native save.
// - sub_82709D18(pickup, active) is SetActive. Gameplay collection calls it with +160 = 1 and r4 = 0; the load
//   serializer above and sub_82869528 (a mode method that clears audio logs) call it too and are not checks.
// - cSkillSet = character + 11460 (owner at +4), level byte +14 + skill. sub_82753CA0 adds XP (dropped for the
//   agent in a bound save); sub_82753EB8(set, skill, level) sets an absolute level.
//
// Save binding differs from Crackdown 1, which hashed the native save file: Crackdown 2 writes several XContent
// files, so the lineage records a fingerprint (SHA-256) of the progress the native serializer writes (the 932
// active flags), computed from guest memory in the serializer itself. A native load is this binding's save
// only when the same flags come back.
#include "generated/default/crackdown2_init.h"

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include <bit>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <toml++/toml.hpp>

#include "bridge.h"
#include "catalog.h"
#include "client.h"
#include "session_state.h"
#include "sha256.h"

REXCVAR_DEFINE_STRING(ap_server, "", "Archipelago",
                      "Archipelago server (host:port, ws://... or wss://...); empty = offline");
REXCVAR_DEFINE_STRING(ap_slot, "", "Archipelago", "Archipelago slot (player) name");
REXCVAR_DEFINE_STRING(ap_password, "", "Archipelago",
                      "Archipelago room password; prefer the launcher, which keeps it in "
                      "<user_data>/archipelago/connection.toml (cvar values can reach debug logs)");
REXCVAR_DEFINE_BOOL(ap_dev_bind_load, false, "Archipelago",
                    "Developer: bind the loaded native save when the profile has no randomized session yet "
                    "(normally only a new game binds); for tests from an existing save");

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace crackdown2::ap {
namespace {
constexpr const char* kTitle = "4D5308BC";
constexpr uint32_t kStatsGlobal = 0x83800E20;
constexpr uint32_t kPlayerGlobal = 0x83800F88;
constexpr uint32_t kSkillSetOffset = 11460;
constexpr uint32_t kRegistrySize = 932;
constexpr uint32_t kClearLogsBegin = 0x82869528, kClearLogsEnd = 0x82869700;  // sub_82869528 call sites
const char* const kSkillNames[] = {"Agility", "Driving", "Explosives", "Strength", "Firearms"};

enum class Mode { idle, transition, bound, foreign };

struct Bridge {
  bool enabled = false;
  fs::path root, sidecar;
  const Catalog* catalog = nullptr;
  std::unique_ptr<SessionState> session;
  std::optional<Contract> stored;  // contract of an existing sidecar
  std::string halt;                // non-empty: local AP processing stopped
  std::string offline;             // non-empty: this server was rejected; play offline

  Client client;
  uint64_t generation = 0;  // generation of the validated connection
  bool server_ready = false;
  std::optional<Contract> server;  // validated server contract (profile/save unset)
  std::string room_seed;
  std::set<int64_t> server_checked;  // latest checked set from Connected/RoomUpdate, for a session bound later
  Clock::time_point last_check_send{};
  std::map<int, std::string> players;  // slot -> name, for messages

  Mode mode = Mode::idle;
  bool load_seen = false, load_bound = false;
  std::string load_reason;
  uint32_t stable_frames = 0;
  std::vector<int64_t> retry_observe;
  Clock::time_point last_grant_try{}, last_observe_retry{};
  uint64_t goal_generation = 0;
};
std::mutex g_mutex;
Bridge g;
thread_local bool g_in_native_load = false;

const Binding& ActiveBinding() { return g.session->binding(); }

// Local failure (sidecar, binding, guest contract): stop all AP processing.
void Halt(const std::string& why) {
  if (!g.halt.empty()) return;
  g.halt = why;
  REXLOG_ERROR("ap: HALTED: {}. AP processing stopped; restart after fixing the cause.", why);
  g.server_ready = false;
  g.client.RequestStop();
}

// Server-side rejection (wrong seed/slot/password, invalid packet): stop talking to this server only. A bound
// save keeps its rules offline; checks persist and are sent on a later connection.
void Disconnect(const std::string& why) {
  if (!g.offline.empty()) return;
  g.offline = why;
  REXLOG_ERROR("ap: server rejected: {}. Playing offline; restart with the right server to sync.", why);
  g.server_ready = false;
  g.generation = 0;
  g.client.RequestStop();
}

// --ap_password if given, else the password the launcher keeps in archipelago/connection.toml (never argv).
std::string Password() {
  if (auto cvar = REXCVAR_GET(ap_password); !cvar.empty()) return cvar;
  try {
    const auto path = g.root / "archipelago" / "connection.toml";
    if (!fs::exists(path)) return {};
    return toml::parse_file(path.string())["password"].value<std::string>().value_or("");
  } catch (const std::exception& e) {
    REXLOG_WARN("ap: cannot read the saved connection settings: {}", e.what());
    return {};
  }
}

std::string NewUuid() {
  std::random_device random;
  char text[33];
  for (int i = 0; i < 4; ++i) std::snprintf(text + 8 * i, 9, "%08x", random());
  return text;
}

// One id per profile, kept across connections (AP Connect.uuid).
std::string ClientUuid() {
  static std::string uuid;
  if (!uuid.empty()) return uuid;
  const auto path = g.root / "archipelago" / "client_uuid";
  if (std::ifstream in{path}; in >> uuid && uuid.size() == 32) return uuid;
  uuid = NewUuid();
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::ofstream out(path);
  if (!(out << uuid << '\n')) REXLOG_WARN("ap: cannot save the client id to {}; using it for this run", path.string());
  return uuid;
}

// Profile directory (XUID) that holds this title's saves.
std::optional<std::string> ProfileXuid() {
  std::error_code ec;
  std::optional<std::string> found;
  for (const auto& entry : fs::directory_iterator(g.root, ec)) {
    const auto name = entry.path().filename().string();
    if (name.size() != 16 || name == "0000000000000000" || !entry.is_directory()) continue;
    if (!fs::exists(entry.path() / kTitle / "00000001", ec)) continue;
    if (found) return std::nullopt;  // ambiguous: refuse to guess
    found = name;
  }
  return found;
}

int64_t Now() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

uint32_t Stats(uint8_t* base) { return REX_LOAD_U32(kStatsGlobal); }

// Registry entry for an ordinal, or 0 when the world is not loaded / does not match the catalog layout.
uint32_t Registry(uint8_t* base) {
  const uint32_t stats = Stats(base);
  return stats && REX_LOAD_U32(stats + 4012) == kRegistrySize ? stats : 0;
}

// The agent's cSkillSet, through the same path as the stats screen (sub_822B4EA0), with a layout guard.
uint32_t PrimarySkills(uint8_t* base) {
  const uint32_t player = REX_LOAD_U32(kPlayerGlobal);
  if (!player) return 0;
  const uint32_t component = REX_LOAD_U32(player + 136);
  if (component < 28) return 0;
  const uint32_t character = component - 28;
  const uint32_t skills = character + kSkillSetOffset;
  return REX_LOAD_U32(skills + 4) == character ? skills : 0;
}

bool SaveIsBound() { return g.mode == Mode::bound && g.session; }

// SHA-256 of the native progress the stats serializer writes: registry count + every active flag.
std::optional<std::string> Fingerprint(uint8_t* base) {
  const uint32_t stats = Registry(base);
  if (!stats) return std::nullopt;
  sha256::SHA256 hash;
  const uint32_t count = kRegistrySize;
  hash.add(&count, sizeof(count));
  for (uint32_t i = 0; i < kRegistrySize; ++i) {
    const uint8_t active = REX_LOAD_U8(REX_LOAD_U32(stats + 12 + 4 * i) + 160);
    hash.add(&active, 1);
  }
  return hash.getHash();
}

bool VerifyCatalog(uint8_t* base, std::string& error) {
  const uint32_t stats = Registry(base);
  if (!stats) { error = "pickup registry not loaded"; return false; }
  for (const auto& [ordinal, p] : g.catalog->pickups) {
    const uint32_t pickup = REX_LOAD_U32(stats + 12 + 4 * ordinal);
    if (!pickup || REX_LOAD_U32(pickup + 136) != p.type || REX_LOAD_U32(pickup + 128) != ordinal) {
      error = "pickup type/ordinal mismatch at " + std::to_string(ordinal);
      return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
      const float v = std::bit_cast<float>(REX_LOAD_U32(pickup + 144 + 4 * axis));
      if (!std::isfinite(v) || std::fabs(v - p.position[axis]) > 0.05f) {
        error = "pickup position mismatch at " + std::to_string(ordinal);
        return false;
      }
    }
  }
  return true;
}

// Playtest record for logic tuning: <user_data>/archipelago/pickups.csv, one row per collected pickup with the
// Agility the logic expects and the agent's actual level. A row with agent < logic means the band is too strict.
void LogPickup(const Pickup& p, int agility) {
  const auto path = g.root / "archipelago" / "pickups.csv";
  std::error_code ec;
  const bool fresh = !fs::exists(path, ec);
  std::ofstream out(path, std::ios::app);
  if (!out) return;
  if (fresh) out << "time,name,ordinal,type,district,x,y,z,logic_agility,agent_agility\n";
  out << Now() << ',' << p.name << ',' << p.ordinal << ',' << p.type << ',' << p.district << ',' << p.position[0]
      << ',' << p.position[1] << ',' << p.position[2] << ',' << p.agility << ',' << agility << '\n';
}

// --- network -------------------------------------------------------------

void SendCommands(nlohmann::json commands) {
  if (g.generation) g.client.Send(g.generation, std::move(commands));
}

void SendPendingChecks() {
  if (!g.server_ready || !g.session || !g.halt.empty()) return;
  auto pending = g.session->PendingChecks();
  g.last_check_send = Clock::now();
  if (pending.empty()) return;
  REXLOG_INFO("ap: sending {} pending check(s)", pending.size());
  SendCommands(nlohmann::json::array({{{"cmd", "LocationChecks"}, {"locations", pending}}}));
}

void RequestResync() {
  REXLOG_WARN("ap: received-items index mismatch; requesting Sync");
  nlohmann::json commands = nlohmann::json::array({{{"cmd", "Sync"}}});
  if (g.session) {
    auto pending = g.session->PendingChecks();
    if (!pending.empty()) commands.push_back({{"cmd", "LocationChecks"}, {"locations", pending}});
  }
  SendCommands(std::move(commands));
}

std::optional<Contract> ValidateConnected(const nlohmann::json& p, std::string& error) {
  try {
    const int slot = p.at("slot").get<int>();
    if (p.at("slot_info").at(std::to_string(slot)).at("game").get<std::string>() != kGame) {
      error = "slot is not a Crackdown 2 slot";
      return {};
    }
    std::set<int64_t> locations;
    for (const char* key : {"missing_locations", "checked_locations"})
      for (const auto& id : p.at(key))
        if (!locations.insert(id.get<int64_t>()).second) {
          error = "duplicate server location";
          return {};
        }
    auto c = ParseSlotContract(*g.catalog, p.at("slot_data").dump(), locations);
    c.binding.game = kGame;
    c.binding.seed = g.room_seed;
    c.binding.team = p.at("team").get<int>();
    c.binding.slot = slot;
    return c;
  } catch (const std::exception& e) {
    error = std::string("malformed Connected/slot_data: ") + e.what();
    return {};
  }
}

std::set<int64_t> CheckedIn(const nlohmann::json& p) {
  std::set<int64_t> out;
  if (p.contains("checked_locations"))
    for (const auto& id : p["checked_locations"]) out.insert(id.get<int64_t>());
  return out;
}

void HandlePacket(const NetEvent& e) {
  const auto& p = e.body;
  if (e.cmd == "RoomInfo") {
    const auto seed = p.value("seed_name", std::string());
    if (seed.empty()) return Disconnect("RoomInfo without seed_name");
    if (g.stored && g.stored->binding.seed != seed)
      return Disconnect("server seed " + seed + " differs from this profile's bound seed " + g.stored->binding.seed);
    if (REXCVAR_GET(ap_slot).empty()) return Disconnect("ap_slot is not set");
    g.generation = e.generation;
    g.room_seed = seed;
    nlohmann::json connect = {{"cmd", "Connect"},
                              {"password", Password()},
                              {"game", kGame},
                              {"name", REXCVAR_GET(ap_slot)},
                              {"uuid", ClientUuid()},
                              {"version", {{"major", 0}, {"minor", 6}, {"build", 0}, {"class", "Version"}}},
                              {"items_handling", 7},
                              {"tags", nlohmann::json::array()},
                              {"slot_data", true}};
    REXLOG_INFO("ap: room seed {}; connecting as slot '{}'", seed, REXCVAR_GET(ap_slot));
    SendCommands(nlohmann::json::array({connect}));
  } else if (e.cmd == "ConnectionRefused") {
    Disconnect("connection refused: " + p.value("errors", nlohmann::json::array()).dump());
  } else if (e.cmd == "Connected") {
    std::string error;
    auto contract = ValidateConnected(p, error);
    if (!contract) return Disconnect(error);
    if (g.stored) {
      auto expected = *g.stored;
      expected.binding.profile.clear();
      expected.binding.save.clear();
      if (!(expected == *contract))
        return Disconnect("server slot/team/catalog/locations differ from this profile's bound session");
    }
    g.server = contract;
    g.server_ready = true;
    g.players.clear();
    for (const auto& player : p.value("players", nlohmann::json::array()))
      if (player.value("team", -1) == contract->binding.team)
        g.players[player.value("slot", 0)] = player.value("alias", player.value("name", std::string()));
    REXLOG_INFO("ap: connected team {} slot {}; {} locations ({} checked on server)", contract->binding.team,
                contract->binding.slot, contract->locations.size(), CheckedIn(p).size());
    // Connected is authoritative: a server restored from an older save forgets checks, which become pending.
    g.server_checked = CheckedIn(p);
    if (g.session) g.session->Acknowledge(ActiveBinding(), g.server_checked, true);
    SendPendingChecks();
  } else if (e.cmd == "ReceivedItems") {
    if (!g.session) return;  // not yet bound: binding requests a full Sync
    std::vector<NetworkItem> items;
    for (const auto& i : p.at("items"))
      items.push_back({i.at("item").get<int64_t>(), i.at("location").get<int64_t>(), i.at("player").get<int>(),
                       i.at("flags").get<int>()});
    const auto index = p.at("index").get<int64_t>();
    const auto receipt = g.session->Receive(ActiveBinding(), index, items);
    if (receipt == Receipt::resync_required) RequestResync();
    else
      REXLOG_INFO("ap: received items index {} count {} ({}); inventory {}", index, items.size(),
                  receipt == Receipt::replaced ? "full" : "appended", g.session->Inventory().size());
  } else if (e.cmd == "RoomUpdate") {
    const auto checked = CheckedIn(p);
    g.server_checked.insert(checked.begin(), checked.end());
    if (g.session && !checked.empty()) g.session->Acknowledge(ActiveBinding(), checked);
  } else if (e.cmd == "PrintJSON") {
    std::string text;
    for (const auto& part : p.value("data", nlohmann::json::array())) {
      if (!part.contains("text") || !part["text"].is_string()) continue;
      auto piece = part["text"].get<std::string>();
      const auto type = part.value("type", std::string());
      try {
        if (type == "player_id" && g.players.contains(std::stoi(piece))) piece = g.players[std::stoi(piece)];
        else if (type == "item_id" && g.catalog->item_names.contains(std::stoll(piece)))
          piece = g.catalog->item_names.at(std::stoll(piece));
        else if (type == "location_id")
          if (const auto* pickup = g.catalog->ByLocation(std::stoll(piece))) piece = pickup->name;
      } catch (const std::exception&) {
      }
      text += piece;
    }
    if (!text.empty()) REXLOG_INFO("ap: server: {}", text);
  }
}

void PumpNetwork() {
  for (auto& e : g.client.Drain()) {
    if (!g.halt.empty() || !g.offline.empty()) continue;
    if (e.kind == NetEvent::Kind::connected_socket) {
      REXLOG_INFO("ap: socket open (generation {})", e.generation);
      continue;
    }
    if (e.kind == NetEvent::Kind::disconnected) {
      if (e.generation == g.generation || !g.generation) {
        REXLOG_WARN("ap: disconnected: {}; checks stay pending and retry on reconnect", e.detail);
        g.server_ready = false;
        g.generation = 0;
      }
      continue;
    }
    if (e.cmd != "RoomInfo" && e.generation != g.generation) continue;  // older socket
    try {
      HandlePacket(e);
    } catch (const std::exception& ex) {
      Disconnect(std::string("packet ") + e.cmd + " rejected: " + ex.what());
    }
  }
}

// --- native lifecycle -----------------------------------------------------

void Observe(int64_t location, bool send = true) {
  try {
    if (g.session->Observe(ActiveBinding(), location)) {
      REXLOG_INFO("ap: check observed location {}", location);
      if (send) SendPendingChecks();
    }
  } catch (const std::exception& e) {
    // The native pickup already happened. Keep it in memory and retry; a crash here is recovered from the
    // save's native flags on the next bound load.
    REXLOG_ERROR("ap: could not persist check {}: {}; will retry", location, e.what());
    g.retry_observe.push_back(location);
  }
}

// After a bound load: natively collected pickups the sidecar lacks (persisted by a native save, observation lost).
void RecoverNativeChecks(uint8_t* base) {
  const uint32_t stats = Registry(base);
  for (auto id : g.session->contract().locations) {
    const auto* p = g.catalog->ByLocation(id);
    if (p && REX_LOAD_U8(REX_LOAD_U32(stats + 12 + 4 * p->ordinal) + 160) == 0) Observe(id, false);
  }
  SendPendingChecks();
}

bool BindNewGame(uint8_t* base, std::string& reason) {
  if (g.session) {
    if (!g.session->HasNativeSave()) return true;  // never saved natively: the same lineage starts over
    // A world change without a native load (e.g. Agent Diagnostics -> Pacific City) keeps the progress in memory:
    // still this binding's save when that progress is in the lineage.
    const auto print = Fingerprint(base);
    if (print && g.session->AcceptLoad(ActiveBinding(), *print)) return true;
    reason = "this profile's randomized save already has native saves and this world's progress is not one of them";
    return false;
  }
  if (!g.server_ready || !g.server) {
    reason = "not connected to a validated Archipelago slot when the game started";
    return false;
  }
  const auto xuid = ProfileXuid();
  if (!xuid) {
    reason = "could not identify a single signed-in profile directory";
    return false;
  }
  auto contract = *g.server;
  contract.binding.profile = *xuid;
  contract.binding.save = NewUuid();
  fs::create_directories(g.sidecar.parent_path());
  g.session = std::make_unique<SessionState>(g.sidecar, contract);
  g.stored = contract;
  g.session->Acknowledge(ActiveBinding(), g.server_checked, true);  // what the server already has
  REXLOG_INFO("ap: new game bound to seed {} slot {} as save {}", contract.binding.seed, contract.binding.slot,
              contract.binding.save);
  SendCommands(nlohmann::json::array({{{"cmd", "Sync"}}}));  // full inventory for the new binding
  return true;
}

void Decide(uint8_t* base) {
  bool bound = false;
  std::string reason;
  try {
    if (g.load_seen && !g.session && REXCVAR_GET(ap_dev_bind_load)) {
      bound = BindNewGame(base, reason);
      if (bound) {
        const auto print = Fingerprint(base);
        if (!print) return Halt("pickup registry not loaded when binding the loaded save");
        g.session->RecordSave(ActiveBinding(), *print);
        REXLOG_WARN("ap: developer bind of the loaded native save (fingerprint {})", print->substr(0, 16));
      }
    } else if (g.load_seen) {
      bound = g.load_bound;
      reason = g.load_reason;
    } else {
      bound = BindNewGame(base, reason);
    }
    if (bound) {
      std::string error;
      if (!VerifyCatalog(base, error)) return Halt("catalog does not match the loaded world: " + error);
      if (g.load_seen) RecoverNativeChecks(base);
    }
  } catch (const std::exception& e) {
    return Halt(std::string("binding failed: ") + e.what());
  }
  g.mode = bound ? Mode::bound : Mode::foreign;
  if (bound) {
    REXLOG_INFO("ap: randomized save active ({}); gameplay XP disabled, {} pending check(s)",
                g.load_seen ? "native load" : "new game", g.session->PendingChecks().size());
    SendPendingChecks();
  } else {
    REXLOG_WARN("ap: this save is not the profile's randomized save ({}); vanilla rules apply", reason);
  }
}

void SendGoal(uint8_t* base) {
  if (!g.server_ready || !g.session->SkillsComplete() || g.goal_generation == g.generation) return;
  const uint32_t skills = PrimarySkills(base);
  if (!skills) return;
  for (const auto& [index, skill] : g.session->contract().skills)
    if (REX_LOAD_U8(skills + 14 + index) < skill.maximum) return;  // goal needs the levels applied in game
  SendCommands(nlohmann::json::array({{{"cmd", "StatusUpdate"}, {"status", 30}}}));
  g.goal_generation = g.generation;
  REXLOG_INFO("ap: CLIENT_GOAL sent (every skill at its maximum, generation {})", g.generation);
}

void Reconcile(PPCContext& ctx, uint8_t* base) {
  const auto now = Clock::now();
  if (g.mode == Mode::transition && g.halt.empty()) {
    // Decide once the world has loaded: full registry and an agent, unchanged for 60 frames.
    g.stable_frames = Registry(base) && PrimarySkills(base) ? g.stable_frames + 1 : 0;
    if (g.stable_frames >= 60) Decide(base);
  }
  if (!SaveIsBound() || !g.halt.empty()) return;
  if (!g.retry_observe.empty() && now - g.last_observe_retry > std::chrono::seconds(2)) {
    g.last_observe_retry = now;
    auto retry = std::move(g.retry_observe);
    g.retry_observe.clear();
    for (auto id : retry) Observe(id);
  }
  if (g.server_ready && now - g.last_check_send > std::chrono::seconds(10)) SendPendingChecks();

  const uint32_t skills = PrimarySkills(base);
  if (!skills || now - g.last_grant_try < std::chrono::milliseconds(500)) return;
  g.last_grant_try = now;
  for (int index = 0; index < kSkillCount; ++index) {
    const int native = REX_LOAD_U8(skills + 14 + index);
    if (native < 1 || native > 6) continue;
    std::optional<int> target;
    try {
      target = g.session->SkillTarget(ActiveBinding(), index, native, true);
    } catch (const std::exception& e) {
      return Halt(std::string(kSkillNames[index]) + " target: " + e.what());
    }
    if (!target) continue;
    auto grant = ctx;
    grant.r3.u64 = skills;
    grant.r4.u64 = static_cast<uint32_t>(index);
    grant.r5.u64 = static_cast<uint32_t>(*target);
    __imp__sub_82753EB8(grant, base);
    const int after = REX_LOAD_U8(skills + 14 + index);
    if (after == *target)
      REXLOG_INFO("ap: {} {} -> {} (inventory {} item(s))", kSkillNames[index], native, after,
                  g.session->Inventory().size());
    else
      REXLOG_ERROR("ap: {} setter left level {} (target {}); will retry", kSkillNames[index], after, *target);
  }
  SendGoal(base);
}
}  // namespace

// --- public entry points ---------------------------------------------------

void Initialize(const fs::path& user_data_root) {
  std::lock_guard lock(g_mutex);
  g.root = user_data_root;
  g.sidecar = user_data_root / "archipelago" / "session.toml";
  try {
    g.catalog = &EmbeddedCatalog();
    g.stored = SessionState::ReadContract(g.sidecar);
    if (g.stored) {
      ValidateStoredContract(*g.catalog, *g.stored);
      g.session = std::make_unique<SessionState>(g.sidecar, *g.stored);
    }
  } catch (const std::exception& e) {
    g.enabled = true;
    Halt(std::string("cannot open ") + g.sidecar.string() + ": " + e.what());
    return;
  }
  const auto server = REXCVAR_GET(ap_server);
  g.enabled = g.stored.has_value() || !server.empty();
  if (!g.enabled) return;
  REXLOG_INFO("ap: catalog {}; profile session {}; server {}", g.catalog->version,
              g.stored ? "seed " + g.stored->binding.seed + " save " + g.stored->binding.save : "unbound",
              server.empty() ? "none (offline)" : server);
  if (!server.empty()) g.client.Start(server);
}

void Shutdown() { g.client.Stop(); }

void OnFrame(PPCContext& ctx, uint8_t* base) {
  std::lock_guard lock(g_mutex);
  if (!g.enabled) return;
  if (g.halt.empty()) PumpNetwork();
  Reconcile(ctx, base);
}
}  // namespace crackdown2::ap

using namespace crackdown2::ap;

// World (re)load: stats reset. Binding is decided again once the new world is up.
REX_HOOK_RAW(sub_8270B448) {
  {
    std::lock_guard lock(g_mutex);
    if (g.enabled) {
      g.mode = Mode::transition;
      g.load_seen = false;
      g.stable_frames = 0;
    }
  }
  __imp__sub_8270B448(ctx, base);
}

// Native load: pickup active flags come back from the save.
REX_HOOK_RAW(sub_8226B2F0) {
  g_in_native_load = true;
  __imp__sub_8226B2F0(ctx, base);
  g_in_native_load = false;
  std::lock_guard lock(g_mutex);
  if (!g.enabled) return;
  g.load_seen = true;
  g.load_bound = false;
  g.load_reason = "profile has no randomized session";
  if (!g.session || !g.halt.empty()) return;
  try {
    const auto print = Fingerprint(base);
    if (!print) {
      g.load_reason = "pickup registry not loaded at the native load";
      return;
    }
    g.load_bound = g.session->AcceptLoad(g.session->binding(), *print);
    g.load_reason = g.load_bound ? "" : "native save " + print->substr(0, 16) + " is not in the bound lineage";
    REXLOG_INFO("ap: native load fingerprint {} -> {}", print->substr(0, 16), g.load_bound ? "bound" : "foreign");
  } catch (const std::exception& e) {
    Halt(std::string("native load check failed: ") + e.what());
  }
}

// Native save: the flags it writes join this binding's lineage before the file is written.
REX_HOOK_RAW(sub_822708A8) {
  {
    std::lock_guard lock(g_mutex);
    if (g.enabled && SaveIsBound() && g.halt.empty()) {
      try {
        if (const auto print = Fingerprint(base)) {
          g.session->RecordSave(ActiveBinding(), *print);
          REXLOG_INFO("ap: native save fingerprint {} recorded in lineage", print->substr(0, 16));
        }
      } catch (const std::exception& e) {
        Halt(std::string("cannot record native save: ") + e.what());
      }
    }
  }
  __imp__sub_822708A8(ctx, base);
}

REX_HOOK_RAW(sub_82709D18) {
  const uint32_t pickup = ctx.r3.u32, lr = static_cast<uint32_t>(ctx.lr);
  const bool collect = (ctx.r4.u32 & 0xFF) == 0 && REX_LOAD_U8(pickup + 160) != 0 && !g_in_native_load &&
                       !(lr >= kClearLogsBegin && lr < kClearLogsEnd);
  crackdown2::ap::ProbeSetActive(ctx, base);
  __imp__sub_82709D18(ctx, base);
  if (!collect) return;
  std::lock_guard lock(g_mutex);
  if (!g.enabled || !SaveIsBound() || !g.halt.empty()) return;
  const uint32_t stats = Registry(base);
  const uint32_t ordinal = REX_LOAD_U32(pickup + 128);
  const auto it = g.catalog->pickups.find(ordinal);
  if (!stats || ordinal >= kRegistrySize || REX_LOAD_U32(stats + 12 + 4 * ordinal) != pickup ||
      it == g.catalog->pickups.end() || it->second.type != REX_LOAD_U32(pickup + 136)) {
    REXLOG_ERROR("ap: collected pickup {:08X} (ordinal {}) does not match the catalog", pickup, ordinal);
    return;
  }
  const uint32_t skills = PrimarySkills(base);
  const int agility = skills ? REX_LOAD_U8(skills + 14) : 0;
  REXLOG_INFO("ap: {} collected ordinal {} lr {:08X} (logic Agility {}, agent Agility {})", it->second.name,
              ordinal, lr, it->second.agility, agility);
  LogPickup(it->second, agility);
  if (g.session->contract().locations.contains(it->second.id)) Observe(it->second.id);
}

// Gameplay XP for the agent is dropped in a bound save: skills rise only from received items.
REX_HOOK_RAW(sub_82753CA0) {
  {
    std::lock_guard lock(g_mutex);
    if (g.enabled && SaveIsBound() && g.halt.empty() && ctx.r3.u32 == PrimarySkills(base)) {
      static uint32_t dropped = 0;
      if (++dropped <= 20 || dropped % 100 == 0)
        REXLOG_INFO("ap: dropped {:.1f} {} XP (lr {:08X}, {} dropped so far)", ctx.f1.f64,
                    ctx.r4.u32 < 5 ? kSkillNames[ctx.r4.u32] : "?", static_cast<uint32_t>(ctx.lr), dropped);
      return;
    }
  }
  crackdown2::ap::ProbeAddXP(ctx, base);
}
