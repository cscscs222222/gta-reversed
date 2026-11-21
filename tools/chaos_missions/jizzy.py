from scm import Snippet, integer, text_key


NAME = "Jizzy"
MISSION_ID = 59


def expand(mission):
    mission.replace(0x1565, "SET_VAR_INT", 1, 540000, 485000 if mission.difficulty == "acimasiz" else 515000)
    mission.replace(0x21B1, "SET_VAR_INT", 1, 120000, 93000 if mission.difficulty == "acimasiz" else 103000)
    mission.replace(0x2D6B, "TASK_CAR_DRIVE_TO_COORD", 5, 15.0, 20.0)
    mission.replace(0x2D8C, "TASK_CAR_DRIVE_WANDER", 2, 15.0, 22.0)
    warning = Snippet(mission.table)
    warning.emit("PRINT_BIG", text_key("CZJIZZY"), integer(3400), integer(6))
    mission.redirect_before(0x2D9F, "PERFORM_SEQUENCE_TASK", warning)
