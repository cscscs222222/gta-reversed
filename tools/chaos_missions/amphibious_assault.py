NAME = "Amphibious Assault"
MISSION_ID = 56


def expand(mission):
    # Broaden two sweep cones and speed up the patrol boats. The other gaps stay open.
    mission.replace(0x3051, "SET_BOAT_CRUISE_SPEED", 1, 7.0, 9.0)
    mission.replace(0x4909, "CREATE_SEARCHLIGHT", 6, 25.0, 30.0)
    mission.replace(0x4B8E, "CREATE_SEARCHLIGHT", 6, 25.0, 28.5)
    mission.replace(0x49AA, "MOVE_SEARCHLIGHT_BETWEEN_COORDS", 7, 0.10, 0.121)
    mission.replace(0x4A81, "MOVE_SEARCHLIGHT_BETWEEN_COORDS", 7, 0.08, 0.098)
