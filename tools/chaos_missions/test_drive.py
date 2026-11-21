from scene_vehicles import clear_interceptor, spawn_interceptor
from scm import Snippet, global_var, integer, local


NAME = "Test Drive"
MISSION_ID = 68


def expand(mission):
    # Raise the pursuit alert after the existing police trigger.
    mission.replace(0x2805, "ALTER_WANTED_LEVEL_NO_DROP", 1, 2, 3)
    mission.reserve_locals(961, 962, 963)
    target, car, driver = map(local, (961, 962, 963))
    init = Snippet(mission.table)
    for handle in (car, driver):
        init.emit("SET_LVAR_INT", handle, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)
    chase = Snippet(mission.table)
    cj = global_var(12)
    chase.emit("IF", integer(0))
    chase.emit("IS_CHAR_IN_ANY_CAR", cj)
    chase.jump("GOTO_IF_FALSE", "done")
    chase.emit("GET_CAR_CHAR_IS_USING", cj, target)
    spawn_interceptor(
        chase, car, driver, target, model=597, driver_model=281, ped_type=6,
        position=(-1999.0, 836.8, 44.1), heading=180.0, speed=28.0, caption="CZCOP",
    )
    chase.mark("done")
    mission.insert_before(0x280C, "SET_LVAR_INT", chase)
    cleanup = clear_interceptor(mission, car, driver)
    mission.insert_before(0x662F, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
