NAME = "Snail Trail"
MISSION_ID = 52


def expand(mission):
    # Tighten the trailing distance in the on-foot pursuit where suspicion is active.
    mission.replace(0x45DA, "IS_NUMBER_GREATER_OR_EQUAL_TO_FLOAT_LVAR", 0, 40.0, 48.0)
    mission.replace(0x460F, "IS_FLOAT_LVAR_GREATER_OR_EQUAL_TO_NUMBER", 1, 100.0, 88.0)
    mission.replace(0x486D, "IS_FLOAT_LVAR_GREATER_OR_EQUAL_TO_NUMBER", 1, 140.0, 129.0)
