from scm import Snippet, integer, text_key


NAME = "Key to Her Heart"
MISSION_ID = 97


def expand(mission):
    # The tail in traffic needs a wider gap and fills the existing
    # suspicion meter sooner than the short stretch on foot.
    hard = mission.difficulty == "acimasiz"
    for gate, timer, notice, radius, interval in (
        (0x17CE, 0x17F2, 0x18D8, 28.0 if hard else 27.0, 50 if hard else 55),
        (0x40E5, 0x4109, 0x41EF, 24.0 if hard else 23.5, 57 if hard else 60),
    ):
        for axis in (2, 3):
            mission.replace(gate, "LOCATE_CHAR_ANY_MEANS_CHAR_3D", axis, 22.5, radius)
        mission.replace(timer, "IS_INT_LVAR_GREATER_THAN_NUMBER", 1, 64, interval)
        warning = Snippet(mission.table)
        warning.emit("PRINT_BIG", text_key("CZMILLI"), integer(3300), integer(6))
        mission.insert_before(notice, "PRINT_NOW", warning)
