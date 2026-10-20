// Host-only AP state (Crackdown 1's session_state, simplified for Crackdown 2). No sockets, guest memory, or
// consumable delivery. Sidecar: <user_data>/archipelago/session.toml.
#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "skill_progression.h"

namespace crackdown2::ap {
inline constexpr const char* kGame = "Crackdown 2";
inline constexpr int kSkillCount = 5;

struct Binding {
  std::string game, seed, profile, save, catalog;
  int schema = 1, team = 0, slot = 0;
  bool operator==(const Binding&) const = default;
};
// Validated against the embedded catalog (catalog.cpp) before it reaches here.
struct Contract {
  Binding binding;
  std::set<int64_t> locations;
  std::map<int, SkillProgression> skills;  // native skill index 0..4 -> progressive item
  int64_t filler_item = 0;                 // Agency Commendation: permanent no-op
  std::string goal;                        // "max_skills"
  bool operator==(const Contract&) const = default;
};
struct NetworkItem {
  int64_t item = 0, location = 0;
  int player = 0, flags = 0;
  bool operator==(const NetworkItem&) const = default;
};
enum class Receipt { replaced, appended, resync_required };

// One coordinator owns this object. Network callbacks must queue copied packets.
// Mutations commit a complete sidecar before becoming visible to the caller.
class SessionState {
 public:
  SessionState(std::filesystem::path path, Contract contract);
  Receipt Receive(const Binding&, int64_t index, const std::vector<NetworkItem>&);
  bool Observe(const Binding&, int64_t location);  // persist BEFORE sending a check
  // authoritative (Connected.checked_locations) replaces the set, so a check a
  // rolled-back server forgot is pending again; RoomUpdate only adds.
  void Acknowledge(const Binding&, const std::set<int64_t>& server_checked, bool authoritative = false);
  std::vector<int64_t> PendingChecks() const;
  // Absolute level the received items give this skill; nullopt when the native level already matches or the
  // inventory is not known. Call again after every native load (older saves carry older levels).
  std::optional<int> SkillTarget(const Binding& active_save, int skill_index, int native_level, bool ready) const;
  // True when every skill's received items reach its maximum (goal "max_skills").
  bool SkillsComplete() const;
  bool NeedsResync() const { return resync_; }
  // Native save lineage: fingerprints (SHA-256 hex) of the native progress the game serialized in this binding.
  // A native load is this binding's save only when its fingerprint is in the lineage.
  void RecordSave(const Binding&, const std::string& fingerprint);
  bool AcceptLoad(const Binding&, const std::string& fingerprint) const;
  bool HasNativeSave() const { return !state_.saves.empty(); }
  // Reads the binding/catalog contract of an existing sidecar so an offline start can reopen it; nullopt when
  // absent. Corrupt files throw.
  static std::optional<Contract> ReadContract(const std::filesystem::path&);
  const std::vector<NetworkItem>& Inventory() const { return state_.inventory; }
  const Contract& contract() const { return contract_; }
  const std::set<int64_t>& Observed() const { return state_.observed; }
  const Binding& binding() const { return contract_.binding; }

 private:
  struct State {
    std::vector<NetworkItem> inventory;
    std::set<int64_t> observed, acknowledged;
    bool inventory_known = false;
    std::set<std::string> saves;
  };
  void Verify(const Binding&) const;
  void Commit(State next);
  std::string Encode(const State&) const;
  State Decode(const std::string&) const;
  std::filesystem::path path_;
  Contract contract_;
  State state_;
  bool resync_ = false;
};
}  // namespace crackdown2::ap
