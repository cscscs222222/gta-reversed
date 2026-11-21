from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "You've Had Your Chips"
MISSION_ID = 86


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    for offset in (0x0A69, 0x1BA4):
        mission.replace(offset, "SET_OBJECT_HEALTH", 1, 1000, 1450 if hard else 1250)

    # Security responds when the final machine is destroyed. The new
    # guards can be fought or bypassed on the return to the casino.
    mission.reserve_locals(962, 963)
    init = Snippet(mission.table)
    response = Snippet(mission.table)
    cleanup = Snippet(mission.table)
    for slot, model, weapon, point in (
        (962, 124, 28, (1083.01, 2099.13, 14.3504)),
        (963, 127, 22, (1092.61, 2116.91, 9.8203)),
    ):
        guard = local(slot)
        init.emit("SET_LVAR_INT", guard, integer(-1))
        x, y, z = point
        label = f"next_guard_{slot}"
        response.emit("IF", integer(2))
        response.emit("HAS_MODEL_LOADED", integer(model))
        response.emit("HAS_MODEL_LOADED", integer(352 if weapon == 28 else 346))
        response.emit(
            "IS_AREA_OCCUPIED", *map(floating, (x - 1, y - 1, z - 0.5, x + 1, y + 1, z + 1.4)),
            *map(integer, (0, 1, 1, 1, 0)), negate=True,
        )
        response.jump("GOTO_IF_FALSE", label)
        response.emit("CREATE_CHAR", integer(24), integer(model), *map(floating, point), guard)
        response.emit("SET_CHAR_DECISION_MAKER", guard, local(73))
        response.emit("GIVE_WEAPON_TO_CHAR", guard, integer(weapon), integer(90))
        response.emit("SET_CHAR_ACCURACY", guard, integer(43 if hard else 33))
        response.emit("SET_CHAR_SHOOT_RATE", guard, integer(48 if hard else 39))
        response.emit("TASK_KILL_CHAR_ON_FOOT_WHILE_DUCKING", guard, global_var(12), *map(integer, (4, 5000, 45)))
        response.mark(label)
        cleanup.emit("IF", integer(0))
        cleanup.emit("DOES_CHAR_EXIST", guard)
        cleanup.jump("GOTO_IF_FALSE", label)
        cleanup.emit("DELETE_CHAR", guard)
        cleanup.mark(label)
    response.emit("IF", integer(21))
    response.emit("DOES_CHAR_EXIST", local(962))
    response.emit("DOES_CHAR_EXIST", local(963))
    response.jump("GOTO_IF_FALSE", "done")
    response.emit("PRINT_BIG", text_key("CZCHIPS"), integer(3900), integer(6))
    response.mark("done")
    mission.insert_before(0x000B, "GOSUB", init)
    mission.insert_before(0x1C51, "PRINT_NOW", response)
    mission.insert_before(0x3881, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
