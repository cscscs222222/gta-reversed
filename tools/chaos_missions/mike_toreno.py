from scm import Snippet, integer


NAME = "Mike Toreno"
MISSION_ID = 66


def expand(mission):
    mission.replace(0x0932, "ADD_VAL_TO_INT_VAR", 1, 330000, 280000 if mission.difficulty == "acimasiz" else 300000)
    mission.replace(0x1599, "TASK_CAR_DRIVE_TO_COORD", 5, 25.0, 29.0)

    # The same stock array element identifies the current van passenger.
    equip = mission.instruction(0x1287, "SET_CURRENT_CHAR_WEAPON")
    actor = equip.operands[0]
    handle = mission.original[actor.start:actor.end]
    pressure = Snippet(mission.table)
    pressure.emit("SET_CHAR_ACCURACY", handle, integer(62))
    pressure.emit("SET_CHAR_SHOOT_RATE", handle, integer(56))
    mission.insert_before(equip.end, "IF", pressure)
