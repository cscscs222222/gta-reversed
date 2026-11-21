from scm import Snippet, floating, integer, local


NAME = "Los Sepulcros"
MISSION_ID = 20


def expand(mission):
    mission.replace(0x4160, "SET_CHAR_HEALTH", 1, 260, 300)

    # Two pickets react over longer distances and fire at different rates.
    code = Snippet(mission.table)
    for slot in (111, 112, 113):
        code.emit("SET_CHAR_MAX_HEALTH", local(slot), integer(150))
        code.emit("SET_CHAR_HEALTH", local(slot), integer(150))
    for slot, distance, rate in ((111, 65.0, 44), (113, 57.0, 38)):
        code.emit("SET_SENSE_RANGE", local(slot), floating(distance))
        code.emit("SET_CHAR_SHOOT_RATE", local(slot), integer(rate))
        code.emit("ADD_ARMOUR_TO_CHAR", local(slot), integer(25))
    successor = mission.instructions[mission.instruction(0x427C, "SET_CHAR_HEALTH").end]
    mission.insert_before(successor.start, successor.name, code)
