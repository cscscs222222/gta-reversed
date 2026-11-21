from scm import Snippet, global_var, integer, local, text_key


def add_timeout_checkpoint(mission, capture_offset, capture_command, timeout_if, timer, stage=None, *, earlier=None):
    """Allow one player rewind per timed section with a recorded position."""
    slots = tuple(range(954, 964))
    mission.reserve_locals(*slots)
    used, x, y, z, heading, health, armour, area, now_area, now_armour = map(local, slots)
    player, cj = global_var(8), global_var(12)
    timer_var = global_var(timer)
    state = None if stage is None else (local(stage[0]), integer(stage[1]))

    def snapshot(new_section=False):
        capture = Snippet(mission.table)
        if new_section:
            # Never reuse a snapshot from an earlier timer if CJ cannot
            # take a new on-foot snapshot at this transition.
            capture.emit("SET_LVAR_INT", used, integer(0))
        capture.emit("IF", integer(3 if state else 2))
        capture.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", used, integer(0))
        capture.emit("IS_CHAR_DEAD", cj, negate=True)
        capture.emit("IS_CHAR_IN_ANY_CAR", cj, negate=True)
        if state:
            capture.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", *state)
        capture.jump("GOTO_IF_FALSE", "done")
        capture.emit("GET_CHAR_COORDINATES", cj, x, y, z)
        capture.emit("GET_CHAR_HEADING", cj, heading)
        capture.emit("GET_CHAR_HEALTH", cj, health)
        capture.emit("GET_CHAR_ARMOUR", cj, armour)
        capture.emit("GET_AREA_VISIBLE", area)
        capture.emit("SET_LVAR_INT", used, integer(1))
        capture.mark("done")
        return capture

    if earlier:
        earlier_offset, earlier_command = earlier
        mission.insert_before(earlier_offset, earlier_command, snapshot())
    mission.insert_before(capture_offset, capture_command, snapshot(new_section=earlier is not None))

    retry = Snippet(mission.table)
    retry.emit("IF", integer(5 if state else 4))
    retry.emit("IS_NUMBER_GREATER_OR_EQUAL_TO_INT_VAR", integer(0), timer_var)
    retry.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", used, integer(1))
    retry.emit("IS_PLAYER_PLAYING", player)
    retry.emit("IS_CHAR_DEAD", cj, negate=True)
    retry.emit("IS_CHAR_IN_ANY_CAR", cj, negate=True)
    if state:
        retry.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", *state)
    retry.jump("GOTO_IF_FALSE", "done")
    retry.emit("GET_AREA_VISIBLE", now_area)
    retry.emit("IF", integer(0))
    retry.emit("IS_INT_LVAR_EQUAL_TO_INT_LVAR", area, now_area)
    retry.jump("GOTO_IF_FALSE", "done")

    # The original mission and its active entities stay alive. Only CJ
    # returns to the earlier spot inside the same stage and interior.
    retry.emit("SET_LVAR_INT", used, integer(2))
    retry.emit("SET_VAR_INT", timer_var, integer(65000 if mission.difficulty == "acimasiz" else 90000))
    retry.emit("SET_PLAYER_CONTROL", player, integer(0))
    retry.emit("DO_FADE", integer(250), integer(0))
    retry.emit("WAIT", integer(300))
    retry.emit("CLEAR_CHAR_TASKS_IMMEDIATELY", cj)
    retry.emit("SET_CHAR_COORDINATES", cj, x, y, z)
    retry.emit("SET_CHAR_HEADING", cj, heading)
    retry.emit("SET_CHAR_HEALTH", cj, health)
    retry.emit("GET_CHAR_ARMOUR", cj, now_armour)
    retry.emit("IF", integer(0))
    retry.emit("IS_INT_LVAR_GREATER_THAN_INT_LVAR", armour, now_armour)
    retry.jump("GOTO_IF_FALSE", "camera")
    retry.emit("SUB_INT_LVAR_FROM_INT_LVAR", armour, now_armour)
    retry.emit("ADD_ARMOUR_TO_CHAR", cj, armour)
    retry.mark("camera")
    retry.emit("SET_CAMERA_BEHIND_PLAYER")
    retry.emit("RESTORE_CAMERA_JUMPCUT")
    retry.emit("DO_FADE", integer(350), integer(1))
    retry.emit("SET_PLAYER_CONTROL", player, integer(1))
    retry.emit("PRINT_HELP", text_key("CZRETRY"))
    retry.mark("done")
    mission.insert_before(timeout_if, "IF", retry)
