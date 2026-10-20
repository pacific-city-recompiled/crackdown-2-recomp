from dataclasses import dataclass
from Options import Choice, DefaultOnToggle, OptionGroup, PerGameCommonOptions, Toggle


class IncludeHiddenOrbs(DefaultOnToggle):
    """Include the 300 hidden orbs as checks alongside the 500 agility orbs."""
    display_name = "Include Hidden Orbs"


class IncludeAudioLogs(Toggle):
    """Include the 52 audio logs as checks."""
    display_name = "Include Audio Logs"


class IncludeRaces(DefaultOnToggle):
    """Include the 15 road races and 15 rooftop races as checks (a check is sent the first time you win each).

    Road races expect one Progressive Driving; rooftop races expect Agility 3."""
    display_name = "Include Races"


class IncludeTacticalLocations(DefaultOnToggle):
    """Include the 27 tactical locations (Cell strongholds) as checks. Logic expects one Progressive Firearms or
    Explosives."""
    display_name = "Include Tactical Locations"


class IncludeAbsorptionUnits(DefaultOnToggle):
    """Include the 27 absorption units (Freak Lairs) as checks. Logic expects one Progressive Firearms or
    Explosives."""
    display_name = "Include Absorption Units"


class IncludeFreakBreaches(DefaultOnToggle):
    """Include the 25 Freak Breaches (they open at night) as checks. Logic expects one Progressive Firearms or
    Explosives."""
    display_name = "Include Freak Breaches"


class AgilityLogic(Choice):
    """How much Agility the logic expects before an orb can be reached.

    **Bands:** the game's own orb tier plus height: low orbs need nothing, the highest rooftops and Agency Tower
    need Agility 4 or 5. **None:** every orb is in logic from the start (for players who like to climb)."""
    display_name = "Agility Logic"
    option_bands = 0
    option_none = 1
    default = 0


class Goal(Choice):
    """**Max Skills:** receive every Progressive Skill and reach the top level in all five skills.

    The Project Sunburst goal (finish the final phase at Agency Tower) comes in a later version."""
    display_name = "Goal"
    option_max_skills = 0
    default = 0


@dataclass
class Crackdown2Options(PerGameCommonOptions):
    include_hidden_orbs: IncludeHiddenOrbs
    include_audio_logs: IncludeAudioLogs
    include_races: IncludeRaces
    include_tactical_locations: IncludeTacticalLocations
    include_absorption_units: IncludeAbsorptionUnits
    include_freak_breaches: IncludeFreakBreaches
    agility_logic: AgilityLogic
    goal: Goal


option_groups = [
    OptionGroup("Checks", [IncludeHiddenOrbs, IncludeAudioLogs, IncludeRaces, IncludeTacticalLocations,
                           IncludeAbsorptionUnits, IncludeFreakBreaches]),
    OptionGroup("Logic", [AgilityLogic]),
    OptionGroup("Goal", [Goal]),
]
