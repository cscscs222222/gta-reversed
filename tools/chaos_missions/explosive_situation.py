from scene_vehicles import clear_interceptor
from scm import Snippet, floating, integer, local, text_key


NAME = "Explosive Situation"
MISSION_ID = 85


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x16AA, "SET_VAR_INT", 1, 150000, 120000 if hard else 130000)

    # Once CJ reaches the escape bike, send another forklift across the
    # quarry floor. It crosses once before the existing ramp checkpoints.
    mission.reserve_locals(962, 963)
    forklift, driver = local(962), local(963)
    init = Snippet(mission.table)
    for handle in (forklift, driver):
        init.emit("SET_LVAR_INT", handle, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)

    x, y, z = 648.25, 839.3, -43.94
    escape = Snippet(mission.table)
    escape.emit("IF", integer(2))
    for model in (530, 260):
        escape.emit("HAS_MODEL_LOADED", integer(model))
    escape.emit(
        "IS_AREA_OCCUPIED", *map(floating, (x - 2, y - 2, z - 1, x + 2, y + 2, z + 2)),
        *map(integer, (0, 1, 1, 1, 0)), negate=True,
    )
    escape.jump("GOTO_IF_FALSE", "done")
    escape.emit("CREATE_CAR", integer(530), *map(floating, (x, y, z)), forklift)
    escape.emit("SET_CAR_HEADING", forklift, floating(220.0))
    escape.emit("CREATE_CHAR_INSIDE_CAR", forklift, integer(4), integer(260), driver)
    escape.emit(
        "TASK_CAR_DRIVE_TO_COORD", driver, forklift, *map(floating, (655.578, 831.109, -43.9456, 9.0)),
        *map(integer, (0, 0, 3)),
    )
    escape.emit("PRINT_BIG", text_key("CZQUARY"), integer(3700), integer(6))
    escape.mark("done")
    mission.insert_before(0x3092, "PRINT_NOW", escape)
    mission.insert_before(0x420B, "MARK_MODEL_AS_NO_LONGER_NEEDED", clear_interceptor(mission, forklift, driver))
