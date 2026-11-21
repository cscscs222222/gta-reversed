from scene_vehicles import passing_vehicle


NAME = "Zeroing In"
MISSION_ID = 67


def expand(mission):
    mission.replace(0x0FD7, "TASK_CAR_DRIVE_TO_COORD", 5, 35.0, 43.0)
    mission.replace(0x11B0, "TASK_CAR_DRIVE_WANDER", 2, 35.0, 41.0)
    # The spare car is scenery, keeping the tracking signal on the real target.
    decoy = passing_vehicle(mission, 602, 41, (-2708.79, -9.59, 3.98), 178.25, 14.0, caption="CZLOOK")
    mission.insert_before(0x0FF3, "SET_LVAR_INT", decoy)
