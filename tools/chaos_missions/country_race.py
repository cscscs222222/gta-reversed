from struct import pack

from scm import Snippet, floating, global_var, integer


def tune_rival_car(mission, race_id, pace, traction):
    # Two story races enter mission 35 with race IDs 25 and 26.
    # A same-offset detour preserves the legacy debug tail in this script.
    command = mission.instruction(0x04BE, "SET_CAR_CRUISE_SPEED")
    operand = command.operands[0]
    vehicle = mission.original[operand.start:operand.end]
    code = Snippet(mission.table)
    code.emit("IF", integer(0))
    code.emit("IS_INT_VAR_EQUAL_TO_NUMBER", global_var(1408), integer(race_id))
    code.jump("GOTO_IF_FALSE", "next_race")
    code.emit("SET_CAR_CRUISE_SPEED", vehicle, floating(pace))
    code.emit("SET_CAR_TRACTION", vehicle, floating(traction))
    code.emit("GOTO", pack("<Bi", 1, -command.end))
    code.mark("next_race")
    mission.redirect_before(command.start, command.name, code)
