from scm import Snippet, integer


NAME = "Toreno's Last Flight"
MISSION_ID = 62


def expand(mission):
    mission.replace(0x1ABF, "SET_CAR_HEALTH", 1, 3000, 3550 if mission.difficulty == "acimasiz" else 3350)
    altitude = 78.0 if mission.difficulty == "acimasiz" else 74.0
    for offset in (0x1BEC, 0x1CB2):
        for arg in (4, 5):
            mission.replace(offset, "HELI_GOTO_COORDS", arg, 70.0, altitude)

    # Configure each dispatched guard through the original guard array.
    equip = mission.instruction(0x13E2, "GIVE_WEAPON_TO_CHAR")
    actor = equip.operands[0]
    handle = mission.original[actor.start:actor.end]
    guards = Snippet(mission.table)
    guards.emit("SET_CHAR_ACCURACY", handle, integer(54))
    guards.emit("SET_CHAR_SHOOT_RATE", handle, integer(43))
    mission.insert_before(equip.end, "SET_CHAR_ONLY_DAMAGED_BY_PLAYER", guards)
