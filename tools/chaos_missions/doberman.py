from scm import Snippet, integer, local


NAME = "Doberman"
MISSION_ID = 21


def expand(mission):
    # Raise Ballas presence in Glen Park before the training gang war begins.
    mission.replace(0x098B, "SET_ZONE_GANG_STRENGTH", 2, 40, 72 if mission.difficulty == "acimasiz" else 62)

    # Strengthen the marked gang leader after the territorial fight.
    leader = local(34)
    health = 175 if mission.difficulty == "acimasiz" else 150
    code = Snippet(mission.table)
    code.emit("SET_CHAR_MAX_HEALTH", leader, integer(health))
    code.emit("SET_CHAR_HEALTH", leader, integer(health))
    code.emit("ADD_ARMOUR_TO_CHAR", leader, integer(35))
    code.emit("SET_CHAR_ACCURACY", leader, integer(57))
    code.emit("SET_CHAR_SHOOT_RATE", leader, integer(48))
    mission.insert_before(0x138A, "SET_CHAR_ONLY_DAMAGED_BY_PLAYER", code)
