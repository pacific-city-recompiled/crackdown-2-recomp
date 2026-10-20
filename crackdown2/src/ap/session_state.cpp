#include "session_state.h"
#include "../../common/toml_edit.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace crackdown2::ap {
namespace {
constexpr size_t kMaxItems = 100000, kMaxLocations = 10000, kMaxSaves = 100000, kMaxBytes = 16 * 1024 * 1024;
constexpr int64_t kMaxId = (int64_t{1} << 53) - 1;
void Require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
bool Id(int64_t id) { return id >= -kMaxId && id <= kMaxId; }
template <class T> T Value(const toml::table& t, const char* key) {
  auto value = t[key].value<T>();
  Require(value.has_value(), "Invalid AP sidecar field");
  return *value;
}
void ValidateItem(const NetworkItem& item, const Contract& contract) {
  bool supported = contract.filler_item != 0 && item.item == contract.filler_item;
  for (const auto& [index, skill] : contract.skills) supported |= item.item == skill.item;
  Require(supported && Id(item.location) && item.player >= 0 && item.flags >= 0 && item.flags <= 7,
          "Unsupported AP item");
}
toml::array Ids(const std::set<int64_t>& ids) {
  toml::array out;
  for (auto id : ids) out.push_back(id);
  return out;
}
bool Sha256(const std::string& h) {
  return h.size() == 64 &&
         std::all_of(h.begin(), h.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
std::set<int64_t> ReadIds(const toml::table& t, const char* key) {
  auto array = t[key].as_array();
  Require(array && array->size() <= kMaxLocations, "Invalid AP location list");
  std::set<int64_t> ids;
  for (const auto& node : *array) {
    auto id = node.value<int64_t>();
    Require(id && Id(*id) && ids.insert(*id).second, "Invalid/duplicate AP location");
  }
  return ids;
}
Contract ParseContract(const toml::table& t) {
  Require(Value<int>(t, "format") == 1, "Unsupported AP sidecar version");
  Contract c;
  c.binding = {Value<std::string>(t, "game"),    Value<std::string>(t, "seed"),
               Value<std::string>(t, "profile"), Value<std::string>(t, "save"),
               Value<std::string>(t, "catalog"), Value<int>(t, "schema"),
               Value<int>(t, "team"),            Value<int>(t, "slot")};
  auto skills = t["skills"].as_array();
  Require(skills && skills->size() == kSkillCount, "Invalid AP skill contract");
  for (const auto& node : *skills) {
    auto skill = node.as_table();
    Require(skill != nullptr, "Invalid AP skill contract");
    Require(c.skills.emplace(Value<int>(*skill, "index"),
                             SkillProgression{Value<int64_t>(*skill, "item"), Value<int>(*skill, "starting"),
                                              Value<int>(*skill, "maximum")}).second,
            "Duplicate AP skill index");
  }
  c.locations = ReadIds(t, "locations");
  c.filler_item = Value<int64_t>(t, "filler_item");
  c.goal = Value<std::string>(t, "goal");
  return c;
}
std::string ReadFile(const std::filesystem::path& path) {
  Require(std::filesystem::file_size(path) <= kMaxBytes, "AP sidecar is too large");
  std::ifstream in(path, std::ios::binary);
  Require(bool(in), "Cannot read AP sidecar");
  std::string text{std::istreambuf_iterator<char>(in), {}};
  Require(!in.bad(), "Cannot read AP sidecar");
  return text;
}
}  // namespace

SessionState::SessionState(std::filesystem::path path, Contract contract)
    : path_(std::move(path)), contract_(std::move(contract)) {
  const auto& b = contract_.binding;
  Require(b.game == kGame && b.schema == 1 && !b.seed.empty() && !b.profile.empty() && !b.save.empty() &&
              !b.catalog.empty() && b.team >= 0 && b.slot > 0,
          "Invalid AP binding");
  for (const auto* text : {&b.seed, &b.profile, &b.save, &b.catalog})
    Require(text->size() <= 1024, "AP identity is too long");
  Require(!contract_.locations.empty() && contract_.locations.size() <= kMaxLocations, "Invalid AP catalog");
  for (auto id : contract_.locations) Require(Id(id), "Invalid catalog location");
  Require(contract_.skills.size() == kSkillCount, "Invalid AP skill contract");
  std::set<int64_t> items;
  for (const auto& [index, skill] : contract_.skills)
    Require(index >= 0 && index < kSkillCount && Id(skill.item) && items.insert(skill.item).second &&
                !contract_.locations.contains(skill.item) && skill.starting >= 1 && skill.maximum <= 6 &&
                skill.starting <= skill.maximum,
            "Invalid AP skill contract");
  Require(contract_.filler_item != 0 && Id(contract_.filler_item) && items.insert(contract_.filler_item).second &&
              !contract_.locations.contains(contract_.filler_item),
          "Invalid AP filler contract");
  Require(contract_.goal == "max_skills", "Unsupported AP goal");
  if (std::filesystem::exists(path_)) {
    state_ = Decode(ReadFile(path_));  // Corrupt/mismatched state is never silently reset.
  } else {
    Commit({});  // Establish durable binding before any grants or observations.
  }
}
void SessionState::Verify(const Binding& binding) const {
  Require(binding == contract_.binding, "AP seed/team/slot/profile/save/catalog mismatch");
}
void SessionState::Commit(State next) {
  auto text = Encode(next);
  Require(text.size() <= kMaxBytes, "AP sidecar is too large");
  config::ConfigTemp temp(path_);
  temp.replace(path_, text);
  state_ = std::move(next);
}
Receipt SessionState::Receive(const Binding& binding, int64_t index, const std::vector<NetworkItem>& items) {
  Verify(binding);
  Require(index >= 0 && items.size() <= kMaxItems, "Invalid AP receipt index/size");
  for (const auto& item : items) ValidateItem(item, contract_);
  // Strict protocol resync: even duplicate/overlapping nonzero batches request
  // Sync + LocationChecks. No partial gap or conflict is added to inventory.
  if (index != 0 &&
      (resync_ || !state_.inventory_known || static_cast<uint64_t>(index) != state_.inventory.size())) {
    resync_ = true;
    return Receipt::resync_required;
  }
  auto next = state_;
  if (index == 0) {
    next.inventory = items;
  } else {
    Require(items.size() <= kMaxItems - next.inventory.size(), "AP inventory is too large");
    next.inventory.insert(next.inventory.end(), items.begin(), items.end());
  }
  next.inventory_known = true;
  Commit(std::move(next));
  resync_ = false;
  return index == 0 ? Receipt::replaced : Receipt::appended;
}
bool SessionState::Observe(const Binding& binding, int64_t location) {
  Verify(binding);
  Require(contract_.locations.contains(location), "Location outside AP catalog");
  if (state_.observed.contains(location)) return false;
  auto next = state_;
  next.observed.insert(location);
  Commit(std::move(next));
  return true;
}
void SessionState::Acknowledge(const Binding& binding, const std::set<int64_t>& checked, bool authoritative) {
  Verify(binding);
  for (auto id : checked) Require(contract_.locations.contains(id), "Server location outside AP catalog");
  auto next = state_;
  if (authoritative) next.acknowledged = checked;
  else next.acknowledged.insert(checked.begin(), checked.end());
  if (next.acknowledged != state_.acknowledged) Commit(std::move(next));
}
std::vector<int64_t> SessionState::PendingChecks() const {
  std::vector<int64_t> pending;
  std::set_difference(state_.observed.begin(), state_.observed.end(), state_.acknowledged.begin(),
                      state_.acknowledged.end(), std::back_inserter(pending));
  return pending;
}
std::optional<Contract> SessionState::ReadContract(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) return std::nullopt;
  return ParseContract(toml::parse(ReadFile(path)));
}
void SessionState::RecordSave(const Binding& binding, const std::string& fingerprint) {
  Verify(binding);
  Require(Sha256(fingerprint), "Invalid native save fingerprint");
  if (state_.saves.contains(fingerprint)) return;
  Require(state_.saves.size() < kMaxSaves, "Too many native saves in AP lineage");
  auto next = state_;
  next.saves.insert(fingerprint);
  Commit(std::move(next));
}
bool SessionState::AcceptLoad(const Binding& binding, const std::string& fingerprint) const {
  Verify(binding);
  Require(Sha256(fingerprint), "Invalid native save fingerprint");
  return state_.saves.contains(fingerprint);
}
std::optional<int> SessionState::SkillTarget(const Binding& active, int index, int native, bool ready) const {
  Verify(active);
  Require(contract_.skills.contains(index), "Invalid native skill index");
  if (!ready || resync_ || !state_.inventory_known) return {};
  Require(native >= 1 && native <= 6, "Invalid native skill level");
  const auto& skill = contract_.skills.at(index);
  const auto count = std::count_if(state_.inventory.begin(), state_.inventory.end(),
                                   [&](const auto& item) { return item.item == skill.item; });
  const int target = static_cast<int>(std::min<int64_t>(skill.maximum, skill.starting + count));
  return native == target ? std::nullopt : std::optional<int>(target);
}
bool SessionState::SkillsComplete() const {
  if (!state_.inventory_known) return false;
  return std::all_of(contract_.skills.begin(), contract_.skills.end(), [&](const auto& entry) {
    const auto& skill = entry.second;
    const auto count = std::count_if(state_.inventory.begin(), state_.inventory.end(),
                                     [&](const auto& item) { return item.item == skill.item; });
    return skill.starting + count >= skill.maximum;
  });
}
std::string SessionState::Encode(const State& s) const {
  const auto& b = contract_.binding;
  toml::table t{{"format", 1},
                {"game", b.game},
                {"schema", b.schema},
                {"seed", b.seed},
                {"profile", b.profile},
                {"save", b.save},
                {"catalog", b.catalog},
                {"team", b.team},
                {"slot", b.slot},
                {"locations", Ids(contract_.locations)},
                {"filler_item", contract_.filler_item},
                {"goal", contract_.goal},
                {"inventory_known", s.inventory_known},
                {"observed", Ids(s.observed)},
                {"acknowledged", Ids(s.acknowledged)}};
  toml::array skills;
  for (const auto& [index, skill] : contract_.skills)
    skills.push_back(toml::table{
        {"index", index}, {"item", skill.item}, {"starting", skill.starting}, {"maximum", skill.maximum}});
  t.insert("skills", std::move(skills));
  toml::array saves;
  for (const auto& hash : s.saves) saves.push_back(hash);
  t.insert("saves", std::move(saves));
  toml::array items;
  for (const auto& i : s.inventory)
    items.push_back(
        toml::table{{"item", i.item}, {"location", i.location}, {"player", i.player}, {"flags", i.flags}});
  t.insert("inventory", std::move(items));
  std::ostringstream out;
  out << toml::toml_formatter{t};
  return out.str();
}
SessionState::State SessionState::Decode(const std::string& text) const {
  auto t = toml::parse(text);
  const auto stored = ParseContract(t);
  Verify(stored.binding);
  Require(stored == contract_, "AP catalog contract mismatch");
  State s;
  auto saves = t["saves"].as_array();
  Require(saves && saves->size() <= kMaxSaves, "Invalid persisted native save list");
  for (const auto& node : *saves) {
    auto hash = node.value<std::string>();
    Require(hash && Sha256(*hash) && s.saves.insert(*hash).second, "Invalid persisted native save");
  }
  s.inventory_known = Value<bool>(t, "inventory_known");
  s.observed = ReadIds(t, "observed");
  s.acknowledged = ReadIds(t, "acknowledged");
  for (const auto* ids : {&s.observed, &s.acknowledged})
    for (auto id : *ids) Require(contract_.locations.contains(id), "Invalid persisted AP location");
  auto items = t["inventory"].as_array();
  Require(items && items->size() <= kMaxItems, "Invalid persisted AP inventory");
  for (const auto& node : *items) {
    auto item = node.as_table();
    Require(item != nullptr, "Invalid persisted AP item");
    NetworkItem i{Value<int64_t>(*item, "item"), Value<int64_t>(*item, "location"), Value<int>(*item, "player"),
                  Value<int>(*item, "flags")};
    ValidateItem(i, contract_);
    s.inventory.push_back(i);
  }
  Require(s.inventory_known || s.inventory.empty(), "Uninitialized AP inventory has items");
  return s;
}
}  // namespace crackdown2::ap
