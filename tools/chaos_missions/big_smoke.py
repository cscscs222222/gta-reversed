from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Big Smoke"
MISSION_ID = 11


def expand(mission):
    mission.reserve_locals(*range(950, 960))
    first_car, second_car, first_driver, second_driver, variant, started, elapsed, gunner = map(
        local, range(952, 960)
    )
    marker, second_gunner = local(950), local(951)
    player = global_var(8)
    cj = global_var(12)
    smoke = global_var(520)
    smoke_car = local(41)
    exit_point = tuple(map(floating, (2341.85, -1658.92, 12.39)))
    limit = 20000 if mission.difficulty == "acimasiz" else 24000

    initialize = Snippet(mission.table)
    for handle in (first_car, second_car, first_driver, second_driver, gunner, second_gunner, marker):
        initialize.emit("SET_LVAR_INT", handle, integer(-1))
    mission.insert_before(0x0000B, "GOSUB", initialize)

    request = Snippet(mission.table)
    for model in (412, 102, 352):
        request.emit("REQUEST_MODEL", integer(model))
    mission.insert_before(0x052E, "LOAD_ALL_MODELS_NOW", request)

    chase = Snippet(mission.table)
    chase.emit("IF", integer(4))
    chase.emit("IS_CAR_DEAD", smoke_car, negate=True)
    chase.emit("IS_CHAR_DEAD", cj, negate=True)
    chase.emit("IS_CHAR_DEAD", smoke, negate=True)
    chase.emit("HAS_MODEL_LOADED", integer(412))
    chase.emit("HAS_MODEL_LOADED", integer(102))
    chase.jump("GOTO_IF_FALSE", "done")
    chase.emit("SET_FADING_COLOUR", integer(0), integer(0), integer(0))
    chase.emit("DO_FADE", integer(220), integer(0))
    chase.emit("WAIT", integer(260))
    chase.emit("GENERATE_RANDOM_INT_IN_RANGE", integer(0), integer(3), variant)

    # The scene opens onto the two existing Grove approach roads. A
    # driver can miss the rendezvous while the other stays on CJ's trail.
    for name, vehicle, driver, passenger, position, heading, car_mission in (
        ("north", first_car, first_driver, gunner, (2473.5, -1655.2, 12.1), 90.0, 15),
        ("south", second_car, second_driver, second_gunner, (2476.68, -1679.57, 12.3451), 90.0, 17),
    ):
        x, y, z = position
        point = tuple(map(floating, position))
        clearance = tuple(map(floating, (x - 2.5, y - 2.5, z - 1, x + 2.5, y + 2.5, z + 2)))
        chase.emit("IF", integer(0))
        chase.emit("IS_AREA_OCCUPIED", *clearance, *map(integer, (0, 1, 1, 1, 0)), negate=True)
        chase.jump("GOTO_IF_FALSE", f"skip_{name}")
        chase.emit("CREATE_CAR", integer(412), *point, vehicle)
        chase.emit("SET_CAR_HEADING", vehicle, floating(heading))
        chase.emit("CHANGE_CAR_COLOUR", vehicle, integer(22), integer(22))
        chase.emit("CREATE_CHAR_INSIDE_CAR", vehicle, integer(25), integer(102), driver)
        if name == "south":
            chase.emit("IF", integer(0))
            chase.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", variant, integer(0))
            chase.jump("GOTO_IF_FALSE", "pursue_south")
            chase.emit("TASK_CAR_DRIVE_WANDER", driver, vehicle, floating(16.0), integer(0))
            chase.jump("GOTO", "skip_south")
            chase.mark("pursue_south")
        chase.emit("IF", integer(0))
        chase.emit("HAS_MODEL_LOADED", integer(352))
        chase.jump("GOTO_IF_FALSE", f"drive_{name}")
        chase.emit("CREATE_CHAR_AS_PASSENGER", vehicle, integer(25), integer(102), integer(0), passenger)
        chase.emit("GIVE_WEAPON_TO_CHAR", passenger, integer(28), integer(120))
        chase.emit("SET_CURRENT_CHAR_WEAPON", passenger, integer(28))
        chase.emit("SET_CHAR_ACCURACY", passenger, integer(58))
        chase.emit("SET_CHAR_SHOOT_RATE", passenger, integer(65))
        chase.emit(
            "TASK_DRIVE_BY", passenger, integer(-1), smoke_car,
            floating(0), floating(0), floating(0), floating(55),
            integer(8), integer(1), integer(65),
        )
        chase.mark(f"drive_{name}")
        chase.emit(
            "TASK_CAR_MISSION", driver, vehicle, smoke_car,
            integer(car_mission), floating(34.0), integer(2),
        )
        chase.mark(f"skip_{name}")

    chase.emit("IF", integer(21))
    chase.emit("DOES_CHAR_EXIST", first_driver)
    chase.emit("DOES_CHAR_EXIST", second_driver)
    chase.jump("GOTO_IF_FALSE", "done")
    # Skipping the stock cutscene can leave the two entry tasks unfinished.
    # Stop the driver's cutscene autopilot before returning manual control.
    chase.emit("CLEAR_CHAR_TASKS", cj)
    for name, actor, command, seat in (
        ("cj", cj, "WARP_CHAR_INTO_CAR", ()),
        ("smoke", smoke, "WARP_CHAR_INTO_CAR_AS_PASSENGER", (integer(0),)),
    ):
        chase.emit("IF", integer(0))
        chase.emit("IS_CHAR_SITTING_IN_CAR", actor, smoke_car, negate=True)
        chase.jump("GOTO_IF_FALSE", f"ready_{name}")
        chase.emit(command, actor, smoke_car, *seat)
        chase.mark(f"ready_{name}")
    chase.emit("SWITCH_WIDESCREEN", integer(0))
    chase.emit("CAMERA_RESET_NEW_SCRIPTABLES")
    chase.emit("RESTORE_CAMERA_JUMPCUT")
    chase.emit("SET_CAMERA_BEHIND_PLAYER")
    chase.emit("SET_PLAYER_CONTROL", player, integer(1))
    chase.emit("SET_CAR_CAN_BE_DAMAGED", smoke_car, integer(1))
    chase.emit("SET_CAR_HEALTH", smoke_car, integer(780))
    chase.emit("ADD_BLIP_FOR_COORD", *exit_point, marker)
    chase.emit("SET_BLIP_AS_FRIENDLY", marker, integer(1))
    chase.emit("DO_FADE", integer(250), integer(1))
    chase.emit("PRINT_WITH_NUMBER_BIG", text_key("CZSMKD"), integer(limit // 1000), integer(4200), integer(6))
    chase.emit("PRINT_NOW", text_key("CZSMK1"), integer(3000), integer(1))
    chase.emit("SET_LVAR_INT", variant, integer(0))
    chase.emit("GET_GAME_TIMER", started)
    chase.mark("drive")
    chase.emit("WAIT", integer(0))
    chase.emit("GET_GAME_TIMER", elapsed)
    chase.emit("SUB_INT_LVAR_FROM_INT_LVAR", elapsed, started)
    chase.emit("IF", integer(1))
    chase.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", elapsed, integer(5400))
    chase.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", variant, integer(0))
    chase.jump("GOTO_IF_FALSE", "limit")
    chase.emit("SET_LVAR_INT", variant, integer(1))
    chase.emit("PRINT_NOW", text_key("CZSMK2"), integer(3500), integer(1))
    chase.mark("limit")
    chase.emit("IF", integer(3))
    chase.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", elapsed, integer(limit), negate=True)
    chase.emit("IS_CAR_DEAD", smoke_car, negate=True)
    chase.emit("IS_CHAR_DEAD", smoke, negate=True)
    chase.emit("IS_CHAR_DEAD", cj, negate=True)
    chase.jump("GOTO_IF_FALSE", "failed")
    chase.emit("IF", integer(2))
    chase.emit("LOCATE_CAR_3D", smoke_car, *exit_point, floating(11), floating(9), floating(4), integer(1))
    chase.emit("IS_CHAR_SITTING_IN_CAR", cj, smoke_car)
    chase.emit("IS_CHAR_SITTING_IN_CAR", smoke, smoke_car)
    chase.jump("GOTO_IF_FALSE", "drive")
    chase.emit("SET_PLAYER_CONTROL", player, integer(0))
    chase.emit("SWITCH_WIDESCREEN", integer(1))
    chase.jump("GOTO", "done")
    chase.mark("failed")
    chase.emit("PRINT_BIG", text_key("CZSMKF"), integer(4500), integer(1))
    chase.emit("RETURN")
    chase.mark("done")
    mission.insert_before(0x07F3, "CLEAR_PRINTS", chase)

    # Remove just the new actors after the stock screen fade.
    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", marker, integer(-1))
    cleanup.jump("GOTO_IF_FALSE", "no_marker")
    cleanup.emit("REMOVE_BLIP", marker)
    cleanup.emit("SET_LVAR_INT", marker, integer(-1))
    cleanup.mark("no_marker")
    for index, (handle, exists, delete) in enumerate((
        (gunner, "DOES_CHAR_EXIST", "DELETE_CHAR"),
        (second_gunner, "DOES_CHAR_EXIST", "DELETE_CHAR"),
        (first_driver, "DOES_CHAR_EXIST", "DELETE_CHAR"),
        (second_driver, "DOES_CHAR_EXIST", "DELETE_CHAR"),
        (first_car, "DOES_VEHICLE_EXIST", "DELETE_CAR"),
        (second_car, "DOES_VEHICLE_EXIST", "DELETE_CAR"),
    )):
        cleanup.emit("IF", integer(0))
        cleanup.emit(exists, handle)
        cleanup.jump("GOTO_IF_FALSE", f"skip_{index}")
        cleanup.emit(delete, handle)
        cleanup.emit("SET_LVAR_INT", handle, integer(-1))
        cleanup.mark(f"skip_{index}")
    mission.insert_before(0x082C, "CAMERA_RESET_NEW_SCRIPTABLES", cleanup)
    mission.insert_before(0x477C, "SET_VAR_INT", cleanup)
