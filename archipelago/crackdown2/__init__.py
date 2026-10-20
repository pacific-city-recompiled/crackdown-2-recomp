"""Crackdown 2: agility, hidden and (optionally) audio-log pickups are checks; skills are items.

The game client is built into the Crackdown 2 Recomp executable. data/catalog.json is the same file the
executable embeds (crackdown2/ap/catalog.json), so item and location IDs cannot drift. First-version logic is
one region; each pickup's "agility" field (native Agility level it needs, from tools/re/make_ap_catalog.py) is the
rule, so playtest corrections go into the shared catalog.
"""
import json
import pkgutil
from typing import ClassVar
import settings
from BaseClasses import Item, ItemClassification, Location, Region, Tutorial
from worlds.AutoWorld import WebWorld, World
from worlds.LauncherComponents import Component, Type, components
from .client import launch_client
from .options import AgilityLogic, Crackdown2Options, option_groups

CATALOG = json.loads(pkgutil.get_data(__name__, "data/catalog.json"))
SKILLS = {f"Progressive {s['name']}": s for s in CATALOG["skills"]}
FILLER = "Agency Commendation"
GAME = "Crackdown 2"


AGILITY_NEEDED = {p["name"]: p["agility"] for p in CATALOG["pickups"] + CATALOG["races"]}
DRIVING_NEEDED = {r["name"]: r["driving"] for r in CATALOG["races"]}
AGILITY_START = SKILLS["Progressive Agility"]["starting"]
DRIVING_START = SKILLS["Progressive Driving"]["starting"]
RACES = {r["name"]: r["id"] for r in CATALOG["races"]}


def pickups(kind: str) -> dict:
    return {p["name"]: p["id"] for p in CATALOG["pickups"] if p["kind"] == kind}


class Crackdown2Item(Item):
    game = GAME


class Crackdown2Location(Location):
    game = GAME


class Crackdown2Settings(settings.Group):
    class LauncherPath(settings.UserFilePath):
        """Path to CrackdownLauncher from your Crackdown Recomp folder."""
        is_exe = True
        description = "Crackdown Recomp launcher (CrackdownLauncher)"

    launcher_path: LauncherPath = LauncherPath("CrackdownLauncher")


components.append(Component("Crackdown 2 Client", func=launch_client, component_type=Type.CLIENT,
                            game_name=GAME, supports_uri=True,
                            description="Start the Crackdown Recomp launcher. The game connects by itself."))


class Crackdown2Web(WebWorld):
    theme = "partyTime"
    option_groups = option_groups
    tutorials = [Tutorial(
        "Multiworld Setup Guide",
        "Install Crackdown 2 Recomp and connect it to an Archipelago room.",
        "English", "setup_en.md", "setup/en", ["Jerrod"],
    )]


class Crackdown2World(World):
    """Collect orbs and win races across Pacific City while your skills arrive from the multiworld."""
    game = GAME
    web = Crackdown2Web()
    settings: ClassVar[Crackdown2Settings]
    settings_key = "crackdown2_options"
    options_dataclass = Crackdown2Options
    options: Crackdown2Options
    item_name_to_id = dict(CATALOG["items"])
    location_name_to_id = {**pickups("agility"), **pickups("hidden"), **pickups("audio_log"), **RACES}
    item_name_groups = {"Skills": set(SKILLS)}
    location_name_groups = {"Agility Orbs": set(pickups("agility")), "Hidden Orbs": set(pickups("hidden")),
                            "Audio Logs": set(pickups("audio_log")),
                            "Road Races": {r["name"] for r in CATALOG["races"] if r["kind"] == "road"},
                            "Rooftop Races": {r["name"] for r in CATALOG["races"] if r["kind"] == "rooftop"}}

    def create_item(self, name: str) -> Crackdown2Item:
        classification = ItemClassification.progression if name in SKILLS else ItemClassification.filler
        return Crackdown2Item(name, classification, self.item_name_to_id[name], self.player)

    def get_filler_item_name(self) -> str:
        return FILLER  # permanent no-op; the inventory keeps receipts

    def enabled_locations(self) -> dict:
        locations = pickups("agility")
        if self.options.include_hidden_orbs:
            locations.update(pickups("hidden"))
        if self.options.include_audio_logs:
            locations.update(pickups("audio_log"))
        if self.options.include_races:
            locations.update(RACES)
        return locations

    def create_regions(self) -> None:
        menu = Region("Menu", self.player, self.multiworld)
        city = Region("Pacific City", self.player, self.multiworld)
        menu.connect(city)
        for name, code in self.enabled_locations().items():
            city.locations.append(Crackdown2Location(self.player, name, code, city))
        self.multiworld.regions += [menu, city]

    def create_items(self) -> None:
        pool = [self.create_item(name) for name, skill in SKILLS.items()
                for _ in range(skill["maximum"] - skill["starting"])]
        count = len(self.enabled_locations())
        if len(pool) > count:
            raise ValueError("Crackdown 2 item pool exceeds the enabled location count")
        pool += [self.create_item(FILLER) for _ in range(count - len(pool))]
        self.multiworld.itempool += pool

    def set_rules(self) -> None:
        if self.options.agility_logic == AgilityLogic.option_bands:
            for location in self.multiworld.get_locations(self.player):
                needed = AGILITY_NEEDED.get(location.name, AGILITY_START) - AGILITY_START
                if needed > 0:
                    location.access_rule = lambda state, n=needed: state.has("Progressive Agility", self.player, n)
        for location in self.multiworld.get_locations(self.player):
            driving = DRIVING_NEEDED.get(location.name, DRIVING_START) - DRIVING_START
            if driving > 0:  # road races: a car fast enough for the time limit
                rule = location.access_rule
                location.access_rule = lambda state, n=driving, r=rule: (
                    r(state) and state.has("Progressive Driving", self.player, n))
        self.multiworld.completion_condition[self.player] = lambda state: all(
            state.has(name, self.player, s["maximum"] - s["starting"]) for name, s in SKILLS.items())

    def fill_slot_data(self) -> dict:
        return {
            "catalog": CATALOG["version"], "schema": 1,
            "skills": [{"index": s["index"], "item": s["item"], "starting": s["starting"], "maximum": s["maximum"]}
                       for s in CATALOG["skills"]],
            "filler_item": CATALOG["filler_item"],
            "goal": "max_skills",
            "include_hidden_orbs": bool(self.options.include_hidden_orbs),
            "include_audio_logs": bool(self.options.include_audio_logs),
            "include_races": bool(self.options.include_races),
        }
