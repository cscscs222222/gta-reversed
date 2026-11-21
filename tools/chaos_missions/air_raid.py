from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Air Raid"
MISSION_ID = 72


def expand(mission):
    mission.reserve_locals(962, 963)
    for speed_offset, next_offset, next_command, low, high in (
        (0x088F, 0x089D, "IF", 13.5, 17.0),
        (0x0B1F, 0x0B2D, "SET_LVAR_INT", 15.0, 19.0),
    ):
        cmd = mission.instruction(speed_offset, "SET_CAR_FORWARD_SPEED")
        vehicle = cmd.operands[0]
        code = Snippet(mission.table)
        code.emit("GENERATE_RANDOM_FLOAT_IN_RANGE", floating(low), floating(high), local(963))
        code.emit("SET_CAR_FORWARD_SPEED", mission.original[vehicle.start:vehicle.end], local(963))
        mission.insert_before(next_offset, next_command, code)
    mission.replace(0x0856, "SET_CAR_HEALTH", 1, 260, 275)
    mission.replace(0x0A86, "SET_CAR_HEALTH", 1, 260, 310 if mission.difficulty == "acimasiz" else 294)

    warned, timer = local(962), global_var(29688)
    notice = Snippet(mission.table)
    notice.emit("IF", integer(2))
    notice.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", warned, integer(0))
    notice.emit("IS_INT_VAR_GREATER_THAN_NUMBER", timer, integer(0))
    notice.emit("IS_NUMBER_GREATER_THAN_INT_VAR", integer(25000), timer)
    notice.jump("GOTO_IF_FALSE", "done")
    notice.emit("SET_LVAR_INT", warned, integer(1))
    notice.emit("PRINT_BIG", text_key("CZAIR"), integer(3200), integer(6))
    notice.mark("done")
    mission.insert_before(0x0787, "WAIT", notice)
