NAME = "Ice Cold Killa"
MISSION_ID = 61


def expand(mission):
    mission.replace(0x119A, "SET_CHAR_ACCURACY", 1, 70, 78)
    mission.replace(0x22E3, "TASK_DRIVE_BY", 9, 30, 52)
    # Keep the escape branches synchronized with the native fire-state check.
    for offset, stock, speed in ((0x26DD, 25.0, 29.0), (0x26F7, 32.0, 37.0),
                                 (0x2751, 23.0, 28.0), (0x276B, 28.0, 34.0)):
        mission.replace(offset, "TASK_CAR_MISSION", 4, stock, speed)
