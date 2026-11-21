NAME = "Photo Opportunity"
MISSION_ID = 58


def expand(mission):
    # Tighten the camera framing around the target while maintaining the stock shot sequence.
    width = 0.68 if mission.difficulty == "acimasiz" else 0.76
    for offset in (0x2A31, 0x2A43):
        mission.replace(offset, "SUB_VAL_FROM_FLOAT_LVAR", 1, 1.0, width)
    for offset in (0x2A67, 0x2A79):
        mission.replace(offset, "ADD_VAL_TO_FLOAT_LVAR", 1, 1.0, width)
    mission.replace(0x2E40, "TASK_STAND_STILL", 1, 1500, 900 if mission.difficulty == "acimasiz" else 1100)
