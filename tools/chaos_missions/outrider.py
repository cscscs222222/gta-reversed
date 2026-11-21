NAME = "Outrider"
MISSION_ID = 60


def expand(mission):
    # Give the three outer roadblock teams distinct pressure.
    mission.replace(0x3237, "SET_CHAR_ACCURACY", 1, 60, 73)
    mission.replace(0x32AD, "SET_CHAR_ACCURACY", 1, 60, 76)
    mission.replace(0x332C, "SET_CHAR_ACCURACY", 1, 60, 69)
