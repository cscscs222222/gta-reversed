from scm import Snippet, integer, local, text_key
from street_obstacles import park_obstacle


NAME = "Deconstruction"
MISSION_ID = 50


def expand(mission):
    # Give the construction site's armed worker more pressure in both combat paths.
    mission.replace(0x4A9E, "SET_CHAR_ACCURACY", 1, 30, 47)
    mission.replace(0x4CB7, "SET_CHAR_ACCURACY", 1, 30, 44)
    for offset in (0x0C51, 0x0C81):
        mission.replace(offset, "SET_CAR_HEALTH", 1, 5000, 2900 if mission.difficulty == "acimasiz" else 3500)

    # Occupy one approach while the player collects a dozer. Use fixed
    # offsets because this mission contains a debug section in its tail.
    mission.reserve_locals(963)
    blocker = local(963)
    setup = Snippet(mission.table)
    setup.emit("SET_LVAR_INT", blocker, integer(-1))
    mission.redirect_before(0x000B, "GOSUB", setup)
    block = Snippet(mission.table)
    park_obstacle(block, "try_done", blocker, 524, (-2119.1, 195.9, 33.74), 112.0, fixed=True)
    block.emit("IF", integer(0))
    block.emit("DOES_VEHICLE_EXIST", blocker)
    block.jump("GOTO_IF_FALSE", "done")
    block.emit("PRINT_BIG", text_key("CZWORK"), integer(4000), integer(6))
    block.mark("done")
    mission.redirect_before(0x0C30, "CREATE_CAR", block)

    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_VEHICLE_EXIST", blocker)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("DELETE_CAR", blocker)
    cleanup.emit("SET_LVAR_INT", blocker, integer(-1))
    cleanup.mark("done")
    mission.redirect_before(0x328A, "CLEAR_AREA", cleanup)
    mission.redirect_before(0x0026, "GOSUB", cleanup)
