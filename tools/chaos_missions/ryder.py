from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Ryder"
MISSION_ID = 12


def expand(mission):
    mission.reserve_locals(961, 962, 963)
    scooter, rider, variant = map(local, (961, 962, 963))
    cj = global_var(12)
    initialize = Snippet(mission.table)
    for handle in (scooter, rider):
        initialize.emit("SET_LVAR_INT", handle, integer(-1))
    mission.insert_before(0x000B, "GOSUB", initialize)
    mission.replace(0x2F48, "SET_CHAR_ACCURACY", 1, 30, 42)
    pursuit = Snippet(mission.table)
    pursuit.emit("TASK_KILL_CHAR_ON_FOOT", local(35), cj)
    mission.insert_before(0x3009, "SET_LVAR_INT", pursuit)
    request = Snippet(mission.table)
    request.emit("REQUEST_MODEL", integer(448))
    mission.insert_before(0x1C7A, "REQUEST_MODEL", request)

    # Once both CJ and Ryder are in the truck, the second employee comes
    # from behind it. The scooter can fixate on the truck or drive off.
    location = tuple(map(floating, (2077.57, -1782.41, 12.35)))
    clearance = tuple(map(floating, (2075.57, -1784.41, 11.65, 2079.57, -1780.41, 14.35)))
    chase = Snippet(mission.table)
    chase.emit("IF", integer(2))
    chase.emit("HAS_MODEL_LOADED", integer(448))
    chase.emit("HAS_MODEL_LOADED", integer(155))
    chase.emit("IS_AREA_OCCUPIED", *clearance, *map(integer, (0, 1, 1, 1, 0)), negate=True)
    chase.jump("GOTO_IF_FALSE", "done")
    chase.emit("CREATE_CAR", integer(448), *location, scooter)
    chase.emit("SET_CAR_HEADING", scooter, floating(180.0))
    chase.emit("CREATE_CHAR_INSIDE_CAR", scooter, integer(25), integer(155), rider)
    chase.emit("GENERATE_RANDOM_INT_IN_RANGE", integer(0), integer(3), variant)
    chase.emit("IF", integer(0))
    chase.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", variant, integer(0))
    chase.jump("GOTO_IF_FALSE", "follow")
    chase.emit("TASK_CAR_DRIVE_WANDER", rider, scooter, floating(15.0), integer(0))
    chase.jump("GOTO", "done")
    chase.mark("follow")
    chase.emit(
        "TASK_CAR_MISSION", rider, scooter, global_var(548),
        integer(17), floating(28.0), integer(2),
    )
    chase.emit("PRINT_BIG", text_key("CZPIZZA"), integer(3400), integer(6))
    chase.mark("done")
    mission.insert_before(0x306F, "SET_LVAR_INT", chase)

    release = Snippet(mission.table)
    for index, (handle, exists, delete) in enumerate((
        (rider, "DOES_CHAR_EXIST", "DELETE_CHAR"),
        (scooter, "DOES_VEHICLE_EXIST", "DELETE_CAR"),
    )):
        release.emit("IF", integer(0))
        release.emit(exists, handle)
        release.jump("GOTO_IF_FALSE", f"skip_{index}")
        release.emit(delete, handle)
        release.mark(f"skip_{index}")
    release.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(448))
    mission.insert_before(0x3A1B, "MARK_MODEL_AS_NO_LONGER_NEEDED", release)
