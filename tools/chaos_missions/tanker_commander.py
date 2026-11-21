from scm import Snippet, integer, local, text_key


NAME = "Tanker Commander"
MISSION_ID = 43


def expand(mission):
    # Raise the pressure on CJ while preserving Catalina's scripted shots.
    for offset, pressure in ((0x20AA, 33), (0x2192, 29), (0x227A, 35), (0x2362, 30), (0x244A, 36), (0x252D, 32)):
        mission.replace(offset, "TASK_DRIVE_BY", 9, 15, pressure)
    mission.replace(0x2BA2, "TASK_CAR_MISSION", 3, 29, 15)
    mission.replace(0x2C1D, "TASK_CAR_MISSION", 3, 32, 17)

    mission.reserve_locals(963)
    warned = local(963)
    warning = Snippet(mission.table)
    warning.emit("IF", integer(0))
    warning.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", warned, integer(0))
    warning.jump("GOTO_IF_FALSE", "done")
    warning.emit("SET_LVAR_INT", warned, integer(1))
    warning.emit("PRINT_BIG", text_key("CZHAUL"), integer(3600), integer(6))
    warning.mark("done")
    mission.insert_before(0x2AA9, "TASK_CAR_MISSION", warning)
