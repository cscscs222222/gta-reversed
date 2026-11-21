from country_race import tune_rival_car


NAME = "Farewell, My Love..."
MISSION_ID = 35


def expand(mission):
    tune_rival_car(mission, 26, 63.0 if mission.difficulty == "acimasiz" else 59.0, 1.06)
