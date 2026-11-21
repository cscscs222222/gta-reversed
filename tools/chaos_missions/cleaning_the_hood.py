from scm import Snippet, floating, global_var, integer, local


NAME = "Cleaning the Hood"
MISSION_ID = 14


def expand(mission):
    mission.reserve_locals(959, 960, 961, 962, 963)
    state, health, backup, guard, sequence = map(local, range(959, 964))
    cj = global_var(12)
    code = Snippet(mission.table)
    code.emit("IF", integer(0))
    code.emit("HAS_MODEL_LOADED", integer(102))
    code.jump("GOTO_IF_FALSE", "done")
    code.emit(
        "CREATE_CHAR", integer(25), integer(102),
        floating(331.9), floating(1124.55), floating(1082.88), guard,
    )
    code.emit("SET_CHAR_AREA_VISIBLE", guard, integer(5))
    code.emit("SET_CHAR_HEADING", guard, floating(90.0))
    code.emit("SET_CHAR_MAX_HEALTH", guard, integer(145))
    code.emit("SET_CHAR_HEALTH", guard, integer(145))
    code.emit("IF", integer(0))
    code.emit("HAS_MODEL_LOADED", integer(336))
    code.jump("GOTO_IF_FALSE", "attack")
    code.emit("GIVE_WEAPON_TO_CHAR", guard, integer(5), integer(1))
    code.emit("SET_CURRENT_CHAR_WEAPON", guard, integer(5))
    code.emit("SET_CHAR_DROPS_WEAPONS_WHEN_DEAD", guard, integer(0))
    code.mark("attack")
    code.emit("OPEN_SEQUENCE_TASK", sequence)
    code.emit("TASK_DUCK", integer(-1), integer(950))
    code.emit("TASK_KILL_CHAR_ON_FOOT", integer(-1), cj)
    code.emit("CLOSE_SEQUENCE_TASK", sequence)
    code.emit("PERFORM_SEQUENCE_TASK", guard, sequence)
    code.emit("CLEAR_SEQUENCE_TASK", sequence)
    code.emit("SET_LVAR_INT", state, integer(1))
    code.mark("done")
    mission.insert_before(0x30B7, "IF", code)

    # A guard who is badly hurt calls in a second attacker. Killing the
    # guard quickly also triggers the response from the adjoining room.
    retreat = Snippet(mission.table)
    retreat.emit("IF", integer(0))
    retreat.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", state, integer(1))
    retreat.jump("GOTO_IF_FALSE", "done")
    retreat.emit("IF", integer(0))
    retreat.emit("IS_CHAR_DEAD", guard, negate=True)
    retreat.jump("GOTO_IF_FALSE", "backup")
    retreat.emit("GET_CHAR_HEALTH", guard, health)
    retreat.emit("IF", integer(0))
    retreat.emit("IS_NUMBER_GREATER_THAN_INT_LVAR", integer(90), health)
    retreat.jump("GOTO_IF_FALSE", "done")
    retreat.emit("TASK_SMART_FLEE_CHAR", guard, cj, floating(24.0), integer(6500))
    retreat.mark("backup")
    retreat.emit("SET_LVAR_INT", state, integer(2))
    point = tuple(map(floating, (325.0, 1126.4, 1082.88)))
    clearance = tuple(map(floating, (323.9, 1125.3, 1082.3, 326.1, 1127.5, 1084.8)))
    retreat.emit("IF", integer(1))
    retreat.emit("HAS_MODEL_LOADED", integer(102))
    retreat.emit("IS_AREA_OCCUPIED", *clearance, *map(integer, (0, 1, 1, 0, 0)), negate=True)
    retreat.jump("GOTO_IF_FALSE", "done")
    retreat.emit("CREATE_CHAR", integer(25), integer(102), *point, backup)
    retreat.emit("SET_CHAR_AREA_VISIBLE", backup, integer(5))
    retreat.emit("SET_CHAR_MAX_HEALTH", backup, integer(125))
    retreat.emit("SET_CHAR_HEALTH", backup, integer(125))
    retreat.emit("TASK_KILL_CHAR_ON_FOOT", backup, cj)
    retreat.mark("done")
    mission.insert_before(0x32E8, "IF", retreat)
