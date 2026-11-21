from scm import Snippet, floating, integer, local, text_key


NAME = "Dam and Blast"
MISSION_ID = 98


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    for offset, sense in ((0x149E, 15.0), (0x1D5F, 15.0), (0x14DB, 13.0), (0x1D9C, 13.0),
                          (0x24FD, 14.0), (0x266E, 12.0)):
        mission.replace(offset, "SET_SENSE_RANGE", 1, 10, sense + (2.0 if hard else 0.0))

    # The extra relief guard walks back toward the generators when all
    # five charges are set. Passing during the patrol stays possible.
    mission.reserve_locals(963)
    guard = local(963)
    start = Snippet(mission.table)
    start.emit("SET_LVAR_INT", guard, integer(-1))
    mission.insert_before(0x000B, "GOSUB", start)

    x, y, z = -957.1, 1947.3, 8.01
    relief = Snippet(mission.table)
    relief.emit("IF", integer(2))
    relief.emit("HAS_MODEL_LOADED", integer(71))
    relief.emit("HAS_MODEL_LOADED", integer(346))
    relief.emit(
        "IS_AREA_OCCUPIED", *map(floating, (x - 0.9, y - 0.9, z - 0.6, x + 0.9, y + 0.9, z + 1.6)),
        *map(integer, (0, 1, 1, 1, 0)), negate=True,
    )
    relief.jump("GOTO_IF_FALSE", "done")
    relief.emit("CREATE_CHAR", integer(25), integer(71), *map(floating, (x, y, z)), guard)
    relief.emit("SET_CHAR_AREA_VISIBLE", guard, integer(17))
    relief.emit("SET_CHAR_HEADING", guard, floating(185.0))
    relief.emit("SET_SENSE_RANGE", guard, floating(15.0 if hard else 12.0))
    relief.emit("GIVE_WEAPON_TO_CHAR", guard, integer(22), integer(45))
    relief.emit("SET_CHAR_ACCURACY", guard, integer(43 if hard else 36))
    relief.emit("SET_CHAR_RELATIONSHIP", guard, integer(4), integer(0))
    relief.emit("TASK_FOLLOW_PATH_NODES_TO_COORD", guard, *map(floating, (-960.0, 1927.0, 8.04)), integer(4), integer(-2))
    relief.emit("PRINT_BIG", text_key("CZRELIF"), integer(3700), integer(6))
    relief.mark("done")
    mission.insert_before(0x3846, "ADD_BLIP_FOR_COORD", relief)

    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_CHAR_EXIST", guard)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("DELETE_CHAR", guard)
    cleanup.emit("SET_LVAR_INT", guard, integer(-1))
    cleanup.mark("done")
    mission.insert_before(0x3904, "DELETE_OBJECT", cleanup)
    mission.insert_before(0x4AC2, "MARK_CHAR_AS_NO_LONGER_NEEDED", cleanup)
