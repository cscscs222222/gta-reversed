from scm import Snippet, integer, local, text_key
from street_obstacles import park_obstacle


NAME = "Against All Odds"
MISSION_ID = 44


def expand(mission):
    # Escalate the existing response to the bombing and retain the paint-shop escape.
    mission.replace(0x35E6, "ALTER_WANTED_LEVEL", 1, 4, 5)

    # Block one of the two roads with a sheriff's truck. The other exit
    # remains available, and the truck never spawns over another vehicle.
    mission.reserve_locals(962, 963)
    route, barricade = local(962), local(963)
    setup = Snippet(mission.table)
    setup.emit("SET_LVAR_INT", barricade, integer(-1))
    mission.insert_before(0x000B, "GOSUB", setup)
    place = Snippet(mission.table)
    place.emit("GENERATE_RANDOM_INT_IN_RANGE", integer(0), integer(2), route)
    place.emit("IF", integer(0))
    place.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", route, integer(0))
    place.jump("GOTO_IF_FALSE", "north")
    park_obstacle(place, "south_done", barricade, 599, (1297.15, 244.48, 18.41), 75.0, police=True, fixed=True)
    place.jump("GOTO", "done")
    place.mark("north")
    park_obstacle(place, "north_done", barricade, 599, (1304.3, 285.7, 18.55), 115.0, police=True, fixed=True)
    place.mark("done")
    mission.insert_before(0x2F1D, "CREATE_CAR", place)

    warning = Snippet(mission.table)
    warning.emit("IF", integer(0))
    warning.emit("DOES_VEHICLE_EXIST", barricade)
    warning.jump("GOTO_IF_FALSE", "done")
    warning.emit("PRINT_BIG", text_key("CZPOLR"), integer(3800), integer(6))
    warning.mark("done")
    mission.insert_before(0x35F4, "DISPLAY_RADAR", warning)

    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_VEHICLE_EXIST", barricade)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("DELETE_CAR", barricade)
    cleanup.mark("done")
    mission.insert_before(0x48B3, "MARK_MODEL_AS_NO_LONGER_NEEDED", cleanup)
