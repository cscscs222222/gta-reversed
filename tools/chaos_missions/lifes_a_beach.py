NAME = "Life's a Beach"
MISSION_ID = 31


def expand(mission):
    mission.replace(0x1138, "IS_INT_VAR_GREATER_OR_EQUAL_TO_NUMBER", 1, 2500, 3000 if mission.difficulty == "acimasiz" else 2750)
    mission.replace(0x1BFD, "SET_CAR_HEALTH", 1, 5000, 2450 if mission.difficulty == "acimasiz" else 2950)
    for offset in (0x2A9F, 0x2CDD):
        mission.replace(offset, "TASK_CAR_MISSION", 4, 30.0, 36.0)
    mission.replace(0x2E3D, "TASK_CAR_DRIVE_TO_COORD", 5, 30.0, 38.0)
