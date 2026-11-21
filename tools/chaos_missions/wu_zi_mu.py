from country_race import tune_rival_car


NAME = "Wu Zi Mu"
MISSION_ID = 35


def expand(mission):
    tune_rival_car(mission, 25, 60.0 if mission.difficulty == "acimasiz" else 57.0, 1.035)
