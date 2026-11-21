from scene_vehicles import passing_vehicle, streaming_commands
from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Riot"
MISSION_ID = 108


def expand(mission):
    mission.insert_before(
        0x01E2, "LOAD_ALL_MODELS_NOW",
        streaming_commands(mission, "REQUEST_MODEL", 420, 262),
    )
    mission.insert_before(
        0x0661, "PRINT_NOW",
        passing_vehicle(mission, 420, 262, (1242.9, -820.6, 82.9), 91.0, 11.0, caption="CZCAB"),
    )
    mission.insert_before(
        0x0C27, "MARK_MODEL_AS_NO_LONGER_NEEDED",
        streaming_commands(mission, "MARK_MODEL_AS_NO_LONGER_NEEDED", 420, 262),
    )

    # A single abandoned car narrows the approach to Grove Street.
    mission.reserve_locals(960, 961)
    tried, obstacle = local(960), local(961)
    init = Snippet(mission.table)
    init.emit("SET_LVAR_INT", tried, integer(0))
    init.emit("SET_LVAR_INT", obstacle, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)

    x, y, z = 2478.4, -1663.6, 12.9
    road = Snippet(mission.table)
    road.emit("IF", integer(3))
    road.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", tried, integer(0))
    road.emit("IS_CHAR_IN_CAR", global_var(12), global_var(31980))
    road.emit(
        "LOCATE_CHAR_ANY_MEANS_3D", global_var(12), *map(floating, (x, y, z, 85.0, 85.0, 9.0)), integer(0),
    )
    road.emit("HAS_MODEL_LOADED", integer(567))
    road.jump("GOTO_IF_FALSE", "done")
    road.emit("SET_LVAR_INT", tried, integer(1))
    road.emit("IF", integer(0))
    road.emit(
        "IS_AREA_OCCUPIED", *map(floating, (x - 3, y - 2, z - 1, x + 3, y + 2, z + 2.5)),
        *map(integer, (0, 1, 1, 1, 0)), negate=True,
    )
    road.jump("GOTO_IF_FALSE", "done")
    road.emit("CREATE_CAR", integer(567), *map(floating, (x, y, z)), obstacle)
    road.emit("SET_CAR_HEADING", obstacle, floating(255.0))
    road.emit("SET_CAR_HEALTH", obstacle, integer(450))
    road.emit("LOCK_CAR_DOORS", obstacle, integer(2))
    road.emit("PRINT_BIG", text_key("CZRIOT"), integer(3600), integer(6))
    road.mark("done")
    mission.insert_before(0x09BD, "IF", road)

    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_VEHICLE_EXIST", obstacle)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("DELETE_CAR", obstacle)
    cleanup.emit("SET_LVAR_INT", obstacle, integer(-1))
    cleanup.mark("done")
    mission.insert_before(0x0A13, "SET_PLAYER_CONTROL", cleanup)
    mission.insert_before(0x0026, "GOSUB", cleanup)
