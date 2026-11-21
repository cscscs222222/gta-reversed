from scene_vehicles import passing_vehicle, streaming_commands


NAME = "King in Exile"
MISSION_ID = 45


def expand(mission):
    # Optional tourist sighting after Cesar's call.
    mission.insert_before(
        0x01B9, "REQUEST_MODEL",
        streaming_commands(mission, "REQUEST_MODEL", 483, 147),
    )
    mission.insert_before(
        0x032D, "REMOVE_BLIP",
        passing_vehicle(mission, 483, 147, (-2035.0, -2529.8, 29.625), 284.27, 13.0, caption="CZTOUR"),
    )
    mission.insert_before(
        0x039E, "MARK_MODEL_AS_NO_LONGER_NEEDED",
        streaming_commands(mission, "MARK_MODEL_AS_NO_LONGER_NEEDED", 483, 147),
    )
