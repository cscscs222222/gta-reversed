from scm import Snippet, integer, local, text_key


NAME = "Beat Down on B Dup"
MISSION_ID = 106


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x10CC, "SET_ZONE_GANG_STRENGTH", 2, 40, 62 if hard else 55)

    # The gate defenders advance from opposite sides while others
    # hold their posts. Keep their original objective markers.
    mission.replace(0x1FAC, "SET_CHAR_STAY_IN_SAME_PLACE", 1, 1, 0)
    mission.replace(0x2DD6, "TASK_STAY_IN_SAME_PLACE", 1, 1, 0)
    for offset, guard, accuracy, rate in (
        (0x1FB3, 42, 42, 48), (0x204F, 45, 53, 54),
        (0x20EB, 48, 44, 45), (0x215F, 51, 49, 51),
    ):
        defend = Snippet(mission.table)
        defend.emit("SET_CHAR_ACCURACY", local(guard), integer(accuracy + (6 if hard else 0)))
        defend.emit("SET_CHAR_SHOOT_RATE", local(guard), integer(rate + (5 if hard else 0)))
        mission.insert_before(offset, "TASK_KILL_CHAR_ON_FOOT", defend)
    warning = Snippet(mission.table)
    warning.emit("PRINT_BIG", text_key("CZGLEN"), integer(3900), integer(6))
    mission.insert_before(0x1F50, "CREATE_CHAR", warning)
