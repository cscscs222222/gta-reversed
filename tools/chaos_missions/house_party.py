from scene_vehicles import streaming_commands
from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "House Party"
MISSION_ID = 34


def expand(mission):
    mission.reserve_locals(958, 959, 960, 961, 962, 963)
    north, south, north_blip, south_blip, started, elapsed = map(local, range(958, 964))
    waves = [
        ("north", north, north_blip, (2529.7, -1690.1, 12.5), 52),
        ("south", south, south_blip, (2467.1, -1710.8, 12.5), 46),
    ]
    if mission.difficulty == "acimasiz":
        mission.reserve_locals(956, 957)
        waves.append(("alley", local(956), local(957), (2478.7, -1645.7, 12.5), 55))
    mission.reserve_locals(953, 954, 955)
    van, driver, announced = map(local, (953, 954, 955))
    mission.insert_before(0x04D3, "LOAD_ALL_MODELS_NOW", streaming_commands(mission, "REQUEST_MODEL", 482))
    mission.insert_before(
        0x03FB3, "MARK_MODEL_AS_NO_LONGER_NEEDED",
        streaming_commands(mission, "MARK_MODEL_AS_NO_LONGER_NEEDED", 482),
    )
    code = Snippet(mission.table)
    player = global_var(12)
    code.emit("IF", integer(1))
    code.emit("HAS_MODEL_LOADED", integer(103))
    code.emit("HAS_MODEL_LOADED", integer(372))
    code.jump("GOTO_IF_FALSE", "done")
    code.emit("SET_LVAR_INT", announced, integer(0))
    code.emit("SET_LVAR_INT", driver, integer(-1))
    van_start = tuple(map(floating, (2480.7, -1729.2, 12.2)))
    code.emit("IF", integer(2))
    code.emit("HAS_MODEL_LOADED", integer(482))
    code.emit("LOCATE_CHAR_ANY_MEANS_2D", player, *van_start[:2], floating(13.0), floating(13.0), integer(0), negate=True)
    code.emit(
        "IS_AREA_OCCUPIED", *map(floating, (2478.5, -1731.4, 11.2, 2482.9, -1727.0, 15.2)),
        *map(integer, (0, 1, 1, 1, 0)), negate=True,
    )
    code.jump("GOTO_IF_FALSE", "van_done")
    code.emit("CREATE_CAR", integer(482), *van_start, van)
    code.emit("SET_CAR_HEADING", van, floating(0.0))
    code.emit("CREATE_CHAR_INSIDE_CAR", van, integer(24), integer(103), driver)
    code.emit("GIVE_WEAPON_TO_CHAR", driver, integer(32), integer(65))
    code.emit("SET_CHAR_ACCURACY", driver, integer(48))
    code.emit("SET_CHAR_SHOOT_RATE", driver, integer(54))
    code.emit(
        "TASK_CAR_DRIVE_TO_COORD", driver, van,
        *map(floating, (2482.1, -1720.2, 12.5, 12.0)),
        *map(integer, (2, 482, 2)),
    )
    code.emit("PRINT", text_key("CZVAN"), integer(3000), integer(1))
    code.emit("SET_LVAR_INT", announced, integer(1))
    code.mark("van_done")

    # At the end of the original wave, bring the attackers in along
    # the available approaches outside CJ's immediate position.
    for name, actor, blip, position, accuracy in waves:
        point = tuple(map(floating, position))
        code.emit("SET_LVAR_INT", actor, integer(-1))
        code.emit("SET_LVAR_INT", blip, integer(-1))
        x, y, z = position
        clearance = tuple(map(floating, (x - 0.9, y - 0.9, z - 0.5, x + 0.9, y + 0.9, z + 2)))
        code.emit("IF", integer(1))
        code.emit("LOCATE_CHAR_ANY_MEANS_2D", player, *point[:2], floating(11.0), floating(11.0), integer(0), negate=True)
        code.emit("IS_AREA_OCCUPIED", *clearance, *map(integer, (0, 1, 1, 1, 0)), negate=True)
        code.jump("GOTO_IF_FALSE", f"skip_{name}")
        code.emit("CREATE_CHAR", integer(25), integer(103), *point, actor)
        code.emit("GIVE_WEAPON_TO_CHAR", actor, integer(32), integer(120))
        code.emit("SET_CHAR_ACCURACY", actor, integer(accuracy))
        code.emit("SET_CHAR_SHOOT_RATE", actor, integer(57))
        code.emit("ADD_BLIP_FOR_CHAR", actor, blip)
        code.emit("TASK_KILL_CHAR_ON_FOOT", actor, player)
        code.mark(f"skip_{name}")

    code.emit("GET_GAME_TIMER", started)
    code.mark("engagement")
    code.emit("WAIT", integer(0))
    code.emit("IF", integer(len(waves)))
    code.emit("IS_CHAR_DEAD", driver)
    for _, actor, _, _, _ in waves:
        code.emit("IS_CHAR_DEAD", actor)
    code.jump("GOTO_IF_FALSE", "time_limit")
    code.jump("GOTO", "cleanup")
    code.mark("time_limit")
    code.emit("GET_GAME_TIMER", elapsed)
    code.emit("SUB_INT_LVAR_FROM_INT_LVAR", elapsed, started)
    code.emit("IF", integer(3))
    code.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", announced, integer(1))
    code.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", elapsed, integer(3500))
    code.emit("IS_CHAR_DEAD", driver, negate=True)
    code.emit("IS_CAR_DEAD", van, negate=True)
    code.jump("GOTO_IF_FALSE", "driver_attack")
    code.emit("SET_CAR_CRUISE_SPEED", van, floating(0.0))
    code.emit("TASK_LEAVE_CAR", driver, van)
    code.emit("SET_LVAR_INT", announced, integer(2))
    code.mark("driver_attack")
    code.emit("IF", integer(2))
    code.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", announced, integer(2))
    code.emit("IS_CHAR_DEAD", driver, negate=True)
    code.emit("IS_CHAR_IN_ANY_CAR", driver, negate=True)
    code.jump("GOTO_IF_FALSE", "check_time")
    code.emit("TASK_KILL_CHAR_ON_FOOT", driver, player)
    code.emit("SET_LVAR_INT", announced, integer(3))
    code.mark("check_time")
    code.emit("IF", integer(0))
    code.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", elapsed, integer(42000 if mission.difficulty == "acimasiz" else 34000))
    code.jump("GOTO_IF_FALSE", "engagement")

    # Let the original scene proceed even if someone retreats or is
    # stuck. The two optional markers must not leak into the next scene.
    code.mark("cleanup")
    code.emit("IF", integer(0))
    code.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", announced, integer(0))
    code.jump("GOTO_IF_FALSE", "release_actors")
    code.emit("IF", integer(0))
    code.emit("DOES_CHAR_EXIST", driver)
    code.jump("GOTO_IF_FALSE", "release_van")
    code.emit("IF", integer(1))
    code.emit("IS_CHAR_DEAD", driver, negate=True)
    code.emit("IS_CHAR_IN_ANY_CAR", driver, negate=True)
    code.jump("GOTO_IF_FALSE", "release_driver")
    code.emit("TASK_SMART_FLEE_CHAR", driver, player, floating(35.0), integer(7000))
    code.mark("release_driver")
    code.emit("MARK_CHAR_AS_NO_LONGER_NEEDED", driver)
    code.mark("release_van")
    code.emit("IF", integer(0))
    code.emit("DOES_VEHICLE_EXIST", van)
    code.jump("GOTO_IF_FALSE", "release_actors")
    code.emit("MARK_CAR_AS_NO_LONGER_NEEDED", van)
    code.mark("release_actors")
    for name, actor, blip, _, _ in waves:
        code.emit("IF", integer(0))
        code.emit("DOES_BLIP_EXIST", blip)
        code.jump("GOTO_IF_FALSE", f"no_blip_{name}")
        code.emit("REMOVE_BLIP", blip)
        code.mark(f"no_blip_{name}")
        code.emit("IF", integer(0))
        code.emit("DOES_CHAR_EXIST", actor)
        code.jump("GOTO_IF_FALSE", f"no_actor_{name}")
        code.emit("IF", integer(0))
        code.emit("IS_CHAR_DEAD", actor, negate=True)
        code.jump("GOTO_IF_FALSE", f"release_{name}")
        code.emit("TASK_SMART_FLEE_CHAR", actor, player, floating(35.0), integer(7000))
        code.mark(f"release_{name}")
        code.emit("MARK_CHAR_AS_NO_LONGER_NEEDED", actor)
        code.mark(f"no_actor_{name}")
    code.mark("done")
    mission.insert_before(0x38B9, "SET_LVAR_INT", code)
