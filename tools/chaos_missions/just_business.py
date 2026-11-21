NAME = "Just Business"
MISSION_ID = 30


def expand(mission):
    # Differentiate the balcony guards from the people at ground level.
    # Both the first approach and the re-created encounter use these guards.
    for offset in (0x1336, 0x1FFF):
        mission.replace(offset, "SET_CHAR_ACCURACY", 1, 60, 75)
    for offset in (0x0FBE, 0x1C87, 0x1185, 0x1E64):
        mission.replace(offset, "SET_CHAR_ACCURACY", 1, 60, 68)
    for offset in (0x10CD, 0x1D96, 0x1361, 0x202A):
        mission.replace(offset, "SET_CHAR_SHOOT_RATE", 1, 50, 61)
    mission.replace(0x3F7D, "SET_CAR_HEALTH", 1, 2000, 1550 if mission.difficulty == "acimasiz" else 1750)
    for offset in (0x6AC1, 0x6B1D):
        mission.replace(offset, "TASK_DRIVE_BY", 9, 10, 30)
