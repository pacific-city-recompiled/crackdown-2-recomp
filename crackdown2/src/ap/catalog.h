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
// One of the 30 races (race manager [0x83801614] + 128 + 4 * ordinal), checked by its GUID at race+8..+20.
struct Race {
  int64_t id = 0;
  uint32_t ordinal = 0;
  std::string kind, name, guid;  // kind: road, rooftop; guid: 32 hex digits
  std::array<float, 3> position{};
};
// A campaign objective (campaign manager [0x838009CC] + 12, vector index), checked by the entry's vtable.
struct Objective {
  int64_t id = 0;
  uint32_t index = 0, iface_vt = 0;
  std::string kind, name;  // kind: tactical, absorption, breach
};
struct Options {
  bool hidden_orbs = true, audio_logs = false, races = true;
  bool tactical = true, absorption = true, breaches = true;
};
struct Catalog {
  std::string version, game;
  std::map<int, SkillProgression> skills;
  int64_t filler_item = 0;
  std::map<uint32_t, Pickup> pickups;  // by ordinal
  std::map<uint32_t, Race> races;      // by ordinal
  std::map<uint32_t, Objective> objectives;  // by campaign vector index
  std::map<int64_t, std::string> item_names;
  std::set<int64_t> Locations(const Options&) const;  // online orbs never (Xbox Live only)
  const Pickup* ByLocation(int64_t id) const;
  std::string LocationName(int64_t id) const;
};
Catalog ParseCatalog(std::string_view json);  // throws on an invalid catalog
const Catalog& EmbeddedCatalog();
// Host-testable slot_data validation, shared by the live bridge. Throws on any mismatch.
Contract ParseSlotContract(const Catalog&, std::string_view slot_data, const std::set<int64_t>& locations);
void ValidateStoredContract(const Catalog&, const Contract&);  // throws; never migrates a binding
}  // namespace crackdown2::ap
