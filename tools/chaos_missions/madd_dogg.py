from scene_vehicles import streaming_commands
from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Madd Dogg"
MISSION_ID = 95


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    for axis in (4, 5):
        mission.replace(0x15FF, "LOCATE_CHAR_ANY_MEANS_3D", axis, 4, 3.15 if hard else 3.45)

    # A parked service van narrows one approach to the hospital. Its
    # location is clear of the hospital's arrival marker.
    mission.reserve_locals(962, 963)
    seen, van = local(962), local(963)
    init = Snippet(mission.table)
    init.emit("SET_LVAR_INT", seen, integer(0))
    init.emit("SET_LVAR_INT", van, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)
    mission.insert_before(0x234B, "PRINT_NOW", streaming_commands(mission, "REQUEST_MODEL", 482))

    x, y, z = 1893.958, 2240.108, 9.82
    road = Snippet(mission.table)
    road.emit("IF", integer(2))
    road.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", seen, integer(0))
    road.emit(
        "LOCATE_CHAR_ANY_MEANS_3D", global_var(12), *map(floating, (x, y, z, 100.0, 100.0, 12.0)), integer(0),
    )
    road.emit("HAS_MODEL_LOADED", integer(482))
    road.jump("GOTO_IF_FALSE", "done")
    road.emit("SET_LVAR_INT", seen, integer(1))
    road.emit("IF", integer(0))
    road.emit(
        "IS_AREA_OCCUPIED", *map(floating, (x - 3, y - 1.8, z - 0.7, x + 3, y + 1.8, z + 2.5)),
        *map(integer, (0, 1, 1, 1, 0)), negate=True,
    )
    road.jump("GOTO_IF_FALSE", "done")
    road.emit("CREATE_CAR", integer(482), *map(floating, (x, y, z)), van)
    road.emit("SET_CAR_HEADING", van, floating(270.0))
    road.emit("LOCK_CAR_DOORS", van, integer(2))
    road.emit("FREEZE_CAR_POSITION", van, integer(1))
    road.emit("PRINT_BIG", text_key("CZHOSP"), integer(3700), integer(6))
    road.mark("done")
    mission.insert_before(0x2592, "SET_LVAR_INT_TO_LVAR_INT", road)

    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_VEHICLE_EXIST", van)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("DELETE_CAR", van)
    cleanup.emit("SET_LVAR_INT", van, integer(-1))
    cleanup.mark("done")
    mission.insert_before(0x2B09, "SET_PLAYER_CONTROL", cleanup)
    finish = Snippet(mission.table)
    finish.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(482))
    mission.insert_before(0x3662, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
    mission.insert_before(0x3667, "MARK_MODEL_AS_NO_LONGER_NEEDED", finish)
