NAME = "New Model Army"
MISSION_ID = 74


def expand(mission):
    # Use the original race clock for the tighter setup.
    mission.replace(0x24DE, "SET_VAR_INT", 1, 480000, 432000 if mission.difficulty == "acimasiz" else 454000)
    mission.replace(0x1E0C, "CREATE_OBJECT_NO_OFFSET", 1, -973, -972.7)
    mission.replace(0x1E23, "CREATE_OBJECT_NO_OFFSET", 1, -973, -973.3)
    mission.replace(0x57DF, "HELI_GOTO_COORDS", 4, 5, 7)
    mission.replace(0x588D, "HELI_GOTO_COORDS", 4, 5, 6)
