from scm import Snippet, integer, local, text_key


NAME = "The Meat Business"
MISSION_ID = 90


def expand(mission):
    for offset, accuracy in ((0x1710, 45), (0x1793, 41), (0x1CC6, 48)):
        mission.replace(offset, "SET_CHAR_ACCURACY", 1, 30, accuracy)

    # Keep two defenders in cover toward the exit, with another
    # advancing under their fire. Leave Ken's original tasks intact.
    for offset, stock, accuracy in ((0x38F4, 35, 50), (0x397E, 40, 46), (0x3B1C, 35, 50)):
        mission.replace(offset, "SET_CHAR_ACCURACY", 1, stock, accuracy)
    hard = mission.difficulty == "acimasiz"
    for offset, slot, rate, hold in ((0x3902, 77, 48, True), (0x398C, 78, 41, False), (0x3B2A, 81, 51, True)):
        guard = Snippet(mission.table)
        guard.emit("SET_CHAR_SHOOT_RATE", local(slot), integer(rate + (5 if hard else 0)))
        if hold:
            guard.emit("SET_CHAR_KINDA_STAY_IN_SAME_PLACE", local(slot), integer(1))
        mission.insert_before(offset, "TASK_KILL_CHAR_ON_FOOT", guard)
    warning = Snippet(mission.table)
    warning.emit("PRINT_BIG", text_key("CZMEAT"), integer(3600), integer(6))
    mission.insert_before(0x3B32, "SET_LVAR_INT", warning)
