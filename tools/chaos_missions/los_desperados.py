from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Los Desperados"
MISSION_ID = 109


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    code = Snippet(mission.table)
    accuracy = (70, 75) if hard else (66, 72)
    code.emit("GENERATE_RANDOM_INT_IN_RANGE", *map(integer, accuracy), local(181))
    mission.insert_before(0x02DC, "SET_LVAR_INT", code)

    # Give the two approaches to the final street different pressure.
    # Keep the rooftop RPG's stock cadence and all original targets.
    mission.reserve_locals(958, 959, 960, 961, 963)
    x, y, z, route_set, support = map(local, (958, 959, 960, 961, 963))
    cj = global_var(12)
    init = Snippet(mission.table)
    init.emit("SET_LVAR_INT", route_set, integer(0))
    init.emit("SET_LVAR_INT", support, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)

    point = (1811.0, -2129.0, 12.6)
    setup = Snippet(mission.table)
    setup.emit("PRINT_BIG", text_key("CZVAGOS"), integer(4000), integer(6))
    setup.emit("IF", integer(1))
    setup.emit("HAS_MODEL_LOADED", integer(466))
    setup.emit(
        "IS_AREA_OCCUPIED", *map(floating, (1808.5, -2131.4, 11.9, 1813.5, -2126.6, 14.8)),
        *map(integer, (0, 1, 1, 1, 0)), negate=True,
    )
    setup.jump("GOTO_IF_FALSE", "done")
    setup.emit("CREATE_CAR", integer(466), *map(floating, point), support)
    setup.emit("SET_CAR_HEADING", support, floating(270.0))
    setup.emit("LOCK_CAR_DOORS", support, integer(2))
    setup.mark("done")
    mission.insert_before(0x70C8, "PRINT_NOW", setup)

    approach = Snippet(mission.table)
    approach.emit("IF", integer(1))
    approach.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", route_set, integer(0))
    approach.emit(
        "LOCATE_CHAR_ANY_MEANS_3D", cj, *map(floating, (1805.0, -2126.0, 12.6, 21.0, 23.0, 8.0)), integer(0),
    )
    approach.jump("GOTO_IF_FALSE", "done")
    approach.emit("SET_LVAR_INT", route_set, integer(1))
    approach.emit("GET_CHAR_COORDINATES", cj, x, y, z)
    approach.emit("IF", integer(0))
    approach.emit("IS_FLOAT_LVAR_GREATER_THAN_NUMBER", x, floating(1804.0))
    approach.jump("GOTO_IF_FALSE", "west")
    for name, guards, hold in (("east", (73, 74), 1), ("west", (71, 72), 0)):
        approach.mark(name)
        for slot in guards:
            guard = local(slot)
            next_guard = f"next_guard_{slot}"
            approach.emit("IF", integer(0))
            approach.emit("IS_CHAR_DEAD", guard, negate=True)
            approach.jump("GOTO_IF_FALSE", next_guard)
            approach.emit("SET_CHAR_KINDA_STAY_IN_SAME_PLACE", guard, integer(hold))
            approach.emit("SET_CHAR_SHOOT_RATE", guard, integer(55 if hard else 45))
            approach.emit(
                "TASK_KILL_CHAR_ON_FOOT_WHILE_DUCKING", guard, cj, *map(integer, (4, 4000, 40)),
            )
            approach.mark(next_guard)
        approach.jump("GOTO", "done")
    approach.mark("done")
    mission.insert_before(0x7149, "IF", approach)
    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_VEHICLE_EXIST", support)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("DELETE_CAR", support)
    cleanup.mark("done")
    mission.insert_before(0x0026, "GOSUB", cleanup)
