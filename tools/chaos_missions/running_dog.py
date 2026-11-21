from scm import Snippet, integer, text_key


NAME = "Running Dog"
MISSION_ID = 28


def expand(mission):
    # The Vagos in the original ambush can leave their initial marks and
    # pursue CJ. Strengthen the drive-by from the crew that picks up the runner.
    mission.replace(0x1F75, "SET_CHAR_KINDA_STAY_IN_SAME_PLACE", 1, 1, 0)
    mission.replace(0x1195, "TASK_DRIVE_BY", 9, 60, 73)
    mission.replace(0x117A, "TASK_CAR_DRIVE_WANDER", 2, 30.0, 37.0)
    mission.replace(0x18F5, "SET_CHAR_HEALTH", 1, 200, 240)
    mission.replace(0x1F54, "SET_CHAR_ACCURACY", 1, 50, 61)

    # One reinforcement wears the runner's outfit. The original marker
    # still identifies the real target.
    mission.replace(0x1EDC, "CREATE_CHAR", 1, 109, 108)
    notice = Snippet(mission.table)
    notice.emit("PRINT_BIG", text_key("CZRUN"), integer(3200), integer(6))
    mission.insert_before(0x1EDC, "CREATE_CHAR", notice)
