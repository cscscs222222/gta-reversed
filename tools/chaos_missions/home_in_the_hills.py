from scene_vehicles import clear_interceptor
from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "A Home in the Hills"
MISSION_ID = 102


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    for axis in (3, 4):
        mission.replace(0x1C2B, "LOCATE_CHAR_ON_FOOT_2D", axis, 5, 4.35 if hard else 4.65)
    mission.replace(0x5306, "SET_CHAR_ACCURACY", 1, 35, 45)
    mission.replace(0x530D, "SET_CHAR_SHOOT_RATE", 1, 50, 59)
    mission.replace(0x53E2, "SET_CHAR_ACCURACY", 1, 35, 49)
    mission.replace(0x5BE4, "SET_CHAR_SHOOT_RATE", 1, 35, 49)
    mission.replace(0x9605, "SET_CAR_CRUISE_SPEED", 1, 25, 33 if hard else 30)
    mission.replace(0x960F, "TASK_CAR_DRIVE_WANDER", 2, 25, 33 if hard else 30)

    # Poppa's backup car initially follows him. When CJ enters a
    # vehicle, the escort switches to trying to cut CJ off.
    mission.reserve_locals(960, 961, 962, 963)
    target, redirected, car, driver = map(local, (960, 961, 962, 963))
    poppa_car, cj = local(527), global_var(12)
    init = Snippet(mission.table)
    init.emit("SET_LVAR_INT", redirected, integer(0))
    for handle in (car, driver):
        init.emit("SET_LVAR_INT", handle, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)

    x, y, z = 1255.7, -812.2, 83.15
    escort = Snippet(mission.table)
    escort.emit("IF", integer(2))
    escort.emit("HAS_MODEL_LOADED", integer(555))
    escort.emit("HAS_MODEL_LOADED", integer(110))
    escort.emit(
        "IS_AREA_OCCUPIED", *map(floating, (x - 2, y - 3, z - 1, x + 2, y + 3, z + 2)),
        *map(integer, (0, 1, 1, 1, 0)), negate=True,
    )
    escort.jump("GOTO_IF_FALSE", "done")
    escort.emit("CREATE_CAR", integer(555), *map(floating, (x, y, z)), car)
    escort.emit("SET_CAR_HEADING", car, floating(162.476))
    escort.emit("CREATE_CHAR_INSIDE_CAR", car, integer(25), integer(110), driver)
    escort.emit("TASK_CAR_MISSION", driver, car, poppa_car, integer(52), floating(31.0), integer(2))
    escort.emit("PRINT_BIG", text_key("CZPOPPA"), integer(3900), integer(6))
    escort.mark("done")
    mission.insert_before(0x928D, "PRINT_NOW", escort)

    pursue = Snippet(mission.table)
    pursue.emit("IF", integer(3))
    pursue.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", redirected, integer(0))
    pursue.emit("IS_CHAR_DEAD", driver, negate=True)
    pursue.emit("IS_CAR_DEAD", car, negate=True)
    pursue.emit("IS_CHAR_IN_ANY_CAR", cj)
    pursue.jump("GOTO_IF_FALSE", "done")
    pursue.emit("GET_CAR_CHAR_IS_USING", cj, target)
    pursue.emit("TASK_CAR_MISSION", driver, car, target, integer(17), floating(35.0 if hard else 32.0), integer(2))
    pursue.emit("SET_LVAR_INT", redirected, integer(1))
    pursue.mark("done")
    mission.insert_before(0x92B9, "WAIT", pursue)
    mission.insert_before(0x9992, "MARK_MODEL_AS_NO_LONGER_NEEDED", clear_interceptor(mission, car, driver))
