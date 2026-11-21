from scm import Snippet, integer, text_key


NAME = "Interdiction"
MISSION_ID = 77


def expand(mission):
    # Keep each helicopter's original target switches, with one crew
    # firing from farther out and the other following more closely.
    for offsets, radius in (((0x4BF6, 0x4D68), 26.0), ((0x6064, 0x61C1), 15.0)):
        for offset in offsets:
            mission.replace(offset, "HELI_FOLLOW_ENTITY", 3, 20.0, radius)
    mission.replace(0x49F5, "SET_CHAR_SHOOT_RATE", 1, 20, 31)
    mission.replace(0x4A00, "SET_CHAR_ACCURACY", 1, 27, 41)
    mission.replace(0x5E9B, "SET_CHAR_SHOOT_RATE", 1, 30, 43)
    mission.replace(0x5EA6, "SET_CHAR_ACCURACY", 1, 27, 38)
    warning = Snippet(mission.table)
    warning.emit("PRINT_BIG", text_key("CZHELI"), integer(3300), integer(6))
    mission.insert_before(0x6064, "HELI_FOLLOW_ENTITY", warning)
