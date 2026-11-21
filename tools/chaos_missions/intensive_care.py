from scm import Snippet, integer, text_key


NAME = "Intensive Care"
MISSION_ID = 89


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x0A70, "TASK_CAR_DRIVE_WANDER", 2, 15.0, 12.0)
    mission.replace(0x0BCC, "TASK_CAR_DRIVE_WANDER", 2, 15.0, 18.0)
    for offset in (0x172B, 0x17B9, 0x192C):
        mission.replace(offset, "TASK_CAR_DRIVE_WANDER", 2, 25.0, 32.0 if hard else 30.0)

    # The original escorts only deploy for the identified ambulance.
    # One closes to ram it, while the second tries to block ahead.
    for offset, role, speed in ((0x20B2, 16, 35.0), (0x220A, 17, 38.0)):
        mission.replace(offset, "TASK_CAR_MISSION", 3, 3, role)
        mission.replace(offset, "TASK_CAR_MISSION", 4, 32, speed + (3.0 if hard else 0.0))
    warning = Snippet(mission.table)
    warning.emit("PRINT_BIG", text_key("CZAMB"), integer(3800), integer(6))
    mission.insert_before(0x16E9, "MARK_MODEL_AS_NO_LONGER_NEEDED", warning)
