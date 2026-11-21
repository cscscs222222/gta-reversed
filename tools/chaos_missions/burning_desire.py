NAME = "Burning Desire"
MISSION_ID = 22


def expand(mission):
    # Allow one interior fire to propagate by one generation. The other
    # fires still follow the stock layout and the player's extinguisher works.
    # Allow chosen room fires to spread along the original rescue route.
    for offset in (0x5E76, 0x5F20, 0x60CA):
        mission.replace(offset, "START_SCRIPT_FIRE", 3, 0, 1)
    if mission.difficulty == "acimasiz":
        mission.replace(0x5E98, "START_SCRIPT_FIRE", 3, 0, 1)
