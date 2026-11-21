from scene_vehicles import clear_interceptor, spawn_interceptor, streaming_commands
from scm import Snippet, global_var, integer, local


NAME = "Highjack"
MISSION_ID = 76


def expand(mission):
    # Preserve the truck's catch-up slowdown. Require tighter alignment
    # and a longer steady hold once Cesar is ready for the jump.
    hard = mission.difficulty == "acimasiz"
    for offset in (0x0E79, 0x2F77):
        mission.replace(offset, "SET_CAR_CRUISE_SPEED", 1, 37.0, 41.5 if hard else 40.0)
    for offset in (0x0F00, 0x10A9):
        mission.replace(offset, "IS_CHAR_IN_ANGLED_AREA_IN_CAR_3D", 7, 3.0, 2.45 if hard else 2.7)
    mission.replace(0x0FCE, "IS_INT_LVAR_GREATER_THAN_NUMBER", 1, 3500, 4100 if hard else 3800)

    mission.reserve_locals(962, 963)
    bike, rider = local(962), local(963)
    init = Snippet(mission.table)
    for handle in (bike, rider):
        init.emit("SET_LVAR_INT", handle, integer(-1))
    mission.insert_before(0x000B, "GOSUB", init)
    mission.insert_before(0x02F6, "REQUEST_MODEL", streaming_commands(mission, "REQUEST_MODEL", 166))

    player_bike = local(36)
    chase = Snippet(mission.table)
    chase.emit("IF", integer(0))
    chase.emit("IS_CHAR_IN_CAR", global_var(12), player_bike)
    chase.jump("GOTO_IF_FALSE", "done")
    spawn_interceptor(
        chase, bike, rider, player_bike, model=461, driver_model=166, ped_type=25,
        position=(-1689.8, 532.3, 36.8), heading=137.0, speed=34.0 if hard else 32.0,
        caption="CZJUMP", car_mission=53,
    )
    chase.mark("done")
    mission.insert_before(0x0E83, "SET_LVAR_INT", chase)

    mission.insert_before(0x0FDD, "SET_PLAYER_CONTROL", clear_interceptor(mission, bike, rider))
    cleanup = clear_interceptor(mission, bike, rider)
    cleanup.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(166))
    mission.insert_before(0x0026, "GOSUB", cleanup)
