from scm import Snippet, integer, local, text_key


NAME = "Architectural Espionage"
MISSION_ID = 96


def expand(mission):
    # Advance the building's existing two-stage alarm and strengthen
    # distinct defenders on the route out after the photographs.
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x5417, "ALTER_WANTED_LEVEL_NO_DROP", 1, 2, 3)
    mission.replace(0x544A, "IS_INT_LVAR_GREATER_THAN_NUMBER", 1, 4000, 2500 if hard else 3200)
    for offset, command, slot, accuracy in (
        (0x4C94, "SET_CHAR_DECISION_MAKER", 56, 43),
        (0x50DB, "TASK_STAY_IN_SAME_PLACE", 53, 38),
        (0x511D, "TASK_KINDA_STAY_IN_SAME_PLACE", 52, 43),
    ):
        guard = Snippet(mission.table)
        guard.emit("SET_CHAR_ACCURACY", local(slot), integer(accuracy + (7 if hard else 0)))
        mission.insert_before(offset, command, guard)
    warning = Snippet(mission.table)
    warning.emit("PRINT_BIG", text_key("CZALR4"), integer(3200), integer(6))
    mission.insert_before(0x5459, "ALTER_WANTED_LEVEL_NO_DROP", warning)
