NAME = "Local Liquor Store"
MISSION_ID = 41


def expand(mission):
    # Stagger the four existing quads on their departure routes.
    for offset, normal, brutal in ((0x0F83, 25, 28), (0x0FC3, 23, 25), (0x1003, 24, 27), (0x1043, 27, 30)):
        mission.replace(offset, "SET_CAR_CRUISE_SPEED", 1, 20.0, float(brutal if mission.difficulty == "acimasiz" else normal))
