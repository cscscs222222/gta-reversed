from scene_vehicles import streaming_commands
from scm import Snippet, floating, global_var, integer, local, text_key
from street_obstacles import park_obstacle


NAME = "Sweet & Kendl"
MISSION_ID = 11
SCRIPT_NAME = b"INTRO1"

# The first two commands target the Ballas driver and passenger. The bike
# chase then gives the passenger separate drive-by orders targeting CJ.
_CHASE_DRIVE_BY = "13 07 03 34 00 02 0C 00 04 FF 06 00000000 06 00000000 06 00000000 06 00409C45 04 04 04 01 04"
EDITS = (
    (0x06FCF4, "E2 02 03 32 00 04", 25, 52),
    (0x06FCFB, "E2 02 03 34 00 04", 25, 57),
    (0x06FD02, "DD 07 03 32 00 04", 30, 60),
    (0x06FD09, "DD 07 03 34 00 04", 30, 66),
    (0x071069, _CHASE_DRIVE_BY, 40, 66),
    (0x0712F4, _CHASE_DRIVE_BY, 40, 66),
    (0x0713D2, _CHASE_DRIVE_BY, 40, 66),
    (0x0720C1, _CHASE_DRIVE_BY, 40, 70),
    (0x07235C, _CHASE_DRIVE_BY, 40, 70),
    (0x07244A, _CHASE_DRIVE_BY, 40, 70),
)


def expand(mission):
    # The driver's on-road fallback aims for CJ's flank instead of the
    # trailing point used by the unmodified bicycle chase.
    for offset, flank in ((0x1E88, -5.5), (0x20B2, 5.5), (0x2EE0, 5.5), (0x311A, -5.5)):
        mission.replace(offset, "GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS", 1, 0.0, flank)
        mission.replace(offset, "GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS", 2, -4.0, -1.5)

    # Allow one road-intercept car per leg. The stock driver keeps the
    # flanking order, so the two cars do not chase the same line.
    mission.reserve_locals(960, 961, 962, 963)
    active, gunner, car, driver = map(local, (960, 961, 962, 963))
    cj = global_var(12)
    for offsets, position, heading in (
        ((0x1E6D, 0x2036), (1038.5, -954.9, 41.5), 100.8),
        ((0x2EC5, 0x309E), (1798.0, -1519.0, 7.0), 270.0),
    ):
        x, y, z = position
        location = tuple(map(floating, position))
        clearance = tuple(map(floating, (x - 2.4, y - 2.4, z - 1, x + 2.4, y + 2.4, z + 2)))
        code = Snippet(mission.table)
        code.emit("IF", integer(4))
        code.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", active, integer(0))
        code.emit("HAS_MODEL_LOADED", integer(412))
        code.emit("HAS_MODEL_LOADED", integer(102))
        code.emit("IS_CAR_DEAD", local(69), negate=True)
        code.emit("IS_AREA_OCCUPIED", *clearance, *map(integer, (0, 1, 1, 1, 0)), negate=True)
        code.jump("GOTO_IF_FALSE", "done")
        code.emit("SET_LVAR_INT", active, integer(1))
        code.emit("SET_LVAR_INT", gunner, integer(-1))
        code.emit("CREATE_CAR", integer(412), *location, car)
        code.emit("SET_CAR_HEADING", car, floating(heading))
        code.emit("CHANGE_CAR_COLOUR", car, integer(22), integer(22))
        code.emit("CREATE_CHAR_INSIDE_CAR", car, integer(25), integer(102), driver)
        code.emit("TASK_CAR_MISSION", driver, car, local(69), integer(17), floating(37.0), integer(2))
        code.emit("PRINT_BIG", text_key("CZBIKE"), integer(3200), integer(6))
        code.emit("IF", integer(0))
        code.emit("HAS_MODEL_LOADED", integer(352))
        code.jump("GOTO_IF_FALSE", "done")
        code.emit("CREATE_CHAR_AS_PASSENGER", car, integer(25), integer(102), integer(0), gunner)
        code.emit("GIVE_WEAPON_TO_CHAR", gunner, integer(28), integer(150))
        code.emit("SET_CURRENT_CHAR_WEAPON", gunner, integer(28))
        code.emit("SET_CHAR_ACCURACY", gunner, integer(54))
        code.emit("SET_CHAR_SHOOT_RATE", gunner, integer(58))
        code.emit(
            "TASK_DRIVE_BY", gunner, cj, integer(-1),
            floating(0), floating(0), floating(0), floating(38), integer(4), integer(1), integer(55),
        )
        code.mark("done")
        for offset in offsets:
            mission.replace(offset, "TASK_CAR_MISSION", 4, 30.0, 38.0)
            mission.insert_before(offset, "TASK_CAR_MISSION", code)

    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", active, integer(1))
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("SET_LVAR_INT", active, integer(0))
    for index, (handle, exists, delete) in enumerate((
        (gunner, "DOES_CHAR_EXIST", "DELETE_CHAR"),
        (driver, "DOES_CHAR_EXIST", "DELETE_CHAR"),
        (car, "DOES_VEHICLE_EXIST", "DELETE_CAR"),
    )):
        cleanup.emit("IF", integer(0))
        cleanup.emit(exists, handle)
        cleanup.jump("GOTO_IF_FALSE", f"skip_{index}")
        cleanup.emit(delete, handle)
        cleanup.mark(f"skip_{index}")
    cleanup.mark("done")
    mission.insert_before(0x2C9C, "SET_LVAR_INT", cleanup)
    mission.insert_before(0x4788, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)

    # Each attempt can put the truck on one of the already used approach
    # streets. The other street stays open. Existing destination tests
    # continue to let CJ approach from either side.
    mission.reserve_locals(947, 948, 949)
    warned, route, truck = map(local, (947, 948, 949))
    mission.insert_before(
        0x0990, "LOAD_ALL_MODELS_NOW",
        streaming_commands(mission, "REQUEST_MODEL", 408),
    )
    block = Snippet(mission.table)
    block.emit("SET_LVAR_INT", truck, integer(-1))
    block.emit("GENERATE_RANDOM_INT_IN_RANGE", integer(0), integer(2), route)
    block.emit("IF", integer(0))
    block.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", route, integer(0))
    block.jump("GOTO_IF_FALSE", "west_side")
    park_obstacle(block, "east_done", truck, 408, (1647.97, -1098.43, 22.94), 128.0, fixed=True)
    block.jump("GOTO", "done")
    block.mark("west_side")
    park_obstacle(block, "west_done", truck, 408, (2341.85, -1658.92, 12.39), 58.0, fixed=True)
    block.mark("done")
    mission.insert_before(0x1D49, "SET_LVAR_INT", block)
    warn = Snippet(mission.table)
    warn.emit("IF", integer(1))
    warn.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", warned, integer(0))
    warn.emit("DOES_VEHICLE_EXIST", truck)
    warn.jump("GOTO_IF_FALSE", "done")
    warn.emit("IF", integer(0))
    warn.emit(
        "LOCATE_CHAR_ANY_MEANS_CAR_2D", global_var(12), truck,
        floating(30.0), floating(30.0), integer(0),
    )
    warn.jump("GOTO_IF_FALSE", "done")
    warn.emit("SET_LVAR_INT", warned, integer(1))
    warn.emit("PRINT", text_key("CZTRUCK"), integer(3600), integer(1))
    warn.mark("done")
    mission.insert_before(0x1CC1, "GOSUB", warn)
    mission.insert_before(0x2CE2, "IF", warn)
    mission.insert_before(
        0x478D, "MARK_MODEL_AS_NO_LONGER_NEEDED",
        streaming_commands(mission, "MARK_MODEL_AS_NO_LONGER_NEEDED", 408),
    )
