from scm import Snippet, integer, local, text_key
from street_obstacles import park_obstacle


NAME = "Customs Fast Track"
MISSION_ID = 69


def expand(mission):
    # Require a more carefully centered container when the replacement
    # pad unlocks, keeping the original crane controls and scenario.
    for axis in (4, 5):
        mission.replace(0x2114, "LOCATE_OBJECT_3D", axis, 6.0, 4.2 if mission.difficulty == "acimasiz" else 4.6)

    mission.reserve_locals(963)
    truck = local(963)
    init = Snippet(mission.table)
    init.emit("SET_LVAR_INT", truck, integer(-1))
    mission.redirect_before(0x000B, "GOSUB", init)
    block = Snippet(mission.table)
    block.emit("REQUEST_MODEL", integer(543))
    block.emit("LOAD_ALL_MODELS_NOW")
    park_obstacle(block, "staged", truck, 543, (-1638.5, 64.0, 2.3), 125.0, fixed=True)
    block.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(543))
    block.emit("IF", integer(0))
    block.emit("DOES_VEHICLE_EXIST", truck)
    block.jump("GOTO_IF_FALSE", "done")
    block.emit("PRINT_BIG", text_key("CZPORT"), integer(3900), integer(6))
    block.mark("done")
    mission.redirect_before(0x22D0, "CREATE_CAR", block)
    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_VEHICLE_EXIST", truck)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("DELETE_CAR", truck)
    cleanup.mark("done")
    mission.redirect_before(0x0026, "GOSUB", cleanup)
