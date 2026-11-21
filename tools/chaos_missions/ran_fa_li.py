NAME = "Ran Fa Li"
MISSION_ID = 54


def expand(mission):
    # Two cars intercept the target from different streets. The original
    # motorcycle teams keep their flanking assignments.
    mission.replace(0x148F, "TASK_CAR_MISSION", 3, 30, 17)
    mission.replace(0x180E, "TASK_CAR_MISSION", 3, 29, 15)
    mission.replace(0x0755, "SET_CAR_HEALTH", 1, 3000, 2150 if mission.difficulty == "acimasiz" else 2500)
    for offset, accuracy in ((0x1086, 68), (0x11AC, 60), (0x12FF, 66), (0x144B, 59), (0x1573, 60)):
        mission.replace(offset, "TASK_DRIVE_BY", 9, 50, accuracy)
