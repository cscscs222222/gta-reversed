NAME = "Gray Imports"
MISSION_ID = 23


def expand(mission):
    # The upper positions and the dealership protection have separate accuracy.
    for offset, accuracy in ((0x0B86, 54), (0x0BC1, 44), (0x0BFC, 47), (0x0C37, 50), (0x0C7E, 44)):
        mission.replace(offset, "SET_CHAR_ACCURACY", 1, 30, accuracy)
    for offset in (0x27D1, 0x281C, 0x2867, 0x28AB, 0x28FD, 0x293A, 0x2977):
        mission.replace(offset, "SET_CHAR_ACCURACY", 1, 20, 39)
    for offset in (0x328B, 0x32CF, 0x3313, 0x3357):
        mission.replace(offset, "SET_CHAR_ACCURACY", 1, 10, 28)
    mission.replace(0x43B9, "SET_CAR_CRUISE_SPEED", 1, 20.0, 27.0)
