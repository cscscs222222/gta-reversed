from scm import Snippet, integer, local, text_key


NAME = "Freefall"
MISSION_ID = 91


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x05BF, "LOCATE_CHAR_IN_CAR_3D", 6, 4.0, 3.2 if hard else 3.55)
    mission.replace(0x2AEA, "SET_CHAR_ACCURACY", 1, 65, 73)

    # Preserve the cabin camera and animation timing. The guards on
    # alternating seats use different accuracy and firing intervals.
    defence = Snippet(mission.table)
    index = local(145)
    for slot, accuracy, rate in ((1, 73, 60), (3, 77, 52)):
        next_seat = f"next_seat_{slot}"
        defence.emit("IF", integer(0))
        defence.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", index, integer(slot))
        defence.jump("GOTO_IF_FALSE", next_seat)
        defence.emit("SET_CHAR_ACCURACY", local(108 + slot), integer(accuracy + (5 if hard else 0)))
        defence.emit("SET_CHAR_SHOOT_RATE", local(108 + slot), integer(rate + (8 if hard else 0)))
        defence.mark(next_seat)
    mission.insert_before(0x1DC2, "SET_LVAR_INT", defence)
    warning = Snippet(mission.table)
    warning.emit("PRINT_BIG", text_key("CZPILOT"), integer(3500), integer(6))
    mission.insert_before(0x0636, "PLANE_FLY_IN_DIRECTION", warning)
