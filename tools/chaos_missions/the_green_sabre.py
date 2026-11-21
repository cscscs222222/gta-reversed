from scm import Snippet, floating, integer, local


NAME = "The Green Sabre"
MISSION_ID = 38


def expand(mission):
    # The outer attackers react from farther away. The inner shooters
    # cover the approach to Sweet and are slower to clear.
    code = Snippet(mission.table)
    for slot, sense, accuracy, health in (
        (243, 80.0, 53, 130), (244, 65.0, 49, 155), (245, 65.0, 48, 155),
        (246, 65.0, 49, 155), (247, 82.0, 55, 140),
    ):
        guard = local(slot)
        code.emit("SET_SENSE_RANGE", guard, floating(sense))
        code.emit("SET_CHAR_ACCURACY", guard, integer(accuracy))
        code.emit("SET_CHAR_MAX_HEALTH", guard, integer(health))
        code.emit("SET_CHAR_HEALTH", guard, integer(health))
        code.emit("SET_CHAR_SHOOT_RATE", guard, integer(56))
    mission.insert_before(0x39EE, "SET_LVAR_INT_TO_VAR_INT", code)
