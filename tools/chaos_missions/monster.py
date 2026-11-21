from scm import Snippet, integer, text_key


NAME = "Monster"
MISSION_ID = 75


def expand(mission):
    # Make the large truck's off-road gates demand more precise lateral alignment.
    width = 6.3 if mission.difficulty == "acimasiz" else 7.0
    for axis in (4, 5):
        mission.replace(0x1BDC, "LOCATE_CHAR_IN_CAR_3D", axis, 8, width)
    start = Snippet(mission.table)
    start.emit("PRINT_BIG", text_key("CZMON"), integer(3300), integer(6))
    mission.insert_before(0x1713, "GET_GAME_TIMER", start)
