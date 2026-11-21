from scm import Snippet, integer, text_key


NAME = "Don Peyote"
MISSION_ID = 88


def expand(mission):
    # One truck moves to block CJ. The other closes from farther back.
    # Both roles keep tracking CJ even if the passengers change cars.
    hard = mission.difficulty == "acimasiz"
    for task, pace, reset, speed, mode in (
        (0x8433, 0x892B, 0x89ED, 45.0 if hard else 42.0, 5),
        (0x84F5, 0x8A1C, 0x8ADE, 41.0 if hard else 38.0, 43),
    ):
        mission.replace(task, "TASK_CAR_MISSION", 3, 3, mode)
        mission.replace(task, "TASK_CAR_MISSION", 4, 35, speed)
        mission.replace(pace, "SET_CAR_CRUISE_SPEED", 1, 35, speed)
        mission.replace(reset, "SET_CAR_MISSION", 1, 3, mode)
    alert = Snippet(mission.table)
    alert.emit("PRINT_BIG", text_key("CZDESRT"), integer(3600), integer(6))
    mission.insert_before(0x8458, "SET_LVAR_INT", alert)
