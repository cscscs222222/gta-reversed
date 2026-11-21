from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Wrong Side of the Tracks"
MISSION_ID = 29
SCRIPT_NAME = b"SMOKE3"

EDITS = (
    # The script creates the four Vagos in either of two branches.
    (0x0DC91B, "E2 02 03 22 00 04", 10, 16),
    (0x0DC922, "E2 02 03 23 00 04", 10, 16),
    (0x0DC929, "E2 02 03 24 00 04", 10, 16),
    (0x0DC930, "E2 02 03 25 00 04", 10, 16),
    (0x0DC937, "DD 07 03 22 00 04", 20, 24),
    (0x0DC93E, "DD 07 03 23 00 04", 20, 24),
    (0x0DC945, "DD 07 03 24 00 04", 30, 34),
    (0x0DC94C, "DD 07 03 25 00 04", 20, 24),
    (0x0DCDA7, "E2 02 03 22 00 04", 10, 16),
    (0x0DCDAE, "E2 02 03 23 00 04", 10, 16),
    (0x0DCDB5, "E2 02 03 24 00 04", 10, 16),
    (0x0DCDBC, "E2 02 03 25 00 04", 10, 16),
    (0x0DCDC3, "DD 07 03 22 00 04", 20, 24),
    (0x0DCDCA, "DD 07 03 23 00 04", 20, 24),
    (0x0DCDD1, "DD 07 03 24 00 04", 30, 34),
    (0x0DCDD8, "DD 07 03 25 00 04", 20, 24),
    (0x0DD1F3, "E2 02 03 25 00 04", 20, 24),
    # Restore Smoke's normal accuracy earlier, when he draws his weapon.
    (0x0DD3FA, "E2 02 02 08 02 04", 70, 85),
    # Raise the slower pursuit speeds while CJ is near the train.
    (0x0DD5A2, "DD 06 03 28 00 06", 15.0, 16.0),
    (0x0DD5BE, "DD 06 03 28 00 06", 18.0, 20.0),
)


def expand(mission):
    # Preserve the slowdown when CJ falls behind. Increase the speed
    # when he is already riding alongside the train.
    for offset, old, new in ((0x2A6B, 28.0, 29.5), (0x2ABF, 32.5, 34.5), (0x2AD0, 30.0, 31.5)):
        mission.replace(offset, "SET_TRAIN_CRUISE_SPEED", 1, old, new)

    # Use the Micro SMG for two of the four escorts in both train setups.
    # The stock model-loading wait covers both SMGs before attachment.
    request = Snippet(mission.table)
    request.emit("REQUEST_MODEL", integer(352))
    request.emit("REQUEST_MODEL", integer(1210))
    mission.reserve_locals(957, 958, 959, 960, 961, 962, 963)
    luggage = local(957)
    x, y, z, transferred = map(local, (958, 959, 960, 961))
    variant, escort = map(local, (962, 963))
    request.emit("GENERATE_RANDOM_INT_IN_RANGE", integer(1), integer(3), variant)
    request.emit("SET_LVAR_INT", escort, integer(-1))
    request.emit("SET_LVAR_INT", luggage, integer(-1))
    mission.insert_before(0x8A7, "REQUEST_MODEL", request)

    for offset in (0x1CD1, 0x1D11, 0x215D, 0x219D):
        mission.replace(offset, "ATTACH_CHAR_TO_CAR", 7, 29, 28)

    for offset in (0x38D2, 0x5098):
        release = Snippet(mission.table)
        release.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(352))
        release.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(1210))
        mission.insert_before(offset, "MARK_MODEL_AS_NO_LONGER_NEEDED", release)

    # One passing train carries a shooter. The four mission targets
    # stay on the original train.
    for choice, begin, crossing, end, position in (
        (1, 0x2BA1, 0x2C14, 0x2C9C, (2289.69, -1034.14, 26.29)),
        (2, 0x2D1E, 0x2D69, 0x2DF1, (2283.36, -336.90, 37.27)),
    ):
        enter = Snippet(mission.table)
        enter.emit("IF", integer(2))
        enter.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", variant, integer(choice))
        enter.emit("HAS_MODEL_LOADED", integer(110))
        enter.emit("HAS_MODEL_LOADED", integer(353))
        enter.jump("GOTO_IF_FALSE", "done")
        enter.emit(
            "CREATE_CHAR", integer(25), integer(110),
            *map(floating, position), escort,
        )
        enter.emit(
            "ATTACH_CHAR_TO_CAR", escort, local(41), floating(0.0),
            floating(3.5), floating(3.0), integer(0), floating(360.0), integer(29),
        )
        enter.emit("SET_CHAR_DECISION_MAKER", escort, local(75))
        enter.emit("SET_CHAR_RELATIONSHIP", escort, integer(4), integer(0))
        enter.emit("SET_CHAR_ACCURACY", escort, integer(27))
        enter.emit("SET_CHAR_SHOOT_RATE", escort, integer(34))
        enter.emit("SET_LVAR_INT", transferred, integer(0))
        enter.mark("done")
        mission.insert_before(begin, "SET_TRAIN_SPEED", enter)

        # The optional shooter switches trains once if the two engines
        # are alongside each other and the transition is off camera.
        swap = Snippet(mission.table)
        swap.emit("IF", integer(2))
        swap.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", transferred, integer(0))
        swap.emit("IS_CHAR_DEAD", escort, negate=True)
        swap.emit("IS_CAR_DEAD", local(40), negate=True)
        swap.jump("GOTO_IF_FALSE", "done")
        swap.emit("GET_CAR_COORDINATES", local(41), x, y, z)
        swap.emit("IF", integer(1))
        swap.emit(
            "LOCATE_CAR_3D", local(40), x, y, z,
            floating(6.0), floating(7.0), floating(2.0), integer(0),
        )
        swap.emit("IS_CHAR_ON_SCREEN", escort, negate=True)
        swap.jump("GOTO_IF_FALSE", "done")
        swap.emit("DETACH_CHAR_FROM_CAR", escort)
        swap.emit(
            "ATTACH_CHAR_TO_CAR", escort, local(40), floating(0.0),
            floating(-8.0), floating(3.0), integer(0), floating(360.0), integer(29),
        )
        swap.emit("SET_LVAR_INT", transferred, integer(1))
        swap.emit("PRINT", text_key("CZTRAIN"), integer(3200), integer(0))
        swap.mark("done")
        mission.insert_before(crossing, "IF", swap)

        leave = Snippet(mission.table)
        leave.emit("IF", integer(1))
        leave.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", transferred, integer(0))
        leave.emit("DOES_CHAR_EXIST", escort)
        leave.jump("GOTO_IF_FALSE", "done")
        leave.emit("DELETE_CHAR", escort)
        leave.emit("SET_LVAR_INT", escort, integer(-1))
        leave.mark("done")
        mission.insert_before(end, "DELETE_MISSION_TRAIN", leave)

    # A shootable briefcase rides with the Vagos. The line is triggered
    # only if the prop takes a hit during Smoke's shooting phase.
    prop = Snippet(mission.table)
    prop.emit("IF", integer(1))
    prop.emit("HAS_MODEL_LOADED", integer(1210))
    prop.emit("IS_CAR_DEAD", local(40), negate=True)
    prop.jump("GOTO_IF_FALSE", "done")
    prop.emit("GET_CAR_COORDINATES", local(40), x, y, z)
    prop.emit("CREATE_OBJECT", integer(1210), x, y, z, luggage)
    prop.emit(
        "ATTACH_OBJECT_TO_CAR", luggage, local(40),
        *map(floating, (0.9, -2.1, 2.95, 0.0, 0.0, 0.0)),
    )
    prop.mark("done")
    mission.insert_before(0x2830, "SET_CHAR_ACCURACY", prop)
    hit = Snippet(mission.table)
    hit.emit("IF", integer(1))
    hit.emit("DOES_OBJECT_EXIST", luggage)
    hit.emit("IS_CHAR_DEAD", global_var(520), negate=True)
    hit.jump("GOTO_IF_FALSE", "done")
    hit.emit("IF", integer(1))
    hit.emit("HAS_OBJECT_BEEN_DAMAGED", luggage)
    hit.emit("IS_CHAR_SHOOTING", global_var(520))
    hit.jump("GOTO_IF_FALSE", "done")
    hit.emit("PRINT", text_key("CZCASE"), integer(3300), integer(1))
    hit.emit("DELETE_OBJECT", luggage)
    hit.emit("SET_LVAR_INT", luggage, integer(-1))
    hit.mark("done")
    mission.insert_before(0x2985, "IF", hit)
