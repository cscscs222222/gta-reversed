NAME = "Small Town Bank"
MISSION_ID = 42


def expand(mission):
    # Require longer aim to restore each hostage's control meter.
    for offset, stock, stricter in ((0x1A62, 0.020, 0.016), (0x1C1B, 0.023, 0.019),
                                   (0x1DD4, 0.026, 0.021), (0x1F8D, 0.025, 0.020)):
        mission.replace(offset, "ADD_TIMED_VAL_TO_FLOAT_LVAR", 1, stock, stricter)
    mission.replace(0x2E05, "TASK_CAR_DRIVE_TO_COORD", 5, 17.0, 21.0)
    mission.replace(0x3022, "TASK_CAR_DRIVE_TO_COORD", 5, 17.0, 20.0)
    mission.replace(0x4C91, "SET_CHAR_SHOOT_RATE", 1, 40, 50)
    mission.replace(0x4C9C, "SET_CHAR_ACCURACY", 1, 60, 69)
