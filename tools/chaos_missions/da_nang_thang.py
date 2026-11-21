from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "The Da Nang Thang"
MISSION_ID = 57


def expand(mission):
    # The two aft positions cover the swim back to the escape marker.
    mission.reserve_locals(962, 963)
    guards = (local(962), local(963))
    setup = Snippet(mission.table)
    for guard in guards:
        setup.emit("SET_LVAR_INT", guard, integer(-1))
    mission.insert_before(0x000B, "GOSUB", setup)
    code = Snippet(mission.table)
    for index, (guard, x, y, z) in enumerate((
        (guards[0], -2334.5, 1533.3, 16.3), (guards[1], -2335.1, 1538.6, 16.3),
    )):
        position = tuple(map(floating, (x, y, z)))
        clearance = tuple(map(floating, (x - 0.9, y - 0.9, z - 0.4, x + 0.9, y + 0.9, z + 2)))
        code.emit("IF", integer(2))
        code.emit("HAS_MODEL_LOADED", integer(121))
        code.emit("HAS_MODEL_LOADED", integer(372))
        code.emit("IS_AREA_OCCUPIED", *clearance, *map(integer, (0, 1, 1, 1, 0)), negate=True)
        code.jump("GOTO_IF_FALSE", f"skip_{index}")
        code.emit("CREATE_CHAR", integer(25), integer(121), *position, guard)
        code.emit("GIVE_WEAPON_TO_CHAR", guard, integer(32), integer(75))
        code.emit("SET_CHAR_ACCURACY", guard, integer(40 if index == 0 else 36))
        code.emit("SET_CHAR_SHOOT_RATE", guard, integer(40))
        code.emit("TASK_SHOOT_AT_CHAR", guard, global_var(12), integer(12000))
        code.mark(f"skip_{index}")
    code.emit("IF", integer(21))
    for guard in guards:
        code.emit("DOES_CHAR_EXIST", guard)
    code.jump("GOTO_IF_FALSE", "done")
    code.emit("PRINT_BIG", text_key("CZDECK"), integer(3700), integer(6))
    code.mark("done")
    mission.insert_before(0x4D4E, "SET_LVAR_INT", code)

    cleanup = Snippet(mission.table)
    for index, guard in enumerate(guards):
        cleanup.emit("IF", integer(0))
        cleanup.emit("DOES_CHAR_EXIST", guard)
        cleanup.jump("GOTO_IF_FALSE", f"skip_{index}")
        cleanup.emit("DELETE_CHAR", guard)
        cleanup.mark(f"skip_{index}")
    mission.insert_before(0x5159, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
