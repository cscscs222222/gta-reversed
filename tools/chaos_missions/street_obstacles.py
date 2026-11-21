from scm import floating, integer


def park_obstacle(code, label, vehicle, model, position, heading, police=False, fixed=False, *, screen_hidden=False):
    x, y, z = position
    point = tuple(map(floating, position))
    clearance = tuple(map(floating, (x - 3, y - 3, z - 1, x + 3, y + 3, z + 3)))
    code.emit("IF", integer(1 if screen_hidden else 2))
    code.emit("HAS_MODEL_LOADED", integer(model))
    if not screen_hidden:
        code.emit("IS_POINT_ON_SCREEN", *point, floating(4.0), negate=True)
    code.emit("IS_AREA_OCCUPIED", *clearance, *map(integer, (0, 1, 1, 1, 0)), negate=True)
    code.jump("GOTO_IF_FALSE", label)
    code.emit("CREATE_CAR", integer(model), *point, vehicle)
    code.emit("SET_CAR_HEADING", vehicle, floating(heading))
    code.emit("LOCK_CAR_DOORS", vehicle, integer(2))
    if fixed:
        code.emit("FREEZE_CAR_POSITION", vehicle, integer(1))
    if police:
        code.emit("SWITCH_CAR_SIREN", vehicle, integer(1))
    code.mark(label)
