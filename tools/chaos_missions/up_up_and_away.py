from scene_vehicles import clear_interceptor, streaming_commands
from scm import Snippet, floating, integer, local, text_key


NAME = "Up, Up and Away!"
MISSION_ID = 100


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x084D, "SET_CHAR_SHOOT_RATE", 1, 50, 59)
    mission.replace(0x0A11, "SET_CHAR_SHOOT_RATE", 1, 50, 62)
    mission.replace(0x0AC5, "SET_CHAR_SHOOT_RATE", 1, 40, 52)
    mission.replace(0x0F3B, "SET_CHAR_ACCURACY", 1, 70, 78)
    mission.replace(0x0F7B, "SET_CHAR_ACCURACY", 1, 70, 75)

    # The wait loop and its inner check must agree on the capture area.
    for offset, negate in ((0x23A7, True), (0x2456, False)):
        for axis in (4, 5):
            mission.replace(offset, "LOCATE_STOPPED_CAR_3D", axis, 6, 4.75 if hard else 5.25, negate=negate)
    for axis in (4, 5):
        mission.replace(0x2A21, "LOCATE_STOPPED_CAR_3D", axis, 20, 15.5 if hard else 17.0, negate=True)

    # A spotter tails the helicopter for the first seconds with the
    # payload. It peels away before the final delivery.
    mission.reserve_locals(955, 956, 957, 958, 959, 962, 963)
    xyz = tuple(map(local, (955, 956, 957)))
    started, elapsed, chopper, pilot = map(local, (958, 959, 962, 963))
    init = Snippet(mission.table)
    for handle in (chopper, pilot):
        init.emit("SET_LVAR_INT", handle, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)
    models = (425, 287)
    mission.insert_before(0x235E, "REQUEST_MODEL", streaming_commands(mission, "REQUEST_MODEL", *models))

    cargo = local(34)
    tail = Snippet(mission.table)
    tail.emit("IF", integer(2))
    tail.emit("IS_CAR_DEAD", cargo, negate=True)
    for model in models:
        tail.emit("HAS_MODEL_LOADED", integer(model))
    tail.jump("GOTO_IF_FALSE", "done")
    tail.emit("GET_OFFSET_FROM_CAR_IN_WORLD_COORDS", cargo, *map(floating, (-32.0, -54.0, 34.0)), *xyz)
    tail.emit("CREATE_CAR", integer(425), *xyz, chopper)
    tail.emit("CREATE_CHAR_INSIDE_CAR", chopper, integer(24), integer(287), pilot)
    tail.emit("SET_HELI_BLADES_FULL_SPEED", chopper)
    tail.emit("HELI_FOLLOW_ENTITY", chopper, integer(-1), cargo, floating(35.0 if hard else 42.0))
    tail.emit("GET_GAME_TIMER", started)
    tail.emit("PRINT_BIG", text_key("CZAIRTL"), integer(3900), integer(6))
    tail.mark("done")
    mission.insert_before(0x29E3, "PRINT_NOW", tail)

    peel = Snippet(mission.table)
    peel.emit("IF", integer(0))
    peel.emit("DOES_VEHICLE_EXIST", chopper)
    peel.jump("GOTO_IF_FALSE", "done")
    peel.emit("GET_GAME_TIMER", elapsed)
    peel.emit("SUB_INT_LVAR_FROM_INT_LVAR", elapsed, started)
    peel.emit("IF", integer(0))
    peel.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", elapsed, integer(16000))
    peel.jump("GOTO_IF_FALSE", "done")
    peel.emit("HELI_GOTO_COORDS", chopper, *map(floating, (2950.0, 1100.0, 85.0, 82.0, 100.0)))
    peel.emit("MARK_CHAR_AS_NO_LONGER_NEEDED", pilot)
    peel.emit("MARK_CAR_AS_NO_LONGER_NEEDED", chopper)
    for handle in (chopper, pilot):
        peel.emit("SET_LVAR_INT", handle, integer(-1))
    peel.mark("done")
    mission.insert_before(0x2A58, "WAIT", peel)
    cleanup = clear_interceptor(mission, chopper, pilot)
    for model in models:
        cleanup.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(model))
    mission.insert_before(0x0026, "GOSUB", cleanup)
