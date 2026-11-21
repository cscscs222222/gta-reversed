NAME = "High Stakes, Low Rider"
MISSION_ID = 36


def expand(mission):
    # Give the four rivals different pacing. Keep the starting sequence and Cesar's car.
    mission.replace(0x0267, "SET_CAR_CRUISE_SPEED", 1, 35.0, 40.0)
    mission.replace(0x0318, "SET_CAR_CRUISE_SPEED", 1, 35.0, 38.0)
    mission.replace(0x03C9, "SET_CAR_CRUISE_SPEED", 1, 35.0, 42.0)
    mission.replace(0x045D, "SET_CAR_CRUISE_SPEED", 1, 35.0, 44.0)
