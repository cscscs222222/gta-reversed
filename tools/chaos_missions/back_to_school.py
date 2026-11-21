from scm import Snippet, global_var, integer, text_key


NAME = "Back to School"
MISSION_ID = 71


def expand(mission):
    # Tune this exercise's timer. Keep the existing scoring and prizes.
    mission.replace(0x1532, "SET_VAR_INT", 1, 11000, 10250 if mission.difficulty == "acimasiz" else 10600)
    code = Snippet(mission.table)
    code.emit("IF", integer(0))
    code.emit("IS_PLAYER_PRESSING_HORN", global_var(8))
    code.jump("GOTO_IF_FALSE", "done")
    code.emit("SET_VAR_INT", global_var(1012), integer(9250))
    code.emit("PRINT_BIG", text_key("CZDRILL"), integer(2800), integer(6))
    code.mark("done")
    mission.insert_before(0x153A, "GOSUB", code)
