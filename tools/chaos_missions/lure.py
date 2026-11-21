NAME = "Lure"
MISSION_ID = 55


def expand(mission):
    mission.replace(0x13A6, "TASK_CAR_DRIVE_TO_COORD", 5, 30.0, 36.0)
    mission.replace(0x25CF, "TASK_CAR_DRIVE_TO_COORD", 5, 20.0, 25.0)
    mission.replace(0x16D3, "TASK_CAR_MISSION", 3, 29, 17)
    mission.replace(0x1701, "TASK_CAR_MISSION", 3, 30, 31)
    mission.replace(0x16D3, "TASK_CAR_MISSION", 4, 30.0, 34.0)
    mission.replace(0x1701, "TASK_CAR_MISSION", 4, 30.0, 36.0)
    for offset in (0x0584, 0x11C1):
        mission.replace(offset, "SET_CAR_HEALTH", 1, 9000, 6800 if mission.difficulty == "acimasiz" else 7600)
    for offset in (0x0642, 0x1C91):
        mission.replace(offset, "TASK_DRIVE_BY", 9, 60, 74)
