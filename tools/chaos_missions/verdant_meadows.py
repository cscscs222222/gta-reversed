from scene_vehicles import passing_vehicle, streaming_commands
from scm import Snippet, integer, local


NAME = "Verdant Meadows"
MISSION_ID = 134


def expand(mission):
    # Mission 134 processes every property purchase. This branch is
    # specific to the airstrip. Mark the branch for the common outro.
    mission.reserve_locals(961)
    bought_airstrip = local(961)
    init = Snippet(mission.table)
    init.emit("SET_LVAR_INT", bought_airstrip, integer(0))
    mission.insert_before(0x000B, "SET_VAR_INT", init)
    request = streaming_commands(mission, "REQUEST_MODEL", 531, 160)
    request.emit("SET_LVAR_INT", bought_airstrip, integer(1))
    mission.insert_before(0x0237, "START_NEW_SCRIPT", request)
    visit = Snippet(mission.table)
    visit.emit("IF", integer(0))
    visit.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", bought_airstrip, integer(1))
    visit.jump("GOTO_IF_FALSE", "done")
    visit.emit("LOAD_ALL_MODELS_NOW")
    passing_vehicle(
        mission, 531, 160, (445.1, 2545.5, 16.4), 90.0, 5.0, code=visit, caption="CZSTRIP",
    )
    visit.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(531))
    visit.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(160))
    visit.mark("done")
    mission.insert_before(0x10D5, "SET_VAR_INT", visit)
