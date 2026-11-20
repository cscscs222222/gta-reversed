#include "MissionState.h"

#include <algorithm>
#include <string_view>

namespace ulw {
namespace {

MissionProfile ProfileFor(const MissionName& name) noexcept {
    const std::string_view thread{name.data()};
    if (thread == "SMOKE3") {
        return MissionProfile::TrainChase;
    }
    if (thread == "DRUGS1") {
        return MissionProfile::ScriptedPursuit;
    }
    if (thread == "ZERO2") {
        return MissionProfile::RemoteFlight;
    }
    if (thread == "FINALEA" || thread == "FINALEB" || thread == "FINALEC") {
        return MissionProfile::Finale;
    }
    return MissionProfile::General;
}

std::uint32_t MissionSettleTime(MissionProfile profile) noexcept {
    switch (profile) {
    case MissionProfile::TrainChase:
    case MissionProfile::ScriptedPursuit:
    case MissionProfile::RemoteFlight:
        return 2100;
    case MissionProfile::Finale:
        return 2600;
    default:
        return 1400;
    }
}

}

MissionName NormalizeMissionName(const std::array<char, 8>& scriptName) noexcept {
    MissionName name{};
    for (std::size_t i = 0; i < scriptName.size() && scriptName[i]; ++i) {
        char c = scriptName[i];
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
            break;
        }
        name[i] = c;
    }
    if (std::string_view{name.data()} == "NONAME") {
        return {};
    }
    return name;
}

const char* MissionProfileName(MissionProfile profile) noexcept {
    switch (profile) {
    case MissionProfile::TrainChase: return "train chase";
    case MissionProfile::ScriptedPursuit: return "scripted pursuit";
    case MissionProfile::RemoteFlight: return "remote flight";
    case MissionProfile::Finale: return "finale";
    default: return "standard";
    }
}

bool MissionState::Update(const MissionObservation& observation) noexcept {
    const auto old = m_Phase;
    const auto previousName = m_Name;
    if (observation.now < m_LastUpdate) {
        *this = {};
    }
    m_LastUpdate = observation.now;

    if (!observation.scriptReady) {
        m_Phase = MissionPhase::Loading;
        m_Name = {};
        m_Profile = MissionProfile::General;
        m_SettleTime = 0;
    } else {
        const bool mission = observation.missionFlag || observation.missionThread;
        if (mission || observation.cutscene || observation.activity) {
            if (mission && observation.name[0]) {
                m_Name = observation.name;
                m_Profile = ProfileFor(m_Name);
            }
            m_Phase = observation.cutscene ? MissionPhase::Cutscene : mission ? MissionPhase::Mission : MissionPhase::Activity;
            m_LastProtected = observation.now;
            m_SettleTime = std::max(m_SettleTime, mission ? MissionSettleTime(m_Profile) : 900u);
        } else if (m_SettleTime && observation.now - m_LastProtected < m_SettleTime) {
            m_Phase = MissionPhase::Recovery;
        } else {
            m_Phase = MissionPhase::FreeRoam;
            m_Name = {};
            m_Profile = MissionProfile::General;
            m_SettleTime = 0;
        }
    }
    return old != m_Phase || previousName != m_Name;
}

}
