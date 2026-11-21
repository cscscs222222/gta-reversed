from scene_vehicles import clear_interceptor, spawn_interceptor, streaming_commands
from scm import Snippet, integer, local


NAME = "Are You Going to San Fierro?"
MISSION_ID = 47


def expand(mission):
    # Limit the two rocket allocations during the helicopter phase.
    rounds = 6 if mission.difficulty == "acimasiz" else 7
    mission.replace(0x20CE, "GIVE_WEAPON_TO_CHAR", 2, 10, rounds)
    mission.replace(0x21EA, "GIVE_WEAPON_TO_CHAR", 2, 10, rounds)
    mission.replace(0x00A5, "SET_VAR_INT", 1, 331000, 282000 if mission.difficulty == "acimasiz" else 307000)
    mission.insert_before(0x038B, "REQUEST_MODEL", streaming_commands(mission, "REQUEST_MODEL", 599))

    # A deputy goes after the camper once CJ starts driving. The
    # original trip to the garage is still the only escape objective.
    mission.reserve_locals(962, 963)
    sheriff, deputy = local(962), local(963)
    setup = Snippet(mission.table)
    setup.emit("SET_LVAR_INT", sheriff, integer(-1))
    setup.emit("SET_LVAR_INT", deputy, integer(-1))
    mission.insert_before(0x000B, "GOSUB", setup)
    pursuit = Snippet(mission.table)
    spawn_interceptor(
        pursuit, sheriff, deputy, local(34), model=599, driver_model=283, ped_type=6,
        position=(-1104.7, -1648.0, 75.4), heading=270.0, speed=32.0, caption="CZFARM",
    )
    mission.insert_before(0x24DE, "SET_CAR_STATUS", pursuit)
    cleanup = clear_interceptor(mission, sheriff, deputy)
    cleanup.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(599))
    mission.insert_before(0x303F, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
