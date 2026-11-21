from scm import Snippet, global_var, integer, local, text_key


NAME = "Cop Wheels"
MISSION_ID = 99


def expand(mission):
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x246B, "ALTER_WANTED_LEVEL_NO_DROP", 1, 1, 2)
    for offset, old, distance in ((0x2426, 25, 32.0), (0x2812, 35, 43.0)):
        for axis in (2, 3):
            mission.replace(offset, "LOCATE_CHAR_ANY_MEANS_CHAR_2D", axis, old, distance)
    mission.replace(0x186F, "IS_CHAR_IN_ANGLED_AREA_IN_CAR_3D", 7, 4.8, 4.35 if hard else 4.6)

    # The pickup order is up to the player. Step up the police response
    # when they mount whichever bike remains after the first three.
    final_bike = Snippet(mission.table)
    final_bike.emit("IF", integer(0))
    final_bike.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", local(273), integer(3))
    final_bike.jump("GOTO_IF_FALSE", "done")
    final_bike.emit("ALTER_WANTED_LEVEL_NO_DROP", global_var(8), integer(3))
    final_bike.emit("PRINT_BIG", text_key("CZ4BIKE"), integer(3800), integer(6))
    final_bike.mark("done")
    mission.insert_before(0x176C, "PRINT_NOW", final_bike)
