from scene_vehicles import passing_vehicle
from scm import Snippet, floating, global_var, integer, local, text_key
from timeout_checkpoint import add_timeout_checkpoint


NAME = "End of the Line"
MISSION_IDS = (110, 111, 112)


def entry_and_smoke(mission):
    hard = mission.difficulty == "acimasiz"
    # Strengthen the outer line, then give two indoor defenders room
    # to push out from cover while the higher positions hold.
    for offset, health in ((0x1CBA, 36), (0x1D11, 30), (0x1DBF, 38), (0x1E6D, 34)):
        mission.replace(offset, "SET_CHAR_HEALTH", 1, 20, health + (8 if hard else 0))
    for offset in (0x1187, 0x1243):
        mission.replace(offset, "TASK_STAY_IN_SAME_PLACE", 1, 1, 0)
    for offset, rate in ((0x25B6, 72), (0x2673, 67), (0x27D8, 72), (0x287F, 66), (0x3FCB, 69)):
        mission.replace(offset, "SET_CHAR_SHOOT_RATE", 1, 60, rate)
    if hard:
        mission.replace(0x2337, "CREATE_PICKUP_WITH_AMMO", 2, 20, 16)

    # Smoke's scripted reinforcements are already set up to emerge
    # off-screen. Split their roles between advancing and holding fire.
    mission.replace(0x6DB7, "SET_CHAR_ACCURACY", 1, 30, 48)
    mission.replace(0x6EDD, "SET_CHAR_ACCURACY", 1, 30, 46)
    mission.reserve_locals(962, 963)
    init = Snippet(mission.table)
    for slot in (962, 963):
        init.emit("SET_LVAR_INT", local(slot), integer(0))
    mission.insert_before(0x0000, "GOSUB", init)

    wave = Snippet(mission.table)
    wave.emit("SET_CHAR_SHOOT_RATE", local(71), integer(58 if hard else 50))
    wave.emit("IF", integer(0))
    wave.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", local(962), integer(0))
    wave.jump("GOTO_IF_FALSE", "done")
    wave.emit("SET_LVAR_INT", local(962), integer(1))
    wave.emit("PRINT_BIG", text_key("CZSMKW"), integer(3500), integer(6))
    wave.mark("done")
    mission.insert_before(0x6DCE, "SET_LVAR_INT", wave)
    cover = Snippet(mission.table)
    cover.emit("SET_CHAR_KINDA_STAY_IN_SAME_PLACE", local(73), integer(1))
    cover.emit("SET_CHAR_SHOOT_RATE", local(73), integer(65 if hard else 56))
    mission.insert_before(0x6EF4, "SET_LVAR_INT", cover)

    # The short subtitle runs once during the fight, before the health
    # gate that starts the original final conversation.
    joke = Snippet(mission.table)
    joke.emit("IF", integer(2))
    joke.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", local(963), integer(0))
    joke.emit("IS_NUMBER_GREATER_OR_EQUAL_TO_INT_VAR", integer(9100), global_var(32000))
    joke.emit("IS_INT_VAR_GREATER_THAN_NUMBER", global_var(32000), integer(8100))
    joke.jump("GOTO_IF_FALSE", "done")
    joke.emit("SET_LVAR_INT", local(963), integer(1))
    joke.emit("PRINT", text_key("CZFOOD"), integer(3200), integer(1))
    joke.mark("done")
    mission.insert_before(0x75EF, "SUB_VAL_FROM_INT_VAR", joke)


def escape(mission):
    hard = mission.difficulty == "acimasiz"
    mission.replace(0x0247, "SET_VAR_INT", 1, 420000, 360000 if hard else 390000)
    mission.replace(0x0B66, "SET_CHAR_ACCURACY", 1, 20, 34)
    mission.replace(0x020AD, "SET_CHAR_SHOOT_RATE", 1, 20, 31)
    mission.replace(0x03573, "START_SCRIPT_FIRE", 4, 2, 3)
    for offset, old, accuracy in ((0x33B8, 10, 27), (0x33FC, 10, 23), (0x347F, 10, 29), (0x3503, 20, 32)):
        mission.replace(offset, "SET_CHAR_ACCURACY", 1, old, accuracy)
    warning = Snippet(mission.table)
    warning.emit("PRINT_BIG", text_key("CZFIRE"), integer(3700), integer(6))
    mission.insert_before(0x3511, "REMOVE_ALL_SCRIPT_FIRES", warning)
    add_timeout_checkpoint(mission, 0x22F0, "IF", 0x030E, 32028, stage=(132, 4))


def pursuit(mission):
    hard = mission.difficulty == "acimasiz"
    # Keep the grip gauge calibrated to the same number of seconds
    # as the rescue window. Narrow the approach horizontally only.
    seconds = 12 if hard else 13
    mission.replace(0x164C, "SET_VAR_INT", 1, 15, seconds)
    for offset in (0x1665, 0x19D3):
        mission.replace(offset, "DIV_FLOAT_VAR_BY_VAL", 1, 15, seconds)
    for axis in (4, 5):
        mission.replace(0x1841, "LOCATE_CHAR_ANY_MEANS_3D", axis, 3, 2.65 if hard else 2.85)
    ready = Snippet(mission.table)
    ready.emit("PRINT_BIG", text_key("CZRESCU"), integer(3700), integer(6))
    mission.insert_before(0x1677, "DISPLAY_ONSCREEN_COUNTER_WITH_STRING", ready)
    saved = Snippet(mission.table)
    saved.emit("PRINT_BIG", text_key("CZGOOD"), integer(3900), integer(6))
    mission.insert_before(0x3FB9, "PRINT_HELP", saved)

    # Place one disabled sedan beyond the earlier turn. It leaves
    # room on the opposite side and is removed before the handoff.
    mission.reserve_locals(961)
    obstacle = local(961)
    init = Snippet(mission.table)
    init.emit("SET_LVAR_INT", obstacle, integer(-1))
    mission.insert_before(0x0000, "GOSUB", init)
    x, y, z = 2208.6, -1720.8, 13.25
    road = Snippet(mission.table)
    road.emit("IF", integer(1))
    road.emit("HAS_MODEL_LOADED", integer(492))
    road.emit(
        "IS_AREA_OCCUPIED", *map(floating, (x - 2.5, y - 2.5, z - 1, x + 2.5, y + 2.5, z + 2)),
        *map(integer, (0, 1, 1, 1, 0)), negate=True,
    )
    road.jump("GOTO_IF_FALSE", "done")
    road.emit("CREATE_CAR", integer(492), *map(floating, (x, y, z)), obstacle)
    road.emit("SET_CAR_HEADING", obstacle, floating(18.0))
    road.emit("SET_CAR_HEALTH", obstacle, integer(350))
    road.emit("LOCK_CAR_DOORS", obstacle, integer(2))
    road.emit("PRINT_BIG", text_key("CZROAD"), integer(3700), integer(6))
    road.mark("done")
    mission.insert_before(0x28B6, "SET_LVAR_INT", road)
    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_VEHICLE_EXIST", obstacle)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("DELETE_CAR", obstacle)
    cleanup.emit("SET_LVAR_INT", obstacle, integer(-1))
    cleanup.mark("done")
    mission.insert_before(0x376B, "CLEAR_ONSCREEN_COUNTER", cleanup)
    mission.insert_before(0xE0B5, "SET_RAILTRACK_RESISTANCE_MULT", cleanup)

    # Raise the initial and final pursuit waves after Sweet takes over.
    for offset in (0x4787, 0x47F7, 0x4E05):
        mission.replace(offset, "TASK_DRIVE_BY", 9, 80, 89)
    for offset in (0x49B3, 0x4A1C, 0x4E6E, 0x50E4, 0x514D, 0x51B6, 0x52D9):
        mission.replace(offset, "TASK_DRIVE_BY", 9, 60, 72)
    for offset, old, health in ((0x4635, 400, 510), (0x46B0, 600, 720), (0x819E, 700, 775)):
        mission.replace(offset, "SET_CAR_HEALTH", 1, old, health)

    # One passing patrol leaves the area after the original ending.
    mission.insert_before(
        0xE0B3, "RETURN",
        passing_vehicle(mission, 596, 280, (2480.1, -1675.3, 12.8), 90.0, 13.0, caption="CZWRONG"),
    )


PARTS = (entry_and_smoke, escape, pursuit)
