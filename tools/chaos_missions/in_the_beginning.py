from scene_vehicles import passing_vehicle, streaming_commands
from scm import Snippet, integer, local, text_key
from street_obstacles import park_obstacle


NAME = "In the Beginning"
MISSION_ID = 2


def expand(mission):
    mission.reserve_locals(960, 961)
    first, second = local(960), local(961)
    initialize = Snippet(mission.table)
    for slot in (first, second):
        initialize.emit("SET_LVAR_INT", slot, integer(-1))
    mission.insert_before(0x0000B, "GOSUB", initialize)

    # All cinematic branches rejoin here, before the controls are restored.
    # Stage the roadblocks while the view is dark and leave the BMX clear.
    block = Snippet(mission.table)
    block.emit("DO_FADE", integer(0), integer(0))
    block.emit("WAIT", integer(0))
    for model in (596, 523, 280):
        block.emit("REQUEST_MODEL", integer(model))
    block.emit("LOAD_ALL_MODELS_NOW")
    park_obstacle(block, "first_done", first, 596, (2259.65, -1265.75, 23.0), 35.0, police=True, screen_hidden=True)
    park_obstacle(block, "second_done", second, 596, (2431.61, -1254.06, 22.8303), 28.0, police=True, screen_hidden=True)
    block.emit("DO_FADE", integer(250), integer(1))
    mission.insert_before(0x01C60, "SWITCH_WIDESCREEN", block)

    rider = passing_vehicle(mission, 523, 280, (2267.25, -1272.5, 23.0), 90.0, 15.0)
    mission.insert_before(0x01C73, "DISPLAY_ZONE_NAMES", rider)

    title = Snippet(mission.table)
    title.emit("PRINT_BIG", text_key("CZSTART"), integer(4200), integer(5))
    mission.insert_before(0x023EE, "SET_LVAR_INT", title)

    # This path clears the earlier title as part of the stock BMX tutorial.
    notice = Snippet(mission.table)
    notice.emit("PRINT_BIG", text_key("CZSTART"), integer(4200), integer(5))
    notice.emit("IF", integer(21))
    notice.emit("DOES_VEHICLE_EXIST", first)
    notice.emit("DOES_VEHICLE_EXIST", second)
    notice.jump("GOTO_IF_FALSE", "no_notice")
    notice.emit("PRINT_NOW", text_key("CZPOLIS"), integer(3800), integer(1))
    notice.mark("no_notice")
    mission.insert_before(0x02542, "PRINT_HELP", notice)

    release = streaming_commands(mission, "MARK_MODEL_AS_NO_LONGER_NEEDED", 523, 280)
    for label, slot in (("first", first), ("second", second)):
        release.emit("IF", integer(0))
        release.emit("DOES_VEHICLE_EXIST", slot)
        release.jump("GOTO_IF_FALSE", f"release_{label}")
        release.emit("MARK_CAR_AS_NO_LONGER_NEEDED", slot)
        release.mark(f"release_{label}")
    mission.insert_before(0x02714, "MARK_MODEL_AS_NO_LONGER_NEEDED", release)
