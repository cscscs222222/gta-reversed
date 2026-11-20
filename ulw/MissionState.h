#pragma once

#include <array>
#include <cstdint>

namespace ulw {

using MissionName = std::array<char, 9>;

enum class MissionPhase : std::uint8_t { Loading, FreeRoam, Mission, Cutscene, Activity, Recovery };
enum class MissionProfile : std::uint8_t { General, TrainChase, ScriptedPursuit, RemoteFlight, Finale };

struct MissionObservation {
    std::uint32_t now{};
    bool scriptReady{};
    bool missionFlag{};
    bool missionThread{};
    bool cutscene{};
    bool activity{};
    MissionName name{};
};

class MissionState {
public:
    bool Update(const MissionObservation& observation) noexcept;
    bool AllowsWorld() const noexcept { return m_Phase == MissionPhase::FreeRoam; }
    MissionPhase Phase() const noexcept { return m_Phase; }
    MissionProfile Profile() const noexcept { return m_Profile; }
    const MissionName& Name() const noexcept { return m_Name; }

private:
    MissionPhase m_Phase{MissionPhase::Loading};
    MissionProfile m_Profile{MissionProfile::General};
    MissionName m_Name{};
    std::uint32_t m_LastUpdate{};
    std::uint32_t m_LastProtected{};
    std::uint32_t m_SettleTime{};
};

MissionName NormalizeMissionName(const std::array<char, 8>& scriptName) noexcept;
const char* MissionProfileName(MissionProfile profile) noexcept;

}
