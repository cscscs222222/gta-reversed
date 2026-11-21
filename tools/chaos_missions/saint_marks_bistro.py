from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Saint Mark's Bistro"
MISSION_ID = 92


def expand(mission):
    # Differentiate the guards at the front tables and the raised back position.
    mission.replace(0x1C09, "SET_CHAR_ACCURACY", 1, 65, 73)
    mission.replace(0x1CE4, "SET_CHAR_ACCURACY", 1, 40, 49)
    mission.replace(0x1E58, "SET_CHAR_SHOOT_RATE", 1, 40, 55)

    # The last two defenders enter the main room when CJ approaches the
    # target's table. The stock target and escape checks stay intact.
    hard = mission.difficulty == "acimasiz"
    mission.reserve_locals(961, 962, 963)
    ready, first, second = map(local, (961, 962, 963))
    cj = global_var(12)
    init = Snippet(mission.table)
    init.emit("SET_LVAR_INT", ready, integer(0))
    for guard in (first, second):
        init.emit("SET_LVAR_INT", guard, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)

    ambush = Snippet(mission.table)
    ambush.emit("IF", integer(2))
    ambush.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", ready, integer(0))
    ambush.emit("IS_CHAR_DEAD", local(40), negate=True)
    ambush.emit(
        "LOCATE_CHAR_ANY_MEANS_3D", cj, *map(floating, (-783.62, 505.055, 1370.79, 11.0, 10.0, 3.0)), integer(0),
    )
    ambush.jump("GOTO_IF_FALSE", "done")
    ambush.emit("SET_LVAR_INT", ready, integer(1))
    cleanup = Snippet(mission.table)
    for guard, model, point, accuracy, label in (
        (first, 124, (-786.511, 505.073, 1370.78), 44, "second"),
        (second, 126, (-781.09, 497.79, 1370.78), 48, "notice"),
    ):
        x, y, z = point
        ambush.emit("IF", integer(2))
        ambush.emit("HAS_MODEL_LOADED", integer(model))
        ambush.emit("HAS_MODEL_LOADED", integer(352))
        ambush.emit(
            "IS_AREA_OCCUPIED", *map(floating, (x - 0.9, y - 0.9, z - 0.5, x + 0.9, y + 0.9, z + 1.4)),
            *map(integer, (0, 1, 1, 1, 0)), negate=True,
        )
        ambush.jump("GOTO_IF_FALSE", label)
        ambush.emit("CREATE_CHAR", integer(24), integer(model), *map(floating, point), guard)
        ambush.emit("SET_CHAR_AREA_VISIBLE", guard, integer(1))
        ambush.emit("GIVE_WEAPON_TO_CHAR", guard, integer(28), integer(80))
        ambush.emit("SET_CHAR_ACCURACY", guard, integer(accuracy + (7 if hard else 0)))
        ambush.emit("SET_CHAR_SHOOT_RATE", guard, integer(49 if hard else 41))
        ambush.emit("TASK_KILL_CHAR_ON_FOOT_WHILE_DUCKING", guard, cj, *map(integer, (4, 3500, 40)))
        ambush.mark(label)
        cleanup.emit("IF", integer(0))
        cleanup.emit("DOES_CHAR_EXIST", guard)
        cleanup.jump("GOTO_IF_FALSE", label)
        cleanup.emit("DELETE_CHAR", guard)
        cleanup.mark(label)
    ambush.emit("IF", integer(21))
    ambush.emit("DOES_CHAR_EXIST", first)
    ambush.emit("DOES_CHAR_EXIST", second)
    ambush.jump("GOTO_IF_FALSE", "done")
    ambush.emit("PRINT_BIG", text_key("CZBISTR"), integer(3500), integer(6))
    ambush.mark("done")
    mission.insert_before(0x2F02, "IF", ambush)
    mission.insert_before(0x3653, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
