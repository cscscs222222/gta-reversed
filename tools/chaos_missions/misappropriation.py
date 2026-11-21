from scm import Snippet, integer, local, text_key


NAME = "Misappropriation"
MISSION_ID = 93


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    for offset, sense in ((0x06D7, 83.0), (0x07E7, 79.0), (0x08CE, 86.0)):
        mission.replace(offset, "SET_SENSE_RANGE", 1, 70, sense + (4.0 if hard else 0.0))
    for offset, slot, accuracy in ((0x07E7, 85, 46), (0x08CE, 86, 49)):
        guard = Snippet(mission.table)
        guard.emit("SET_CHAR_ACCURACY", local(slot), integer(accuracy + (7 if hard else 0)))
        guard.emit("SET_CHAR_SHOOT_RATE", local(slot), integer(51 if hard else 42))
        mission.insert_before(offset, "SET_SENSE_RANGE", guard)

    # If an agent reaches the case first, the stock code gives them the
    # objective marker. Explain that handover at the moment it happens.
    handover = Snippet(mission.table)
    handover.emit("PRINT_BIG", text_key("CZCASE2"), integer(4000), integer(6))
    mission.insert_before(0x19D4, "ADD_BLIP_FOR_CHAR", handover)
