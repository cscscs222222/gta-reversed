from scm import Snippet, integer, text_key
from timeout_checkpoint import add_timeout_checkpoint


NAME = "Breaking the Bank at Caligula's"
MISSION_ID = 101


def expand(mission):
    # Raise the two casino security waves separately. Keep the approach
    # crew's settings and the original alarm and story branches.
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x3F42, "SET_CHAR_ACCURACY", 1, 20, 33)
    mission.replace(0x3F4D, "SET_CHAR_SHOOT_RATE", 1, 20, 29)
    mission.replace(0x4223, "SET_CHAR_ACCURACY", 1, 20, 35)
    mission.replace(0x4218, "SET_CHAR_SHOOT_RATE", 1, 20, 33)
    mission.replace(0x8B88, "SET_CHAR_SHOOT_RATE", 1, 30, 43)
    mission.replace(0x8B93, "SET_CHAR_ACCURACY", 1, 30, 41)
    mission.replace(0x11DA, "SET_VAR_INT", 1, 240000, 228000 if hard else 235000)
    mission.replace(0x425A, "SET_VAR_INT", 1, 420000, 375000 if hard else 395000)

    # Only use a retry after the interior setup, or after the crew's
    # second timer starts. The second phase gets a fresh snapshot.
    add_timeout_checkpoint(mission, 0x42DD, "IF", 0x0A4D, 31736, earlier=(0x28A5, "SET_LVAR_INT"))
    exit_guard = Snippet(mission.table)
    exit_guard.emit("PRINT_BIG", text_key("CZVAULT"), integer(3900), integer(6))
    mission.insert_before(0x8BBF, "PRINT_NOW", exit_guard)
