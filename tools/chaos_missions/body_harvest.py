from scene_vehicles import clear_interceptor, passing_vehicle, spawn_interceptor, streaming_commands
from scm import Snippet, global_var, integer, local


NAME = "Body Harvest"
MISSION_ID = 46


def expand(mission):
    mission.replace(0x0EC6, "SET_CHAR_ACCURACY", 1, 50, 64)
    mission.replace(0x0F0C, "SET_CHAR_ACCURACY", 1, 50, 59)
    for offset, accuracy in ((0x10B7, 60), (0x10FC, 56), (0x1141, 62), (0x1186, 57)):
        mission.replace(offset, "SET_CHAR_ACCURACY", 1, 50, accuracy)
    mission.insert_before(
        0x00F9, "LOAD_ALL_MODELS_NOW",
        streaming_commands(mission, "REQUEST_MODEL", 572),
    )
    # A farmhand in the stock mower crosses the approach to the combine.
    mission.insert_before(
        0x0E01, "PRINT_NOW",
        passing_vehicle(mission, 572, 160, (-1023.71, -947.35, 129.292), 177.348, 6.0),
    )
    mission.insert_before(
        0x409E, "MARK_MODEL_AS_NO_LONGER_NEEDED",
        streaming_commands(mission, "MARK_MODEL_AS_NO_LONGER_NEEDED", 572),
    )

    mission.reserve_locals(960, 961)
    tractor, driver, combine = local(960), local(961), local(34)
    setup = Snippet(mission.table)
    setup.emit("SET_LVAR_INT", tractor, integer(-1))
    setup.emit("SET_LVAR_INT", driver, integer(-1))
    mission.insert_before(0x000B, "GOSUB", setup)

    chase = Snippet(mission.table)
    chase.emit("IF", integer(0))
    chase.emit("IS_CHAR_IN_CAR", global_var(12), combine)
    chase.jump("GOTO_IF_FALSE", "done")
    spawn_interceptor(
        chase, tractor, driver, combine, model=531, driver_model=157, ped_type=24,
        position=(-1046.6, -1287.2, 128.28), heading=180.0, speed=17.0, caption="CZTRACT",
    )
    chase.mark("done")
    mission.insert_before(0x2A7D, "SET_CAR_FORWARD_SPEED", chase)
    cleanup = clear_interceptor(mission, tractor, driver)
    mission.insert_before(0x2D62, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
    mission.insert_before(0x0026, "GOSUB", cleanup)
