NAME = "Supply Lines..."
MISSION_ID = 73


def expand(mission):
    # The first vehicles move slowly while the last chase covers more
    # ground. This keeps fuel available for actually reaching the targets.
    mission.replace(0x06C0, "TASK_CAR_DRIVE_WANDER", 2, 3.0, 4.5)
    mission.replace(0x17BF, "TASK_CAR_DRIVE_WANDER", 2, 12.0, 16.0 if mission.difficulty == "acimasiz" else 15.0)
