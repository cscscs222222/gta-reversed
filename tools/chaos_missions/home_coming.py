from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Home Coming"
MISSION_ID = 104


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    for offset in (0x0E88, 0x0EA6):
        mission.replace(offset, "SET_ZONE_GANG_STRENGTH", 2, 40, 58 if hard else 51)

    # After Grove is retaken, two remaining Ballas try to stop CJ.
    # Their attack tasks target CJ and never add a new win condition.
    mission.reserve_locals(962, 963)
    init = Snippet(mission.table)
    rally = Snippet(mission.table)
    cleanup = Snippet(mission.table)
    for slot, model, point, holds_cover in (
        (962, 28, (2474.2, -1681.1, 12.6), True),
        (963, 29, (2466.8, -1665.0, 12.6), False),
    ):
        guard = local(slot)
        init.emit("SET_LVAR_INT", guard, integer(-1))
        x, y, z = point
        next_guard = f"next_guard_{slot}"
        rally.emit("IF", integer(2))
        rally.emit("HAS_MODEL_LOADED", integer(model))
        rally.emit("HAS_MODEL_LOADED", integer(355))
        rally.emit(
            "IS_AREA_OCCUPIED", *map(floating, (x - 1, y - 1, z - 0.6, x + 1, y + 1, z + 1.4)),
            *map(integer, (0, 1, 1, 1, 0)), negate=True,
        )
        rally.jump("GOTO_IF_FALSE", next_guard)
        rally.emit("CREATE_CHAR", integer(17), integer(model), *map(floating, point), guard)
        rally.emit("GIVE_WEAPON_TO_CHAR", guard, integer(30), integer(90))
        rally.emit("SET_CHAR_ACCURACY", guard, integer(40 if hard else 34))
        rally.emit("SET_CHAR_SHOOT_RATE", guard, integer(44 if hard else 37))
        if holds_cover:
            rally.emit("SET_CHAR_KINDA_STAY_IN_SAME_PLACE", guard, integer(1))
        rally.emit(
            "TASK_KILL_CHAR_ON_FOOT_WHILE_DUCKING", guard, global_var(12), *map(integer, (4, 5200, 35)),
        )
        rally.mark(next_guard)
        cleanup.emit("IF", integer(0))
        cleanup.emit("DOES_CHAR_EXIST", guard)
        cleanup.jump("GOTO_IF_FALSE", next_guard)
        cleanup.emit("DELETE_CHAR", guard)
        cleanup.mark(next_guard)

    rally.emit("IF", integer(21))
    rally.emit("DOES_CHAR_EXIST", local(962))
    rally.emit("DOES_CHAR_EXIST", local(963))
    rally.jump("GOTO_IF_FALSE", "done")
    rally.emit("PRINT_BIG", text_key("CZGROVE"), integer(3900), integer(6))
    rally.mark("done")
    mission.insert_before(0x000B, "GOSUB", init)
    mission.insert_before(0x11C0, "PRINT_NOW", rally)
    mission.insert_before(0x1BA0, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
