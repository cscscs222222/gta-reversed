from scene_vehicles import streaming_commands
from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Fish in a Barrel"
MISSION_ID = 87


def expand(mission):
    mission.reserve_locals(963)
    mission.insert_before(
        0x004B, "LOAD_CUTSCENE",
        streaming_commands(mission, "REQUEST_MODEL", 171),
    )
    # A casino employee briefly turns to look at CJ outside. The scene
    # ends on the original branch and has no extra required action.
    staff = local(963)
    code = Snippet(mission.table)
    code.emit("IF", integer(0))
    code.emit("HAS_MODEL_LOADED", integer(171))
    code.jump("GOTO_IF_FALSE", "done")
    code.emit(
        "CREATE_CHAR", integer(4), integer(171),
        *map(floating, (2023.5, 1006.6, 9.81)), staff,
    )
    code.emit("TASK_LOOK_AT_CHAR", staff, global_var(12), integer(3500))
    code.emit("PRINT_BIG", text_key("CZORDER"), integer(5100), integer(6))
    code.emit("MARK_CHAR_AS_NO_LONGER_NEEDED", staff)
    code.mark("done")
    code.emit("MARK_MODEL_AS_NO_LONGER_NEEDED", integer(171))
    mission.insert_before(0x011F, "DO_FADE", code)
