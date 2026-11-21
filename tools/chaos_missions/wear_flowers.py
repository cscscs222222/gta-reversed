from scene_vehicles import clear_interceptor, passing_vehicle, spawn_interceptor, streaming_commands
from scm import Snippet, integer, local


NAME = "Wear Flowers in Your Hair"
MISSION_ID = 49


def expand(mission):
    mission.replace(0x12AB, "SET_CAR_DENSITY_MULTIPLIER", 0, 1.0, 1.3)
    mission.replace(0x1CEB, "SET_CAR_DENSITY_MULTIPLIER", 0, 1.0, 1.24)
    mission.replace(0x0D13, "SET_CAR_HEALTH", 1, 3000, 1900 if mission.difficulty == "acimasiz" else 2250)
    mission.insert_before(0x0CDC, "REQUEST_MODEL", streaming_commands(mission, "REQUEST_MODEL", 498, 466, 147))

    # The van hand-off leaves an available lane for the passing courier.
    mail = Snippet(mission.table)
    mail.emit("LOAD_ALL_MODELS_NOW")
    passing_vehicle(mission, 498, 147, (-1684.0, 432.0, 6.7), 229.0, 12.0, mail, caption="CZMAIL")
    mission.insert_before(0x1D20, "SET_CAR_STATUS", mail)

    mission.reserve_locals(960, 961)
    tail, driver = local(960), local(961)
    setup = Snippet(mission.table)
    setup.emit("SET_LVAR_INT", tail, integer(-1))
    setup.emit("SET_LVAR_INT", driver, integer(-1))
    mission.insert_before(0x000B, "GOSUB", setup)
    follow = Snippet(mission.table)
    spawn_interceptor(
        follow, tail, driver, local(34), model=466, driver_model=147, ped_type=4,
        position=(-2235.4, 128.3, 34.35), heading=180.0, speed=22.0, caption="CZTAIL", car_mission=52,
    )
    mission.insert_before(0x42D8, "MARK_OBJECT_AS_NO_LONGER_NEEDED", follow)
    cleanup = clear_interceptor(mission, tail, driver)
    mission.insert_before(0x4369, "SET_PLAYER_CONTROL", cleanup)
    mission.insert_before(0x0026, "GOSUB", cleanup)
    mission.insert_before(
        0x46F6, "MARK_MODEL_AS_NO_LONGER_NEEDED",
        streaming_commands(mission, "MARK_MODEL_AS_NO_LONGER_NEEDED", 498, 466, 147),
    )
