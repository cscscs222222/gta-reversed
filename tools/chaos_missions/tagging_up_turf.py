from scene_vehicles import streaming_commands
from scm import Snippet, floating, global_var, integer, local


NAME = "Tagging Up Turf"
MISSION_ID = 13


def expand(mission):
    mission.reserve_locals(961, 962, 963)
    reinforcement, variant, sequence = map(local, (961, 962, 963))
    cj = global_var(12)
    mission.insert_before(0x0490, "LOAD_ALL_MODELS_NOW", streaming_commands(mission, "REQUEST_MODEL", 346))
    mission.insert_before(
        0x39F6, "MARK_MODEL_AS_NO_LONGER_NEEDED",
        streaming_commands(mission, "MARK_MODEL_AS_NO_LONGER_NEEDED", 346),
    )
    code = Snippet(mission.table)
    code.emit("IF", integer(0))
    code.emit("HAS_MODEL_LOADED", integer(102))
    code.jump("GOTO_IF_FALSE", "done")
    code.emit(
        "CREATE_CHAR", integer(25), integer(102),
        floating(2394.1), floating(-1471.7), floating(22.99), reinforcement,
    )
    code.emit("SET_CHAR_HEADING", reinforcement, floating(270.0))
    code.emit("SET_CHAR_MAX_HEALTH", reinforcement, integer(140))
    code.emit("SET_CHAR_HEALTH", reinforcement, integer(140))
    code.emit("IF", integer(0))
    code.emit("HAS_MODEL_LOADED", integer(346))
    code.jump("GOTO_IF_FALSE", "choose_approach")
    code.emit("GIVE_WEAPON_TO_CHAR", reinforcement, integer(22), integer(20))
    code.emit("SET_CURRENT_CHAR_WEAPON", reinforcement, integer(22))
    code.emit("SET_CHAR_DROPS_WEAPONS_WHEN_DEAD", reinforcement, integer(0))
    code.emit("SET_CHAR_ACCURACY", reinforcement, integer(48 if mission.difficulty == "acimasiz" else 40))
    code.emit("SET_CHAR_SHOOT_RATE", reinforcement, integer(45))
    code.mark("choose_approach")
    code.emit("GENERATE_RANDOM_INT_IN_RANGE", integer(0), integer(3), variant)
    code.emit("IF", integer(0))
    code.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", variant, integer(0))
    code.jump("GOTO_IF_FALSE", "rush")

    # The lookout either attacks at once or ducks before opening fire.
    code.emit("TASK_KILL_CHAR_ON_FOOT", reinforcement, cj)
    code.jump("GOTO", "done")
    code.mark("rush")
    code.emit("OPEN_SEQUENCE_TASK", sequence)
    code.emit("TASK_DUCK", integer(-1), integer(650))
    code.emit("TASK_KILL_CHAR_ON_FOOT", integer(-1), cj)
    code.emit("CLOSE_SEQUENCE_TASK", sequence)
    code.emit("PERFORM_SEQUENCE_TASK", reinforcement, sequence)
    code.emit("CLEAR_SEQUENCE_TASK", sequence)
    code.mark("done")
    # This is entered only once, after the tag encounter turns hostile.
    mission.insert_before(0x2669, "IF", code)
