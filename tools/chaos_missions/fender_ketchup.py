from scm import Snippet, global_var, integer, local, text_key


NAME = "Fender Ketchup"
MISSION_ID = 84


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    speed = 35.0 if hard else 34.0
    mission.replace(0x1CC3, "IS_FLOAT_LVAR_GREATER_THAN_NUMBER", 1, 30.0, speed)
    mission.replace(0x1D43, "IS_NUMBER_GREATER_THAN_FLOAT_LVAR", 0, 30.0, speed)
    mission.replace(0x1D1D, "IS_INT_LVAR_GREATER_THAN_NUMBER", 1, 600, 820 if hard else 780)
    mission.replace(0x1A78, "ADD_VAL_TO_INT_VAR", 1, 7, 5)
    mission.replace(0x1AA5, "ADD_VAL_TO_INT_VAR", 1, 3, 2)
    mission.replace(0x1B26, "DIV_INT_LVAR_BY_VAL", 1, 250, 300)

    # Turn and jump still contribute independently. A mixed run earns
    # a single bonus, so repeating one move is less efficient.
    mission.reserve_locals(961, 962, 963)
    turn, jump, bonus = map(local, (961, 962, 963))
    reset = Snippet(mission.table)
    for flag in (turn, jump, bonus):
        reset.emit("SET_LVAR_INT", flag, integer(0))
    mission.insert_before(0x0268, "SET_VAR_INT", reset)

    turning = Snippet(mission.table)
    turning.emit("SET_LVAR_INT", turn, integer(1))
    mission.insert_before(0x1A78, "ADD_VAL_TO_INT_VAR", turning)
    landing = Snippet(mission.table)
    landing.emit("IF", integer(0))
    landing.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", local(51), integer(550))
    landing.jump("GOTO_IF_FALSE", "done")
    landing.emit("SET_LVAR_INT", jump, integer(1))
    landing.mark("done")
    mission.insert_before(0x1B26, "DIV_INT_LVAR_BY_VAL", landing)

    varied = Snippet(mission.table)
    varied.emit("IF", integer(2))
    for flag, value in ((turn, 1), (jump, 1), (bonus, 0)):
        varied.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", flag, integer(value))
    varied.jump("GOTO_IF_FALSE", "done")
    varied.emit("ADD_VAL_TO_INT_VAR", global_var(30112), integer(18))
    varied.emit("SET_LVAR_INT", bonus, integer(1))
    varied.emit("PRINT_BIG", text_key("CZMIXED"), integer(3500), integer(6))
    varied.mark("done")
    mission.insert_before(0x192B, "IF", varied)
