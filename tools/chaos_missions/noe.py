from scm import Snippet, integer, local, text_key


NAME = "N.O.E."
MISSION_ID = 79


def expand(mission):
    # Lower both sides of the radar threshold together so recovery still
    # works at the new altitude.
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x10B4, "IS_FLOAT_LVAR_GREATER_THAN_NUMBER", 1, 40, 35 if hard else 37)
    mission.replace(0x11B3, "IS_FLOAT_LVAR_GREATER_THAN_NUMBER", 1, 41, 36 if hard else 38)
    if hard:
        mission.replace(0x05E7, "SET_VAR_INT", 1, 10, 9)

    # A brief rain shower starts after the drop. The original mission
    # cleanup also releases forced weather if the player fails in flight.
    mission.reserve_locals(959, 960, 961)
    phase, started, elapsed = map(local, (959, 960, 961))
    init = Snippet(mission.table)
    init.emit("SET_LVAR_INT", phase, integer(0))
    mission.insert_before(0x000B, "GOSUB", init)

    rain = Snippet(mission.table)
    rain.emit("IF", integer(0))
    rain.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", phase, integer(0))
    rain.jump("GOTO_IF_FALSE", "done")
    rain.emit("FORCE_WEATHER_NOW", integer(16))
    rain.emit("GET_GAME_TIMER", started)
    rain.emit("SET_LVAR_INT", phase, integer(1))
    rain.emit("PRINT_BIG", text_key("CZRAIN"), integer(3900), integer(6))
    rain.mark("done")
    mission.insert_before(0x0DF2, "SET_LVAR_INT", rain)

    dry = Snippet(mission.table)
    dry.emit("IF", integer(0))
    dry.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", phase, integer(1))
    dry.jump("GOTO_IF_FALSE", "done")
    dry.emit("GET_GAME_TIMER", elapsed)
    dry.emit("SUB_INT_LVAR_FROM_INT_LVAR", elapsed, started)
    dry.emit("IF", integer(0))
    dry.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", elapsed, integer(18000))
    dry.jump("GOTO_IF_FALSE", "done")
    dry.emit("FORCE_WEATHER_NOW", integer(17))
    dry.emit("SET_LVAR_INT", phase, integer(2))
    dry.mark("done")
    mission.insert_before(0x0CA1, "GOSUB", dry)
