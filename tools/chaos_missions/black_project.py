from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Black Project"
MISSION_ID = 81


def expand(mission):
    # Widen selected moving beams, keeping the beams that flank them at
    # stock width. The same searchlights are created by both setup paths.
    hard = mission.difficulty == "acimasiz"
    for offset, before, after in (
        (0x0AA1, 12.5, 14.0), (0x213E, 12.5, 14.0),
        (0x0C1B, 10.5, 12.0), (0x22B8, 10.5, 12.0),
        (0x0E52, 10.5, 12.5), (0x24EF, 10.5, 12.5),
    ):
        mission.replace(offset, "CREATE_SEARCHLIGHT", 6, before, after + (0.5 if hard else 0.0))

    mission.reserve_locals(963)
    guard = local(963)
    init = Snippet(mission.table)
    init.emit("SET_LVAR_INT", guard, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)

    # Reinforce the surface only after the jetpack escape starts.
    alert = Snippet(mission.table)
    alert.emit("PRINT_BIG", text_key("CZALARM"), integer(3700), integer(6))
    for slot, sense, rate, accuracy in ((109, 39.0, 48, 37), (111, 46.0, 43, 39)):
        alert.emit("SET_SENSE_RANGE", local(slot), floating(sense))
        alert.emit("SET_CHAR_SHOOT_RATE", local(slot), integer(rate + (8 if hard else 0)))
        alert.emit("SET_CHAR_ACCURACY", local(slot), integer(accuracy + (6 if hard else 0)))
    mission.insert_before(0x6804, "SET_LVAR_INT", alert)

    # The native exit radius triggers this once CJ is near the surface.
    x, y, z = 278.2, 1897.9, 16.8
    ambush = Snippet(mission.table)
    ambush.emit("IF", integer(2))
    ambush.emit("HAS_MODEL_LOADED", integer(287))
    ambush.emit("HAS_MODEL_LOADED", integer(356))
    ambush.emit(
        "IS_AREA_OCCUPIED",
        *map(floating, (x - 1, y - 1, z - 0.6, x + 1, y + 1, z + 1.6)),
        *map(integer, (0, 1, 1, 1, 0)), negate=True,
    )
    ambush.jump("GOTO_IF_FALSE", "done")
    ambush.emit("CREATE_CHAR", integer(24), integer(287), *map(floating, (x, y, z)), guard)
    ambush.emit("SET_CHAR_AREA_VISIBLE", guard, integer(0))
    ambush.emit("SET_CHAR_DECISION_MAKER", guard, local(159))
    ambush.emit("SET_CHAR_HEADING", guard, floating(140.0))
    ambush.emit("GIVE_WEAPON_TO_CHAR", guard, integer(31), integer(90))
    ambush.emit("SET_CHAR_ACCURACY", guard, integer(47 if hard else 40))
    ambush.emit("SET_CHAR_SHOOT_RATE", guard, integer(55 if hard else 48))
    ambush.emit("SET_SENSE_RANGE", guard, floating(48.0))
    ambush.emit("TASK_STAY_IN_SAME_PLACE", guard, integer(1))
    ambush.emit("SET_CHAR_RELATIONSHIP", guard, integer(4), integer(0))
    ambush.emit("TASK_SHOOT_AT_CHAR", guard, global_var(12), integer(10000))
    ambush.mark("done")
    mission.insert_before(0x6AEF, "REPORT_MISSION_AUDIO_EVENT_AT_POSITION", ambush)

    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_CHAR_EXIST", guard)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("DELETE_CHAR", guard)
    cleanup.mark("done")
    mission.insert_before(0x75E9, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
