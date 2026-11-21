from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Robbing Uncle Sam"
MISSION_ID = 26


def expand(mission):
    mission.replace(0x2066, "SET_CHAR_ACCURACY", 1, 35, 50)
    mission.replace(0x4109, "LOCATE_OBJECT_3D", 4, 0.71, 0.65)
    mission.replace(0x4109, "LOCATE_OBJECT_3D", 5, 1.0, 0.90)
    mission.replace(0x3851, "SET_CAR_HEALTH", 1, 2500, 1800 if mission.difficulty == "acimasiz" else 2050)
    for offset, default, raised in ((0x6F79, 40, 53), (0x6FF8, 40, 53), (0x71A2, 60, 68), (0x7221, 60, 68)):
        mission.replace(offset, "TASK_DRIVE_BY", 9, default, raised)

    mission.reserve_locals(962, 963)
    soldiers = (local(962), local(963))
    setup = Snippet(mission.table)
    for soldier in soldiers:
        setup.emit("SET_LVAR_INT", soldier, integer(-1))
    mission.redirect_before(0x000B, "GOSUB", setup)

    # Stage the two additional guards as control returns after loading the
    # first crate. Leave the forklift, stock crates and Ryder's tasks in place.
    reinforcements = Snippet(mission.table)
    reinforcements.emit("IF", integer(2))
    reinforcements.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", local(127), integer(1))
    reinforcements.emit("HAS_MODEL_LOADED", integer(287))
    reinforcements.emit("HAS_MODEL_LOADED", integer(346))
    reinforcements.jump("GOTO_IF_FALSE", "done")
    for index, (soldier, (x, y, z)) in enumerate(zip(soldiers, (
        (2768.74, -2413.79, 12.62),
        (2790.4, -2434.1, 12.63),
    ), strict=True)):
        point = tuple(map(floating, (x, y, z)))
        clearance = tuple(map(floating, (x - 1.1, y - 1.1, z - 0.5, x + 1.1, y + 1.1, z + 2)))
        reinforcements.emit("IF", integer(1))
        reinforcements.emit("DOES_CHAR_EXIST", soldier, negate=True)
        reinforcements.emit("IS_AREA_OCCUPIED", *clearance, *map(integer, (0, 1, 1, 1, 0)), negate=True)
        reinforcements.jump("GOTO_IF_FALSE", f"next_{index}")
        reinforcements.emit("CREATE_CHAR", integer(24), integer(287), *point, soldier)
        reinforcements.emit("GIVE_WEAPON_TO_CHAR", soldier, integer(22), integer(90))
        reinforcements.emit("SET_CURRENT_CHAR_WEAPON", soldier, integer(22))
        reinforcements.emit("SET_CHAR_ACCURACY", soldier, integer(52))
        reinforcements.emit("SET_CHAR_SHOOT_RATE", soldier, integer(48))
        reinforcements.emit("TASK_KILL_CHAR_ON_FOOT", soldier, global_var(12))
        reinforcements.mark(f"next_{index}")
    reinforcements.emit("IF", integer(21))
    for soldier in soldiers:
        reinforcements.emit("DOES_CHAR_EXIST", soldier)
    reinforcements.jump("GOTO_IF_FALSE", "done")
    reinforcements.emit("PRINT_BIG", text_key("CZDEPOT"), integer(3700), integer(6))
    reinforcements.mark("done")
    mission.redirect_before(0x04333, "SET_PLAYER_CONTROL", reinforcements)

    cleanup = Snippet(mission.table)
    for index, soldier in enumerate(soldiers):
        cleanup.emit("IF", integer(0))
        cleanup.emit("DOES_CHAR_EXIST", soldier)
        cleanup.jump("GOTO_IF_FALSE", f"next_{index}")
        cleanup.emit("DELETE_CHAR", soldier)
        cleanup.emit("SET_LVAR_INT", soldier, integer(-1))
        cleanup.mark(f"next_{index}")
    mission.redirect_before(0x04DDA, "PRINT_NOW", cleanup)
    mission.redirect_before(0x0026, "GOSUB", cleanup)
