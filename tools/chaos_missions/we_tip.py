NAME = "555 We Tip"
MISSION_ID = 51


def expand(mission):
    # Use the original valet timer, including its failure and cleanup path.
    mission.replace(0x20AB, "SET_VAR_INT", 1, 150000, 112000 if mission.difficulty == "acimasiz" else 128000)
    width = 2.3 if mission.difficulty == "acimasiz" else 2.5
    for axis in (4, 5):
        mission.replace(0x2733, "LOCATE_CAR_3D", axis, 3.0, width)
