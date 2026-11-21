NAME = "Madd Dogg's Rhymes"
MISSION_ID = 32


def expand(mission):
    # Reverse two patrol circuits. Keep the walkable corners and the
    # animations associated with each corner.
    for stops in ((0x0DD0, 0x0DED, 0x0E0E, 0x0E2B), (0x1EB8, 0x1ED5, 0x1EF6, 0x1F13)):
        route = [mission.instruction(stop, "EXTEND_PATROL_ROUTE").args for stop in stops]
        for offset, old, new in zip(stops, route, (route[2], route[1], route[0], route[3]), strict=True):
            if old[3:] != new[3:]:
                raise ValueError("Patrol point animation does not match")
            for i, (old_coordinate, new_coordinate) in enumerate(zip(old[:3], new[:3], strict=True)):
                if old_coordinate != new_coordinate:
                    mission.replace(offset, "EXTEND_PATROL_ROUTE", i, old_coordinate, new_coordinate)

    # A guard can alert the neighboring patrol when he sees CJ.
    for offset, old, radius in ((0x1636, 5.0, 9.5), (0x16A7, 5.0, 10.0), (0x1A9F, 10.0, 14.0), (0x1B0B, 5.0, 10.0)):
        mission.replace(offset, "SET_INFORM_RESPECTED_FRIENDS", 1, old, radius)
