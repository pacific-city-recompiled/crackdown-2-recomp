from dataclasses import dataclass
from Options import Choice, DefaultOnToggle, OptionGroup, PerGameCommonOptions, Toggle


class IncludeHiddenOrbs(DefaultOnToggle):
    """Include the 300 hidden orbs as checks alongside the 500 agility orbs."""
    display_name = "Include Hidden Orbs"


class IncludeAudioLogs(Toggle):
    """Include the 52 audio logs as checks."""
    display_name = "Include Audio Logs"


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
    agility_logic: AgilityLogic
    goal: Goal


option_groups = [
    OptionGroup("Checks", [IncludeHiddenOrbs, IncludeAudioLogs]),
    OptionGroup("Logic", [AgilityLogic]),
    OptionGroup("Goal", [Goal]),
]
