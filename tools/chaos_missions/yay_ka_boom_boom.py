from scm import Snippet, global_var, integer, local, text_key


NAME = "Yay Ka-Boom-Boom"
MISSION_ID = 63


def expand(mission):
    mission.replace(0x1CBA, "SET_CHAR_ACCURACY", 1, 30, 43)
    mission.replace(0x25DE, "SET_CHAR_ACCURACY", 1, 30, 45)
    mission.replace(0x2DA8, "SET_CHAR_ACCURACY", 1, 30, 47)
    mission.replace(0x2DB3, "SET_CHAR_SHOOT_RATE", 1, 30, 45)

    # Two of the stock exit guards move toward CJ. The other three keep
    # their positions, so the outside group no longer waits in one line.
    wave = Snippet(mission.table)
    wave.emit("PRINT_BIG", text_key("CZEXIT"), integer(3500), integer(6))
    for actor in (local(108), local(111)):
        wave.emit("SET_CHAR_ACCURACY", actor, integer(56))
        wave.emit("SET_CHAR_SHOOT_RATE", actor, integer(51))
        wave.emit("TASK_KILL_CHAR_ON_FOOT", actor, global_var(12))
    mission.insert_before(0x3342, "IF", wave)
