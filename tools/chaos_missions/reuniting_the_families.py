NAME = "Reuniting the Families"
MISSION_ID = 37


def expand(mission):
    # Apply the pressure to the motel squad on both entries.
    for offset, accuracy in (
        (0x25C5, 43), (0x25E5, 47), (0x2629, 42), (0x2649, 48),
        (0x2F58, 46), (0x2F96, 51), (0x2FD4, 48), (0x3012, 55),
        (0x3050, 45), (0x308E, 46), (0x30CC, 43), (0x310A, 50),
    ):
        mission.replace(offset, "SET_CHAR_ACCURACY", 1, 30, accuracy)

    # Keep the escape restart and HUD's original denominator. CJ begins
    # both driving sections with less protection against the pursuit.
    health = 4800 if mission.difficulty == "acimasiz" else 5200
    for offset in (0x8C7E, 0x9A4E):
        mission.replace(offset, "SET_CAR_HEALTH", 1, 6000, health)
    for offset, old, new in ((0x99CB, 40, 52), (0x9D7C, 40, 54), (0xA033, 30, 44), (0xCF31, 30, 43)):
        mission.replace(offset, "TASK_DRIVE_BY", 9, old, new)
