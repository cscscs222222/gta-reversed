from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Badlands"
MISSION_ID = 39


def expand(mission):
    # Distinguish two federal escorts from the rest of the team. They keep
    # their original patrols and only fight when the mission instructs them.
    for offset, slot, accuracy, rate in (
        (0x1DC, 291, 56, 42),
        (0x256, 293, 46, 37),
    ):
        code = Snippet(mission.table)
        code.emit("SET_CHAR_ACCURACY", local(slot), integer(accuracy))
        code.emit("SET_CHAR_SHOOT_RATE", local(slot), integer(rate))
        code.emit("SET_SENSE_RANGE", local(slot), floating(48.0))
        mission.insert_before(offset, "CREATE_CHAR", code)

    pace = 58.0 if mission.difficulty == "acimasiz" else 54.0
    mission.replace(0x2719, "TASK_CAR_DRIVE_WANDER", 2, 50.0, pace)
    mission.replace(0x2728, "SET_CAR_CRUISE_SPEED", 1, 50.0, pace)

    # At the original vehicle escape, a lookalike takes off on foot.
    # The photographed target is still the man with the original blip.
    mission.reserve_locals(963)
    decoy = local(963)
    setup = Snippet(mission.table)
    setup.emit("SET_LVAR_INT", decoy, integer(-1))
    mission.insert_before(0x000B, "GOSUB", setup)
    flight = Snippet(mission.table)
    flight.emit("IF", integer(1))
    flight.emit("HAS_MODEL_LOADED", integer(147))
    flight.emit("IS_AREA_OCCUPIED", *map(floating, (-2800.9, -1522.3, 136.8, -2799.3, -1520.7, 139.8)),
                *map(integer, (0, 1, 1, 1, 0)), negate=True)
    flight.jump("GOTO_IF_FALSE", "done")
    flight.emit("CREATE_CHAR", integer(4), integer(147), *map(floating, (-2800.11, -1521.49, 137.37)), decoy)
    flight.emit("TASK_SMART_FLEE_CHAR", decoy, global_var(12), floating(52.0), integer(10000))
    flight.emit("PRINT_BIG", text_key("CZPHOTO"), integer(3800), integer(6))
    flight.mark("done")
    mission.insert_before(0x1520, "SET_VAR_INT", flight)
    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_CHAR_EXIST", decoy)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("MARK_CHAR_AS_NO_LONGER_NEEDED", decoy)
    cleanup.mark("done")
    mission.insert_before(0x0026, "GOSUB", cleanup)
