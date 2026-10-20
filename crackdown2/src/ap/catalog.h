// Versioned AP catalog (crackdown2/ap/catalog.json, embedded at build time; tools/re/make_ap_catalog.py).
// The same file is the ID source for the apworld (archipelago/crackdown2/data/catalog.json).
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include "session_state.h"

namespace crackdown2::ap {
// One registry entry (player stats [0x83800E20] + 12 + 4 * ordinal): orbs and audio logs.
struct Pickup {
  int64_t id = 0;
  uint32_t ordinal = 0, type = 0, district = 0;
  int agility = 1;  // native Agility level the logic expects (catalog "agility")
  std::string kind, name;  // kind: agility, hidden, online, audio_log
  std::array<float, 3> position{};
};
struct Options {
  bool hidden_orbs = true, audio_logs = false;
};
struct Catalog {
  std::string version, game;
  std::map<int, SkillProgression> skills;
  int64_t filler_item = 0;
  std::map<uint32_t, Pickup> pickups;  // by ordinal
  std::map<int64_t, std::string> item_names;
  std::set<int64_t> Locations(const Options&) const;  // online orbs never (Xbox Live only)
  const Pickup* ByLocation(int64_t id) const;
};
Catalog ParseCatalog(std::string_view json);  // throws on an invalid catalog
const Catalog& EmbeddedCatalog();
// Host-testable slot_data validation, shared by the live bridge. Throws on any mismatch.
Contract ParseSlotContract(const Catalog&, std::string_view slot_data, const std::set<int64_t>& locations);
void ValidateStoredContract(const Catalog&, const Contract&);  // throws; never migrates a binding
}  // namespace crackdown2::ap
