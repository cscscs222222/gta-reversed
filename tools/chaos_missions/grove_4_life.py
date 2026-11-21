from scene_vehicles import clear_interceptor, streaming_commands
from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Grove 4 Life"
MISSION_ID = 107


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    for offset, normal in ((0x0321, 109), (0x033F, 115)):
        mission.replace(offset, "SET_ZONE_GANG_STRENGTH", 2, 100, normal + (7 if hard else 0))

    # A surviving Ballas driver retreats out of the second battle. The
    # second occupant exits and covers their retreat.
    mission.reserve_locals(959, 960, 961, 962, 963)
    armed, seen, backup, driver, passenger = map(local, (959, 960, 961, 962, 963))
    cj = global_var(12)
    init = Snippet(mission.table)
    for flag in (seen, armed):
        init.emit("SET_LVAR_INT", flag, integer(0))
    for handle in (backup, driver, passenger):
        init.emit("SET_LVAR_INT", handle, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)
    models = (102, 104)
    mission.insert_before(0x00FA, "REQUEST_MODEL", streaming_commands(mission, "REQUEST_MODEL", *models))

    x, y, z = 2007.6, -1610.3, 12.4
    encounter = Snippet(mission.table)
    encounter.emit("IF", integer(6))
    encounter.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", seen, integer(0))
    encounter.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", local(52), integer(1))
    encounter.emit("IS_GANG_WAR_GOING_ON")
    encounter.emit("HAS_MODEL_LOADED", integer(492))
    for model in models:
        encounter.emit("HAS_MODEL_LOADED", integer(model))
    encounter.emit(
        "LOCATE_CHAR_ANY_MEANS_3D", cj, *map(floating, (x, y, z, 87.0, 87.0, 12.0)), integer(0),
    )
    encounter.jump("GOTO_IF_FALSE", "passenger")
    encounter.emit("IF", integer(0))
    encounter.emit(
        "IS_AREA_OCCUPIED", *map(floating, (x - 2.5, y - 2.5, z - 0.8, x + 2.5, y + 2.5, z + 2)),
        *map(integer, (0, 1, 1, 1, 0)), negate=True,
    )
    encounter.jump("GOTO_IF_FALSE", "passenger")
    encounter.emit("SET_LVAR_INT", seen, integer(1))
    encounter.emit("CREATE_CAR", integer(492), *map(floating, (x, y, z)), backup)
    encounter.emit("SET_CAR_HEADING", backup, floating(180.0))
    encounter.emit("CREATE_CHAR_INSIDE_CAR", backup, integer(17), integer(102), driver)
    encounter.emit("CREATE_CHAR_AS_PASSENGER", backup, integer(17), integer(104), integer(0), passenger)
    encounter.emit("TASK_LEAVE_CAR", passenger, backup)

    encounter.mark("passenger")
    encounter.emit("IF", integer(2))
    encounter.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", armed, integer(0))
    encounter.emit("IS_CHAR_DEAD", passenger, negate=True)
    encounter.emit("HAS_MODEL_LOADED", integer(355))
    encounter.jump("GOTO_IF_FALSE", "done")
    encounter.emit("IF", integer(0))
    encounter.emit("IS_CHAR_IN_ANY_CAR", passenger, negate=True)
    encounter.jump("GOTO_IF_FALSE", "done")
    encounter.emit("SET_LVAR_INT", armed, integer(1))
    encounter.emit("GIVE_WEAPON_TO_CHAR", passenger, integer(30), integer(75))
    encounter.emit("SET_CHAR_ACCURACY", passenger, integer(45 if hard else 37))
    encounter.emit("TASK_KILL_CHAR_ON_FOOT", passenger, cj)
    encounter.emit("IF", integer(1))
    encounter.emit("IS_CHAR_DEAD", driver, negate=True)
    encounter.emit("IS_CAR_DEAD", backup, negate=True)
    encounter.jump("GOTO_IF_FALSE", "done")
    encounter.emit("TASK_CAR_DRIVE_WANDER", driver, backup, floating(13.0), integer(0))
    encounter.emit("PRINT_BIG", text_key("CZBACK"), integer(3800), integer(6))
    encounter.mark("done")
    mission.insert_before(0x0930, "IF", encounter)

    cleanup = clear_interceptor(mission, backup, driver)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_CHAR_EXIST", passenger)
    cleanup.jump("GOTO_IF_FALSE", "models")
    cleanup.emit("DELETE_CHAR", passenger)
    cleanup.mark("models")
    for model in models:
        cleanup.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(model))
    mission.insert_before(0x0D3E, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
