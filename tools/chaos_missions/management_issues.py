NAME = "Management Issues"
MISSION_ID = 33


def expand(mission):
    window = 6.5 if mission.difficulty == "acimasiz" else 7.5
    for axis in (3, 4):
        mission.replace(0x2753, "LOCATE_CAR_2D", axis, 9.0, window)
        mission.replace(0x278E, "LOCATE_STOPPED_CAR_2D", axis, 10.0, window)
    mission.replace(0x263B, "SET_CAR_CRUISE_SPEED", 1, 15.0, 19.0)
