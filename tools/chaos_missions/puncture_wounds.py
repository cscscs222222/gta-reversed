NAME = "Puncture Wounds"
MISSION_ID = 70


def expand(mission):
    mission.replace(0x18E0, "SET_CAR_CRUISE_SPEED", 1, 50.0, 57.0)
    # Speed up the open stretches. Keep the slower corners as interception points.
    for offset, speed in ((0x19BF, 26.0), (0x19E0, 24.0), (0x1A64, 24.0), (0x1A85, 26.0), (0x1B35, 25.0)):
        mission.replace(offset, "TASK_CAR_DRIVE_TO_COORD", 5, 20.0, speed)
