from scm import Snippet, integer, text_key


NAME = "Vertical Bird"
MISSION_ID = 103


def expand(mission):
    mission.replace(0x6893, "SET_CHAR_ACCURACY", 1, 70, 77)
    mission.replace(0x6BBB, "SET_CHAR_ACCURACY", 1, 70, 79)
    mission.replace(0x689E, "SET_CHAR_SHOOT_RATE", 1, 70, 74)
    mission.replace(0x6BC6, "SET_CHAR_SHOOT_RATE", 1, 70, 76)
    mission.replace(0xB57A, "PLANE_FLY_IN_DIRECTION", 1, 190, 202)

    # Disperse the three existing interceptors by altitude, heading
    # and following distance when CJ starts the escape in the Hydra.
    hard = mission.difficulty == "acimasiz"
    for position, angle, follow, altitude, heading, radius in (
        (0x47D8, 0x47F0, 0x482E, 89.0, 18.0, 85.0),
        (0x488D, 0x48A5, 0x48E3, 113.0, 332.0, 57.0 if hard else 62.0),
        (0x4942, 0x495A, 0x4998, 136.0, 310.0, 73.0),
    ):
        original_z = mission.instruction(position, "SET_CAR_COORDINATES").args[3]
        mission.replace(position, "SET_CAR_COORDINATES", 3, original_z, altitude)
        mission.replace(angle, "SET_CAR_HEADING", 1, 0, heading)
        mission.replace(follow, "PLANE_FOLLOW_ENTITY", 3, 70, radius)
    alert = Snippet(mission.table)
    alert.emit("PRINT_BIG", text_key("CZHYDRA"), integer(4000), integer(6))
    mission.insert_before(0x47A7, "SET_LVAR_INT", alert)
