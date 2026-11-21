from scm import Snippet, floating, integer, local, text_key


NAME = "Green Goo"
MISSION_ID = 82


def expand(mission):
    # The engineer accelerates during the actual pursuit, rather than
    # increasing speed while CJ is getting the jetpack.
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x0A8A, "SET_TRAIN_CRUISE_SPEED", 1, 10.0, 13.5 if hard else 12.5)
    code = Snippet(mission.table)
    for slot, accuracy, rate in ((67, 26, 29), (69, 34, 31)):
        code.emit("SET_CHAR_ACCURACY", local(slot), integer(accuracy + (12 if hard else 7)))
        code.emit("SET_CHAR_SHOOT_RATE", local(slot), integer(rate + (11 if hard else 6)))
    mission.insert_before(0x044A, "CREATE_OBJECT_NO_OFFSET", code)

    # Use otherwise empty carriages for lookalike cargo. Only the three
    # original crates remain in the radar and completion checks.
    mission.reserve_locals(957, 958, 959, 962, 963)
    coords = tuple(map(local, (957, 958, 959)))
    decoys = (local(962), local(963))
    init = Snippet(mission.table)
    for decoy in decoys:
        init.emit("SET_LVAR_INT", decoy, integer(-1))
    mission.insert_before(0x0000, "GOSUB", init)

    cargo = Snippet(mission.table)
    for index, (carriage, decoy) in enumerate(zip((local(60), local(62)), decoys)):
        next_car = f"next_car_{index}"
        cargo.emit("IF", integer(0))
        cargo.emit("DOES_VEHICLE_EXIST", carriage)
        cargo.jump("GOTO_IF_FALSE", next_car)
        cargo.emit("GET_CAR_COORDINATES", carriage, *coords)
        cargo.emit("CREATE_OBJECT_NO_OFFSET", integer(-261), *coords, decoy)
        cargo.emit(
            "ATTACH_OBJECT_TO_CAR", decoy, carriage,
            *map(floating, (0.0, 5.0, -1.0661, 0.0, 0.0, 90.2)),
        )
        cargo.emit("SET_OBJECT_COLLISION", decoy, integer(0))
        cargo.mark(next_car)
    mission.insert_before(0x0A0A, "SET_LVAR_INT", cargo)

    warning = Snippet(mission.table)
    warning.emit("IF", integer(21))
    for decoy in decoys:
        warning.emit("DOES_OBJECT_EXIST", decoy)
    warning.jump("GOTO_IF_FALSE", "done")
    warning.emit("PRINT_BIG", text_key("CZCRATE"), integer(3900), integer(6))
    warning.mark("done")
    mission.insert_before(0x0AC5, "PRINT_NOW", warning)

    cleanup = Snippet(mission.table)
    for index, decoy in enumerate(decoys):
        next_crate = f"next_crate_{index}"
        cleanup.emit("IF", integer(0))
        cleanup.emit("DOES_OBJECT_EXIST", decoy)
        cleanup.jump("GOTO_IF_FALSE", next_crate)
        cleanup.emit("DELETE_OBJECT", decoy)
        cleanup.mark(next_crate)
    mission.insert_before(0x001B, "GOSUB", cleanup)
