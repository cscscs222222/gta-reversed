#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "IntelligenceCore.h"
#include "MissionState.h"
#include "PoliceInteractions.h"
#include "StreetSystems.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <string>

namespace {

static_assert(sizeof(void*) == 4, "The GTA San Andreas adapter requires an x86 build");
static_assert(sizeof(ulw::Point) == 12);

constexpr std::uintptr_t ImageBase = 0x400000;
constexpr std::uintptr_t ShotDispatchCall = 0x4C39AC;
constexpr std::uintptr_t NativeShotResponse = 0x4BC710;
constexpr std::uintptr_t NativeFireResponse = 0x4BBFB0;
constexpr std::uintptr_t NativeDamageResponse = 0x4C0170;
constexpr std::uintptr_t NativeGunTask = 0x62A380;
constexpr std::uintptr_t GunTaskVtable = 0x86D724;
constexpr std::uintptr_t NativeArmedControl = 0x62CCE0;
constexpr std::uintptr_t NativeArmedNext = 0x62C190;
constexpr std::uintptr_t ArmedTaskVtable = 0x86D918;
constexpr std::uintptr_t GunControlVtable = 0x86D76C;
constexpr std::uintptr_t CarPathLinks = 0x96F974;
constexpr std::uintptr_t CarPathLinkCounts = 0x970354;
constexpr std::uintptr_t NativeArrestControl = 0x68D350;
constexpr std::uintptr_t ArrestTaskVtable = 0x8709A8;
constexpr std::uintptr_t KeyboardState = 0xB73190;
constexpr std::uintptr_t PedPool = 0xB74490;
constexpr std::uintptr_t TaskPool = 0xB744A8;
constexpr std::uintptr_t VehiclePool = 0xB74494;
constexpr std::uintptr_t PoliceHelicopters = 0xC1C964;
constexpr std::uintptr_t ActiveCamera = 0xB6F028;
constexpr std::uintptr_t ScriptSpace = 0xA49960;
constexpr std::uintptr_t MissionFlagOffset = 0xA476AC;
constexpr std::size_t MainScriptLimit = 200000;
constexpr std::size_t MissionScriptLimit = 69000;
constexpr std::uintptr_t ActiveScriptList = 0xA8B42C;
constexpr std::uintptr_t ScriptPool = 0xA8B430;
constexpr std::size_t ScriptSlotSize = 0xE0;
constexpr std::size_t ScriptCount = 96;
constexpr std::uintptr_t StandardWanderVtable = 0x85A200;
constexpr std::uint32_t Walking = 4;
constexpr std::uint32_t Jogging = 5;
constexpr std::uint32_t Running = 6;

using ShotResponse = void(__thiscall*)(void* handler, const void* shot, void* activeTask, void* simplestTask);
using DamageResponse = void(__thiscall*)(void*, const void*, void*, void*, void*);
using GunTask = bool(__thiscall*)(void*, void*);
using ArmedControl = void*(__thiscall*)(void*, void*);
using ArmedSubTask = void*(__thiscall*)(void*, unsigned, void*);
using PedReference = int(__cdecl*)(const void* ped);
using PedFromReference = const void*(__cdecl*)(int);
using HelpMessage = void(__cdecl*)(const char*, bool, bool, bool);
using AbortTask = bool(__thiscall*)(void*, void*, int, const void*);
using TaskType = unsigned(__thiscall*)(const void*);
using LineClear = bool(__cdecl*)(const ulw::Point&, const ulw::Point&, bool, bool, bool, bool, bool, bool, bool);
struct CollisionPoint {
    ulw::Point position;
    std::array<std::byte, 0x20> fields;
};
static_assert(sizeof(CollisionPoint) == 0x2C);
using TraceLine = bool(__cdecl*)(const ulw::Point&, const ulw::Point&, CollisionPoint&, void*&,
    bool, bool, bool, bool, bool, bool, bool, bool);
using GroundHeight = float(__cdecl*)(ulw::Point, bool*, void**);
using ReserveCover = void*(__cdecl*)(void*, const ulw::Point&, bool);
using CoverCoordinates = bool(__cdecl*)(const void*, void*, const ulw::Point&, ulw::Point&);
using ReleaseCover = void(__thiscall*)(void*, void*);
using RunToPoint = void*(__thiscall*)(void*, unsigned, const ulw::Point&, float, bool, bool);
using StandStill = void*(__thiscall*)(void*, int, bool, bool, float);
using ReleasePedCover = void(__thiscall*)(void*);
using AllocateTask = void*(__cdecl*)(std::size_t);
using TurnToPoint = void*(__thiscall*)(void*, const ulw::Point&, float, float);
using FleePoint = void*(__thiscall*)(void*, const ulw::Point&, bool, float, int);
using SeekPoint = ulw::Point*(__thiscall*)(void*, ulw::Point*, const void*);
using WantedPointer = const void*(__cdecl*)(int);
using TrafficSpeed = float(__cdecl*)(void*);
using TrafficPair = void(__cdecl*)(const void*, void*, float*, float);
using MissionSteering = void(__cdecl*)(void*, float*, float*, float*, bool*);
using CarAI = void(__cdecl*)(void*);
using CarHorn = void(__thiscall*)(void*);
using FlyHelicopter = void(__cdecl*)(void*, float, float, bool);
using HeadingForOffset = float(__cdecl*)(float, float);
using AddSearchLight = void(__cdecl*)(const ulw::Point&, const ulw::Point&, float, float, std::uint32_t, std::uint8_t, std::uint8_t);
using RoadblockNodeInfo = bool(__cdecl*)(std::uint32_t, float*, ulw::Point*);
using PlayerVehicle = const void*(__cdecl*)(int, bool);
using ChaseNode = bool(__cdecl*)(void*, float, float, float, const void*);
using BlockSteering = void(__cdecl*)(void*, const void*, float, float, float, float, float*, float*, float*, bool*);
using PopulationUpdate = void(__cdecl*)(bool);
using RandomCars = void(__cdecl*)();
using ProcessScripts = void(__cdecl*)();
using AddAttractorPed = bool(__cdecl*)(int, const void*, ulw::Point, void*, int);
using WanderControl = void*(__thiscall*)(void*, void*);
using FootpathBlocked = bool(__thiscall*)(void*, void*, const std::uint32_t*);
using FindChatPartner = bool(__thiscall*)(void*, void*);
using PedLookHeading = void(__thiscall*)(void*, float, bool, bool);
using PedLookTimer = void(__thiscall*)(void*, std::uint32_t);
using FireWeapon = bool(__thiscall*)(void*, void*, const void*, const void*, const void*, const void*, const void*);
using ProcessWheel = void(__thiscall*)(void*, void*, void*, void*, void*, int, float, float, float,
    std::int8_t, float*, std::int32_t*, std::uint16_t);
using ProcessBikeWheel = void(__thiscall*)(void*, void*, void*, void*, void*, int, float, float, float, float,
    std::int8_t, float*, std::int32_t*, std::int32_t, std::uint16_t);

ulw::IntelligenceCore s_Core{};
ulw::MissionState s_Missions{};
ulw::PoliceSearch s_Police{};
ulw::PatrolTeam s_PatrolTeam{};
ulw::PoliceIncidents s_PoliceIncidents{};
ulw::DrivingSuspicion s_DrivingSuspicion{};
ulw::SurrenderInput s_Surrender{};
struct SurrenderApproach {
    std::uint32_t officer{};
    std::uintptr_t parent{};
    std::uintptr_t child{};
};
SurrenderApproach s_SurrenderApproach{};
std::optional<std::uint32_t> s_LastPoliceHint{};
ulw::TrafficHorns s_TrafficHorns{};
ulw::RoadblockAttempts s_Roadblocks{};
ulw::CrashScenes s_Crashes{};
ulw::CrashWitnesses s_CrashWitnesses{};
ulw::CombatPressure s_CombatPressure{};
ulw::CombatManeuvers s_CombatManeuvers{};
ulw::GangLosses s_GangLosses{};
ulw::RoadPhysics s_RoadPhysics{};
std::array<ulw::EmergencyUnit, 16> s_EmergencyUnits{};
struct TrafficPass {
    const void* car{};
    ulw::DriverProfile driver{};
    float wet{};
};
TrafficPass s_TrafficPass{};
struct AirObservation {
    std::uint32_t id{};
    std::uint32_t at{};
    ulw::SearchTarget search{};
    bool observed{};
};
std::array<AirObservation, 2> s_AirPatrols{};
std::filesystem::path s_LogPath;
std::array<unsigned, 4> s_Reactions{};
std::array<unsigned, 6> s_SearchStages{};
unsigned s_RestrictedDrivers{};
unsigned s_YieldingDrivers{};
unsigned s_CrashSlowdowns{};
unsigned s_PatrolRoutes{};
unsigned s_AttractorUses{};
unsigned s_AirSearches{};
unsigned s_RoadblockAttempts{};
unsigned s_CrashGlances{};
unsigned s_CrashReroutes{};
unsigned s_EmergencyApproaches{};
unsigned s_ExtendedBursts{};
unsigned s_ShorterBursts{};
unsigned s_SuppressionHits{};
unsigned s_CoverSearches{};
unsigned s_CoverMoves{};
unsigned s_CoverHolds{};
unsigned s_CoverPeeks{};
unsigned s_Withdrawals{};
unsigned s_AllyWaits{};
unsigned s_GangCasualties{};
unsigned s_InterceptRoutes{};
unsigned s_PatrolBraking{};
unsigned s_BendLimits{};
unsigned s_FollowingLimits{};
unsigned s_QueueHornSkips{};
unsigned s_PoliceReports{};
unsigned s_SuspicionWarnings{};
unsigned s_InvestigationRoutes{};
unsigned s_VoluntaryArrests{};
unsigned s_AllocationFailures{};
std::uint32_t s_LastLogTime{};
std::uint32_t s_LastGameTime{};
bool s_Diagnostics{};
bool s_WasActive{};
bool s_StreetActive{};
bool s_UseSightingForRoadblocks{};
bool s_MissionObserver{};
bool s_ExecutingScripts{};
bool s_CrashGlancesEnabled{};
bool s_PressureObserver{};
bool s_CoverFollowup{};
bool s_FollowingTraffic{};
bool s_BendTraffic{};
bool s_PoliceWitnesses{};
bool s_PoliceCruise{};
bool s_PoliceHud{};
bool s_CivilianShots{};
std::uint32_t s_StreetTime{};

template<typename T>
T Read(const void* object, std::size_t offset = 0) noexcept {
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
    return value;
}

template<typename T>
T GameValue(std::uintptr_t address) noexcept {
    return Read<T>(reinterpret_cast<const void*>(address));
}

template<typename T>
T GameFunction(std::uintptr_t address) noexcept {
    return reinterpret_cast<T>(address);
}

void Log(const char* message) noexcept {
    if (s_LogPath.empty()) {
        OutputDebugStringA(message);
        return;
    }
    std::FILE* log = nullptr;
    if (_wfopen_s(&log, s_LogPath.c_str(), L"a") == 0) {
        std::fprintf(log, "%s\n", message);
        std::fclose(log);
    }
}

void LogActivity(std::uint32_t now) noexcept {
    if (now < s_LastLogTime) {
        s_LastLogTime = now;
    }
    if (!s_Diagnostics || now - s_LastLogTime < 30000) {
        return;
    }
    using ulw::SearchPhase;
    char message[576]{};
    std::snprintf(message, sizeof(message),
        "Look=%u Flee=%u Panic=%u taskPoolFailures=%u pursuitUpdates=%u reportUpdates=%u searchUpdates=%u patrolRoutes=%u airSearches=%u roadblockAttempts=%u limitedSpeedCalls=%u emergencyYieldCalls=%u crashSpeedCalls=%u attractorUses=%u crashGlances=%u crashReroutes=%u emergencyApproaches=%u extendedBursts=%u shorterBursts=%u suppressionHits=%u coverSearches=%u coverMoves=%u",
        s_Reactions[1], s_Reactions[2], s_Reactions[3], s_AllocationFailures,
        s_SearchStages[static_cast<unsigned>(SearchPhase::Pursue)],
        s_SearchStages[static_cast<unsigned>(SearchPhase::Respond)],
        s_SearchStages[static_cast<unsigned>(SearchPhase::Search)],
        s_PatrolRoutes, s_AirSearches, s_RoadblockAttempts, s_RestrictedDrivers, s_YieldingDrivers, s_CrashSlowdowns,
        s_AttractorUses, s_CrashGlances, s_CrashReroutes, s_EmergencyApproaches,
        s_ExtendedBursts, s_ShorterBursts, s_SuppressionHits, s_CoverSearches, s_CoverMoves);
    Log(message);
    std::snprintf(message, sizeof(message), "interceptRoutes=%u patrolBraking=%u bendLimits=%u followingLimits=%u queueHornSkips=%u",
        s_InterceptRoutes, s_PatrolBraking, s_BendLimits, s_FollowingLimits, s_QueueHornSkips);
    Log(message);
    std::snprintf(message, sizeof(message), "policeReports=%u drivingWarnings=%u investigationRoutes=%u voluntaryArrests=%u",
        s_PoliceReports, s_SuspicionWarnings, s_InvestigationRoutes, s_VoluntaryArrests);
    Log(message);
    std::snprintf(message, sizeof(message), "coverHolds=%u coverPeeks=%u tacticalWithdrawals=%u allyWaits=%u gangCasualties=%u",
        s_CoverHolds, s_CoverPeeks, s_Withdrawals, s_AllyWaits, s_GangCasualties);
    Log(message);
    s_LastLogTime = now;
}

bool Matches(std::uintptr_t address, const auto& bytes) noexcept {
    MEMORY_BASIC_INFORMATION page{};
    if (!VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) ||
        page.State != MEM_COMMIT || page.AllocationBase != reinterpret_cast<void*>(ImageBase) ||
        (page.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0 ||
        address + bytes.size() > reinterpret_cast<std::uintptr_t>(page.BaseAddress) + page.RegionSize) {
        return false;
    }
    return std::memcmp(reinterpret_cast<const void*>(address), bytes.data(), bytes.size()) == 0;
}

bool SupportedImage() noexcept {
    const auto* const image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    if (reinterpret_cast<std::uintptr_t>(image) != ImageBase) {
        return false;
    }
    const auto dos = Read<IMAGE_DOS_HEADER>(image);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0x40 || dos.e_lfanew > 0x400) {
        return false;
    }
    const auto header = Read<IMAGE_NT_HEADERS32>(image, dos.e_lfanew);
    return header.Signature == IMAGE_NT_SIGNATURE &&
        header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
        header.FileHeader.TimeDateStamp == 0x427101CA &&
        header.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        header.OptionalHeader.AddressOfEntryPoint == 0x424570 &&
        header.OptionalHeader.SizeOfImage == 0x1177000;
}

ulw::MissionObservation NativeMissionState(std::uint32_t now) noexcept {
    const auto offset = GameValue<std::uint32_t>(MissionFlagOffset);
    const bool scriptReady = offset != 0 && offset <= MainScriptLimit - sizeof(std::uint32_t);
    const bool mission = GameValue<std::uint8_t>(0xA444B1) != 0 ||
        (scriptReady && GameValue<std::uint32_t>(ScriptSpace + offset) != 0);
    const bool cutscene = GameValue<std::uint8_t>(0xB5F851) != 0 || GameValue<std::uint8_t>(0xB5F852) != 0;
    const bool activity = GameValue<std::uint8_t>(0xA444A8) != 0 || GameValue<std::uint8_t>(0xA43088) == 1;
    return {now, scriptReady, mission, false, cutscene, activity, {}};
}

bool FreeRoam() noexcept {
    if (s_ExecutingScripts || (s_MissionObserver && !s_Missions.AllowsWorld()) ||
        GameValue<std::uint8_t>(0xB7CB48) || GameValue<std::uint8_t>(0xB7CB49)) {
        return false;
    }
    const auto state = NativeMissionState(0);
    return state.scriptReady && !state.missionFlag && !state.cutscene && !state.activity;
}

ulw::Point EntityPosition(const void* entity) noexcept {
    const auto* const matrix = Read<const void*>(entity, 0x14);
    return matrix ? Read<ulw::Point>(matrix, 0x30) : Read<ulw::Point>(entity, 0x04);
}

ulw::Point EntityForward(const void* entity) noexcept {
    const auto* const matrix = Read<const void*>(entity, 0x14);
    if (matrix) {
        return Read<ulw::Point>(matrix, 0x10);
    }
    const auto heading = Read<float>(entity, 0x10);
    return {-std::sin(heading), std::cos(heading), 0.0f};
}

void ClearStreetMemory() noexcept {
    s_Police.Reset();
    s_PatrolTeam.Reset();
    s_PoliceIncidents.Reset();
    s_DrivingSuspicion.Reset();
    s_Surrender.Reset();
    s_SurrenderApproach = {};
    s_LastPoliceHint.reset();
    s_TrafficHorns.Reset();
    s_Roadblocks.Reset();
    s_AirPatrols = {};
    s_RoadPhysics.Reset();
    s_Crashes.Reset();
    s_CrashWitnesses.Reset();
    s_CombatPressure.Reset();
    s_CombatManeuvers.Reset();
    s_GangLosses.Reset();
    s_EmergencyUnits = {};
}

bool StreetActive(std::uint32_t now) noexcept {
    const auto active = FreeRoam();
    if ((s_StreetActive && !active) || now < s_StreetTime) {
        ClearStreetMemory();
    }
    s_StreetActive = active;
    s_StreetTime = now;
    return active;
}

void InspectMissionThread(ulw::MissionObservation& state) noexcept {
    auto address = GameValue<std::uintptr_t>(ActiveScriptList);
    for (std::size_t i = 0; address != 0 && i < ScriptCount; ++i) {
        // Foreign ASIs can own extra script objects. Rely on the native
        // flags whenever the linked list leaves the engine's script pool.
        if (address < ScriptPool || address >= ScriptPool + ScriptSlotSize * ScriptCount ||
            (address - ScriptPool) % ScriptSlotSize != 0) {
            return;
        }
        const auto* const script = reinterpret_cast<const void*>(address);
        if (Read<std::uint8_t>(script, 0xC4) && !Read<std::uint8_t>(script, 0xC7)) {
            const auto base = Read<std::uintptr_t>(script, 0x10);
            const bool missionBuffer = base >= ScriptSpace + MainScriptLimit &&
                base < ScriptSpace + MainScriptLimit + MissionScriptLimit;
            if (missionBuffer || Read<std::uint8_t>(script, 0xDC)) {
                state.missionThread = true;
                state.name = ulw::NormalizeMissionName(Read<std::array<char, 8>>(script, 0x08));
                return;
            }
        }
        address = Read<std::uintptr_t>(script);
    }
}

void LogMissionState() noexcept {
    const auto& name = s_Missions.Name();
    const char* const label = name[0] ? name.data() : "unnamed";
    char message[160]{};
    switch (s_Missions.Phase()) {
    case ulw::MissionPhase::Loading:
        Log("World systems waiting for the mission script.");
        return;
    case ulw::MissionPhase::FreeRoam:
        Log("Free roam: world systems resumed.");
        return;
    case ulw::MissionPhase::Mission:
        std::snprintf(message, sizeof(message), "Mission protection: %s (%s).", label,
            ulw::MissionProfileName(s_Missions.Profile()));
        break;
    case ulw::MissionPhase::Cutscene:
        std::snprintf(message, sizeof(message), "Cutscene protection: %s.", label);
        break;
    case ulw::MissionPhase::Activity:
        Log("Scripted activity: world systems suspended.");
        return;
    case ulw::MissionPhase::Recovery:
        std::snprintf(message, sizeof(message), "Mission cleanup: %s. World systems briefly suspended.", label);
        break;
    }
    Log(message);
}

void __cdecl OnProcessScripts() noexcept {
    s_ExecutingScripts = true;
    GameFunction<ProcessScripts>(0x46A000)();
    s_ExecutingScripts = false;

    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    auto state = NativeMissionState(now);
    if (state.scriptReady) {
        InspectMissionThread(state);
    }
    const bool changed = s_Missions.Update(state);
    if (changed && !s_Missions.AllowsWorld()) {
        ClearStreetMemory();
        s_Core.Reset();
        s_WasActive = false;
        s_StreetActive = false;
    }
    if (changed && s_Diagnostics) {
        LogMissionState();
    }
}

float Fog() noexcept {
    return std::max(GameValue<float>(0xC81300), GameValue<float>(0xC812FC));
}

ulw::CityActivity CityActivity() noexcept {
    if (GameValue<std::int32_t>(0xB72914) != 0 || !StreetActive(GameValue<std::uint32_t>(0xB7CB84))) {
        return {1.0f, 1.0f};
    }
    return ulw::AmbientActivity(GameValue<std::uint8_t>(0xB70153), GameValue<std::uint8_t>(0xB7014E),
        GameValue<std::uint16_t>(0xC81314), GameValue<float>(0xC81308), Fog());
}

class ScopedCivilianDensity {
public:
    ScopedCivilianDensity(std::uintptr_t address, float scale) noexcept :
        m_Count(*reinterpret_cast<float*>(address)),
        m_Original(m_Count),
        m_Scaled(std::isfinite(m_Original) && m_Original > 0.0f ? m_Original * scale : m_Original) {
        if (m_Scaled < m_Original) {
            m_Count = m_Scaled;
        }
    }

    ~ScopedCivilianDensity() {
        if (m_Scaled < m_Original && m_Count == m_Scaled) {
            m_Count = m_Original;
        }
    }

    ScopedCivilianDensity(const ScopedCivilianDensity&) = delete;
    ScopedCivilianDensity& operator=(const ScopedCivilianDensity&) = delete;

private:
    float& m_Count;
    float m_Original;
    float m_Scaled;
};

void __cdecl OnPopulation(bool generate) noexcept {
    const ScopedCivilianDensity density(0xC0BC40, generate ? CityActivity().pedestrians : 1.0f);
    GameFunction<PopulationUpdate>(0x616650)(generate);
}

void __cdecl OnRandomCars() noexcept {
    const ScopedCivilianDensity density(0xC0BC30, CityActivity().cars);
    GameFunction<RandomCars>(0x4341C0)();
}

bool __cdecl OnAttractorPed(int model, const void* attractor, ulw::Point position, void* entity, int decisionMaker) noexcept {
    const auto add = GameFunction<AddAttractorPed>(0x614210);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!attractor || !entity || decisionMaker != -1 || GameValue<std::int32_t>(0xB72914) != 0 || !StreetActive(now)) {
        return add(model, attractor, position, entity, decisionMaker);
    }
    const auto hour = GameValue<std::uint8_t>(0xB70153);
    const auto type = Read<std::uint8_t>(attractor, 0x34);
    const auto chance = ulw::AttractorUsePercent(type, hour, GameValue<std::uint8_t>(0xB7014E),
        GameValue<std::uint16_t>(0xC81314), GameValue<float>(0xC81324));
    // The same spot keeps its availability for a game hour, including
    // when the engine retries the spawn in the same frame.
    const auto location = std::bit_cast<std::uint32_t>(position.x) ^ std::rotl(std::bit_cast<std::uint32_t>(position.y), 11);
    const auto seed = (location ^ (hour * 0x9e3779b9u)) * 0x45d9f3bu;
    if ((seed >> 16) % 100 >= chance) {
        return false;
    }
    const auto added = add(model, attractor, position, entity, decisionMaker);
    if (added && (type == 1 || type == 2 || type == 3 || type == 4 || type == 8 || type == 9)) {
        ++s_AttractorUses;
    }
    return added;
}

bool PedOnFoot(const void* ped) noexcept {
    return Read<std::uint8_t>(ped, 0x484) == 1 && Read<std::uint8_t>(ped, 0x2F) == 0 &&
        !(Read<std::uint8_t>(ped, 0x46D) & 0x07) && Read<float>(ped, 0x540) > 0.0f;
}

bool AmbientWalker(const void* task, const void* ped) noexcept {
    if (!task || !ped || Read<std::uintptr_t>(task) != StandardWanderVtable || Read<const void*>(task, 0x04) ||
        !PedOnFoot(ped)) {
        return false;
    }
    const auto type = Read<std::uint32_t>(ped, 0x598);
    return type == 4 || type == 5;
}

ulw::PedestrianRoutine PedestrianRoutineFor(const void* ped) noexcept {
    return ulw::PickPedestrianRoutine(Read<std::uint16_t>(ped, 0x20), GameValue<std::uint8_t>(0xB70153),
        GameValue<std::uint8_t>(0xB7014E), GameValue<std::uint16_t>(0xC81314), GameValue<float>(0xC81324));
}

void NoticeCrash(void* ped, std::uint32_t now) noexcept {
    if (Read<std::uint8_t>(ped, 0x46C) & 0xBC || Read<std::uint32_t>(ped, 0x748) >= now ||
        !GameValue<void*>(PedPool)) {
        return;
    }
    const auto origin = EntityPosition(ped);
    const auto incident = s_Crashes.Closest(origin, 12.0f, now);
    if (!incident) {
        return;
    }
    const auto reference = GameFunction<PedReference>(0x54FF60)(ped);
    if (reference < 0 || (reference & 0x80)) {
        return;
    }
    const auto id = static_cast<std::uint32_t>(reference);
    if (s_CrashWitnesses.Noticed(id, *incident) ||
        !s_CrashWitnesses.TakeScan(id, now, GameValue<std::uint32_t>(0xB7CB4C))) {
        return;
    }
    const ulw::Point eye{origin.x, origin.y, origin.z + 0.65f};
    auto target = incident->position;
    target.z += 0.7f;
    if (!GameFunction<LineClear>(0x56A490)(eye, target, true, false, false, true, true, false, false)) {
        return;
    }
    const auto yaw = std::atan2(origin.x - target.x, target.y - origin.y);
    GameFunction<PedLookHeading>(0x5DEDC0)(ped, yaw, false, false);
    GameFunction<PedLookTimer>(0x5DF8D0)(ped, 900 + (Read<std::uint16_t>(ped, 0x20) & 0x1FFu));
    s_CrashWitnesses.Remember(id, *incident);
    ++s_CrashGlances;
    LogActivity(now);
}

void* __fastcall OnWanderControl(void* task, void*, void* ped) noexcept {
    const auto native = GameFunction<WanderControl>(0x674C30);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!AmbientWalker(task, ped) || !StreetActive(now)) {
        return native(task, ped);
    }
    auto* const paceField = static_cast<std::byte*>(task) + 0x0C;
    const auto previous = Read<std::uint32_t>(paceField);
    const auto* const runningTask = Read<const void*>(task, 0x08);
    const auto routine = PedestrianRoutineFor(ped);
    const bool jog = previous == Walking && routine.pace == ulw::PedestrianPace::Jog;
    if (jog) {
        std::memcpy(paceField, &Jogging, sizeof(Jogging));
    }
    auto* const next = native(task, ped);
    if (jog) {
        std::memcpy(paceField, &previous, sizeof(previous));
    }
    if (s_CrashGlancesEnabled && next && next == runningTask) {
        NoticeCrash(ped, now);
    }
    return next;
}

bool __fastcall OnChatPartner(void* task, void*, void* ped) noexcept {
    if (AmbientWalker(task, ped) && StreetActive(GameValue<std::uint32_t>(0xB7CB84)) && !PedestrianRoutineFor(ped).chat) {
        return false;
    }
    return GameFunction<FindChatPartner>(0x66FDA0)(task, ped);
}

bool IsPlayerPed(const void* entity) noexcept {
    return entity && (Read<std::uint8_t>(entity, 0x36) & 7) == 3 && Read<std::uint32_t>(entity, 0x598) == 0;
}

unsigned StreetFighterFaction(const void* ped, bool countFatality = false) noexcept {
    const auto faction = Read<std::uint32_t>(ped, 0x598);
    if (faction < 6 || faction > 16 || Read<std::uint8_t>(ped, 0x2F) != 0 ||
        (Read<std::uint8_t>(ped, 0x46D) & 0x07) || (!countFatality && !(Read<float>(ped, 0x540) > 0.0f))) {
        return 0;
    }
    // The engine marks free-roam territory attack waves as mission peds.
    const bool attackWave = faction >= 7 && (Read<std::uint8_t>(ped, 0x475) & 0x10);
    return (attackWave || Read<std::uint8_t>(ped, 0x484) == 1) ? faction : 0;
}

int StreetPedReference(const void* ped) noexcept {
    if (!GameValue<void*>(PedPool)) {
        return -1;
    }
    const auto ref = GameFunction<PedReference>(0x54FF60)(ped);
    return ref >= 0 && !(ref & 0x80) ? ref : -1;
}

bool IsSuppressed(const void* ped, std::uint32_t now) noexcept {
    const auto ref = s_PressureObserver ? StreetPedReference(ped) : -1;
    return ref >= 0 && s_CombatPressure.Suppressed(static_cast<std::uint32_t>(ref), now);
}

void __fastcall OnDamageResponse(void* handler, void*, const void* event, void* active,
    void* simplest, void* aborted) noexcept {
    const auto* const victim = Read<const void*>(handler);
    const auto weapon = Read<std::uint32_t>(event, 0x1C);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    // Only damage already calculated by the game counts as a hit.
    if (weapon >= 22 && weapon <= 34 && IsPlayerPed(Read<const void*>(event, 0x14)) &&
        Read<bool>(event, 0x0C) && Read<bool>(event, 0x42) &&
        (Read<float>(event, 0x38) > 0 || Read<float>(event, 0x3C) > 0) && StreetActive(now)) {
        const auto hitAt = Read<std::uint32_t>(event, 0x18);
        const bool fatal = Read<bool>(event, 0x40) && Read<float>(victim, 0x540) <= 0.0f;
        const auto faction = StreetFighterFaction(victim, fatal);
        const auto ref = faction ? StreetPedReference(victim) : -1;
        if (ref >= 0) {
            const auto pedRef = static_cast<std::uint32_t>(ref);
            if (fatal) {
                if (s_CoverFollowup && now - hitAt <= 250 && s_GangLosses.Record(pedRef, faction, EntityPosition(victim), now)) {
                    ++s_GangCasualties;
                }
            } else if (s_CombatPressure.RecordHit(pedRef, hitAt, now)) {
                ++s_SuppressionHits;
            }
        }
    }
    GameFunction<DamageResponse>(NativeDamageResponse)(handler, event, active, simplest, aborted);
}

bool __fastcall OnGunTask(void* task, void*, void* ped) noexcept {
    const auto process = GameFunction<GunTask>(NativeGunTask);
    constexpr std::uint8_t FireBurst = 3;
    if (Read<std::uint8_t>(task, 0x0F) != FireBurst || !IsPlayerPed(Read<const void*>(task, 0x1C))) {
        return process(task, ped);
    }
    const auto faction = StreetFighterFaction(ped);
    const auto slot = Read<std::uint8_t>(ped, 0x718);
    // SMGs and assault rifles occupy engine weapon slots 4 and 5.
    if (!faction || (slot != 4 && slot != 5)) {
        return process(task, ped);
    }
    const auto* const weapon = static_cast<const std::byte*>(ped) + 0x5A0 + slot * 0x1C;
    const auto type = Read<std::uint32_t>(weapon);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (type < 28 || type > 32 || !StreetActive(now)) {
        return process(task, ped);
    }
    auto& burstLength = *reinterpret_cast<std::uint16_t*>(static_cast<std::byte*>(task) + 0x34);
    const auto native = burstLength;
    const auto adjusted = ulw::CombatBurstLength(native, Read<unsigned>(weapon, 0x08), faction,
        Read<float>(ped, 0x540), IsSuppressed(ped, now));
    if (native == adjusted) {
        return process(task, ped);
    }
    const auto remaining = Read<std::uint16_t>(task, 0x36);
    const auto previousCommand = Read<std::uint8_t>(task, 0x10);
    // The engine reads this request when it starts a burst. Its remaining
    // shot count, reloads and animations stay under native control.
    burstLength = adjusted;
    const auto finished = process(task, ped);
    burstLength = native;
    if (!finished && Read<std::uint8_t>(task, 0x10) == FireBurst &&
        Read<std::uint16_t>(task, 0x36) == adjusted && (remaining != adjusted || previousCommand != FireBurst)) {
        ++(adjusted > native ? s_ExtendedBursts : s_ShorterBursts);
    }
    return finished;
}

bool HasCoverGround(ulw::Point position) noexcept {
    auto probe = position;
    probe.z += 0.35f;
    bool found{};
    const auto ground = GameFunction<GroundHeight>(0x5696C0)(probe, &found, nullptr);
    const auto height = position.z - ground;
    return found && height >= 0.45f && height <= 1.65f;
}

bool FireBlocked(ulw::Point threat, ulw::Point destination, float maximumGap = 100.0f) noexcept {
    threat.z += 0.5f;
    destination.z += 0.5f;
    const auto trace = GameFunction<TraceLine>(0x56BA00);
    CollisionPoint impact{};
    void* obstacle{};
    // Use the weapon trace's see-through and shoot-through material checks.
    return trace(threat, destination, impact, obstacle, true, true, false, true, true, true, false, true) &&
        ulw::DistanceSquared(impact.position, destination) <= maximumGap * maximumGap;
}

bool WalkableCombatRoute(ulw::Point origin, ulw::Point destination) noexcept {
    const ulw::Point midpoint{(origin.x + destination.x) * 0.5f, (origin.y + destination.y) * 0.5f,
        (origin.z + destination.z) * 0.5f};
    const auto clear = GameFunction<LineClear>(0x56A490);
    if (!HasCoverGround(destination) || !HasCoverGround(midpoint) ||
        !clear(origin, destination, true, true, false, true, true, false, false)) {
        return false;
    }
    origin.z += 0.5f;
    destination.z += 0.5f;
    return clear(origin, destination, true, true, false, true, true, false, false);
}

bool SafeCoverRoute(ulw::Point origin, ulw::Point threat, ulw::Point destination) noexcept {
    return ulw::CombatCoverApproach(origin, threat, destination) && !FireBlocked(threat, origin) &&
        FireBlocked(threat, destination, 4.0f) && WalkableCombatRoute(origin, destination);
}

const void* StreetThreat(const void* task) noexcept {
    const auto* const target = Read<const void*>(task, 0x0C);
    return IsPlayerPed(target) && Read<std::uint8_t>(target, 0x2F) == 0 &&
        !(Read<std::uint8_t>(target, 0x46D) & 0x07) && Read<float>(target, 0x540) > 0.0f ? target : nullptr;
}

const void* StreetCombatWeapon(const void* ped) noexcept {
    const auto slot = Read<std::uint8_t>(ped, 0x718);
    if (slot < 2 || slot > 6) {
        return nullptr;
    }
    const auto* const weapon = static_cast<const std::byte*>(ped) + 0x5A0 + slot * 0x1C;
    const auto type = Read<unsigned>(weapon);
    return type >= 22 && type <= 34 ? weapon : nullptr;
}

void RememberCombatMove(std::uint32_t pedRef, void* task, void* child, const void* cover, ulw::Point destination,
    std::uint32_t now, unsigned faction, ulw::CombatMove phase) noexcept {
    s_CombatManeuvers.Start(pedRef, {reinterpret_cast<std::uintptr_t>(task),
        reinterpret_cast<std::uintptr_t>(child), reinterpret_cast<std::uintptr_t>(cover),
        destination, now, faction, phase});
}

void ReleaseManeuverCover(void* ped, const ulw::CombatManeuver& move) noexcept {
    if (move.cover && Read<std::uintptr_t>(ped, 0x788) == move.cover) {
        GameFunction<ReleasePedCover>(0x5E0270)(ped);
    }
}

void* CreateCombatRun(const ulw::Point& destination) noexcept {
    if (auto* const move = GameFunction<AllocateTask>(0x61A5A0)(0x30)) {
        return GameFunction<RunToPoint>(0x667CD0)(move, Running, destination, 0.5f, true, false);
    }
    ++s_AllocationFailures;
    return nullptr;
}

std::optional<ulw::Point> CombatMoveDestination(ulw::Point origin, ulw::Point threat, bool withdraw,
    bool preferLeft) noexcept {
    const auto dx = origin.x - threat.x;
    const auto dy = origin.y - threat.y;
    const auto length = std::hypot(dx, dy);
    const auto awayX = dx / length;
    const auto awayY = dy / length;
    const auto back = withdraw ? 8.0f : 0.0f;
    const auto side = withdraw ? 2.6f : 2.9f;
    const auto first = preferLeft ? -side : side;
    for (const auto lateral : {first, -first}) {
        const ulw::Point destination{origin.x + awayX * back - awayY * lateral,
            origin.y + awayY * back + awayX * lateral, origin.z};
        if ((withdraw || !FireBlocked(threat, destination)) && WalkableCombatRoute(origin, destination)) {
            return destination;
        }
    }
    return std::nullopt;
}

void* StartWithdrawal(void* task, std::uint32_t pedRef, ulw::Point origin, ulw::Point threat,
    std::uint32_t now, unsigned faction) noexcept {
    if (const auto destination = CombatMoveDestination(origin, threat, true, (pedRef & 1) != 0)) {
        if (auto* const run = CreateCombatRun(*destination)) {
            RememberCombatMove(pedRef, task, run, nullptr, *destination, now, faction, ulw::CombatMove::Withdraw);
            ++s_Withdrawals;
            return run;
        }
    }
    return nullptr;
}

void* __fastcall OnArmedNext(void* task, void*, void* ped) noexcept {
    const auto next = GameFunction<ArmedControl>(NativeArmedNext);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    const auto ref = StreetPedReference(ped);
    if (ref < 0 || !StreetActive(now)) {
        return next(task, ped);
    }
    const auto pedRef = static_cast<std::uint32_t>(ref);
    const auto move = s_CombatManeuvers.Current(pedRef, reinterpret_cast<std::uintptr_t>(task),
        Read<std::uintptr_t>(task, 0x08));
    if (!move) {
        return next(task, ped);
    }
    s_CombatManeuvers.Clear(pedRef);
    const auto* const target = StreetThreat(task);
    const auto* const weapon = StreetCombatWeapon(ped);
    const auto faction = StreetFighterFaction(ped);
    if (!target || !weapon || !faction || now - move->started > 4200 ||
        (move->phase != ulw::CombatMove::Approach && move->phase != ulw::CombatMove::Hold)) {
        ReleaseManeuverCover(ped, *move);
        return next(task, ped);
    }
    const auto origin = EntityPosition(ped);
    const auto threat = EntityPosition(target);
    if (!ulw::CombatCoverInRange(origin, threat) || ulw::DistanceSquared(origin, move->destination) > 1.8f * 1.8f) {
        ReleaseManeuverCover(ped, *move);
        return next(task, ped);
    }
    const auto total = Read<unsigned>(weapon, 0x0C);
    const bool blocked = FireBlocked(threat, origin);
    const bool stillReserved = move->cover && Read<std::uintptr_t>(ped, 0x788) == move->cover;
    if (move->phase == ulw::CombatMove::Approach && blocked && stillReserved) {
        const auto clip = Read<unsigned>(weapon, 0x08);
        const auto reloadAt = Read<std::uint32_t>(weapon, 0x10);
        const auto reloadRemaining = Read<unsigned>(weapon, 0x04) == 2 && reloadAt > now ? reloadAt - now : 0u;
        const auto delay = ulw::CombatCoverHold(faction, clip, total, reloadRemaining);
        if (auto* const memory = GameFunction<AllocateTask>(0x61A5A0)(0x20)) {
            auto* const hold = GameFunction<StandStill>(0x62F310)(memory, static_cast<int>(delay), false, false, 8.0f);
            RememberCombatMove(pedRef, task, hold, reinterpret_cast<const void*>(move->cover), origin, now,
                faction, ulw::CombatMove::Hold);
            ++s_CoverHolds;
            return hold;
        }
        ++s_AllocationFailures;
    }
    ReleaseManeuverCover(ped, *move);
    if (ulw::CombatShouldWithdraw(faction, Read<float>(ped, 0x540), total) ||
        s_GangLosses.UnderStrength(faction, origin, now)) {
        if (auto* const run = StartWithdrawal(task, pedRef, origin, threat, now, faction)) {
            return run;
        }
        return next(task, ped);
    }
    if (!blocked && move->phase == ulw::CombatMove::Hold) {
        constexpr unsigned GunControl = 0x3FC;
        return GameFunction<ArmedSubTask>(0x626FC0)(task, GunControl, ped);
    }
    if (blocked && move->phase == ulw::CombatMove::Hold) {
        if (const auto destination = CombatMoveDestination(origin, threat, false, (pedRef & 1) != 0)) {
            if (auto* const run = CreateCombatRun(*destination)) {
                RememberCombatMove(pedRef, task, run, nullptr, *destination, now, faction, ulw::CombatMove::Peek);
                ++s_CoverPeeks;
                return run;
            }
        }
    }
    return next(task, ped);
}

void* __fastcall OnArmedControl(void* task, void*, void* ped) noexcept {
    const auto current = GameFunction<ArmedControl>(NativeArmedControl)(task, ped);
    const auto* const target = StreetThreat(task);
    const auto faction = StreetFighterFaction(ped);
    if (!current || current != Read<void*>(task, 0x08) || !target || !faction ||
        (Read<std::uint8_t>(ped, 0x46E) & 0x70)) {
        return current;
    }
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!StreetActive(now) || !GameValue<void*>(TaskPool)) {
        return current;
    }
    const auto ref = StreetPedReference(ped);
    if (ref < 0) {
        return current;
    }
    const auto pedRef = static_cast<std::uint32_t>(ref);
    if (s_CoverFollowup) {
        const auto parentId = reinterpret_cast<std::uintptr_t>(task);
        const auto childId = reinterpret_cast<std::uintptr_t>(current);
        if (const auto move = s_CombatManeuvers.Current(pedRef, parentId, childId)) {
            if (now - move->started <= 4200) {
                return current;
            }
            ReleaseManeuverCover(ped, *move);
            s_CombatManeuvers.Clear(pedRef);
            return GameFunction<ArmedControl>(NativeArmedNext)(task, ped);
        }
        if (const auto interrupted = s_CombatManeuvers.TakeInterrupted(pedRef, parentId, childId)) {
            ReleaseManeuverCover(ped, *interrupted);
        }
    }
    if (Read<std::uintptr_t>(current) != GunControlVtable || Read<void*>(ped, 0x788) ||
        !s_CombatPressure.Suppressed(pedRef, now)) {
        return current;
    }

    const auto origin = EntityPosition(ped);
    const auto threat = EntityPosition(target);
    if (!ulw::CombatCoverInRange(origin, threat) ||
        !s_CombatPressure.TakeCoverSearch(pedRef, now, GameValue<std::uint32_t>(0xB7CB4C))) {
        return current;
    }
    const auto* const weapon = s_CoverFollowup ? StreetCombatWeapon(ped) : nullptr;
    const bool needsWithdrawal = weapon && (ulw::CombatShouldWithdraw(faction, Read<float>(ped, 0x540),
        Read<unsigned>(weapon, 0x0C)) || s_GangLosses.UnderStrength(faction, origin, now));
    if (s_CoverFollowup && !needsWithdrawal && s_CombatManeuvers.AllyMoving(pedRef, faction, origin, now)) {
        ++s_AllyWaits;
        return current;
    }
    ++s_CoverSearches;
    auto* const point = GameFunction<ReserveCover>(0x6992B0)(ped, threat, false);
    if (point) {
        ulw::Point destination{};
        if (GameFunction<CoverCoordinates>(0x699570)(point, ped, threat, destination)) {
            destination.z += 1.0f; // Match the height adjustment in the native cover task.
            if (SafeCoverRoute(origin, threat, destination)) {
                if (auto* const run = CreateCombatRun(destination)) {
                    *reinterpret_cast<void**>(static_cast<std::byte*>(ped) + 0x788) = point;
                    if (s_CoverFollowup) {
                        RememberCombatMove(pedRef, task, run, point, destination, now, faction, ulw::CombatMove::Approach);
                    }
                    ++s_CoverMoves;
                    return run;
                }
            }
        }
        GameFunction<ReleaseCover>(0x698EF0)(point, ped);
    }
    if (needsWithdrawal) {
        if (auto* const run = StartWithdrawal(task, pedRef, origin, threat, now, faction)) {
            return run;
        }
    }
    return current;
}

bool __fastcall OnWeaponFire(void* weapon, void*, void* ped, const void* start, const void* muzzle,
    const void* target, const void* aim, const void* alternate) noexcept {
    const auto fire = GameFunction<FireWeapon>(0x742300);
    const auto weaponType = Read<std::uint32_t>(weapon);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    const auto faction = StreetFighterFaction(ped);
    if (!faction || !IsPlayerPed(target) || weaponType < 22 || weaponType > 34 || !StreetActive(now)) {
        return fire(weapon, ped, start, muzzle, target, aim, alternate);
    }
    auto& accuracy = *(static_cast<std::uint8_t*>(ped) + 0x71A);
    const auto native = accuracy;
    accuracy = ulw::CombatAccuracy(native, faction, Read<float>(ped, 0x540),
        ulw::DistanceSquared(EntityPosition(ped), EntityPosition(target)),
        GameValue<float>(0xC81308), Fog(), GameValue<std::uint8_t>(0xB70153), IsSuppressed(ped, now));
    const auto fired = fire(weapon, ped, start, muzzle, target, aim, alternate);
    accuracy = native;
    return fired;
}

unsigned CurrentWantedLevel() noexcept {
    const auto* const wanted = GameFunction<WantedPointer>(0x56E230)(0);
    return wanted && !(Read<std::uint8_t>(wanted, 0x1E) & 7) ? Read<std::uint32_t>(wanted, 0x2C) : 0u;
}

unsigned ActiveWantedLevel() noexcept {
    const auto level = CurrentWantedLevel();
    if (!level) {
        s_Police.Reset();
        s_PatrolTeam.Reset();
        s_AirPatrols = {};
        s_Roadblocks.Reset();
    }
    return level;
}

bool ShowPoliceHint(const char* text, std::uint32_t now) noexcept {
    if (!s_PoliceHud || (s_LastPoliceHint && now - *s_LastPoliceHint < 12000) ||
        GameValue<std::int32_t>(0xBAA474) != 0) {
        return false;
    }
    GameFunction<HelpMessage>(0x588BE0)(text, true, false, false);
    s_LastPoliceHint = now;
    return true;
}

bool VisibleStreetPoint(const void* observer, ulw::Point origin, ulw::Point target, float range,
    bool car) noexcept {
    const auto distance = ulw::DistanceSquared(origin, target);
    if (!(distance <= range * range) || std::abs(target.z - origin.z) > 5.0f) {
        return false;
    }
    const auto forward = EntityForward(observer);
    const auto ahead = (target.x - origin.x) * forward.x + (target.y - origin.y) * forward.y;
    if (distance > 6.0f * 6.0f && !(ahead > 0.0f && ahead * ahead >= 0.20f * distance)) {
        return false;
    }
    origin.z += car ? 1.45f : 0.6f;
    target.z += car ? 1.35f : 0.55f;
    return GameFunction<LineClear>(0x56A490)(origin, target, true, false, false, true, true, true, false);
}

bool WitnessStillPresent(const ulw::IncidentLead& lead) noexcept {
    if (lead.kind != ulw::IncidentKind::Witness) {
        return true;
    }
    const auto* const pool = GameValue<const void*>(PedPool);
    if (!pool || (lead.observer >> 8) >= Read<std::uint32_t>(pool, 0x08)) {
        return false;
    }
    const auto* const witness = GameFunction<PedFromReference>(0x54FF90)(static_cast<int>(lead.observer));
    return witness && Read<float>(witness, 0x540) > 0.0f;
}

bool CanSubmit(const void* player) noexcept {
    return IsPlayerPed(player) && Read<std::uint8_t>(player, 0x2F) == 0 &&
        (Read<std::uint8_t>(player, 0x46C) & 1) && !(Read<std::uint8_t>(player, 0x46D) & 7) &&
        !(Read<std::uint8_t>(player, 0x46E) & 1) && !(Read<std::uint8_t>(player, 0x474) & 0x40) &&
        Read<float>(player, 0x540) > 0 && Read<std::uint8_t>(player, 0x718) == 0 &&
        ulw::DistanceSquared(Read<ulw::Point>(player, 0x44), {}) <= 0.045f * 0.045f;
}

void* ReplaceArrestSubtask(void* arrest, void* cop, void* child, unsigned type) noexcept {
    const auto table = Read<const void*>(child);
    const auto abort = Read<AbortTask>(table, 0x18);
    return abort(child, cop, 1, nullptr) ? GameFunction<ArmedSubTask>(0x68CF80)(arrest, type, cop) : nullptr;
}

void* __fastcall OnArrestControl(void* arrest, void*, void* cop) noexcept {
    const auto native = GameFunction<ArmedControl>(NativeArrestControl);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!StreetActive(now) || !PedOnFoot(cop) || Read<unsigned>(cop, 0x598) != 6 ||
        !GameValue<void*>(TaskPool)) {
        return native(arrest, cop);
    }
    auto* const child = Read<void*>(arrest, 0x08);
    const auto* const player = Read<const void*>(arrest, 0x10);
    const auto ref = StreetPedReference(cop);
    if (!child || !IsPlayerPed(player) || ref < 0 || (Read<std::uint8_t>(player, 0x474) & 0x40)) {
        return native(arrest, cop);
    }
    const auto officer = static_cast<std::uint32_t>(ref);
    const auto parentId = reinterpret_cast<std::uintptr_t>(arrest);
    const auto childId = reinterpret_cast<std::uintptr_t>(child);
    const bool approaching = s_SurrenderApproach.officer == officer && s_SurrenderApproach.parent == parentId &&
        s_SurrenderApproach.child == childId;
    const auto wanted = CurrentWantedLevel();
    const auto copPosition = EntityPosition(cop);
    const auto playerPosition = EntityPosition(player);
    const auto distance = ulw::DistanceSquared(copPosition, playerPosition);
    const bool held = GameValue<std::int16_t>(KeyboardState + 8 * sizeof(std::int16_t)) != 0;
    const bool eligible = (wanted == 1 || wanted == 2) && CanSubmit(player);
    const auto ready = s_Surrender.Update(now, held, eligible);
    constexpr unsigned KillOnFoot = 0x3E8;
    constexpr unsigned SeekSuspect = 0x38B;
    constexpr unsigned SimpleArrest = 0x44C;
    const auto childType = Read<TaskType>(Read<const void*>(child), 0x10)(child);
    if (approaching && (!eligible || !held || !(distance <= 13.0f * 13.0f))) {
        if (auto* const replacement = ReplaceArrestSubtask(arrest, cop, child, KillOnFoot)) {
            s_SurrenderApproach = {};
            return replacement;
        }
        return native(arrest, cop);
    }
    if (!eligible || !(distance <= 9.0f * 9.0f) || std::abs(copPosition.z - playerPosition.z) > 1.7f ||
        childType == SimpleArrest) {
        return native(arrest, cop);
    }
    if (!held) {
        ShowPoliceHint("POLIS: Teslim olmak icin silahsiz dur ve F9'u basili tut.", now);
    }
    if (!ready) {
        return native(arrest, cop);
    }
    auto copEye = copPosition;
    auto playerEye = playerPosition;
    copEye.z += 0.55f;
    playerEye.z += 0.55f;
    if (!GameFunction<LineClear>(0x56A490)(copEye, playerEye, true, true, false, true, true, false, false) ||
        !s_Surrender.Claim(officer, now)) {
        return native(arrest, cop);
    }
    if (distance <= 3.3f * 3.3f) {
        if (auto* const detain = ReplaceArrestSubtask(arrest, cop, child, SimpleArrest)) {
            s_Surrender.Commit();
            s_SurrenderApproach = {};
            ++s_VoluntaryArrests;
            return detain;
        }
    } else if (childType == KillOnFoot) {
        if (auto* const approach = ReplaceArrestSubtask(arrest, cop, child, SeekSuspect)) {
            s_SurrenderApproach = {officer, parentId, reinterpret_cast<std::uintptr_t>(approach)};
            return approach;
        }
    }
    return native(arrest, cop);
}

ulw::SearchTarget SearchForPlayer(const void* observer, ulw::Point origin, ulw::Point player,
    std::uint32_t id, std::uint32_t now, ulw::PatrolMode mode) noexcept {
    if (s_Police.ShouldSample(id, now, GameValue<std::uint32_t>(0xB7CB4C))) {
        const bool driving = mode == ulw::PatrolMode::Car;
        const bool airborne = mode == ulw::PatrolMode::Air;
        const auto range = ulw::SightRange(GameValue<float>(0xC81308), Fog(), GameValue<std::uint8_t>(0xB70153)) *
            (airborne ? 2.45f : 1.0f);
        const auto distance = ulw::DistanceSquared(origin, player);
        const auto forward = EntityForward(observer);
        const auto dot = forward.x * (player.x - origin.x) + forward.y * (player.y - origin.y);
        const bool facing = airborne || distance < 81.0f || (dot > 0.0f && dot * dot >= 0.22f * distance);
        auto eye = origin;
        eye.z += airborne ? -1.3f : driving ? 1.3f : 0.65f;
        auto target = player;
        target.z += 0.7f;
        const bool visible = distance <= range * range && facing &&
            GameFunction<LineClear>(0x56A490)(eye, target, true, !airborne, false, true, true, false, false);
        s_Police.Observe(id, now, origin, player, visible, mode);
    }
    auto search = s_Police.Target(id, now, origin, mode);
    if (s_PoliceWitnesses && (search.phase == ulw::SearchPhase::Lost || search.phase == ulw::SearchPhase::Observe)) {
        if (const auto lead = s_PoliceIncidents.Nearby(origin, now, true); lead && WitnessStillPresent(*lead)) {
            search = {ulw::SearchPhase::Respond, lead->point};
        }
    }
    ++s_SearchStages[static_cast<unsigned>(search.phase)];
    LogActivity(now);
    return search;
}

ulw::Point* __fastcall OnSeek(void* task, void*, ulw::Point* result, const void* officer) noexcept {
    auto* const native = GameFunction<SeekPoint>(0x46FEE0)(task, result, officer);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!result || !officer || !GameValue<void*>(PedPool) || !StreetActive(now) ||
        Read<std::uint32_t>(officer, 0x598) != 6 || !PedOnFoot(officer)) {
        return native;
    }
    const auto* const player = Read<const void*>(task, 0x0C);
    if (!player || Read<std::uint32_t>(player, 0x598) != 0 || Read<std::uint8_t>(player, 0x2F) != 0 ||
        !(Read<float>(player, 0x540) > 0.0f)) {
        return native;
    }
    if (!ActiveWantedLevel()) {
        return native;
    }
    const auto reference = GameFunction<PedReference>(0x54FF60)(officer);
    if (reference < 0 || (reference & 0x80)) {
        return native;
    }
    *result = SearchForPlayer(officer, EntityPosition(officer), *result,
        static_cast<std::uint32_t>(reference), now, ulw::PatrolMode::Foot).point;
    return native;
}

bool IsRoadCar(const void* car) noexcept {
    return Read<std::int32_t>(car, 0x594) == 0 && Read<std::uint8_t>(car, 0x2F) == 0 &&
        Read<std::int8_t>(car, 0x424) < 0;
}

bool CruisePatrol(const void* car) noexcept {
    const auto* const driver = Read<const void*>(car, 0x460);
    return IsRoadCar(car) && Read<std::uint8_t>(car, 0x4A4) != 2 && Read<std::uint8_t>(car, 0x3BA) == 1 &&
        (Read<std::uint8_t>(car, 0x428) & 1) && driver && Read<unsigned>(driver, 0x598) == 6 &&
        Read<float>(driver, 0x540) > 0;
}

void ObserveCruisePatrol(const void* car, std::uint32_t ref, std::uint32_t now) noexcept {
    if (!s_PoliceCruise || !CruisePatrol(car) || CurrentWantedLevel() != 0 ||
        !s_PoliceIncidents.TakeProbe(ref | 0x80000000u, now, GameValue<std::uint32_t>(0xB7CB4C))) {
        return;
    }
    const auto position = EntityPosition(car);
    const auto* const driven = GameFunction<PlayerVehicle>(0x56E0D0)(0, false);
    auto latest = position;
    unsigned evidence{};
    if (driven && IsPlayerPed(Read<const void*>(driven, 0x460)) && IsRoadCar(driven)) {
        const auto playerPosition = EntityPosition(driven);
        const auto hitAt = Read<std::uint32_t>(driven, 0x3C);
        const bool hardImpact = hitAt && now - hitAt < 850 && Read<float>(driven, 0xD8) > 7.0f;
        const auto risk = ulw::DrivingEvidence(ulw::DistanceSquared(Read<ulw::Point>(driven, 0x44), {}), hardImpact);
        const auto sight = ulw::SightRange(GameValue<float>(0xC81308), Fog(), GameValue<std::uint8_t>(0xB70153));
        if (risk && VisibleStreetPoint(car, position, playerPosition, std::min(41.0f, sight), true)) {
            evidence = risk;
            latest = playerPosition;
        }
    }
    if (s_DrivingSuspicion.Observe(ref, now, latest, evidence) &&
        ShowPoliceHint("POLIS: Yavasla ve dikkatli sur.", now)) {
        ++s_SuspicionWarnings;
    }
    if (const auto crash = s_Crashes.Closest(position, 29.0f, now);
        crash && now - crash->impactAt < 3600 && VisibleStreetPoint(car, position, crash->position, 29.0f, true)) {
        if (s_PoliceIncidents.Record(ulw::IncidentKind::Crash, ref | 0x80000000u, crash->position, now, false)) {
            ++s_PoliceReports;
        }
    }
}

void __cdecl OnPatrolCruiseNode(void* car) noexcept {
    const auto native = GameFunction<CarAI>(0x42DE80);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!CruisePatrol(car) || !StreetActive(now) || CurrentWantedLevel() != 0 || !GameValue<void*>(VehiclePool)) {
        native(car);
        return;
    }
    const auto reference = GameFunction<PedReference>(0x54FFC0)(car);
    if (reference >= 0 && !(reference & 0x80)) {
        const auto origin = EntityPosition(car);
        auto destination = s_DrivingSuspicion.Follow(static_cast<std::uint32_t>(reference), now);
        if (!destination) {
            if (const auto lead = s_PoliceIncidents.Nearby(origin, now, false); lead && WitnessStillPresent(*lead)) {
                destination = lead->point;
            }
        }
        if (destination) {
            const auto range = ulw::DistanceSquared(origin, *destination);
            if (range >= 9.0f * 9.0f && range <= 57.0f * 57.0f && std::abs(origin.z - destination->z) < 5.0f) {
                const auto currentNode = Read<std::uint32_t>(car, 0x390);
                const auto nextNode = Read<std::uint32_t>(car, 0x394);
                const auto nextLink = Read<std::uint16_t>(car, 0x3A6);
                GameFunction<ChaseNode>(0x426EF0)(car, destination->x, destination->y, destination->z, nullptr);
                if (Read<std::uint32_t>(car, 0x390) != currentNode || Read<std::uint32_t>(car, 0x394) != nextNode ||
                    Read<std::uint16_t>(car, 0x3A6) != nextLink) {
                    ++s_InvestigationRoutes;
                    return;
                }
            }
        }
    }
    native(car);
}

bool PatrolReference(const void* car, std::uint32_t now, std::uint32_t& id, unsigned& wanted) noexcept {
    if (!StreetActive(now) || !IsRoadCar(car) || !GameValue<void*>(VehiclePool) ||
        Read<std::uint8_t>(car, 0x4A4) == 2 || !(Read<std::uint8_t>(car, 0x428) & 1)) {
        return false;
    }
    const auto* const driver = Read<const void*>(car, 0x460);
    if (!driver || Read<std::uint32_t>(driver, 0x598) != 6) {
        return false;
    }
    wanted = ActiveWantedLevel();
    if (!wanted) {
        return false;
    }
    const auto reference = GameFunction<PedReference>(0x54FFC0)(car);
    if (reference < 0 || (reference & 0x80)) {
        return false;
    }
    id = static_cast<std::uint32_t>(reference) | 0x80000000u;
    return true;
}

bool __cdecl OnChaseNode(void* car, float x, float y, float z, const void* targetCar) noexcept {
    const auto pick = GameFunction<ChaseNode>(0x426EF0);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    std::uint32_t id{};
    unsigned wanted{};
    if (GameValue<std::int32_t>(0xB72914) != 0 || !PatrolReference(car, now, id, wanted)) {
        return pick(car, x, y, z, targetCar);
    }
    const auto origin = EntityPosition(car);
    const auto observed = targetCar ? EntityPosition(targetCar) : ulw::Point{x, y, z};
    const auto search = SearchForPlayer(car, origin, observed, id, now, ulw::PatrolMode::Car);
    const auto assignment = s_PatrolTeam.Assign(id, now, targetCar ? wanted : 0, origin, search);
    if (search.phase == ulw::SearchPhase::Pursue) {
        if (assignment.role == ulw::PatrolRole::Intercept) {
            ++s_InterceptRoutes;
            const auto goal = assignment.destination;
            return pick(car, goal.x, goal.y, goal.z, nullptr);
        }
        return pick(car, x, y, z, targetCar);
    }
    ++s_PatrolRoutes;
    if (search.phase == ulw::SearchPhase::Lost || search.phase == ulw::SearchPhase::Observe) {
        GameFunction<CarAI>(0x42DE80)(car);
        return false;
    }
    return pick(car, search.point.x, search.point.y, search.point.z, nullptr);
}

void __cdecl OnBlockSteering(void* car, const void* target, float x, float y, float vx, float vy,
    float* steer, float* gas, float* brake, bool* handbrake) noexcept {
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    std::uint32_t id{};
    unsigned wanted{};
    bool observedTarget{};
    if (target && GameValue<std::int32_t>(0xB72914) == 0 && PatrolReference(car, now, id, wanted)) {
        const auto origin = EntityPosition(car);
        const auto search = SearchForPlayer(car, origin, EntityPosition(target), id, now, ulw::PatrolMode::Car);
        s_PatrolTeam.Assign(id, now, wanted, origin, search);
        observedTarget = search.phase == ulw::SearchPhase::Pursue;
        if (!observedTarget) {
            ++s_PatrolRoutes;
            x = search.point.x;
            y = search.point.y;
            const auto dx = x - origin.x;
            const auto dy = y - origin.y;
            if (dx * dx + dy * dy < 4.0f) {
                const auto forward = EntityForward(car);
                x = origin.x + forward.x * 12.0f;
                y = origin.y + forward.y * 12.0f;
            }
            vx = 0.0f;
            vy = 0.0f;
            target = nullptr;
        }
    }
    GameFunction<BlockSteering>(0x4335E0)(car, target, x, y, vx, vy, steer, gas, brake, handbrake);
    if (observedTarget && !*handbrake) {
        const auto origin = EntityPosition(car);
        const auto front = EntityPosition(target);
        const ulw::Point offset{front.x - origin.x, front.y - origin.y, front.z - origin.z};
        const auto response = ulw::PatrolFollowing({*gas, *brake}, wanted, offset, EntityForward(car),
            Read<ulw::Point>(car, 0x44), Read<ulw::Point>(target, 0x44), GameValue<float>(0xC81308));
        if (response.brake > *brake || response.gas < *gas) {
            *gas = response.gas;
            *brake = response.brake;
            ++s_PatrolBraking;
        }
    }
}

std::uint32_t AirPatrolId(const void* heli) noexcept {
    if (!heli || Read<std::uint16_t>(heli, 0x22) != 497 || Read<std::uint8_t>(heli, 0x3BA) != 40 ||
        !GameValue<void*>(VehiclePool)) {
        return 0;
    }
    const auto reference = GameFunction<PedReference>(0x54FFC0)(heli);
    return reference >= 0 && !(reference & 0x80) ? static_cast<std::uint32_t>(reference) | 0xc0000000u : 0;
}

void __cdecl OnPoliceHeliFlight(void* heli, float heading, float distance, bool policeFlight) noexcept {
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (policeFlight && StreetActive(now) && GameValue<std::int32_t>(0xB72914) == 0) {
        for (std::size_t slot = 0; slot < s_AirPatrols.size(); ++slot) {
            const auto* const spawned = GameValue<const void*>(PoliceHelicopters + slot * sizeof(void*));
            if (heli != spawned) {
                continue;
            }
            const auto id = AirPatrolId(heli);
            if (id && ActiveWantedLevel() >= 3) {
                const auto origin = EntityPosition(heli);
                const auto destination = Read<ulw::Point>(heli, 0x3EC);
                const auto search = SearchForPlayer(heli, origin, destination, id, now, ulw::PatrolMode::Air);
                s_AirPatrols[slot] = {id, now, search, true};
                if (search.phase != ulw::SearchPhase::Pursue) {
                    ++s_AirSearches;
                    const auto point = ulw::AirSearchPoint(search, origin, EntityForward(heli), destination.z);
                    std::memcpy(static_cast<std::byte*>(heli) + 0x3EC, &point, sizeof(point));
                    const auto dx = point.x - origin.x;
                    const auto dy = point.y - origin.y;
                    heading = GameFunction<HeadingForOffset>(0x53CC70)(dx, dy);
                    distance = std::sqrt(dx * dx + dy * dy);
                }
            } else {
                s_AirPatrols[slot] = {};
            }
            break;
        }
    }
    GameFunction<FlyHelicopter>(0x429A70)(heli, heading, distance, policeFlight);
}

void __cdecl OnSearchLight(const ulw::Point& origin, const ulw::Point& target,
    float radius, float power, std::uint32_t corona, std::uint8_t clip, std::uint8_t shadow) noexcept {
    const auto add = GameFunction<AddSearchLight>(0x6C45B0);
    if (GameValue<std::int32_t>(0xB72914) == 0 && FreeRoam()) {
        const auto now = GameValue<std::uint32_t>(0xB7CB84);
        for (std::size_t slot = 0; slot < s_AirPatrols.size(); ++slot) {
            const auto* const heli = GameValue<const void*>(PoliceHelicopters + slot * sizeof(void*));
            const auto& observation = s_AirPatrols[slot];
            if (!heli || corona != reinterpret_cast<std::uintptr_t>(heli) + 0x0B ||
                !observation.observed || observation.id != AirPatrolId(heli) || now - observation.at > 500) {
                continue;
            }
            if (observation.search.phase == ulw::SearchPhase::Pursue) {
                break;
            }
            if (ulw::HasSearchContact(observation.search.phase)) {
                add(origin, observation.search.point, radius, power, corona, clip, shadow);
            }
            return;
        }
    }
    add(origin, target, radius, power, corona, clip, shadow);
}

const std::byte* ExteriorPathNode(std::uint32_t address, bool vehicle) noexcept {
    const auto area = address & 0xFFFFu;
    const auto index = address >> 16;
    const auto counts = vehicle ? 0x970114u : 0x96FFF4u;
    if (area >= 64 || index >= GameValue<std::uint32_t>(counts + area * sizeof(std::uint32_t))) {
        return nullptr;
    }
    const auto* const nodes = GameValue<const std::byte*>(0x96F854 + area * sizeof(void*));
    return nodes ? nodes + index * 0x1C : nullptr;
}

ulw::Point PathPosition(const std::byte* node) noexcept {
    return {
        static_cast<float>(Read<std::int16_t>(node, 0x08)) / 8.0f,
        static_cast<float>(Read<std::int16_t>(node, 0x0A)) / 8.0f,
        static_cast<float>(Read<std::int16_t>(node, 0x0C)) / 8.0f,
    };
}

bool __fastcall OnFootpathBlocked(void* task, void*, void* ped, const std::uint32_t* address) noexcept {
    if (GameFunction<FootpathBlocked>(0x671EF0)(task, ped, address)) {
        return true;
    }
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!address || !AmbientWalker(task, ped) || !StreetActive(now)) {
        return false;
    }
    const auto* const node = ExteriorPathNode(*address, false);
    if (node && s_Crashes.ObstructsFootpath(EntityPosition(ped), PathPosition(node), now)) {
        ++s_CrashReroutes;
        return true;
    }
    return false;
}

bool __cdecl OnRoadblockNode(std::uint32_t address, float* width, ulw::Point* crossRoad) noexcept {
    const auto info = GameFunction<RoadblockNodeInfo>(0x460EE0);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!StreetActive(now) || GameValue<std::int32_t>(0xB72914) != 0) {
        return info(address, width, crossRoad);
    }
    const auto wanted = ActiveWantedLevel();
    if (wanted < 3 || !s_Roadblocks.Ready(now, wanted)) {
        return false;
    }
    const auto* const node = ExteriorPathNode(address, true);
    if (!node) {
        return false;
    }
    const auto flags = Read<std::uint8_t>(node, 0x18);
    const auto links = flags & 0x0Fu;
    if (links < 2 || (flags & 0xB0)) {
        return false;
    }
    const auto road = PathPosition(node);
    const auto* const player = GameFunction<PlayerVehicle>(0x56E0D0)(-1, false);
    if (!player || !info(address, width, crossRoad)) {
        return false;
    }
    const bool highway = (Read<std::uint8_t>(node, 0x19) & 0x20) != 0;
    if (!ulw::SuitableRoadblock({road, EntityPosition(player), *width,
        ulw::DistanceSquared(Read<ulw::Point>(player, 0x44), {}), wanted, links, highway, true,
        s_UseSightingForRoadblocks, s_Police.HasRecentReport(road, now)})) {
        return false;
    }
    const auto* const camera = reinterpret_cast<const void*>(ActiveCamera);
    if (!Read<const void*>(camera, 0x14)) {
        return false;
    }
    const auto viewpoint = EntityPosition(camera);
    auto candidate = road;
    candidate.z += 1.25f;
    if (ulw::InCameraCone(viewpoint, EntityForward(camera), candidate) &&
        GameFunction<LineClear>(0x56A490)(viewpoint, candidate, true, false, false, true, true, false, false)) {
        return false;
    }
    s_Roadblocks.Record(now);
    ++s_RoadblockAttempts;
    LogActivity(now);
    return true;
}

bool HasServiceDestination(std::uint8_t mission) noexcept {
    return mission == 8 || mission == 9 || mission == 12 || mission == 13;
}

void __cdecl OnCarAI(void* car) noexcept {
    GameFunction<CarAI>(0x41DA30)(car);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!StreetActive(now) || !IsRoadCar(car) || !GameValue<void*>(VehiclePool)) {
        return;
    }
    const auto emergencyFlags = Read<std::uint8_t>(car, 0x428) & 7;
    const bool ambient = Read<std::uint8_t>(car, 0x4A4) == 1;
    if (!ambient && !emergencyFlags) {
        return;
    }
    const auto ref = GameFunction<PedReference>(0x54FFC0)(car);
    if (ref < 0 || (ref & 0x80)) {
        return;
    }
    const auto key = static_cast<std::uint32_t>(ref);
    ObserveCruisePatrol(car, key, now);
    if (ambient) {
        s_Crashes.Observe({key, now, Read<std::uint32_t>(car, 0x3C), EntityPosition(car),
            ulw::DistanceSquared(Read<ulw::Point>(car, 0x44), {}), Read<float>(car, 0xD8)});
    }
    auto& entry = s_EmergencyUnits[(key >> 8) % s_EmergencyUnits.size()];
    if (!emergencyFlags || Read<std::uint8_t>(car, 0x4A4) == 2) {
        if (entry.vehicle == key) {
            entry = {};
        }
        return;
    }
    const auto mission = Read<std::uint8_t>(car, 0x3BA);
    const auto position = EntityPosition(car);
    const auto velocity = Read<ulw::Point>(car, 0x44);
    const auto status = Read<std::uint8_t>(car, 0x36) >> 3;
    const bool onDuty = (emergencyFlags & 6) != 0;
    const bool stationary = velocity.x * velocity.x + velocity.y * velocity.y <= 0.085f * 0.085f;
    const bool stoppedAtDestination = HasServiceDestination(mission) &&
        ulw::DistanceSquared(position, Read<ulw::Point>(car, 0x3EC)) <= 24.0f * 24.0f;
    if (onDuty && status != 5 && stationary && (mission == 0 || mission == 10 || stoppedAtDestination)) {
        entry = {position, velocity, key, now, ulw::EmergencyState::OnScene};
    } else if ((Read<std::uint8_t>(car, 0x42D) & 0x80) && Read<const void*>(car, 0x460)) {
        entry = {position, velocity, key, now, ulw::EmergencyState::Responding};
    } else if (entry.vehicle == key) {
        entry = {};
    }
}

float YieldSpeedScale(ulw::Point position, ulw::Point forward, ulw::Point velocity, std::uint32_t now) noexcept {
    auto scale = 1.0f;
    for (const auto& unit : s_EmergencyUnits) {
        scale = std::min(scale, ulw::EmergencyYieldScale(position, forward, velocity, now, unit));
    }
    return scale;
}

void __cdecl OnServiceApproach(void* car, float* steer, float* gas, float* brake, bool* handbrake) noexcept {
    GameFunction<MissionSteering>(0x436A90)(car, steer, gas, brake, handbrake);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!StreetActive(now) || !IsRoadCar(car) || Read<std::uint8_t>(car, 0x4A4) == 2 ||
        !(Read<std::uint8_t>(car, 0x428) & 6) || !HasServiceDestination(Read<std::uint8_t>(car, 0x3BA)) ||
        *handbrake) {
        return;
    }
    const auto* const driver = Read<const void*>(car, 0x460);
    const auto driverType = driver ? Read<std::uint32_t>(driver, 0x598) : 0;
    if (driverType != 18 && driverType != 19) {
        return;
    }
    const auto target = Read<ulw::Point>(car, 0x3EC);
    const auto position = EntityPosition(car);
    const auto dx = target.x - position.x;
    const auto dy = target.y - position.y;
    const auto motion = Read<ulw::Point>(car, 0x44);
    if (std::abs(target.z - position.z) > 6.5f || dx * motion.x + dy * motion.y <= 0) {
        return;
    }
    const auto adjusted = ulw::EmergencyApproach({*gas, *brake}, dx * dx + dy * dy,
        motion.x * motion.x + motion.y * motion.y, GameValue<float>(0xC81308), Fog());
    if (adjusted.brake > *brake || adjusted.gas < *gas) {
        *gas = adjusted.gas;
        *brake = adjusted.brake;
        ++s_EmergencyApproaches;
        LogActivity(now);
    }
}

const void* NextRoadLink(const void* car) noexcept {
    const auto route = Read<std::uint16_t>(car, 0x3A6);
    if (route == 0xFFFF) {
        return nullptr;
    }
    const unsigned area = route >> 10;
    const unsigned index = route & 0x3FF;
    const auto* const links = GameValue<const std::byte*>(CarPathLinks + sizeof(void*) * area);
    return links && index < GameValue<std::uint32_t>(CarPathLinkCounts + sizeof(std::uint32_t) * area)
        ? links + index * 0x0E : nullptr;
}

float BendSpeedScale(const void* car, ulw::Point position, ulw::Point forward, float wet,
    ulw::DriverProfile driver) noexcept {
    const auto* const next = NextRoadLink(car);
    const auto sign = Read<std::int8_t>(car, 0x3B6);
    if (!next || (sign != -1 && sign != 1)) {
        return 1.0f;
    }
    const auto x = static_cast<float>(Read<std::int16_t>(next)) * 0.125f - position.x;
    const auto y = static_cast<float>(Read<std::int16_t>(next, 2)) * 0.125f - position.y;
    if (x * forward.x + y * forward.y < -4.0f) {
        return 1.0f;
    }
    const ulw::Point direction{static_cast<float>(Read<std::int8_t>(next, 8) * sign) * 0.01f,
        static_cast<float>(Read<std::int8_t>(next, 9) * sign) * 0.01f, 0.0f};
    if (ulw::DistanceSquared({}, direction) < 0.65f) {
        return 1.0f;
    }
    return ulw::TrafficBendScale(forward, direction, std::hypot(x, y),
        (Read<std::uint8_t>(next, 0x0C) & 3) != 0, wet, driver);
}

void __cdecl OnTrafficPair(const void* front, void* rear, float* limit, float wish) noexcept {
    GameFunction<TrafficPair>(0x42D0E0)(front, rear, limit, wish);
    if (s_TrafficPass.car != rear || !(*limit > 0.0f) || !IsRoadCar(front)) {
        return;
    }
    const auto cap = ulw::FollowingSpeed({*limit, EntityPosition(rear), EntityPosition(front),
        EntityForward(rear), EntityForward(front), Read<ulw::Point>(rear, 0x44), Read<ulw::Point>(front, 0x44),
        s_TrafficPass.driver, Read<float>(front, 0x8C) >= 3100.0f, s_TrafficPass.wet});
    if (cap < *limit) {
        *limit = cap;
        ++s_FollowingLimits;
    }
}

bool AmbientTrafficCar(const void* car) noexcept {
    const auto* const driver = Read<const void*>(car, 0x460);
    const auto style = Read<std::uint8_t>(car, 0x3B9);
    return IsRoadCar(car) && Read<std::uint8_t>(car, 0x4A4) == 1 && Read<std::uint8_t>(car, 0x3BA) == 1 &&
        (style == 0 || style == 1 || style == 5 || style == 6) &&
        !(Read<std::uint8_t>(car, 0x428) & 7) && driver && Read<std::uint8_t>(driver, 0x484) == 1;
}

ulw::DriverProfile DriverForCar(const void* car) noexcept {
    return ulw::TrafficDriver(Read<std::uint16_t>(car, 0x20), Read<std::uint16_t>(car, 0x22), Read<float>(car, 0x8C));
}

void __fastcall OnTrafficHorn(void* car, void*) noexcept {
    const auto native = GameFunction<CarHorn>(0x6A3820);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    // Leave the engine's pedestrian warnings and moving-car horns intact.
    if ((Read<std::uint8_t>(car, 0x3DB) & 3) == 1 && AmbientTrafficCar(car) &&
        Read<float>(car, 0x3C8) <= 3.5f && ulw::DistanceSquared(Read<ulw::Point>(car, 0x44), {}) < 0.035f * 0.035f &&
        StreetActive(now) && GameValue<void*>(VehiclePool)) {
        const auto id = GameFunction<PedReference>(0x54FFC0)(car);
        if (id >= 0 && !(id & 0x80) && !s_TrafficHorns.Allow(static_cast<std::uint32_t>(id), now, DriverForCar(car))) {
            ++s_QueueHornSkips;
            return;
        }
    }
    native(car);
}

float __cdecl OnTrafficSpeed(void* car) noexcept {
    const auto findLimit = GameFunction<TrafficSpeed>(0x434400);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!AmbientTrafficCar(car) || !StreetActive(now)) {
        return findLimit(car);
    }
    const auto driver = DriverForCar(car);
    const auto wet = GameValue<float>(0xC81308);
    const auto previous = s_TrafficPass;
    if (s_FollowingTraffic) {
        s_TrafficPass = {car, driver, wet};
    }
    const auto limit = findLimit(car);
    s_TrafficPass = previous;
    const auto collidedAt = Read<std::uint32_t>(car, 0x3C);
    const auto position = EntityPosition(car);
    const auto forward = EntityForward(car);
    const auto bendScale = s_BendTraffic ? BendSpeedScale(car, position, forward, wet, driver) : 1.0f;
    const auto emergencyScale = YieldSpeedScale(position, forward, Read<ulw::Point>(car, 0x44), now);
    const auto trafficIncident = s_Crashes.Ahead(position, forward, now);
    const auto adjusted = ulw::TrafficLimit({
        limit, static_cast<float>(Read<std::uint8_t>(car, 0x3D0)), wet, Fog(), GameValue<std::uint8_t>(0xB70153),
        collidedAt != 0 && now - collidedAt < 1350 && Read<float>(car, 0xD8) > 5.0f,
        emergencyScale, trafficIncident, driver, bendScale,
    });
    if (adjusted < limit) {
        ++s_RestrictedDrivers;
        s_YieldingDrivers += emergencyScale < 1.0f ? 1u : 0u;
        s_CrashSlowdowns += trafficIncident ? 1u : 0u;
        s_BendLimits += bendScale < 0.98f ? 1u : 0u;
    }
    LogActivity(now);
    return adjusted;
}

ulw::WheelForces TunePlayerWheel(const void* vehicle, ulw::WheelForces forces, int wheel,
    bool damagedTyre, bool motorcycle) noexcept {
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    const auto type = motorcycle ? 9 : 0;
    if (!StreetActive(now) || Read<std::int32_t>(vehicle, 0x594) != type || Read<std::uint8_t>(vehicle, 0x2F) != 0 ||
        Read<std::int8_t>(vehicle, 0x424) >= 0 || Read<std::uint8_t>(vehicle, 0x4A4) == 2 ||
        (Read<std::uint8_t>(vehicle, 0x36) >> 3) != 0 || !GameValue<void*>(VehiclePool)) {
        return forces;
    }
    const auto ref = GameFunction<PedReference>(0x54FFC0)(vehicle);
    if (ref < 0 || (ref & 0x80)) {
        return forces;
    }
    const auto speed = Read<ulw::Point>(vehicle, 0x44);
    const auto wear = motorcycle ? 0.0f : ulw::PanelWearAtWheel(Read<std::uint32_t>(vehicle, 0x5B4), wheel);
    const auto engineDamage = motorcycle ? std::uint8_t{} : Read<std::uint8_t>(vehicle, 0x5A4);
    return s_RoadPhysics.Tune(forces, {
        static_cast<std::uint32_t>(ref), now, Read<float>(vehicle, 0x8C), Read<float>(vehicle, 0x4C0),
        GameValue<float>(0xC81308), ulw::DistanceSquared(speed, {}), Read<float>(vehicle, 0x4A0), damagedTyre,
        Read<float>(vehicle, 0x49C), wear, engineDamage, motorcycle,
    });
}

void __fastcall OnWheel(void* car, void*, void* forward, void* right, void* contactSpeed, void* contact,
    int wheels, float thrust, float brake, float grip, std::int8_t wheel, float* wheelSpeed,
    std::int32_t* wheelState, std::uint16_t status) noexcept {
    const auto forces = TunePlayerWheel(car, {thrust, brake, grip}, wheel, status != 0, false);
    GameFunction<ProcessWheel>(0x6D6C00)(car, forward, right, contactSpeed, contact, wheels,
        forces.thrust, forces.brake, forces.grip, wheel, wheelSpeed, wheelState, status);
}

void __fastcall OnBikeWheel(void* bike, void*, void* forward, void* right, void* contactSpeed, void* contact,
    int wheels, float thrust, float brake, float grip, float destabilize, std::int8_t wheel, float* wheelSpeed,
    std::int32_t* wheelState, std::int32_t special, std::uint16_t status) noexcept {
    const auto forces = TunePlayerWheel(bike, {thrust, brake, grip}, wheel, status != 0, true);
    GameFunction<ProcessBikeWheel>(0x6D73B0)(bike, forward, right, contactSpeed, contact, wheels,
        forces.thrust, forces.brake, forces.grip, destabilize, wheel, wheelSpeed, wheelState, special, status);
}

bool CivilianActive(std::uint32_t now) noexcept {
    if (!FreeRoam()) {
        if (s_WasActive) {
            s_Core.Reset();
            s_WasActive = false;
        }
        return false;
    }
    if (now < s_LastGameTime) {
        s_Core.Reset();
        s_LastLogTime = now;
    }
    s_WasActive = true;
    s_LastGameTime = now;
    return true;
}

void* MakeResponseTask(const ulw::Decision& decision) noexcept {
    if (decision.response == ulw::Response::Vanilla) {
        return nullptr;
    }
    const bool look = decision.response == ulw::Response::Look;
    auto* const task = GameFunction<AllocateTask>(0x61A5A0)(look ? 0x28 : 0x44);
    if (!task) {
        ++s_AllocationFailures;
        return nullptr;
    }
    ++s_Reactions[static_cast<unsigned>(decision.response)];
    if (look) {
        return GameFunction<TurnToPoint>(0x66B910)(task, decision.focus, 0.5f, 0.2f);
    }
    const bool panic = decision.response == ulw::Response::Panic;
    return GameFunction<FleePoint>(0x65BD20)(task, decision.focus, panic, panic ? 30.0f : 22.0f, panic ? 4800 : 2900);
}

void* ChooseReaction(const void* handler, const void* shot) noexcept {
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!CivilianActive(now)) {
        return nullptr;
    }
    const auto* const ped = Read<const void*>(handler);
    const auto* const shooter = Read<const void*>(shot, 0x14);
    if (!ped || !shooter || shooter == ped || !PedOnFoot(ped) ||
        Read<std::uint8_t>(shot, 0x30) || Read<std::uint8_t>(shooter, 0x2F) != 0) {
        return nullptr;
    }
    const auto type = Read<std::uint32_t>(ped, 0x598);
    const auto selectedTask = Read<std::int16_t>(shot, 0x0E);
    // Preserve dispatch for armed responses, crime handling and other
    // special decisions. These are NONE, SMART_FLEE_ENTITY and TURN.
    if ((type != 4 && type != 5) || (selectedTask != 200 && selectedTask != 911 && selectedTask != 920)) {
        return nullptr;
    }
    const auto position = EntityPosition(ped);
    const auto source = Read<ulw::Point>(shot, 0x18);
    const auto dx = source.x - position.x;
    const auto dy = source.y - position.y;
    const auto dz = source.z - position.z;
    const auto distance = dx * dx + dy * dy + dz * dz;
    if (!(distance <= 42.0f * 42.0f) || std::abs(dz) > 14.0f ||
        !s_Core.TakeBudget(GameValue<std::uint32_t>(0xB7CB4C))) {
        return nullptr;
    }
    if (!GameValue<void*>(PedPool) || !GameValue<void*>(TaskPool)) {
        return nullptr;
    }
    const auto reference = GameFunction<PedReference>(0x54FF60)(ped);
    if (reference < 0 || (reference & 0x80) != 0) {
        return nullptr;
    }

    const auto forward = EntityForward(ped);
    const auto dot = forward.x * dx + forward.y * dy;
    const bool facing = dot > 0.0f && dot * dot >= 0.35f * (dx * dx + dy * dy);
    const ulw::Point eye{position.x, position.y, position.z + 0.65f};
    const auto lineClear = GameFunction<LineClear>(0x56A490);
    const auto sight = ulw::SightRange(GameValue<float>(0xC81308), Fog(), GameValue<std::uint8_t>(0xB70153));
    const bool visible = facing && distance <= sight * sight && lineClear(eye, source, true, true, false, true, true, false, false);
    return MakeResponseTask(s_Core.ReactToShot({static_cast<std::uint32_t>(reference), now, source, distance, visible}));
}

void* ChooseFireReaction(const void* handler, const void* fire) noexcept {
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!CivilianActive(now) || Read<std::int16_t>(fire, 0x0E) != 200 ||
        !GameValue<void*>(PedPool) || !GameValue<void*>(TaskPool)) {
        return nullptr;
    }
    const auto* const ped = Read<const void*>(handler);
    if (!ped || !PedOnFoot(ped)) {
        return nullptr;
    }
    const auto type = Read<std::uint32_t>(ped, 0x598);
    const auto source = Read<ulw::Point>(fire, 0x14);
    auto origin = EntityPosition(ped);
    const auto distance = ulw::DistanceSquared(origin, source);
    if ((type != 4 && type != 5) || !(distance <= 18.0f * 18.0f) ||
        std::abs(origin.z - source.z) > 3.0f || !s_Core.TakeBudget(GameValue<std::uint32_t>(0xB7CB4C))) {
        return nullptr;
    }
    const auto reference = GameFunction<PedReference>(0x54FF60)(ped);
    if (reference < 0 || (reference & 0x80)) {
        return nullptr;
    }
    origin.z += 0.65f;
    const auto visible = GameFunction<LineClear>(0x56A490)(origin, source, true, true, false, true, true, false, false);
    return MakeResponseTask(s_Core.ReactToHazard({static_cast<std::uint32_t>(reference), now, source, distance, visible}));
}

void __fastcall OnFire(void* handler, void*, const void* fire, void* activeTask, void* simplestTask) noexcept {
    if (auto* const task = ChooseFireReaction(handler, fire)) {
        std::memcpy(static_cast<std::byte*>(handler) + 0x24, &task, sizeof(task));
    } else {
        GameFunction<ShotResponse>(NativeFireResponse)(handler, fire, activeTask, simplestTask);
    }
}

void RecordShotForPolice(const void* handler, const void* shot, std::uint32_t now) noexcept {
    if (!s_PoliceWitnesses || !StreetActive(now)) {
        return;
    }
    const auto* const observer = Read<const void*>(handler);
    const auto* const shooter = Read<const void*>(shot, 0x14);
    if (!observer || !shooter || shooter == observer || (Read<std::uint8_t>(shooter, 0x36) & 7) != 3 ||
        Read<std::uint8_t>(shooter, 0x2F) != 0 || !PedOnFoot(observer)) {
        return;
    }
    const auto type = Read<unsigned>(observer, 0x598);
    if (type != 4 && type != 5 && type != 6) {
        return;
    }
    const auto source = Read<ulw::Point>(shot, 0x18);
    const auto position = EntityPosition(observer);
    const bool cop = type == 6;
    const auto maximumRange = cop ? 37.0f : 25.0f;
    if (!(ulw::DistanceSquared(position, source) <= maximumRange * maximumRange) ||
        !(std::abs(position.z - source.z) <= 5.0f)) {
        return;
    }
    const auto ref = StreetPedReference(observer);
    if (ref < 0 || (!cop && (static_cast<unsigned>(ref) >> 8) % 3 != 0) ||
        !s_PoliceIncidents.TakeProbe(static_cast<std::uint32_t>(ref), now, GameValue<std::uint32_t>(0xB7CB4C))) {
        return;
    }
    const auto sight = ulw::SightRange(GameValue<float>(0xC81308), Fog(), GameValue<std::uint8_t>(0xB70153));
    const bool visible = VisibleStreetPoint(observer, position, source, std::min(maximumRange, sight), false);
    if (!visible && (!cop || Read<bool>(shot, 0x30))) {
        return;
    }
    const auto kind = visible ? (cop ? ulw::IncidentKind::OfficerSight : ulw::IncidentKind::Witness)
        : ulw::IncidentKind::GunshotSound;
    if (s_PoliceIncidents.Record(kind, static_cast<std::uint32_t>(ref), source, now, IsPlayerPed(shooter))) {
        ++s_PoliceReports;
    }
}

void __fastcall OnShot(void* handler, void*, const void* shot, void* activeTask, void* simplestTask) noexcept {
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    RecordShotForPolice(handler, shot, now);
    auto* const task = s_CivilianShots ? ChooseReaction(handler, shot) : nullptr;
    if (task) {
        // The native dispatcher takes ownership through this response slot.
        std::memcpy(static_cast<std::byte*>(handler) + 0x24, &task, sizeof(task));
    } else {
        GameFunction<ShotResponse>(NativeShotResponse)(handler, shot, activeTask, simplestTask);
    }
    LogActivity(now);
}

bool PatchCalls(std::initializer_list<std::uintptr_t> sites, std::uintptr_t original, std::uintptr_t entry) noexcept {
    std::array<DWORD, 4> protections{};
    if (sites.size() > protections.size()) {
        return false;
    }
    for (const auto site : sites) {
        std::array<std::uint8_t, 5> expected{0xE8};
        const auto displacement = original - (site + expected.size());
        std::memcpy(expected.data() + 1, &displacement, sizeof(displacement));
        if (!Matches(site, expected)) {
            return false;
        }
    }
    std::size_t opened{};
    for (const auto site : sites) {
        if (!VirtualProtect(reinterpret_cast<void*>(site + 1), 4, PAGE_EXECUTE_READWRITE, &protections[opened])) {
            break;
        }
        ++opened;
    }
    const bool ready = opened == sites.size();
    if (ready) {
        for (const auto site : sites) {
            auto* const operand = reinterpret_cast<void*>(site + 1);
            const auto displacement = entry - (site + 5);
            std::memcpy(operand, &displacement, sizeof(displacement));
            if (!FlushInstructionCache(GetCurrentProcess(), operand, sizeof(displacement))) {
                Log("Warning: Windows did not confirm instruction cache refresh.");
            }
        }
    }
    // Calls can share a page. Restore it to its initial protection last.
    while (opened) {
        --opened;
        DWORD previous{};
        if (!VirtualProtect(reinterpret_cast<void*>(sites.begin()[opened] + 1), 4, protections[opened], &previous)) {
            Log("Warning: Windows did not restore a game code page.");
        }
    }
    return ready;
}

bool PatchTaskMethod(std::uintptr_t slot, std::uintptr_t original, std::uintptr_t replacement) noexcept {
    std::array<std::uint8_t, sizeof(original)> expected{};
    std::memcpy(expected.data(), &original, sizeof(original));
    if (!Matches(slot, expected)) {
        return false;
    }
    auto* const method = reinterpret_cast<void*>(slot);
    DWORD protection{};
    if (!VirtualProtect(method, sizeof(replacement), PAGE_READWRITE, &protection)) {
        return false;
    }
    std::memcpy(method, &replacement, sizeof(replacement));
    DWORD unused{};
    if (!VirtualProtect(method, sizeof(replacement), protection, &unused)) {
        Log("Warning: Windows did not restore a game task table page.");
    }
    return true;
}

using Entry = std::array<std::uint8_t, 8>;

bool PedQueriesMatch() noexcept {
    return Matches(0x54FF60, Entry{0x8B, 0x4C, 0x24, 0x04, 0x56, 0x8B, 0x35, 0x90}) &&
        Matches(0x56A490, Entry{0x83, 0xEC, 0x5C, 0x66, 0x81, 0x3D, 0x78, 0xCD});
}

bool VehicleQueriesMatch() noexcept {
    return Matches(0x54FFC0, Entry{0x8B, 0x4C, 0x24, 0x04, 0x56, 0x8B, 0x35, 0x94});
}

bool InstallShotDispatch() noexcept {
    constexpr std::array<std::uint8_t, 15> dispatch{
        0x55, 0x53, 0x57, 0x8B, 0xCE, 0xE8, 0x5F, 0x8D, 0xFF, 0xFF, 0xE9, 0xFE, 0x02, 0x00, 0x00,
    };
    return Matches(0x4C39A7, dispatch) &&
        Matches(NativeShotResponse, Entry{0x64, 0xA1, 0, 0, 0, 0, 0x6A, 0xFF}) &&
        PedQueriesMatch() &&
        PatchCalls({ShotDispatchCall}, NativeShotResponse, reinterpret_cast<std::uintptr_t>(&OnShot));
}

bool CivilianTasksMatch() noexcept {
    return Matches(0x4BCB3A, std::array<std::uint8_t, 3>{0x89, 0x47, 0x24}) &&
        Matches(0x61A5A0, Entry{0x8B, 0x0D, 0xA8, 0x44, 0xB7, 0x00, 0xE9, 0x55}) &&
        Matches(0x65BD20, Entry{0x56, 0x8B, 0xF1, 0xE8, 0x88, 0xE6, 0xFB, 0xFF}) &&
        Matches(0x66B910, Entry{0x56, 0x8B, 0xF1, 0xE8, 0x98, 0xEA, 0xFA, 0xFF});
}

bool InstallPoliceCruise() noexcept {
    return PedQueriesMatch() && VehicleQueriesMatch() &&
        Matches(0x56E230, Entry{0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7D, 0x07}) &&
        Matches(0x56E0D0, Entry{0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7D, 0x07}) &&
        Matches(0x426EF0, Entry{0x83, 0xEC, 0x40, 0x53, 0x55, 0x56, 0x57, 0x66}) &&
        Matches(0x42DE80, Entry{0x83, 0xEC, 0x44, 0x56, 0x8B, 0x74, 0x24, 0x4C}) &&
        PatchCalls({0x432C41}, 0x42DE80, reinterpret_cast<std::uintptr_t>(&OnPatrolCruiseNode));
}

bool InstallSurrender() noexcept {
    return PedQueriesMatch() &&
        Matches(0x56E230, Entry{0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7D, 0x07}) &&
        Matches(NativeArrestControl, Entry{0x83, 0xEC, 0x2C, 0x55, 0x8B, 0x6C, 0x24, 0x34}) &&
        Matches(0x68CF80, Entry{0x64, 0xA1, 0, 0, 0, 0, 0x6A, 0xFF}) &&
        Matches(0x68D283, std::array<std::uint8_t, 5>{0xE8, 0x98, 0xE3, 0xFF, 0xFF}) &&
        PatchTaskMethod(ArrestTaskVtable + 10 * sizeof(void*), NativeArrestControl,
            reinterpret_cast<std::uintptr_t>(&OnArrestControl));
}

bool InstallPolice() noexcept {
    return PedQueriesMatch() &&
        Matches(0x46FEE0, Entry{0x83, 0xEC, 0x0C, 0x56, 0x8B, 0xC1, 0x8B, 0x50}) &&
        Matches(0x56E230, Entry{0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7D, 0x07}) &&
        PatchCalls({0x46FC3A}, 0x46FEE0, reinterpret_cast<std::uintptr_t>(&OnSeek));
}

bool InstallPatrolRoutes() noexcept {
    const bool queries = PedQueriesMatch() && VehicleQueriesMatch() &&
        Matches(0x56E230, Entry{0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7D, 0x07});
    const bool routes = queries &&
        Matches(0x426EF0, Entry{0x83, 0xEC, 0x40, 0x53, 0x55, 0x56, 0x57, 0x66}) &&
        Matches(0x42DE80, Entry{0x83, 0xEC, 0x44, 0x56, 0x8B, 0x74, 0x24, 0x4C}) &&
        PatchCalls({0x432BD9}, 0x426EF0, reinterpret_cast<std::uintptr_t>(&OnChaseNode));
    const bool steering = queries &&
        Matches(0x4335E0, Entry{0xD9, 0x44, 0x24, 0x18, 0xD9, 0x44, 0x24, 0x14}) &&
        PatchCalls({0x4372A2}, 0x4335E0, reinterpret_cast<std::uintptr_t>(&OnBlockSteering));
    Log(routes ? "Installed: patrol search routes." : "Skipped: pursuit route function differs.");
    Log(steering ? "Installed: patrol block steering." : "Skipped: pursuit steering function differs.");
    return routes || steering;
}

bool InstallAirSupport() noexcept {
    const bool flight =
        PedQueriesMatch() && VehicleQueriesMatch() &&
        Matches(0x56E230, Entry{0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7D, 0x07}) &&
        Matches(0x42AAD0, Entry{0x83, 0xEC, 0x10, 0x53, 0x55, 0x8B, 0x6C, 0x24}) &&
        Matches(0x429A70, Entry{0x81, 0xEC, 0x90, 0x00, 0x00, 0x00, 0x53, 0x56}) &&
        Matches(0x53CC70, Entry{0xD9, 0x44, 0x24, 0x04, 0xD8, 0x1D, 0x50, 0x8B}) &&
        PatchCalls({0x42AC81}, 0x429A70, reinterpret_cast<std::uintptr_t>(&OnPoliceHeliFlight));
    Log(flight ? "Installed: police helicopter search." : "Skipped: police flight function differs.");
    const bool searchlight = flight &&
        Matches(0x6C54C0, std::array<std::uint8_t, 4>{0x8D, 0x56, 0x0B, 0x52}) &&
        Matches(0x6C45B0, Entry{0x8B, 0x15, 0x6C, 0xC9, 0xC1, 0x00, 0x8B, 0x4C}) &&
        PatchCalls({0x6C54F7}, 0x6C45B0, reinterpret_cast<std::uintptr_t>(&OnSearchLight));
    Log(searchlight ? "Installed: police searchlight tracks search reports." : "Skipped: police searchlight function differs.");
    return flight;
}

bool InstallRoadblocks() noexcept {
    return Matches(0x4629E0, Entry{0x6A, 0xFF, 0x68, 0x73, 0x7C, 0x83, 0x00, 0x64}) &&
        Matches(0x460EE0, Entry{0x8B, 0x44, 0x24, 0x04, 0x0F, 0xB7, 0x4C, 0x24}) &&
        Matches(0x56E0D0, Entry{0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7D, 0x07}) &&
        Matches(0x56E230, Entry{0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x7D, 0x07}) &&
        Matches(0x56A490, Entry{0x83, 0xEC, 0x5C, 0x66, 0x81, 0x3D, 0x78, 0xCD}) &&
        PatchCalls({0x462BFF}, 0x460EE0, reinterpret_cast<std::uintptr_t>(&OnRoadblockNode));
}

bool InstallMissionObserver() noexcept {
    return Matches(0x46A000, Entry{0xA0, 0x88, 0x30, 0xA4, 0x00, 0x83, 0xEC, 0x10}) &&
        Matches(0x46A1D1, std::array<std::uint8_t, 6>{0x8B, 0x35, 0x2C, 0xB4, 0xA8, 0x00}) &&
        Matches(0x46A226, std::array<std::uint8_t, 6>{0x38, 0x9F, 0xC4, 0x00, 0x00, 0x00}) &&
        Matches(0x469F60, std::array<std::uint8_t, 6>{0x8A, 0x86, 0xDC, 0x00, 0x00, 0x00}) &&
        PatchCalls({0x53BFC7}, 0x46A000, reinterpret_cast<std::uintptr_t>(&OnProcessScripts));
}

bool InstallTraffic() noexcept {
    const auto speed = Matches(0x434400, Entry{0x83, 0xEC, 0x24, 0x56, 0x8B, 0x74, 0x24, 0x2C}) &&
        PatchCalls({0x4347C3, 0x435302}, 0x434400, reinterpret_cast<std::uintptr_t>(&OnTrafficSpeed));
    s_BendTraffic = speed &&
        Matches(0x42EA06, std::array<std::uint8_t, 7>{0x03, 0x0C, 0x95, 0x74, 0xF9, 0x96, 0x00});
    Log(s_BendTraffic ? "Installed: road geometry speed limits." : "Skipped: road link layout differs.");
    s_FollowingTraffic = speed &&
        Matches(0x42D0E0, Entry{0x83, 0xEC, 0x30, 0x53, 0x55, 0x56, 0x57, 0x8B}) &&
        Matches(0x432599, std::array<std::uint8_t, 4>{0x52, 0x50, 0x56, 0x55}) &&
        PatchCalls({0x43259D}, 0x42D0E0, reinterpret_cast<std::uintptr_t>(&OnTrafficPair));
    Log(s_FollowingTraffic ? "Installed: driver-specific following gaps." : "Skipped: traffic pair function differs.");
    const auto horns = VehicleQueriesMatch() &&
        Matches(0x6A3820, Entry{0x56, 0x8B, 0xF1, 0xF6, 0x86, 0xDB, 0x03, 0x00}) &&
        PatchCalls({0x6B5275, 0x6B52B8}, 0x6A3820, reinterpret_cast<std::uintptr_t>(&OnTrafficHorn));
    Log(horns ? "Installed: queue horn cooldowns." : "Skipped: car horn function differs.");
    return speed;
}

bool InstallServiceApproach() noexcept {
    return Matches(0x437C20, Entry{0x83, 0xEC, 0x34, 0x56, 0x8B, 0x74, 0x24, 0x3C}) &&
        Matches(0x436A90, Entry{0x81, 0xEC, 0xE4, 0x00, 0x00, 0x00, 0x55, 0x56}) &&
        Matches(0x437E39, std::array<std::uint8_t, 6>{0x8D, 0x44, 0x24, 0x1C, 0x50, 0x56}) &&
        PatchCalls({0x437E3F}, 0x436A90, reinterpret_cast<std::uintptr_t>(&OnServiceApproach));
}

bool InstallCarObservations() noexcept {
    return VehicleQueriesMatch() &&
        Matches(0x41DA30, Entry{0x90, 0xE9, 0x12, 0x62, 0xFE, 0xFF, 0x6A, 0xFF}) &&
        PatchCalls({0x6B51CD, 0x6B52A2}, 0x41DA30, reinterpret_cast<std::uintptr_t>(&OnCarAI));
}

bool InstallPhysics() noexcept {
    return VehicleQueriesMatch() &&
        Matches(0x6D6C00, Entry{0x83, 0xEC, 0x48, 0xD9, 0x05, 0x50, 0x8B, 0x85}) &&
        PatchCalls({0x6A567B, 0x6A56E9, 0x6A5B49, 0x6A5BB7}, 0x6D6C00,
            reinterpret_cast<std::uintptr_t>(&OnWheel));
}

bool InstallMotorcyclePhysics() noexcept {
    return VehicleQueriesMatch() &&
        Matches(0x6B9250, Entry{0x6A, 0xFF, 0x68, 0xEB, 0x82, 0x84, 0x00, 0x64}) &&
        Matches(0x6D73B0, Entry{0x83, 0xEC, 0x40, 0xD9, 0x05, 0x50, 0x8B, 0x85}) &&
        PatchCalls({0x6BAF3D, 0x6BB4C1, 0x6BB855}, 0x6D73B0, reinterpret_cast<std::uintptr_t>(&OnBikeWheel));
}

void InstallCombat() noexcept {
    const bool aiming = Matches(0x742300, Entry{0x83, 0xEC, 0x3C, 0x53, 0x56, 0x57, 0x8B, 0xF1}) &&
        Matches(0x73FBFD, std::array<std::uint8_t, 7>{0x0F, 0xB6, 0x97, 0x1A, 0x07, 0, 0}) &&
        PatchCalls({0x61ECCD}, 0x742300, reinterpret_cast<std::uintptr_t>(&OnWeaponFire));
    Log(aiming ? "Installed: police and gang aiming profiles." : "Skipped: weapon function differs.");
    const bool bursts = Matches(NativeGunTask, Entry{0x6A, 0xFF, 0x64, 0xA1, 0, 0, 0, 0}) &&
        Matches(0x62AB2B, Entry{0x66, 0x8B, 0x4E, 0x34, 0x66, 0x89, 0x4E, 0x36}) &&
        PatchTaskMethod(GunTaskVtable + 7 * sizeof(void*), NativeGunTask,
            reinterpret_cast<std::uintptr_t>(&OnGunTask));
    Log(bursts ? "Installed: police and gang automatic bursts." : "Skipped: gun task differs.");
    s_PressureObserver = PedQueriesMatch() &&
        Matches(NativeDamageResponse, Entry{0x64, 0xA1, 0, 0, 0, 0, 0x6A, 0xFF}) &&
        PatchCalls({0x4C3961}, NativeDamageResponse, reinterpret_cast<std::uintptr_t>(&OnDamageResponse));
    Log(s_PressureObserver ? "Installed: gunfire suppression from player hits." : "Skipped: damage dispatcher differs.");
    const bool cover = s_PressureObserver &&
        Matches(NativeArmedControl, Entry{0x83, 0xEC, 0x30, 0x53, 0x55, 0x56, 0x57, 0x8B}) &&
        Matches(ArmedTaskVtable + 8 * sizeof(void*), std::array<std::uint8_t, 4>{0x90, 0xC1, 0x62, 0}) &&
        Matches(0x62C3DF, Entry{0x57, 0x8B, 0xCE, 0xE8, 0x19, 0x51, 0xFF, 0xFF}) &&
        Matches(0x6992B0, Entry{0x83, 0xEC, 0x28, 0xD9, 0x05, 0x9C, 0x99, 0x85}) &&
        Matches(0x699570, Entry{0x8B, 0x44, 0x24, 0x04, 0x8B, 0x50, 0x14, 0x81}) &&
        Matches(0x698EF0, Entry{0x53, 0x8B, 0x5C, 0x24, 0x08, 0x56, 0x57, 0x8D}) &&
        Matches(0x56BA00, Entry{0x83, 0xEC, 0x60, 0x66, 0x81, 0x3D, 0x78, 0xCD}) &&
        Matches(0x5696C0, Entry{0x83, 0xEC, 0x38, 0x8B, 0x44, 0x24, 0x3C, 0x8B}) &&
        Matches(0x61A5A0, Entry{0x8B, 0x0D, 0xA8, 0x44, 0xB7, 0x00, 0xE9, 0x55}) &&
        Matches(0x667CD0, Entry{0x56, 0x8B, 0xF1, 0xE8, 0xB8, 0x26, 0xFB, 0xFF}) &&
        PatchTaskMethod(ArmedTaskVtable + 10 * sizeof(void*), NativeArmedControl,
            reinterpret_cast<std::uintptr_t>(&OnArmedControl));
    Log(cover ? "Installed: pressure-triggered cover movement." : "Skipped: armed cover task or pressure observer differs.");
    s_CoverFollowup = cover &&
        Matches(NativeArmedNext, Entry{0x6A, 0xFF, 0x64, 0xA1, 0, 0, 0, 0}) &&
        Matches(0x62F310, Entry{0x56, 0x8B, 0xF1, 0xE8, 0x78, 0xB0, 0xFE, 0xFF}) &&
        Matches(0x5E0270, Entry{0x56, 0x8B, 0xF1, 0x8B, 0x8E, 0x88, 0x07, 0x00}) &&
        Matches(0x626FC0, Entry{0x6A, 0xFF, 0x64, 0xA1, 0, 0, 0, 0}) &&
        PatchTaskMethod(ArmedTaskVtable + 8 * sizeof(void*), NativeArmedNext,
            reinterpret_cast<std::uintptr_t>(&OnArmedNext));
    Log(s_CoverFollowup ? "Installed: cover holds, lateral moves and withdrawals." : "Skipped: armed follow-up task differs.");
}

void InstallCity() noexcept {
    const bool pedestrians = Matches(0x616650, Entry{0x0F, 0xBF, 0x05, 0x14, 0x13, 0xC8, 0, 0x83}) &&
        PatchCalls({0x53C030}, 0x616650, reinterpret_cast<std::uintptr_t>(&OnPopulation));
    const bool cars = Matches(0x4341C0, Entry{0xA0, 0x51, 0xF8, 0xB5, 0, 0x84, 0xC0, 0x74}) &&
        PatchCalls({0x53C1C1}, 0x4341C0, reinterpret_cast<std::uintptr_t>(&OnRandomCars));
    const bool attractors = Matches(0x614210, Entry{0x64, 0xA1, 0, 0, 0, 0, 0xD9, 0x05}) &&
        PatchCalls({0x615C0D}, 0x614210, reinterpret_cast<std::uintptr_t>(&OnAttractorPed));
    Log(pedestrians ? "Installed: ambient pedestrian schedules." : "Skipped: population function differs.");
    Log(cars ? "Installed: ambient car schedules." : "Skipped: car generation function differs.");
    Log(attractors ? "Installed: neighborhood activity schedules." : "Skipped: pedestrian attractor function differs.");
}

bool InstallStreetPedestrians() noexcept {
    const bool routines = Matches(0x674C30, Entry{0x83, 0xEC, 0x0C, 0x55, 0x56, 0x8B, 0xF1, 0x8B}) &&
        PatchTaskMethod(StandardWanderVtable + 10 * sizeof(void*), 0x674C30,
            reinterpret_cast<std::uintptr_t>(&OnWanderControl));
    Log(routines ? "Installed: civilian walking routines." : "Skipped: civilian wander task differs.");
    const bool social = Matches(0x672600, Entry{0x56, 0x8B, 0xF1, 0x8A, 0x46, 0x30, 0x84, 0xC0}) &&
        Matches(0x66FDA0, Entry{0x6A, 0xFF, 0x68, 0x90, 0x55, 0x84, 0x00, 0x64}) &&
        PatchCalls({0x672672}, 0x66FDA0, reinterpret_cast<std::uintptr_t>(&OnChatPartner));
    Log(social ? "Installed: civilian conversation timing." : "Skipped: civilian conversation function differs.");
    s_CrashGlancesEnabled = routines && PedQueriesMatch() &&
        Matches(0x5DEDC0, Entry{0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x48, 0x07, 0x00}) &&
        Matches(0x5DF8D0, Entry{0xA1, 0x84, 0xCB, 0xB7, 0x00, 0x3B, 0x81, 0x48});
    Log(s_CrashGlancesEnabled ? "Installed: civilian crash attention." : "Skipped: civilian looking function differs.");
    const bool reroute = Matches(0x674560, Entry{0x83, 0xEC, 0x08, 0x56, 0x8B, 0xF1, 0x8B, 0x4E}) &&
        Matches(0x671EF0, Entry{0x83, 0xEC, 0x18, 0x56, 0x8B, 0x74, 0x24, 0x20}) &&
        PatchCalls({0x67458F, 0x6745E1}, 0x671EF0, reinterpret_cast<std::uintptr_t>(&OnFootpathBlocked));
    Log(reroute ? "Installed: crash-aware footpaths." : "Skipped: footpath obstacle function differs.");
    return s_CrashGlancesEnabled || reroute;
}

void Install(const std::filesystem::path& config) noexcept {
    if (!SupportedImage()) {
        Log("Skipped: this executable does not match the supported Hoodlum 1.0 US layout.");
        return;
    }
    if (GameValue<void*>(PedPool)) {
        Log("Skipped: the ASI loader initialized after game pool creation.");
        return;
    }

    // The loader calls this before gameplay. Pin the image so the
    // dispatcher can never retain an address into an unloaded DLL.
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&OnShot), &pinned)) {
        Log("Skipped: could not pin the module.");
        return;
    }
    Log("Ultimate Living World 0.14.0");
    s_MissionObserver = InstallMissionObserver();
    Log(s_MissionObserver ? "Installed: mission lifecycle protection and diagnostics."
        : "Skipped: mission observer differs. Native mission flags still protect the campaign.");
    const bool civilianEnabled = GetPrivateProfileIntW(L"Modules", L"Civilian", 1, config.c_str()) != 0;
    const bool policeEnabled = GetPrivateProfileIntW(L"Modules", L"Police", 1, config.c_str()) != 0;
    const bool civilianTasks = civilianEnabled && CivilianTasksMatch();
    const bool witnessQuery = policeEnabled &&
        Matches(0x54FF90, Entry{0x8B, 0x4C, 0x24, 0x04, 0x8B, 0x15, 0x90, 0x44});
    const bool shotDispatch = (civilianTasks || witnessQuery) && InstallShotDispatch();
    s_CivilianShots = civilianTasks && shotDispatch;
    s_PoliceWitnesses = witnessQuery && shotDispatch;
    if (civilianEnabled) {
        Log(s_CivilianShots ? "Installed: civilian gunshot reactions." : "Skipped: civilian dispatcher differs.");
        const bool fire = civilianTasks && PedQueriesMatch() &&
            Matches(NativeFireResponse, Entry{0x64, 0xA1, 0, 0, 0, 0, 0x6A, 0xFF}) &&
            PatchCalls({0x4C3C73}, NativeFireResponse, reinterpret_cast<std::uintptr_t>(&OnFire));
        Log(fire ? "Installed: civilian fire reactions." : "Skipped: fire dispatcher differs.");
    }
    if (policeEnabled) {
        s_PoliceHud = Matches(0x588BE0, Entry{0xA0, 0xC0, 0xAE, 0xBA, 0, 0x84, 0xC0, 0x0F});
        Log(s_PoliceWitnesses ? "Installed: gunshot leads and witness reports." : "Skipped: police witness dispatcher differs.");
        const auto foot = InstallPolice();
        Log(foot ? "Installed: foot patrol sight and search." : "Skipped: police seek function differs.");
        const auto cars = InstallPatrolRoutes();
        s_UseSightingForRoadblocks = foot || cars;
        s_PoliceCruise = InstallPoliceCruise();
        Log(s_PoliceCruise ? "Installed: civilian driving warnings and patrol investigations."
            : "Skipped: patrol cruise dispatcher differs.");
        Log(InstallSurrender() ? "Installed: voluntary arrest on F9." : "Skipped: arrest task differs.");
    }
    if (GetPrivateProfileIntW(L"Modules", L"AirSupport", 1, config.c_str())) {
        const auto air = InstallAirSupport();
        s_UseSightingForRoadblocks = s_UseSightingForRoadblocks || air;
    }
    if (GetPrivateProfileIntW(L"Modules", L"Roadblocks", 1, config.c_str())) {
        Log(InstallRoadblocks() ? "Installed: dynamic roadblock placement filter." : "Skipped: roadblock function differs.");
    }
    bool traffic{};
    if (GetPrivateProfileIntW(L"Modules", L"Traffic", 1, config.c_str())) {
        traffic = InstallTraffic();
        Log(traffic ? "Installed: traffic speed limits." : "Skipped: traffic speed function differs.");
    }
    if (GetPrivateProfileIntW(L"Modules", L"Emergency", 1, config.c_str())) {
        Log(InstallServiceApproach() ? "Installed: ambulance and fire crew approach." : "Skipped: emergency driving function differs.");
    }
    if (GetPrivateProfileIntW(L"Modules", L"VehiclePhysics", 1, config.c_str())) {
        Log(InstallPhysics() ? "Installed: automobile grip, damage and heat." : "Skipped: automobile wheel call sites differ.");
        Log(InstallMotorcyclePhysics() ? "Installed: motorcycle grip and heat." : "Skipped: motorcycle wheel call sites differ.");
    }
    if (GetPrivateProfileIntW(L"Modules", L"Combat", 1, config.c_str())) {
        InstallCombat();
    }
    bool streetPedestrians{};
    if (GetPrivateProfileIntW(L"Modules", L"City", 1, config.c_str())) {
        InstallCity();
        streetPedestrians = InstallStreetPedestrians();
    }
    if (traffic || streetPedestrians || s_PoliceCruise) {
        Log(InstallCarObservations() ? "Installed: siren and collision observations." : "Skipped: car observations.");
    }
}

}

extern "C" __declspec(dllexport) void __cdecl InitializeASI() noexcept {
    static bool initialized{};
    if (initialized) {
        return;
    }
    initialized = true;
    try {
        HMODULE module{};
        wchar_t path[MAX_PATH]{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&InitializeASI), &module)) {
            return;
        }
        const auto length = GetModuleFileNameW(module, path, MAX_PATH);
        if (!length || length >= MAX_PATH) {
            return;
        }
        const auto directory = std::filesystem::path(path).parent_path();
        const auto config = directory / L"UltimateLivingWorld.ini";
        s_LogPath = directory / L"UltimateLivingWorld.log";
        if (!GetPrivateProfileIntW(L"General", L"Enabled", 1, config.c_str())) {
            Log("Disabled in UltimateLivingWorld.ini.");
            return;
        }
        s_Diagnostics = GetPrivateProfileIntW(L"General", L"Diagnostics", 1, config.c_str()) != 0;
        Install(config);
    } catch (...) {
        Log("Initialization failed while preparing the configuration or log.");
    }
}
