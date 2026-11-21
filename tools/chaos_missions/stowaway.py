from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Stowaway"
MISSION_ID = 80


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x09C8, "SET_CAR_FORWARD_SPEED", 1, 12, 12.5)
    mission.replace(0x09DC, "SET_CAR_FORWARD_SPEED", 1, 12, 15 if hard else 14)
    mission.replace(0x0BB9, "SET_CAR_FORWARD_SPEED", 1, 30, 32.5 if hard else 31.5)

    # Stage an unarmed defender only after CJ enters the aircraft interior.
    mission.reserve_locals(963)
    guard = local(963)
    init = Snippet(mission.table)
    init.emit("SET_LVAR_INT", guard, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)

    x, y, z = 315.3, 997.4, 1955.2
    boarding = Snippet(mission.table)
    boarding.emit("IF", integer(1))
    boarding.emit("HAS_MODEL_LOADED", integer(166))
    boarding.emit(
        "IS_AREA_OCCUPIED",
        *map(floating, (x - 0.6, y - 0.6, z - 0.5, x + 0.6, y + 0.6, z + 1.5)),
        *map(integer, (0, 0, 1, 0, 0)), negate=True,
    )
    boarding.jump("GOTO_IF_FALSE", "done")
    boarding.emit("CREATE_CHAR", integer(26), integer(166), *map(floating, (x, y, z)), guard)
    boarding.emit("SET_CHAR_AREA_VISIBLE", guard, integer(9))
    health = integer(125 if hard else 110)
    boarding.emit("SET_CHAR_MAX_HEALTH", guard, health)
    boarding.emit("SET_CHAR_HEALTH", guard, health)
    boarding.emit("TASK_KILL_CHAR_ON_FOOT", guard, global_var(12))
    boarding.emit("PRINT_BIG", text_key("CZCARGO"), integer(3500), integer(6))
    boarding.mark("done")
    mission.insert_before(0x2B16, "SET_LVAR_INT", boarding)

    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_CHAR_EXIST", guard)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("DELETE_CHAR", guard)
    cleanup.emit("SET_LVAR_INT", guard, integer(-1))
    cleanup.mark("done")
    mission.insert_before(0x30B6, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
    mission.insert_before(0x564D, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
