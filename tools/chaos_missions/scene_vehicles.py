from scm import Snippet, floating, integer, local, text_key


def passing_vehicle(mission, vehicle_model, driver_model, position, heading, speed, code=None, *, caption=None):
    """Stage a short-lived ambient vehicle at an existing mission location."""
    mission.reserve_locals(962, 963)
    vehicle, driver = local(962), local(963)
    x, y, z = position
    point = tuple(map(floating, position))
    clear_area = tuple(map(floating, (x - 2.5, y - 2.5, z - 1, x + 2.5, y + 2.5, z + 2)))
    code = code if code is not None else Snippet(mission.table)
    code.emit("IF", integer(3))
    code.emit("HAS_MODEL_LOADED", integer(vehicle_model))
    code.emit("HAS_MODEL_LOADED", integer(driver_model))
    code.emit("IS_POINT_ON_SCREEN", *point, floating(4.0), negate=True)
    code.emit("IS_AREA_OCCUPIED", *clear_area, *map(integer, (0, 1, 1, 1, 0)), negate=True)
    code.jump("GOTO_IF_FALSE", "vehicle_done")
    code.emit("CREATE_CAR", integer(vehicle_model), *point, vehicle)
    code.emit("SET_CAR_HEADING", vehicle, floating(heading))
    code.emit("CREATE_CHAR_INSIDE_CAR", vehicle, integer(4), integer(driver_model), driver)
    code.emit("TASK_CAR_DRIVE_WANDER", driver, vehicle, floating(speed), integer(0))
    if caption:
        code.emit("PRINT_BIG", text_key(caption), integer(3400), integer(6))
    code.emit("MARK_CHAR_AS_NO_LONGER_NEEDED", driver)
    code.emit("MARK_CAR_AS_NO_LONGER_NEEDED", vehicle)
    code.mark("vehicle_done")
    return code


def spawn_interceptor(code, vehicle, driver, target, *, model, driver_model, ped_type, position, heading, speed, caption, car_mission=17):
    x, y, z = position
    point = tuple(map(floating, position))
    clearance = tuple(map(floating, (x - 2.5, y - 2.5, z - 1, x + 2.5, y + 2.5, z + 2)))
    code.emit("IF", integer(4))
    code.emit("DOES_VEHICLE_EXIST", vehicle, negate=True)
    code.emit("IS_CAR_DEAD", target, negate=True)
    code.emit("HAS_MODEL_LOADED", integer(model))
    code.emit("HAS_MODEL_LOADED", integer(driver_model))
    code.emit("IS_AREA_OCCUPIED", *clearance, *map(integer, (0, 1, 1, 1, 0)), negate=True)
    code.jump("GOTO_IF_FALSE", "no_interceptor")
    code.emit("CREATE_CAR", integer(model), *point, vehicle)
    code.emit("SET_CAR_HEADING", vehicle, floating(heading))
    code.emit("CREATE_CHAR_INSIDE_CAR", vehicle, integer(ped_type), integer(driver_model), driver)
    code.emit("TASK_CAR_MISSION", driver, vehicle, target, integer(car_mission), floating(speed), integer(2))
    code.emit("PRINT_BIG", text_key(caption), integer(3700), integer(6))
    code.mark("no_interceptor")


def clear_interceptor(mission, vehicle, driver):
    code = Snippet(mission.table)
    for label, handle, exists, delete in (
        ("driver", driver, "DOES_CHAR_EXIST", "DELETE_CHAR"),
        ("vehicle", vehicle, "DOES_VEHICLE_EXIST", "DELETE_CAR"),
    ):
        code.emit("IF", integer(0))
        code.emit(exists, handle)
        code.jump("GOTO_IF_FALSE", label)
        code.emit(delete, handle)
        code.emit("SET_LVAR_INT", handle, integer(-1))
        code.mark(label)
    return code


def streaming_commands(mission, command, *models):
    code = Snippet(mission.table)
    for model in models:
        code.emit(command, integer(model))
    return code
