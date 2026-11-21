NAME = "Drive-By"
MISSION_ID = 17


def expand(mission):
    # The later waves explicitly overwrite the shared accuracy variable.
    # Tune every assignment that feeds the active shooters.
    for offset, old, normal, brutal in (
        (0x11BB, 30, 46, 52),
        (0x39D1, 50, 60, 68),
        (0x3DA4, 30, 47, 55),
    ):
        mission.replace(offset, "SET_VAR_INT", 1, old, brutal if mission.difficulty == "acimasiz" else normal)
    for offset in (0x00CAA, 0x032F4):
        mission.replace(offset, "SET_CAR_HEALTH", 1, 1450, 1050 if mission.difficulty == "acimasiz" else 1180)
    mission.replace(0x3FC4, "ALTER_WANTED_LEVEL_NO_DROP", 1, 2, 3)
    mission.replace(0x466B, "ALTER_WANTED_LEVEL_NO_DROP", 1, 2, 3)
