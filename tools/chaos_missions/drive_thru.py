from scene_vehicles import streaming_commands
from scm import Snippet, floating, global_var, integer, local


NAME = "Drive-Thru"
MISSION_ID = 15


def expand(mission):
    # Use the first junction for a brief feint, then raise pursuit speeds
    # on the open stretches. Existing arrival and cutscene gates stay in use.
    for offset, old, new in (
        (0x1388, 13.0, 11.0),
        (0x13A9, 15.0, 19.0),
        (0x13CA, 18.0, 21.0),
        (0x13EB, 18.0, 21.0),
        (0x142D, 20.0, 23.0),
    ):
        mission.replace(offset, "TASK_CAR_DRIVE_TO_COORD", 5, old, new)
    mission.replace(0x1769, "SET_CHAR_ACCURACY", 1, 40, 58)
    mission.replace(0x1770, "TASK_DRIVE_BY", 9, 60, 76)
    mission.replace(0x1874, "TASK_DRIVE_BY", 9, 45, 30)
    mission.replace(0x1852, "SET_CAR_HEALTH", 1, 3100, 1350 if mission.difficulty == "acimasiz" else 1550)

    # The riders use two stock pursuit-road positions. They take part
    # in the chase, while the original Ballas car remains the objective.
    mission.reserve_locals(960, 961, 962, 963)
    mission.insert_before(0x12DD, "REQUEST_MODEL", streaming_commands(mission, "REQUEST_MODEL", 468))
    chase = Snippet(mission.table)
    for label, bike, rider, point, speed in (
        ("north", local(960), local(961), (2418.4, -1928.84, 12.3906), 28.0),
        ("south", local(962), local(963), (2410.74, -1960.03, 12.3906), 27.0),
    ):
        place = tuple(map(floating, point))
        x, y, z = point
        clearance = tuple(map(floating, (x - 2, y - 2, z - 1, x + 2, y + 2, z + 2)))
        chase.emit("IF", integer(3))
        chase.emit("HAS_MODEL_LOADED", integer(468))
        chase.emit("HAS_MODEL_LOADED", integer(103))
        chase.emit("IS_CAR_DEAD", global_var(544), negate=True)
        chase.emit("IS_AREA_OCCUPIED", *clearance, *map(integer, (0, 1, 1, 1, 0)), negate=True)
        chase.jump("GOTO_IF_FALSE", f"skip_{label}")
        chase.emit("CREATE_CAR", integer(468), *place, bike)
        chase.emit("SET_CAR_HEADING", bike, floating(180.0))
        chase.emit("CREATE_CHAR_INSIDE_CAR", bike, integer(25), integer(103), rider)
        chase.emit(
            "TASK_CAR_MISSION", rider, bike, global_var(544),
            integer(17), floating(speed), integer(2),
        )
        chase.mark(f"skip_{label}")
    mission.insert_before(0x1770, "TASK_DRIVE_BY", chase)
    mission.insert_before(
        0x3307, "MARK_MODEL_AS_NO_LONGER_NEEDED",
        streaming_commands(mission, "MARK_MODEL_AS_NO_LONGER_NEEDED", 468),
    )
