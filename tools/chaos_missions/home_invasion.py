from scm import Snippet, floating, global_var, integer, local, text_key


NAME = "Home Invasion"
MISSION_ID = 24


def expand(mission):
    # Tighten the existing interior noise mechanic while keeping gradual
    # decay and making the original warning appear sooner.
    mission.replace(0x438D, "IS_FLOAT_LVAR_GREATER_THAN_NUMBER", 1, 20.0, 17.0)
    mission.replace(0x439E, "SUB_VAL_FROM_FLOAT_LVAR", 1, 20.0, 17.0)
    mission.replace(0x5B44, "IS_INT_VAR_GREATER_THAN_NUMBER", 1, 30, 24)

    cj = global_var(12)
    height, sample, noise = map(local, (162, 166, 167))
    mission.reserve_locals(959, 960, 961, 962)
    state, started, elapsed, marker = map(local, range(959, 963))
    clock = tuple(map(floating, (2809.0, -1168.0, 1024.57)))
    loud_floor_z = floating(1026.6)

    setup = Snippet(mission.table)
    setup.emit("SET_LVAR_INT", marker, integer(-1))
    mission.insert_before(0x000B, "GOSUB", setup)

    floor = Snippet(mission.table)
    floor.emit("IF", integer(0))
    floor.emit("IS_FLOAT_LVAR_GREATER_THAN_NUMBER", height, loud_floor_z)
    floor.jump("GOTO_IF_FALSE", "done")
    floor.emit("MULT_FLOAT_LVAR_BY_VAL", sample, floating(1.24 if mission.difficulty == "acimasiz" else 1.16))
    floor.mark("done")
    mission.insert_before(0x42E3, "GOTO", floor)

    # Trigger once on the upper floor. Feed the alarm into the native
    # noise meter so the existing wake-up and pursuit still decide the outcome.
    alarm = Snippet(mission.table)
    alarm.emit("IF", integer(3))
    alarm.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", state, integer(0))
    alarm.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", local(171), integer(0))
    alarm.emit("IS_FLOAT_LVAR_GREATER_THAN_NUMBER", height, loud_floor_z)
    alarm.emit("IS_FLOAT_LVAR_GREATER_THAN_NUMBER", noise, floating(22.0))
    alarm.jump("GOTO_IF_FALSE", "check")
    alarm.emit("SET_LVAR_INT", state, integer(1))
    alarm.emit("GET_GAME_TIMER", started)
    alarm.emit("ADD_BLIP_FOR_COORD", *clock, marker)
    alarm.emit("SET_BLIP_AS_FRIENDLY", marker, integer(1))
    alarm.emit("REPORT_MISSION_AUDIO_EVENT_AT_POSITION", *clock, integer(19600))
    alarm.emit("PRINT_BIG", text_key("CZCLOCK"), integer(4100), integer(6))
    alarm.mark("check")
    alarm.emit("IF", integer(0))
    alarm.emit("IS_INT_LVAR_EQUAL_TO_NUMBER", state, integer(1))
    alarm.jump("GOTO_IF_FALSE", "done")
    alarm.emit("IF", integer(0))
    alarm.emit("LOCATE_CHAR_ON_FOOT_3D", cj, *clock, floating(1.55), floating(1.55), floating(1.6), integer(1))
    alarm.jump("GOTO_IF_FALSE", "countdown")
    alarm.emit("PRINT_BIG", text_key("CZQUIET"), integer(2300), integer(6))
    alarm.jump("GOTO", "remove")
    alarm.mark("countdown")
    alarm.emit("GET_GAME_TIMER", elapsed)
    alarm.emit("SUB_INT_LVAR_FROM_INT_LVAR", elapsed, started)
    alarm.emit("IF", integer(0))
    alarm.emit("IS_INT_LVAR_GREATER_THAN_NUMBER", elapsed, integer(9000 if mission.difficulty == "acimasiz" else 11000))
    alarm.jump("GOTO_IF_FALSE", "done")
    alarm.emit("REPORT_MISSION_AUDIO_EVENT_AT_POSITION", *clock, integer(19600))
    alarm.emit("ADD_VAL_TO_FLOAT_LVAR", noise, floating(100.0))
    alarm.mark("remove")
    alarm.emit("SET_LVAR_INT", state, integer(2))
    alarm.emit("REMOVE_BLIP", marker)
    alarm.emit("SET_LVAR_INT", marker, integer(-1))
    alarm.mark("done")
    mission.insert_before(0x43BA, "CSET_VAR_INT_TO_LVAR_FLOAT", alarm)

    cleanup = Snippet(mission.table)
    cleanup.emit("IF", integer(0))
    cleanup.emit("DOES_BLIP_EXIST", marker)
    cleanup.jump("GOTO_IF_FALSE", "done")
    cleanup.emit("REMOVE_BLIP", marker)
    cleanup.emit("SET_LVAR_INT", marker, integer(-1))
    cleanup.mark("done")
    mission.insert_before(0x5F9D, "CLEAR_ONSCREEN_COUNTER", cleanup)
    mission.insert_before(0x0026, "GOSUB", cleanup)
