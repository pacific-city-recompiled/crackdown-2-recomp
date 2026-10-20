#include "catalog.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "ap_catalog_data.h"  // generated: kCatalogJson / kCatalogJsonSize

namespace crackdown2::ap {
namespace {
void Require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
constexpr uint32_t kRegistrySize = 932;  // 500 agility + 80 online + 300 hidden + 52 audio logs (run crackdown2_003)
}  // namespace

std::set<int64_t> Catalog::Locations(const Options& o) const {
  std::set<int64_t> ids;
  for (const auto& [ordinal, p] : pickups)
    if (p.kind == "agility" || (p.kind == "hidden" && o.hidden_orbs) || (p.kind == "audio_log" && o.audio_logs))
      ids.insert(p.id);
  return ids;
}

const Pickup* Catalog::ByLocation(int64_t id) const {
  for (const auto& [ordinal, p] : pickups)
    if (p.id == id) return &p;
  return nullptr;
}

Catalog ParseCatalog(std::string_view text) {
  auto j = nlohmann::json::parse(text);
  Catalog c;
  c.version = j.at("version").get<std::string>();
  c.game = j.at("game").get<std::string>();
  Require(c.game == kGame && !c.version.empty(), "Invalid AP catalog header");
  std::set<int64_t> items;
  for (const auto& s : j.at("skills")) {
    const int index = s.at("index").get<int>();
    SkillProgression skill{s.at("item").get<int64_t>(), s.at("starting").get<int>(), s.at("maximum").get<int>()};
    Require(index >= 0 && index < kSkillCount && skill.starting >= 1 && skill.maximum <= 6 &&
                skill.starting <= skill.maximum && items.insert(skill.item).second &&
                c.skills.emplace(index, skill).second,
            "Invalid AP catalog skill");
    c.item_names[skill.item] = "Progressive " + s.at("name").get<std::string>();
  }
  Require(c.skills.size() == kSkillCount, "Invalid AP catalog skills");
  c.filler_item = j.at("filler_item").get<int64_t>();
  Require(items.insert(c.filler_item).second, "Invalid AP catalog filler");
  c.item_names[c.filler_item] = "Agency Commendation";
  std::set<int64_t> ids;
  for (const auto& o : j.at("pickups")) {
    Pickup p;
    p.id = o.at("id").get<int64_t>();
    p.ordinal = o.at("ordinal").get<uint32_t>();
    p.type = o.at("type").get<uint32_t>();
    p.district = o.at("district").get<uint32_t>();
    p.kind = o.at("kind").get<std::string>();
    p.name = o.at("name").get<std::string>();
    const auto& pos = o.at("position");
    p.position = {pos.at(0).get<float>(), pos.at(1).get<float>(), pos.at(2).get<float>()};
    const bool kind_ok = (p.kind == "agility" && p.type <= 3) || (p.kind == "hidden" && p.type == 4) ||
                         (p.kind == "audio_log" && p.type == 9) || (p.kind == "online" && p.type == 10);
    Require(kind_ok && p.ordinal < kRegistrySize && p.district <= 3 && !items.contains(p.id) &&
                ids.insert(p.id).second &&
                std::all_of(p.position.begin(), p.position.end(), [](float v) { return std::isfinite(v); }) &&
                c.pickups.emplace(p.ordinal, p).second,
            "Invalid AP catalog pickup");
  }
  Require(c.pickups.size() == kRegistrySize, "Incomplete AP catalog");
  return c;
}

Contract ParseSlotContract(const Catalog& catalog, std::string_view text, const std::set<int64_t>& locations) {
  const auto data = nlohmann::json::parse(text);
  Contract c;
  c.binding.catalog = data.at("catalog").get<std::string>();
  c.binding.schema = data.at("schema").get<int>();
  Require(c.binding.catalog == catalog.version && c.binding.schema == 1, "Slot catalog mismatch");
  for (const auto& s : data.at("skills"))
    Require(c.skills
                .emplace(s.at("index").get<int>(), SkillProgression{s.at("item").get<int64_t>(),
                                                                     s.at("starting").get<int>(),
                                                                     s.at("maximum").get<int>()})
                .second,
            "Duplicate slot skill");
  Require(c.skills == catalog.skills, "Slot skills mismatch");
  c.filler_item = data.at("filler_item").get<int64_t>();
  Require(c.filler_item == catalog.filler_item, "Slot filler mismatch");
  c.goal = data.at("goal").get<std::string>();
  Require(c.goal == "max_skills", "Unsupported slot goal");
  const Options options{data.at("include_hidden_orbs").get<bool>(), data.at("include_audio_logs").get<bool>()};
  Require(locations == catalog.Locations(options), "Slot locations mismatch");
  c.locations = locations;
  return c;
}

const Catalog& EmbeddedCatalog() {
  static const Catalog catalog =
      ParseCatalog(std::string_view(reinterpret_cast<const char*>(kCatalogJson), kCatalogJsonSize));
  return catalog;
}

void ValidateStoredContract(const Catalog& catalog, const Contract& c) {
  Require(c.binding.catalog == catalog.version && c.binding.schema == 1 && c.skills == catalog.skills &&
              c.filler_item == catalog.filler_item && c.goal == "max_skills",
          "Stored catalog/skills differ from the embedded catalog");
  bool any_option = false;
  for (bool hidden : {false, true})
    for (bool logs : {false, true}) any_option |= c.locations == catalog.Locations({hidden, logs});
  Require(any_option, "Stored locations differ from the embedded catalog");
}
}  // namespace crackdown2::ap
