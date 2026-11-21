NAME = "T-Bone Mendez"
MISSION_ID = 65


def expand(mission):
    mission.replace(0x226B, "SET_LVAR_FLOAT", 1, 15.0, 20.0)
    # Match the far branch to the end of the active distance curve.
    mission.replace(0x2305, "ADD_VAL_TO_FLOAT_LVAR", 1, 40.0, 46.0)
    mission.replace(0x231A, "SET_LVAR_FLOAT", 1, 20.0, 26.0)
