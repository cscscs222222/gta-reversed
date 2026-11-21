from scene_vehicles import clear_interceptor
from scm import Snippet, floating, integer, local, text_key


NAME = "Pier 69"
MISSION_ID = 64


def expand(mission):
    # Make Ryder's boat keep more pace when CJ closes in, preserving the fallback speeds.
    mission.replace(0x7101, "SET_BOAT_CRUISE_SPEED", 1, 40.0, 46.0)
    mission.replace(0x7131, "SET_BOAT_CRUISE_SPEED", 1, 30.0, 34.0)

    # Use the game's original boat-route task along a parallel line in
    # the bay. Ryder's boat and its original escape checks remain the target.
    mission.reserve_locals(961, 962, 963)
    sequence, escort, driver = map(local, (961, 962, 963))
    init = Snippet(mission.table)
    for handle in (escort, driver):
        init.emit("SET_LVAR_INT", handle, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)

    code = Snippet(mission.table)
    code.emit("REQUEST_MODEL", integer(102))
    code.emit("LOAD_ALL_MODELS_NOW")
    code.emit("IF", integer(2))
    code.emit("HAS_MODEL_LOADED", integer(452))
    code.emit("HAS_MODEL_LOADED", integer(102))
    code.emit("IS_AREA_OCCUPIED", *map(floating, (-1528.5, 1370.4, -1.0, -1519.5, 1376.4, 2.0)),
              *map(integer, (0, 1, 1, 1, 0)), negate=True)
    code.jump("GOTO_IF_FALSE", "done")
    code.emit("CREATE_CAR", integer(452), *map(floating, (-1524.0, 1373.4, 0.0)), escort)
    code.emit("SET_CAR_HEADING", escort, floating(273.0))
    code.emit("CREATE_CHAR_INSIDE_CAR", escort, integer(25), integer(102), driver)
    code.emit("OPEN_SEQUENCE_TASK", sequence)
    for point in ((-1498.0, 1384.0, 0.0), (-1348.0, 1326.0, 0.0), (-1250.0, 1186.0, 0.0)):
        code.emit("TASK_CAR_DRIVE_TO_COORD", integer(-1), escort, *map(floating, point),
                  floating(31.0), integer(0), integer(0), integer(2))
    code.emit("CLOSE_SEQUENCE_TASK", sequence)
    code.emit("PERFORM_SEQUENCE_TASK", driver, sequence)
    code.emit("CLEAR_SEQUENCE_TASK", sequence)
    code.emit("PRINT_BIG", text_key("CZBOAT"), integer(3600), integer(6))
    code.mark("done")
    mission.insert_before(0x70DB, "SET_LVAR_INT", code)
    cleanup = clear_interceptor(mission, escort, driver)
    mission.insert_before(0x0026, "GOSUB", cleanup)
