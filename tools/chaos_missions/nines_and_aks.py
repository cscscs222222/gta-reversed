from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Nines and AK's"
MISSION_ID = 16


def expand(mission):
    # Leave the practice retries and story rewards in place. Limit the
    # starting supply and tighten the two small target-practice allocations.
    mission.replace(0x0C49, "GIVE_WEAPON_TO_CHAR", 2, 30000, 60)
    mission.replace(0x2355, "SET_CHAR_AMMO", 2, 10, 8)
    mission.replace(0x23C2, "GIVE_WEAPON_TO_CHAR", 2, 10, 8)
    mission.replace(0x31AC, "SET_CHAR_AMMO", 2, 10, 8)

    # Move the five bottles in the player round. The stock demonstration,
    # scoring, and retry code still uses the original objects and handles.
    mission.reserve_locals(*range(959, 964))
    phase_start, now, elapsed, sway, destination = map(local, range(959, 964))
    setup = Snippet(mission.table)
    setup.emit("GET_GAME_TIMER", phase_start)
    setup.emit("SET_LVAR_FLOAT", sway, floating(0.85 if mission.difficulty == "acimasiz" else 0.6))
    setup.emit("PRINT_BIG", text_key("CZAIM"), integer(3700), integer(6))
    mission.insert_before(0x2243, "SET_LVAR_INT", setup)

    move = Snippet(mission.table)
    move.emit("GET_GAME_TIMER", now)
    move.emit("SET_LVAR_INT_TO_LVAR_INT", elapsed, now)
    move.emit("SUB_INT_LVAR_FROM_INT_LVAR", elapsed, phase_start)
    move.emit("IF", integer(0))
    move.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", elapsed, integer(1350))
    move.jump("GOTO_IF_FALSE", "targets")
    move.emit("SET_LVAR_INT_TO_LVAR_INT", phase_start, now)
    move.emit("MULT_FLOAT_LVAR_BY_VAL", sway, floating(-1.0))
    move.mark("targets")
    for index in range(5):
        bottle = global_var(7016 + 4 * index)
        move.emit("IF", integer(0))
        move.emit("DOES_OBJECT_EXIST", bottle)
        move.jump("GOTO_IF_FALSE", f"next_{index}")
        move.emit("SET_LVAR_FLOAT_TO_LVAR_FLOAT", destination, local(94 + index))
        move.emit("ADD_FLOAT_LVAR_TO_FLOAT_LVAR", destination, sway)
        move.emit(
            "SLIDE_OBJECT", bottle, local(89 + index), destination, local(99 + index),
            floating(0.02), floating(0.022 + 0.004 * (index % 3)), floating(0.02), integer(0),
        )
        move.mark(f"next_{index}")
    mission.insert_before(0x2267, "IF", move)
