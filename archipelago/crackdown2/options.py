from dataclasses import dataclass
from Options import Choice, DefaultOnToggle, OptionGroup, PerGameCommonOptions, Toggle


class IncludeHiddenOrbs(DefaultOnToggle):
    """Include the 300 hidden orbs as checks alongside the 500 agility orbs."""
    display_name = "Include Hidden Orbs"


class IncludeAudioLogs(Toggle):
    """Include the 52 audio logs as checks."""
    display_name = "Include Audio Logs"


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
    goal: Goal


option_groups = [
    OptionGroup("Checks", [IncludeHiddenOrbs, IncludeAudioLogs]),
    OptionGroup("Goal", [Goal]),
]
