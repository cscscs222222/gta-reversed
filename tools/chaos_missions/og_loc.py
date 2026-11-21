from scm import Snippet, floating, local


NAME = "OG Loc"
MISSION_ID = 27


def expand(mission):
    # Freddy's recorded turns advance at different speeds. Leave the
    # segments with synchronized crossing traffic at their stock pace.
    wait = 2600 if mission.difficulty == "acimasiz" else 3150
    for offset in (0x2913, 0x2D80, 0x2FD4):
        mission.replace(offset, "IS_INT_LVAR_GREATER_THAN_NUMBER", 1, 4000, wait)

    for start, speed in ((0x25B2, 1.12), (0x2960, 1.09), (0x2D8F, 1.15)):
        recording = mission.instruction(start, "START_PLAYBACK_RECORDED_CAR")
        next_command = mission.instructions[recording.end]
        code = Snippet(mission.table)
        code.emit("SET_PLAYBACK_SPEED", local(35), floating(speed))
        mission.insert_before(next_command.start, next_command.name, code)
