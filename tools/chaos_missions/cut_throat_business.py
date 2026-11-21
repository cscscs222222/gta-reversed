from scm import Snippet, integer, text_key


NAME = "Cut Throat Business"
MISSION_ID = 105


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    for check, floor, stock, pace in (
        (0x25D1, 0x25E2, 0.9, 1.03 if hard else 0.98),
        (0x28BE, 0x28CF, 0.6, 0.77 if hard else 0.72),
    ):
        mission.replace(check, "IS_NUMBER_GREATER_THAN_FLOAT_LVAR", 0, stock, pace)
        mission.replace(floor, "SET_LVAR_FLOAT", 1, stock, pace)

    # Bring two boats and the last two parked vans closer to the
    # pursuit line. Keep Loc's recorded path and a clear side to pass.
    for offset, stock, shifted in (
        (0x153C, 44.50199890136719, 47.0),
        (0x15B4, 53.400699615478516, 50.6),
        (0x2D6C, 1064.5472412109375, 1062.8),
        (0x2D8D, 1050.5997314453125, 1052.3),
    ):
        mission.replace(offset, "CREATE_CAR", 1, stock, shifted)
    notice = Snippet(mission.table)
    notice.emit("PRINT_BIG", text_key("CZKART"), integer(3600), integer(6))
    mission.insert_before(0x2DAE, "ADD_VAL_TO_INT_LVAR", notice)
