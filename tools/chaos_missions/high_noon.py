from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "High Noon"
MISSION_ID = 94


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    for offset in (0x0490, 0x0838, 0x08B3):
        mission.replace(offset, "TASK_CAR_DRIVE_TO_COORD", 5, 40, 47 if hard else 44)
    mission.replace(0x074D, "TASK_CAR_DRIVE_WANDER", 2, 40, 46 if hard else 43)

    # A brief brake feint during the pursuit leaves the existing road
    # task active. It happens once and only while Pulaski is driving.
    mission.reserve_locals(962, 963)
    speed, feinted = local(962), local(963)
    car, pulaski, cj = local(112), local(113), global_var(12)
    start = Snippet(mission.table)
    start.emit("SET_LVAR_INT", feinted, integer(0))
    mission.insert_before(0x0000, "GOSUB", start)

    feint = Snippet(mission.table)
    feint.emit("IF", integer(4))
    feint.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", feinted, integer(0))
    feint.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", local(47), integer(1))
    feint.emit("IS_CHAR_DEAD", pulaski, negate=True)
    feint.emit("IS_CAR_DEAD", car, negate=True)
    feint.emit("LOCATE_CHAR_ANY_MEANS_CAR_2D", cj, car, floating(35.0), floating(35.0), integer(0))
    feint.jump("GOTO_IF_FALSE", "done")
    feint.emit("IF", integer(0))
    feint.emit("IS_CHAR_IN_CAR", pulaski, car)
    feint.jump("GOTO_IF_FALSE", "done")
    feint.emit("GET_CAR_SPEED", car, speed)
    feint.emit("IF", integer(0))
    feint.emit("IS_FLOAT_LVAR_GREATER_THAN_NUMBER", speed, floating(12.0))
    feint.jump("GOTO_IF_FALSE", "done")
    feint.emit("SET_CAR_TEMP_ACTION", car, integer(6), integer(550 if hard else 750))
    feint.emit("SET_LVAR_INT", feinted, integer(1))
    feint.emit("PRINT_BIG", text_key("CZBRAKE"), integer(3200), integer(6))
    feint.mark("done")
    mission.insert_before(0x07E3, "IF", feint)
