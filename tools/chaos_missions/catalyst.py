from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Catalyst"
MISSION_ID = 25


def expand(mission):
    mission.replace(0x136E, "SET_CHAR_ACCURACY", 1, 50, 63)
    mission.replace(0x01DAB, "SET_CHAR_SHOOT_RATE", 1, 40, 51)
    mission.replace(0x01DB6, "SET_CHAR_ACCURACY", 1, 40, 52)
    mission.replace(0x02929, "SET_VAR_INT", 1, 90000, 70000 if mission.difficulty == "acimasiz" else 77000)

    # The four stock passengers arrive by road. The additional pair enters
    # from opposite sides of the tracks once the car has stopped.
    mission.reserve_locals(960, 961, 962, 963)
    guards = ((local(960), local(962)), (local(961), local(963)))
    setup = Snippet(mission.table)
    for guard, marker in guards:
        setup.emit("SET_LVAR_INT", guard, integer(-1))
        setup.emit("SET_LVAR_INT", marker, integer(-1))
    mission.insert_before(0x000B, "GOSUB", setup)

    flank = Snippet(mission.table)
    for index, ((guard, marker), (x, y, z)) in enumerate(zip(guards, (
        (2286.8, -1149.2, 25.75),
        (2300.1, -1156.1, 25.66),
    ), strict=True)):
        point = tuple(map(floating, (x, y, z)))
        clearance = tuple(map(floating, (x - 0.85, y - 0.85, z - 0.5, x + 0.85, y + 0.85, z + 2)))
        flank.emit("IF", integer(2))
        flank.emit("HAS_MODEL_LOADED", integer(102))
        flank.emit("HAS_MODEL_LOADED", integer(352))
        flank.emit("IS_AREA_OCCUPIED", *clearance, *map(integer, (0, 1, 1, 1, 0)), negate=True)
        flank.jump("GOTO_IF_FALSE", f"next_{index}")
        flank.emit("CREATE_CHAR", integer(25), integer(102), *point, guard)
        flank.emit("GIVE_WEAPON_TO_CHAR", guard, integer(28), integer(150))
        flank.emit("SET_CURRENT_CHAR_WEAPON", guard, integer(28))
        flank.emit("SET_CHAR_ACCURACY", guard, integer(53))
        flank.emit("SET_CHAR_SHOOT_RATE", guard, integer(55))
        flank.emit("TASK_KILL_CHAR_ON_FOOT", guard, global_var(12))
        flank.emit("ADD_BLIP_FOR_CHAR", guard, marker)
        flank.mark(f"next_{index}")
    flank.emit("IF", integer(21))
    for guard, _ in guards:
        flank.emit("DOES_CHAR_EXIST", guard)
    flank.jump("GOTO_IF_FALSE", "done")
    flank.emit("PRINT_BIG", text_key("CZCAT"), integer(3800), integer(6))
    flank.mark("done")
    mission.insert_before(0x02097, "ADD_VAL_TO_INT_LVAR", flank)

    # Keep the board-the-train marker gated on every living attacker.
    mission.replace(0x0223B, "IF", 0, 3, 5)
    remaining = Snippet(mission.table)
    cleanup = Snippet(mission.table)
    clear_dead = Snippet(mission.table)
    for index, (guard, marker) in enumerate(guards):
        remaining.emit("IS_CHAR_DEAD", guard)
        clear_dead.emit("IF", integer(1))
        clear_dead.emit("IS_CHAR_DEAD", guard)
        clear_dead.emit("DOES_BLIP_EXIST", marker)
        clear_dead.jump("GOTO_IF_FALSE", f"next_{index}")
        clear_dead.emit("REMOVE_BLIP", marker)
        clear_dead.mark(f"next_{index}")
        cleanup.emit("IF", integer(0))
        cleanup.emit("DOES_BLIP_EXIST", marker)
        cleanup.jump("GOTO_IF_FALSE", f"no_marker_{index}")
        cleanup.emit("REMOVE_BLIP", marker)
        cleanup.mark(f"no_marker_{index}")
        cleanup.emit("IF", integer(0))
        cleanup.emit("DOES_CHAR_EXIST", guard)
        cleanup.jump("GOTO_IF_FALSE", f"no_guard_{index}")
        cleanup.emit("DELETE_CHAR", guard)
        cleanup.mark(f"no_guard_{index}")
    mission.insert_before(0x02253, "GOTO_IF_FALSE", remaining)
    mission.insert_before(0x020B0, "SET_LVAR_INT", clear_dead)
    mission.insert_before(0x048B5, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
