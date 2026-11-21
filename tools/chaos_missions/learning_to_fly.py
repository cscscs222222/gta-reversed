from scm import Snippet, integer, local, text_key


NAME = "Learning to Fly"
MISSION_ID = 83


def expand(mission):
    # Hold the shoulder input when the approach exercise starts. Choose
    # the weather once per attempt, before restoring it at cleanup.
    mission.reserve_locals(962)
    weather = local(962)
    reset = Snippet(mission.table)
    reset.emit("SET_LVAR_INT", weather, integer(0))
    mission.insert_before(0x425B, "SET_VAR_INT", reset)

    code = Snippet(mission.table)
    code.emit("IF", integer(1))
    code.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", weather, integer(0))
    code.emit("IS_BUTTON_PRESSED", integer(0), integer(4))
    code.jump("GOTO_IF_FALSE", "apply_weather")
    code.emit("GENERATE_RANDOM_INT_IN_RANGE", integer(15), integer(17), weather)
    code.emit("PRINT_BIG", text_key("CZFLY"), integer(3500), integer(6))
    code.mark("apply_weather")
    code.emit("IF", integer(0))
    code.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", weather, integer(0))
    code.jump("GOTO_IF_FALSE", "done")
    code.emit("FORCE_WEATHER_NOW", weather)
    code.mark("done")
    mission.insert_before(0x4318, "IF", code)
