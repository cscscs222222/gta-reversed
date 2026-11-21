NAME = "Cesar Vialpando"
MISSION_ID = 19


def expand(mission):
    # Use a quicker cadence for the lowrider's scripted hydraulic demo.
    # Keep both sides of the phase boundary aligned so the car settles.
    mission.replace(0x15BA, "IS_NUMBER_GREATER_THAN_INT_LVAR", 0, 1800, 1710)
    mission.replace(0x1604, "IS_INT_LVAR_GREATER_THAN_NUMBER", 1, 1800, 1710)
    mission.replace(0x1617, "IS_NUMBER_GREATER_THAN_INT_LVAR", 0, 2500, 2310)
