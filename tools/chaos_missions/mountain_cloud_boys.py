NAME = "Mountain Cloud Boys"
MISSION_ID = 53


def expand(mission):
    # One driver blocks Woozie's car and the other tries to ram it.
    mission.replace(0x4A90, "TASK_CAR_MISSION", 3, 2, 17)
    mission.replace(0x4AC9, "TASK_CAR_MISSION", 3, 2, 15)
    mission.replace(0x4A90, "TASK_CAR_MISSION", 4, 20.0, 26.0)
    mission.replace(0x4AC9, "TASK_CAR_MISSION", 4, 20.0, 29.0)
    mission.replace(0x4AFD, "SET_CAR_HEALTH", 1, 3000, 2150 if mission.difficulty == "acimasiz" else 2450)
