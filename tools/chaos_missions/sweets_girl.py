from scm import Snippet, floating, integer, local, text_key


NAME = "Sweet's Girl"
MISSION_ID = 18


def expand(mission):
    mission.replace(0x08D1, "SET_CHAR_ACCURACY", 1, 40, 55)
    mission.replace(0x095E, "SET_CHAR_ACCURACY", 1, 40, 51)
    mission.replace(0x0FBC, "SET_CHAR_ACCURACY", 1, 65, 73)
    mission.replace(0x11FA, "ADD_VAL_TO_INT_LVAR", 1, 60, 67)

    # One of the existing cars tries to ram CJ while the other cuts
    # ahead. The passenger is introduced after the recorded car sequence.
    mission.replace(0x1BB3, "SET_CAR_MISSION", 1, 5, 3)
    mission.reserve_locals(963)
    gunner = local(963)
    rescue_car, enemy_car = local(59), local(63)
    initialize = Snippet(mission.table)
    initialize.emit("SET_LVAR_INT", gunner, integer(-1))
    mission.insert_before(0x000B, "GOSUB", initialize)

    chase = Snippet(mission.table)
    chase.emit("IF", integer(3))
    chase.emit("IS_CAR_DEAD", enemy_car, negate=True)
    chase.emit("IS_CAR_DEAD", rescue_car, negate=True)
    chase.emit("HAS_MODEL_LOADED", integer(105))
    chase.emit("HAS_MODEL_LOADED", integer(346))
    chase.jump("GOTO_IF_FALSE", "done")
    chase.emit("CREATE_CHAR_AS_PASSENGER", enemy_car, integer(24), integer(105), integer(0), gunner)
    chase.emit("GIVE_WEAPON_TO_CHAR", gunner, integer(22), integer(45))
    chase.emit("SET_CURRENT_CHAR_WEAPON", gunner, integer(22))
    chase.emit("SET_CHAR_ACCURACY", gunner, integer(55))
    chase.emit("SET_CHAR_SHOOT_RATE", gunner, integer(48))
    chase.emit(
        "TASK_DRIVE_BY", gunner, integer(-1), rescue_car,
        floating(0), floating(0), floating(0), floating(35), integer(8), integer(1), integer(55),
    )
    chase.emit("PRINT_BIG", text_key("CZSWEET"), integer(3500), integer(6))
    chase.mark("done")
    mission.insert_before(0x1E29, "SET_PLAYER_CONTROL", chase)

    cease_fire = Snippet(mission.table)
    cease_fire.emit("IF", integer(0))
    cease_fire.emit("IS_CHAR_DEAD", gunner, negate=True)
    cease_fire.jump("GOTO_IF_FALSE", "done")
    cease_fire.emit("CLEAR_CHAR_TASKS", gunner)
    cease_fire.emit("REMOVE_ALL_CHAR_WEAPONS", gunner)
    cease_fire.mark("done")
    mission.insert_before(0x249B, "SET_LVAR_INT", cease_fire)

    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_CHAR_EXIST", gunner)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("DELETE_CHAR", gunner)
    cleanup.mark("done")
    mission.insert_before(0x3CD0, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
