#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "IntelligenceCore.h"
#include "CityLife.h"
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
constexpr std::uintptr_t RoadNodes = 0x96F854;
constexpr std::uintptr_t RoadNodeCounts = 0x970114;
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
constexpr std::uintptr_t NativeWanderNext = 0x674140;
constexpr std::uintptr_t NativeWanderFirst = 0x6740E0;
constexpr std::uintptr_t WalkPointVtable = 0x86FD50;
constexpr std::uintptr_t SurfaceFlags = 0xB795CC;
constexpr std::uintptr_t NativeFlight = 0x6D85F0;
constexpr std::uintptr_t NativeBoat = 0x6DBCE0;
constexpr std::size_t FlightHandlingOffset = 0x388;
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
using LaneOffset = float(__thiscall*)(const void*);
using TrafficLight = bool(__cdecl*)(const void*, bool);
using BridgeLight = bool(__cdecl*)(const void*);
using CollisionModel = const void*(__thiscall*)(const void*);
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
using NearbyEntities = void(__cdecl*)(const ulw::Point&, float, bool, std::int16_t*, std::int16_t,
    void**, bool, bool, bool, bool, bool);
using ModelEffect = const void*(__thiscall*)(const void*, int);
using AttractorManager = void*(__cdecl*)();
using EmptyAttractorSlot = bool(__thiscall*)(void*, const void*, const void*);
using UseAttractor = void*(__thiscall*)(void*, const void*, void*);
using UsePhone = void*(__thiscall*)(void*, int);
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
using FlyingControl = void(__thiscall*)(void*, int, float, float, float, float);
using ProcessBoatControl = void(__thiscall*)(void*, const void*, float*, bool, bool);
using SpringCollision = bool(__thiscall*)(void*, float, void*, void*, float, float, float*);
using SpringCollisionAlt = bool(__thiscall*)(void*, float, void*, void*, float, float, void*, float*);
using SpringDamping = bool(__thiscall*)(void*, float, float, void*, void*, void*);
using DriveAcceleration = float(__thiscall*)(void*, const float*, std::uint8_t*, float*, float*, float*, float*,
    std::uint8_t, std::uint8_t);

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
ulw::TrafficLaneChanges s_LaneChanges{};
ulw::RoadblockAttempts s_Roadblocks{};
ulw::CrashScenes s_Crashes{};
ulw::CrashWitnesses s_CrashWitnesses{};
ulw::CityLife s_CityLife{};
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
struct FlightGround {
    int vehicle{-1};
    std::uint32_t time{};
    ulw::Point position{};
    float height{};
    bool found{};
};
FlightGround s_FlightGround{};
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
unsigned s_LanePasses{};
unsigned s_LaneReturns{};
unsigned s_PoliceReports{};
unsigned s_SuspicionWarnings{};
unsigned s_InvestigationRoutes{};
unsigned s_VoluntaryArrests{};
unsigned s_CityVisits{};
unsigned s_CrashWaits{};
unsigned s_CrashDepartures{};
unsigned s_CrashCalls{};
unsigned s_SurfaceResponses{};
unsigned s_SpringResponses{};
unsigned s_DriveResponses{};
unsigned s_PlaneResponses{};
unsigned s_HelicopterResponses{};
unsigned s_BoatResponses{};
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
bool s_CityTasks{};
bool s_CityVenues{};
bool s_CityPhones{};
bool s_CarContacts{};
bool s_BikeContacts{};
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
    std::snprintf(message, sizeof(message), "lanePasses=%u laneReturns=%u", s_LanePasses, s_LaneReturns);
    Log(message);
    std::snprintf(message, sizeof(message), "policeReports=%u drivingWarnings=%u investigationRoutes=%u voluntaryArrests=%u",
        s_PoliceReports, s_SuspicionWarnings, s_InvestigationRoutes, s_VoluntaryArrests);
    Log(message);
    std::snprintf(message, sizeof(message), "scheduledVisits=%u crashWaits=%u crashDepartures=%u crashCalls=%u",
        s_CityVisits, s_CrashWaits, s_CrashDepartures, s_CrashCalls);
    Log(message);
    std::snprintf(message, sizeof(message), "surfaceResponses=%u springResponses=%u driveResponses=%u",
        s_SurfaceResponses, s_SpringResponses, s_DriveResponses);
    Log(message);
    std::snprintf(message, sizeof(message), "planeResponses=%u helicopterResponses=%u boatResponses=%u",
        s_PlaneResponses, s_HelicopterResponses, s_BoatResponses);
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

float Dot(ulw::Point left, ulw::Point right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
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
    s_LaneChanges.Reset();
    s_Roadblocks.Reset();
    s_AirPatrols = {};
    s_FlightGround = {};
    s_RoadPhysics.Reset();
    s_Crashes.Reset();
    s_CrashWitnesses.Reset();
    s_CityLife.Reset();
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

ulw::Point EffectPosition(const void* entity, const void* effect) noexcept {
    const auto origin = EntityPosition(entity);
    const auto local = Read<ulw::Point>(effect);
    const auto* const matrix = Read<const void*>(entity, 0x14);
    if (matrix) {
        const auto right = Read<ulw::Point>(matrix);
        const auto forward = Read<ulw::Point>(matrix, 0x10);
        const auto up = Read<ulw::Point>(matrix, 0x20);
        return {
            origin.x + local.x * right.x + local.y * forward.x + local.z * up.x,
            origin.y + local.x * right.y + local.y * forward.y + local.z * up.y,
            origin.z + local.x * right.z + local.y * forward.z + local.z * up.z,
        };
    }
    const auto forward = EntityForward(entity);
    return {origin.x + local.x * forward.y + local.y * forward.x,
        origin.y - local.x * forward.x + local.y * forward.y, origin.z + local.z};
}

bool __cdecl OnAttractorPed(int model, const void* attractor, ulw::Point position, void* entity, int decisionMaker) noexcept {
    const auto add = GameFunction<AddAttractorPed>(0x614210);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!attractor || !entity || decisionMaker != -1 || GameValue<std::int32_t>(0xB72914) != 0 || !StreetActive(now)) {
        return add(model, attractor, position, entity, decisionMaker);
    }
    const auto hour = GameValue<std::uint8_t>(0xB70153);
    const auto type = Read<std::uint8_t>(attractor, 0x34);
    const auto entityType = Read<std::uint8_t>(entity, 0x36) & 7;
    if (s_CityVenues && ulw::IsCityVenue(type) && (entityType == 1 || entityType == 4) &&
        Read<std::uint8_t>(entity, 0x2F) == 0 && Read<std::uint8_t>(attractor, 0x0C) == 3) {
        s_CityLife.ObserveVenue({EntityPosition(entity), EffectPosition(entity, attractor),
            Read<std::uint16_t>(entity, 0x22), GameValue<std::uint16_t>(0xC81314),
            static_cast<ulw::CityVenue>(type)}, now);
    }
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

int StreetPedReference(const void* ped) noexcept;

void* StartResidentTask(std::uint32_t ref, void* wander, void* child, std::uint32_t now,
    ulw::ResidentAction action, ulw::Point focus) noexcept {
    if (child) {
        s_CityLife.Start(ref, {reinterpret_cast<std::uintptr_t>(wander), reinterpret_cast<std::uintptr_t>(child),
            now, action, focus});
    }
    return child;
}

bool CityPathVisible(ulw::Point from, ulw::Point to) noexcept {
    from.z += 0.55f;
    to.z += 0.55f;
    return GameFunction<LineClear>(0x56A490)(from, to, true, false, false, true, true, false, false);
}

void* TryScheduledVisit(void* wander, const void* ped, std::uint32_t ref, std::uint32_t now) noexcept {
    const auto region = GameValue<std::uint16_t>(0xC81314);
    const auto kind = ulw::PreferredVenue(Read<std::uint16_t>(ped, 0x20), GameValue<std::uint8_t>(0xB70153),
        GameValue<std::uint8_t>(0xB7014E), region, GameValue<float>(0xC81324));
    if (!kind || !s_CityLife.TakeVisitAttempt(ref, now, GameValue<std::uint32_t>(0xB7CB4C))) {
        return nullptr;
    }
    const auto origin = EntityPosition(ped);
    const auto venue = s_CityLife.FindVenue(*kind, origin, region, now);
    if (!venue || venue->model >= 20000 || !CityPathVisible(origin, venue->point)) {
        return nullptr;
    }
    const auto* const model = GameValue<const void*>(0xA9B0C8 + venue->model * sizeof(void*));
    if (!model) {
        return nullptr;
    }
    const auto effects = Read<std::uint8_t>(model, 0x0D);
    if (!effects) {
        return nullptr;
    }
    std::int16_t count{};
    std::array<void*, 20> entities{};
    GameFunction<NearbyEntities>(0x564A20)(venue->owner, 1.75f, false, &count,
        static_cast<std::int16_t>(entities.size()), entities.data(), true, false, false, true, false);
    const auto found = std::clamp(static_cast<int>(count), 0, static_cast<int>(entities.size()));
    for (int i = 0; i < found; ++i) {
        auto* const entity = entities[static_cast<unsigned>(i)];
        if (!entity || Read<std::uint16_t>(entity, 0x22) != venue->model ||
            Read<std::uint8_t>(entity, 0x2F) != 0 || ulw::DistanceSquared(EntityPosition(entity), venue->owner) > 0.9f * 0.9f) {
            continue;
        }
        for (int effect = 0; effect < effects; ++effect) {
            const auto* const candidate = GameFunction<ModelEffect>(0x4C4C70)(model, effect);
            if (!candidate || Read<std::uint8_t>(candidate, 0x0C) != 3 ||
                Read<std::uint8_t>(candidate, 0x34) != static_cast<std::uint8_t>(*kind) ||
                ulw::DistanceSquared(EffectPosition(entity, candidate), venue->point) > 1.1f * 1.1f) {
                continue;
            }
            auto* const manager = GameFunction<AttractorManager>(0x5EE190)();
            if (!manager || !GameFunction<EmptyAttractorSlot>(0x5EBB00)(manager, candidate, entity)) {
                return nullptr;
            }
            auto* const memory = GameFunction<AllocateTask>(0x61A5A0)(0x24);
            if (!memory) {
                ++s_AllocationFailures;
                return nullptr;
            }
            auto* const action = GameFunction<UseAttractor>(0x6321F0)(memory, candidate, entity);
            s_CityLife.ReserveVenue(*venue, now);
            ++s_CityVisits;
            return StartResidentTask(ref, wander, action, now, ulw::ResidentAction::Visit, venue->point);
        }
    }
    return nullptr;
}

void* NoticeCrash(void* wander, void* ped, std::uint32_t now, bool canAct) noexcept {
    if (Read<std::uint8_t>(ped, 0x46C) & 0xBC || Read<std::uint32_t>(ped, 0x748) >= now ||
        !GameValue<void*>(PedPool)) {
        return nullptr;
    }
    const auto origin = EntityPosition(ped);
    const auto incident = s_Crashes.Closest(origin, 16.0f, now);
    if (!incident) {
        return nullptr;
    }
    const auto reference = GameFunction<PedReference>(0x54FF60)(ped);
    if (reference < 0 || (reference & 0x80)) {
        return nullptr;
    }
    const auto id = static_cast<std::uint32_t>(reference);
    if (s_CrashWitnesses.Noticed(id, *incident) ||
        !s_CrashWitnesses.TakeScan(id, now, GameValue<std::uint32_t>(0xB7CB4C))) {
        return nullptr;
    }
    const ulw::Point eye{origin.x, origin.y, origin.z + 0.65f};
    auto target = incident->position;
    target.z += 0.7f;
    if (!GameFunction<LineClear>(0x56A490)(eye, target, true, false, false, true, true, false, false)) {
        return nullptr;
    }
    const auto yaw = std::atan2(origin.x - target.x, target.y - origin.y);
    GameFunction<PedLookHeading>(0x5DEDC0)(ped, yaw, false, false);
    GameFunction<PedLookTimer>(0x5DF8D0)(ped, 900 + (Read<std::uint16_t>(ped, 0x20) & 0x1FFu));
    s_CrashWitnesses.Remember(id, *incident);
    ++s_CrashGlances;
    LogActivity(now);
    if (!canAct) {
        return nullptr;
    }
    const auto reaction = ulw::ReactToStreetCrash(Read<std::uint16_t>(ped, 0x20), ulw::DistanceSquared(origin, target));
    switch (reaction) {
    case ulw::CrashReaction::Leave:
        if (auto* const memory = GameFunction<AllocateTask>(0x61A5A0)(0x44)) {
            ++s_CrashDepartures;
            return StartResidentTask(id, wander, GameFunction<FleePoint>(0x65BD20)(memory, incident->position,
                false, 13.0f, 2100), now, ulw::ResidentAction::Leave, incident->position);
        }
        break;
    case ulw::CrashReaction::Pause:
        if (auto* const memory = GameFunction<AllocateTask>(0x61A5A0)(0x20)) {
            ++s_CrashWaits;
            return StartResidentTask(id, wander, GameFunction<StandStill>(0x62F310)(memory, 850, false, false, 8.0f),
                now, ulw::ResidentAction::Pause, incident->position);
        }
        break;
    case ulw::CrashReaction::Call:
        if (s_CityPhones && s_PoliceCruise && s_CityLife.CanCall(*incident, now)) {
            if (auto* const memory = GameFunction<AllocateTask>(0x61A5A0)(0x20)) {
                s_CityLife.StartCall(*incident, now);
                return StartResidentTask(id, wander, GameFunction<UsePhone>(0x6348A0)(memory, 2400),
                    now, ulw::ResidentAction::Call, incident->position);
            }
            break;
        }
        return nullptr;
    case ulw::CrashReaction::Glance:
        return nullptr;
    }
    ++s_AllocationFailures;
    return nullptr;
}

void* __fastcall OnWanderNext(void* wander, void*, void* ped) noexcept {
    if (!AmbientWalker(wander, ped)) {
        return GameFunction<WanderControl>(NativeWanderNext)(wander, ped);
    }
    const auto ref = StreetPedReference(ped);
    if (ref >= 0) {
        const auto id = static_cast<std::uint32_t>(ref);
        const auto action = s_CityLife.Current(id, reinterpret_cast<std::uintptr_t>(wander), Read<std::uintptr_t>(wander, 0x08));
        if (action) {
            s_CityLife.Finish(id);
            const auto now = GameValue<std::uint32_t>(0xB7CB84);
            if (StreetActive(now) && action->action == ulw::ResidentAction::Call && now - action->started >= 1700) {
                ++s_CrashCalls;
                if (s_PoliceIncidents.Record(ulw::IncidentKind::Crash, id, action->focus, now, false)) {
                    ++s_PoliceReports;
                }
            }
            constexpr std::uint16_t InvalidArea = 0xFFFF;
            auto* const memory = static_cast<std::byte*>(wander);
            std::memcpy(memory + 0x18, &InvalidArea, sizeof(InvalidArea));
            std::memcpy(memory + 0x1C, &InvalidArea, sizeof(InvalidArea));
            return GameFunction<WanderControl>(NativeWanderFirst)(wander, ped);
        }
    }
    return GameFunction<WanderControl>(NativeWanderNext)(wander, ped);
}

void* __fastcall OnWanderControl(void* task, void*, void* ped) noexcept {
    const auto native = GameFunction<WanderControl>(0x674C30);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!AmbientWalker(task, ped) || !StreetActive(now)) {
        return native(task, ped);
    }
    auto* const paceField = static_cast<std::byte*>(task) + 0x0C;
    const auto previous = Read<std::uint32_t>(paceField);
    auto* const runningTask = Read<void*>(task, 0x08);
    const auto ref = s_CityTasks ? StreetPedReference(ped) : -1;
    if (ref >= 0 && s_CityLife.Current(static_cast<std::uint32_t>(ref), reinterpret_cast<std::uintptr_t>(task),
        reinterpret_cast<std::uintptr_t>(runningTask))) {
        return runningTask;
    }
    const auto routine = PedestrianRoutineFor(ped);
    const bool jog = previous == Walking && routine.pace == ulw::PedestrianPace::Jog;
    if (jog) {
        std::memcpy(paceField, &Jogging, sizeof(Jogging));
    }
    auto* const next = native(task, ped);
    if (jog) {
        std::memcpy(paceField, &previous, sizeof(previous));
    }
    const bool canAct = ref >= 0 && next && next == runningTask &&
        Read<std::uintptr_t>(runningTask) == WalkPointVtable && GameValue<void*>(TaskPool);
    if (canAct) {
        s_CityLife.Finish(static_cast<std::uint32_t>(ref));
    }
    if (s_CrashGlancesEnabled && next && next == runningTask) {
        if (auto* const crash = NoticeCrash(task, ped, now, canAct)) {
            return crash;
        }
    }
    if (s_CityVenues && canAct) {
        if (auto* const visit = TryScheduledVisit(task, ped, static_cast<std::uint32_t>(ref), now)) {
            return visit;
        }
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

const void* RoadLink(std::uint16_t route) noexcept {
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
    const auto* const next = RoadLink(Read<std::uint16_t>(car, 0x3A6));
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

bool PlainRoadNode(std::uint32_t address) noexcept {
    const auto area = address & 0xFFFFu;
    const auto index = address >> 16;
    if (area >= 64 || index >= GameValue<std::uint32_t>(RoadNodeCounts + area * 4)) {
        return false;
    }
    const auto* const nodes = GameValue<const std::byte*>(RoadNodes + area * 4);
    return nodes && (Read<std::uint8_t>(nodes, index * 0x1C + 0x18) & 0xBFu) == 2;
}

struct LaneLink {
    ulw::Point center{};
    ulw::Point forward{};
    int lanes{};
};

LaneLink ReadLaneLink(const void* link, int sign, int lane) noexcept {
    if (!link || (sign != 1 && sign != -1) || lane < 0 || (Read<std::uint8_t>(link, 0x0C) & 7)) {
        return {};
    }
    const auto limits = Read<unsigned char>(link, 0x0B);
    const auto lanes = static_cast<int>((sign < 0 ? limits >> 3 : limits) & 7);
    if (lanes < 2 || lane >= lanes) {
        return {};
    }
    const ulw::Point forward{Read<std::int8_t>(link, 8) * static_cast<float>(sign) * 0.01f,
        Read<std::int8_t>(link, 9) * static_cast<float>(sign) * 0.01f, 0.0f};
    const auto length = std::hypot(forward.x, forward.y);
    if (!(length > 0.97f && length < 1.03f)) {
        return {};
    }
    const auto offset = (GameFunction<LaneOffset>(0x44DB00)(link) + static_cast<float>(lane)) * 5.4f;
    const ulw::Point center{Read<std::int16_t>(link) * 0.125f + forward.y * offset,
        Read<std::int16_t>(link, 2) * 0.125f - forward.x * offset, 0.0f};
    return {center, {forward.x / length, forward.y / length, 0.0f}, lanes};
}

struct RoadFootprint {
    float halfLength;
    float halfWidth;
};

RoadFootprint RoadBodySize(const void* entity, unsigned type) noexcept {
    const RoadFootprint fallback = type == 2 ? RoadFootprint{3.5f, 1.45f} :
        type == 3 ? RoadFootprint{0.60f, 0.60f} : RoadFootprint{1.45f, 1.45f};
    const auto* const model = GameFunction<CollisionModel>(0x535300)(entity);
    if (!model) {
        return fallback;
    }
    const auto low = Read<ulw::Point>(model);
    const auto high = Read<ulw::Point>(model, sizeof(ulw::Point));
    const auto width = std::max(std::abs(low.x), std::abs(high.x));
    const auto length = std::max(std::abs(low.y), std::abs(high.y));
    if (!(width >= 0.15f && width <= 16.0f && length >= 0.15f && length <= 32.0f)) {
        return fallback;
    }
    return {length, width};
}

void TryTrafficLane(void* car, std::uint32_t now) noexcept {
    constexpr int PhysicsStatus = 3;
    if ((Read<unsigned char>(car, 0x36) >> 3) != PhysicsStatus || !AmbientTrafficCar(car) ||
        Read<std::uint8_t>(car, 0x3BB) != 0 || (Read<std::uint8_t>(car, 0x3DB) & 0x18) ||
        (Read<std::uint8_t>(car, 0x3DC) & 0x02) || !(Read<float>(car, 0x4C0) >= 420.0f) ||
        GameValue<std::int32_t>(0xB72914) != 0 || !GameValue<void*>(VehiclePool) || !StreetActive(now)) {
        return;
    }
    const auto lane = Read<std::int8_t>(car, 0x3B7);
    if (lane < 0 || lane != Read<std::int8_t>(car, 0x3B8)) {
        return;
    }
    const auto current = ReadLaneLink(RoadLink(Read<std::uint16_t>(car, 0x3A4)), Read<std::int8_t>(car, 0x3B5), lane);
    const auto next = ReadLaneLink(RoadLink(Read<std::uint16_t>(car, 0x3A6)), Read<std::int8_t>(car, 0x3B6), lane);
    if (current.lanes < 2 || next.lanes != current.lanes ||
        Dot(current.forward, next.forward) < 0.985f || Dot(current.forward, EntityForward(car)) < 0.94f ||
        !PlainRoadNode(Read<std::uint32_t>(car, 0x390)) || !PlainRoadNode(Read<std::uint32_t>(car, 0x394))) {
        return;
    }
    const auto position = EntityPosition(car);
    const auto axis = current.forward;
    const ulw::Point right{axis.y, -axis.x, 0.0f};
    const ulw::Point toCurrent{current.center.x - position.x, current.center.y - position.y, 0.0f};
    const ulw::Point toNext{next.center.x - position.x, next.center.y - position.y, 0.0f};
    const auto ahead = Dot(toNext, axis);
    const auto laneCenter = Dot(toCurrent, right);
    const auto speed = Dot(Read<ulw::Point>(car, 0x44), axis) * 50.0f;
    const auto wet = GameValue<float>(0xC81308);
    if (!(ahead >= 22.0f && ahead <= 150.0f && Dot(toCurrent, axis) <= 4.0f && std::abs(laneCenter) <= 1.65f &&
        std::abs(Dot(toNext, right) - laneCenter) < 1.65f && speed >= -0.2f && speed <= 20.0f &&
        wet >= 0.0f && wet < 0.82f && Fog() < 0.48f) ||
        GameFunction<TrafficLight>(0x49D610)(car, true) || GameFunction<BridgeLight>(0x49D420)(car)) {
        return;
    }
    const auto id = GameFunction<PedReference>(0x54FFC0)(car);
    if (id < 0 || (id & 0x80) ||
        !s_LaneChanges.BeginProbe(static_cast<std::uint32_t>(id), now, GameValue<std::uint32_t>(0xB7CB4C))) {
        return;
    }
    const auto footprint = RoadBodySize(car, 2);
    const auto cruise = std::clamp(Read<std::uint8_t>(car, 0x3D0) * 0.50f, 5.0f, 19.0f);
    const ulw::LaneOpportunity road{static_cast<std::uint32_t>(id), now, position, right, lane, current.lanes,
        5.4f, laneCenter, ahead, speed, cruise, footprint.halfLength, footprint.halfWidth, wet, DriverForCar(car)};

    constexpr std::size_t ScanCapacity = 64;
    std::array<void*, ScanCapacity> entities{};
    std::int16_t found{};
    GameFunction<NearbyEntities>(0x564A20)(position, 53.0f, true, &found, static_cast<std::int16_t>(entities.size()),
        entities.data(), false, true, true, true, false);
    if (found <= 0 || found >= static_cast<std::int16_t>(entities.size())) {
        return;
    }
    std::array<ulw::LaneBody, ScanCapacity> bodies;
    unsigned count{};
    for (int i = 0; i < found; ++i) {
        const auto* const entity = entities[static_cast<std::size_t>(i)];
        if (!entity || entity == car || (Read<std::uint8_t>(entity, 0x1C) & 1) == 0) {
            continue;
        }
        const auto type = Read<std::uint8_t>(entity, 0x36) & 7u;
        if (type < 2 || type > 4) {
            continue;
        }
        const auto other = EntityPosition(entity);
        const ulw::Point gap{other.x - position.x, other.y - position.y, other.z - position.z};
        if (std::abs(gap.z) >= 2.45f) {
            continue;
        }
        const auto velocity = Read<ulw::Point>(entity, 0x44);
        const auto forward = EntityForward(entity);
        const auto alignment = Dot(forward, axis);
        const auto body = RoadBodySize(entity, type);
        const auto alongSize = body.halfLength * std::abs(alignment) + body.halfWidth * std::abs(Dot(forward, right));
        const auto sideSize = body.halfWidth * std::abs(alignment) + body.halfLength * std::abs(Dot(forward, right));
        const auto otherID = type == 2 ? GameFunction<PedReference>(0x54FFC0)(entity) : -1;
        const bool alignedVehicle = otherID >= 0 && !(otherID & 0x80) && alignment >= 0.78f &&
            Read<std::int32_t>(entity, 0x594) == 0;
        bodies[count++] = {static_cast<std::uint32_t>(otherID), Dot(gap, axis), Dot(gap, right), gap.z,
            Dot(velocity, axis) * 50.0f, Dot(velocity, right) * 50.0f, alongSize, sideSize, alignedVehicle};
    }
    const auto choice = s_LaneChanges.Decide(road, std::span<const ulw::LaneBody>{bodies.data(), count});
    if (!choice) {
        return;
    }
    const auto lateral = laneCenter + static_cast<float>(choice->lane - lane) * road.laneWidth;
    const ulw::Point start{position.x + axis.x * 2.0f, position.y + axis.y * 2.0f, position.z + 0.55f};
    const ulw::Point end{position.x + axis.x * 10.0f + right.x * lateral,
        position.y + axis.y * 10.0f + right.y * lateral, start.z};
    if (!GameFunction<LineClear>(0x56A490)(start, end, true, false, false, true, true, false, false)) {
        return;
    }
    const auto selected = static_cast<std::int8_t>(choice->lane);
    std::memcpy(static_cast<std::byte*>(car) + 0x3B8, &selected, sizeof(selected));
    s_LaneChanges.Commit(road, *choice);
    ++(choice->returning ? s_LaneReturns : s_LanePasses);
    LogActivity(now);
}

void __cdecl OnTrafficSteering(void* car, float* steer, float* gas, float* brake, bool* handbrake) noexcept {
    TryTrafficLane(car, GameValue<std::uint32_t>(0xB7CB84));
    GameFunction<MissionSteering>(0x434900)(car, steer, gas, brake, handbrake);
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

bool TunableRoadVehicle(const void* vehicle, bool motorcycle, std::uint32_t now) noexcept {
    const auto type = motorcycle ? 9 : 0;
    return Read<std::int32_t>(vehicle, 0x594) == type && Read<std::uint8_t>(vehicle, 0x2F) == 0 &&
        Read<std::int8_t>(vehicle, 0x424) < 0 && Read<std::uint8_t>(vehicle, 0x4A4) != 2 &&
        (Read<std::uint8_t>(vehicle, 0x36) >> 3) == 0 && GameValue<void*>(VehiclePool) && StreetActive(now);
}

ulw::RoadVehicle RoadVehicleClass(const void* vehicle, bool motorcycle) noexcept {
    return ulw::ClassifyRoadVehicle(Read<std::uint16_t>(vehicle, 0x22), Read<float>(vehicle, 0x8C),
        (Read<std::uint32_t>(vehicle, 0x38C) & 0x00300000u) != 0, motorcycle);
}

ulw::DrivenAxle DriveLayout(const void* vehicle, bool motorcycle) noexcept {
    if (motorcycle) {
        return ulw::DrivenAxle::Rear;
    }
    const auto* const handling = Read<const void*>(vehicle, 0x384);
    if (handling) {
        switch (Read<char>(handling, 0x74)) {
        case 'F': return ulw::DrivenAxle::Front;
        case 'R': return ulw::DrivenAxle::Rear;
        case '4': return ulw::DrivenAxle::Both;
        }
    }
    return ulw::DrivenAxle::Unknown;
}

ulw::SurfaceContact RoadSurface(unsigned id) noexcept {
    constexpr unsigned SurfaceCount = 179;
    if (id >= SurfaceCount) {
        return {};
    }
    const auto flags = SurfaceFlags + id * 0x0Cu;
    return ulw::ClassifySurface(GameValue<std::uint32_t>(flags), GameValue<std::uint32_t>(flags + 4));
}

ulw::WheelContact ReadWheelContact(const void* vehicle, int wheel, bool motorcycle,
    const void* wheelRight, const void* contactSpeed) noexcept {
    if ((motorcycle && (!s_BikeContacts || wheel < 0 || wheel > 1)) ||
        (!motorcycle && (!s_CarContacts || wheel < 0 || wheel > 3))) {
        return {};
    }
    const auto index = static_cast<unsigned>(wheel);
    auto hit = index;
    if (motorcycle) {
        hit *= 2;
        if (Read<float>(vehicle, 0x710 + (hit + 1) * 4) < Read<float>(vehicle, 0x710 + hit * 4)) {
            ++hit;
        }
    }
    const auto ratio = Read<float>(vehicle, (motorcycle ? 0x710u : 0x7D4u) + hit * 4);
    if (!(ratio >= 0.0f && ratio < 1.0f)) {
        return {};
    }
    const auto material = Read<std::uint8_t>(vehicle, (motorcycle ? 0x683u : 0x747u) + hit * 0x2C);
    return {RoadSurface(material), RoadVehicleClass(vehicle, motorcycle), DriveLayout(vehicle, motorcycle),
        motorcycle ? 4 + wheel : wheel, ratio, Dot(Read<ulw::Point>(vehicle, 0x44), EntityForward(vehicle)),
        Dot(Read<ulw::Point>(wheelRight), Read<ulw::Point>(contactSpeed)),
        (Read<std::uint8_t>(vehicle, 0x428) & 0x20) != 0};
}

ulw::WheelForces TunePlayerWheel(const void* vehicle, ulw::WheelForces forces, int wheel,
    bool damagedTyre, bool motorcycle, const void* wheelRight, const void* contactSpeed, const void* wheelState) noexcept {
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!TunableRoadVehicle(vehicle, motorcycle, now)) {
        return forces;
    }
    const auto ref = GameFunction<PedReference>(0x54FFC0)(vehicle);
    if (ref < 0 || (ref & 0x80)) {
        return forces;
    }
    const auto speed = Read<ulw::Point>(vehicle, 0x44);
    const auto wear = motorcycle ? 0.0f : ulw::PanelWearAtWheel(Read<std::uint32_t>(vehicle, 0x5B4), wheel);
    const auto engineDamage = motorcycle ? std::uint8_t{} : Read<std::uint8_t>(vehicle, 0x5A4);
    const auto contact = ReadWheelContact(vehicle, wheel, motorcycle, wheelRight, contactSpeed);
    const auto state = Read<unsigned>(wheelState);
    if (contact.surface.material != ulw::RoadMaterial::Unknown && contact.surface.material != ulw::RoadMaterial::Paved) {
        ++s_SurfaceResponses;
    }
    return s_RoadPhysics.Tune(forces, {
        static_cast<std::uint32_t>(ref), now, Read<float>(vehicle, 0x8C), Read<float>(vehicle, 0x4C0),
        GameValue<float>(0xC81308), ulw::DistanceSquared(speed, {}), Read<float>(vehicle, 0x4A0), damagedTyre,
        Read<float>(vehicle, 0x49C), wear, engineDamage, motorcycle, contact, state == 1 || state == 2,
    });
}

void __fastcall OnWheel(void* car, void*, void* forward, void* right, void* contactSpeed, void* contact,
    int wheels, float thrust, float brake, float grip, std::int8_t wheel, float* wheelSpeed,
    std::int32_t* wheelState, std::uint16_t status) noexcept {
    const auto forces = TunePlayerWheel(car, {thrust, brake, grip}, wheel, status != 0, false,
        right, contactSpeed, wheelState);
    GameFunction<ProcessWheel>(0x6D6C00)(car, forward, right, contactSpeed, contact, wheels,
        forces.thrust, forces.brake, forces.grip, wheel, wheelSpeed, wheelState, status);
}

void __fastcall OnBikeWheel(void* bike, void*, void* forward, void* right, void* contactSpeed, void* contact,
    int wheels, float thrust, float brake, float grip, float destabilize, std::int8_t wheel, float* wheelSpeed,
    std::int32_t* wheelState, std::int32_t special, std::uint16_t status) noexcept {
    const auto forces = TunePlayerWheel(bike, {thrust, brake, grip}, wheel, status != 0, true,
        right, contactSpeed, wheelState);
    GameFunction<ProcessBikeWheel>(0x6D73B0)(bike, forward, right, contactSpeed, contact, wheels,
        forces.thrust, forces.brake, forces.grip, destabilize, wheel, wheelSpeed, wheelState, special, status);
}

bool TunableSuspension(const void* car) noexcept {
    return TunableRoadVehicle(car, false, GameValue<std::uint32_t>(0xB7CB84)) &&
        Read<std::uint16_t>(car, 0x22) != 432 && !(Read<std::uint32_t>(car, 0x38C) & 0x00020000u);
}

ulw::SuspensionSpring PlayerSpring(const void* car, ulw::SuspensionSpring native, float travel, const void* point) noexcept {
    if (!TunableSuspension(car)) {
        return native;
    }
    const auto adjustment = ulw::TuneSpring(native, RoadVehicleClass(car, false), travel, GameValue<float>(0xC81308),
        Read<float>(car, 0x4A0), Read<float>(car, 0x49C), Dot(EntityForward(car), Read<ulw::Point>(point)) >= 0.0f);
    if (adjustment.force != native.force || adjustment.bias != native.bias) {
        ++s_SpringResponses;
    }
    return adjustment;
}

bool __fastcall OnSpring(void* car, void*, float force, void* direction, void* point, float travel,
    float bias, float* dampingLimit) noexcept {
    const auto spring = PlayerSpring(car, {force, bias}, travel, point);
    return GameFunction<SpringCollision>(0x543C90)(car, spring.force, direction, point, travel, spring.bias, dampingLimit);
}

bool __fastcall OnSpringAlt(void* car, void*, float force, void* direction, void* point, float travel,
    float bias, void* normal, float* dampingLimit) noexcept {
    const auto spring = PlayerSpring(car, {force, bias}, travel, point);
    return GameFunction<SpringCollisionAlt>(0x543D60)(car, spring.force, direction, point, travel, spring.bias, normal, dampingLimit);
}

bool __fastcall OnSpringDamping(void* car, void*, float damping, float limit, void* direction, void* point,
    void* collisionSpeed) noexcept {
    if (TunableSuspension(car)) {
        const auto forward = EntityForward(car);
        const auto offset = Read<ulw::Point>(point);
        const auto* const matrix = Read<const void*>(car, 0x14);
        const auto right = matrix ? Read<ulw::Point>(matrix) : ulw::Point{forward.y, -forward.x, 0};
        const unsigned wheel = (Dot(forward, offset) >= 0.0f ? 0u : 1u) + (Dot(right, offset) >= 0.0f ? 2u : 0u);
        const auto material = s_CarContacts ? RoadSurface(Read<std::uint8_t>(car, 0x747 + wheel * 0x2C)) : ulw::SurfaceContact{};
        const auto reaction = ulw::TuneDamping(damping, RoadVehicleClass(car, false), material,
            Dot(Read<ulw::Point>(direction), Read<ulw::Point>(collisionSpeed)));
        s_SpringResponses += reaction != damping ? 1u : 0u;
        damping = reaction;
    }
    return GameFunction<SpringDamping>(0x543E90)(car, damping, limit, direction, point, collisionSpeed);
}

struct TransmissionData {
    std::array<ulw::GearBand, 6> gears;
    char drive;
    char engine;
    std::uint8_t count;
    std::byte alignment;
    std::uint32_t flags;
    float acceleration;
    float inertia;
    float maxVelocity;
    float maxFlatVelocity;
    float maxReverse;
    float velocity;
};
static_assert(sizeof(TransmissionData) == 0x68 && offsetof(TransmissionData, velocity) == 0x64);

float __fastcall OnDriveAcceleration(void* transmission, void*, const float* gas, std::uint8_t* gear,
    float* gearChange, float* velocity, float* inertiaA, float* inertiaB, std::uint8_t grounded, std::uint8_t cheat) noexcept {
    const auto native = GameFunction<DriveAcceleration>(0x6D05E0);
    const auto carAddress = reinterpret_cast<std::uintptr_t>(gas) - 0x49C;
    const auto* const car = reinterpret_cast<const void*>(carAddress);
    if (!gas || !gear || !grounded || cheat || !(*gas > 0.0f && *gas <= 1.0f) ||
        reinterpret_cast<std::uintptr_t>(gear) != carAddress + 0x4B4 ||
        !TunableRoadVehicle(car, false, GameValue<std::uint32_t>(0xB7CB84))) {
        return native(transmission, gas, gear, gearChange, velocity, inertiaA, inertiaB, grounded, cheat);
    }
    const auto type = RoadVehicleClass(car, false);
    const auto grade = EntityForward(car).z;
    const auto pedal = ulw::ShapeThrottle(*gas, type, grade);
    auto adjusted = Read<TransmissionData>(transmission);
    if (*gear > adjusted.count || adjusted.count == 0 || adjusted.count >= adjusted.gears.size()) {
        return native(transmission, gas, gear, gearChange, velocity, inertiaA, inertiaB, grounded, cheat);
    }
    if (!ulw::TuneShiftBands(adjusted.gears, adjusted.count, type, *gas, grade)) {
        return native(transmission, &pedal, gear, gearChange, velocity, inertiaA, inertiaB, grounded, cheat);
    }
    ++s_DriveResponses;
    const auto acceleration = native(&adjusted, &pedal, gear, gearChange, velocity, inertiaA, inertiaB, grounded, cheat);
    // The native routine publishes the sampled velocity to its transmission.
    // Preserve that output while keeping this car's shift thresholds local.
    std::memcpy(static_cast<std::byte*>(transmission) + 0x64, &adjusted.velocity, sizeof(adjusted.velocity));
    return acceleration;
}

int PlayerTransport(const void* vehicle, std::uint32_t now) noexcept {
    if (Read<std::uint8_t>(vehicle, 0x2F) != 0 || (Read<std::uint8_t>(vehicle, 0x36) >> 3) != 0 ||
        Read<std::int8_t>(vehicle, 0x424) >= 0 || Read<std::uint8_t>(vehicle, 0x4A4) == 2 ||
        !(Read<float>(vehicle, 0x4C0) >= 300.0f) || !IsPlayerPed(Read<const void*>(vehicle, 0x460)) ||
        !GameValue<void*>(VehiclePool) || GameValue<std::int32_t>(0xB72914) != 0 || !StreetActive(now)) {
        return -1;
    }
    const auto reference = GameFunction<PedReference>(0x54FFC0)(vehicle);
    return reference >= 0 && !(reference & 0x80) ? reference : -1;
}

float FlightClearance(int reference, std::uint32_t now, ulw::Point position) noexcept {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
        return -1.0f;
    }
    const auto& last = s_FlightGround.position;
    const ulw::Point horizontal{position.x, position.y, last.z};
    if (reference != s_FlightGround.vehicle || now - s_FlightGround.time >= 250 ||
        ulw::DistanceSquared(last, horizontal) >= 256.0f || std::abs(position.z - last.z) >= 10.0f) {
        auto probe = position;
        probe.z += 1.5f;
        bool found{};
        const auto height = GameFunction<GroundHeight>(0x5696C0)(probe, &found, nullptr);
        s_FlightGround = {reference, now, position, height, found && std::isfinite(height)};
    }
    return s_FlightGround.found ? std::max(0.0f, position.z - s_FlightGround.height) : -1.0f;
}

struct FlightHandling {
    std::int32_t id;
    float thrust;
    float thrustFallOff;
    float yaw;
    float yawStability;
    float sideSlip;
    float roll;
    float rollStability;
    float pitch;
    std::array<float, 13> nativeFields;
};
static_assert(sizeof(FlightHandling) == 0x58 && offsetof(FlightHandling, pitch) == 0x20);

void __fastcall OnFlyingControl(void* aircraft, void*, int flightModel, float rudder, float elevator, float aileron,
    float throttle) noexcept {
    const auto native = GameFunction<FlyingControl>(NativeFlight);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    const auto type = Read<std::int32_t>(aircraft, 0x594);
    const auto* const original = Read<const FlightHandling*>(aircraft, FlightHandlingOffset);
    // A Hydra in hover uses the helicopter solver with a different native handling profile.
    const auto helicopter = type == 3 && flightModel == 6;
    const auto plane = type == 4 && flightModel == 3;
    if ((!plane && !helicopter) || !original || (Read<std::uint32_t>(aircraft, 0x40) & 0x300u)) {
        return native(aircraft, flightModel, rudder, elevator, aileron, throttle);
    }
    const auto reference = PlayerTransport(aircraft, now);
    if (reference < 0) {
        return native(aircraft, flightModel, rudder, elevator, aileron, throttle);
    }
    const auto position = EntityPosition(aircraft);
    const auto forwardSpeed = Dot(Read<ulw::Point>(aircraft, 0x44), EntityForward(aircraft));
    const auto clearance = FlightClearance(reference, now, position);
    const auto wheels = Read<std::array<float, 4>>(aircraft, 0x7D4);
    const auto grounded = std::any_of(wheels.begin(), wheels.end(), [](float travel) {
        return travel >= 0.0f && travel < 0.98f;
    });
    const auto model = Read<std::uint16_t>(aircraft, 0x22);
    const auto tune = ulw::TuneFlight(ulw::ClassifyAircraft(model, helicopter), forwardSpeed, position.z, clearance, grounded);
    if (tune.yaw != 1.0f || tune.roll != 1.0f || tune.pitch != 1.0f || tune.sideSlip != 1.0f || tune.thrust != 1.0f) {
        auto adjusted = Read<FlightHandling>(original);
        adjusted.yaw *= tune.yaw;
        adjusted.roll *= tune.roll;
        adjusted.pitch *= tune.pitch;
        adjusted.sideSlip *= tune.sideSlip;
        adjusted.thrust *= tune.thrust;
        // The native solver reads the handling pointer synchronously. Restore
        // the shared profile before returning to the vehicle controller.
        const FlightHandling* profile = &adjusted;
        auto* const slot = static_cast<std::byte*>(aircraft) + FlightHandlingOffset;
        std::memcpy(slot, &profile, sizeof(profile));
        native(aircraft, flightModel, rudder, elevator, aileron, throttle);
        std::memcpy(slot, &original, sizeof(original));
        ++(helicopter ? s_HelicopterResponses : s_PlaneResponses);
        LogActivity(now);
        return;
    }
    native(aircraft, flightModel, rudder, elevator, aileron, throttle);
}

struct BoatHandling {
    std::array<std::uint32_t, 4> thrustGeometry;
    float planingForce;
    std::array<float, 4> nativeFields;
    ulw::Point movementResistance;
    ulw::Point turnResistance;
};
static_assert(sizeof(BoatHandling) == 0x3C && offsetof(BoatHandling, movementResistance) == 0x24);

void __fastcall OnBoatControl(void* boat, void*, const BoatHandling* handling, float* immersion,
    bool collided, bool postCollision) noexcept {
    const auto native = GameFunction<ProcessBoatControl>(NativeBoat);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    const auto model = Read<std::uint16_t>(boat, 0x22);
    if (!handling || !immersion || collided || postCollision || model == 460 || model == 539 ||
        Read<std::int32_t>(boat, 0x594) != 5 || !(Read<std::uint32_t>(boat, 0x40) & 0x100u) ||
        GameValue<std::uint8_t>(0x969153) || PlayerTransport(boat, now) < 0) {
        return native(boat, handling, immersion, collided, postCollision);
    }
    const auto speed = Read<ulw::Point>(boat, 0x44);
    const auto tune = ulw::TuneHull(ulw::ClassifyHull(model), Dot(speed, EntityForward(boat)), *immersion,
        speed.z, Read<float>(boat, 0x49C));
    if (tune.longitudinalDrag != 1.0f || tune.lateralDrag != 1.0f || tune.yawDrag != 1.0f || tune.planing != 1.0f) {
        auto adjusted = Read<BoatHandling>(handling);
        adjusted.movementResistance.x = ulw::TuneResistance(adjusted.movementResistance.x, tune.lateralDrag);
        adjusted.movementResistance.y = ulw::TuneResistance(adjusted.movementResistance.y, tune.longitudinalDrag);
        adjusted.turnResistance.z = ulw::TuneResistance(adjusted.turnResistance.z, tune.yawDrag);
        adjusted.planingForce *= tune.planing;
        native(boat, &adjusted, immersion, collided, postCollision);
        ++s_BoatResponses;
        LogActivity(now);
        return;
    }
    native(boat, handling, immersion, collided, postCollision);
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
    const auto lanes = s_BendTraffic && VehicleQueriesMatch() &&
        Matches(0x434900, Entry{0x83, 0xEC, 0x60, 0x55, 0x56, 0x8B, 0x74, 0x24}) &&
        Matches(0x434AA2, std::array<std::uint8_t, 7>{0x0F, 0xBE, 0x86, 0xB7, 0x03, 0x00, 0x00}) &&
        Matches(0x434ADE, std::array<std::uint8_t, 7>{0x0F, 0xBE, 0x86, 0xB8, 0x03, 0x00, 0x00}) &&
        Matches(0x44DB00, Entry{0x51, 0x8A, 0x41, 0x0B, 0x8A, 0xD0, 0x80, 0xE2}) &&
        Matches(0x42DF0E, std::array<std::uint8_t, 5>{0x0F, 0xB6, 0x44, 0x28, 0x18}) &&
        Matches(0x49D610, Entry{0x83, 0xEC, 0x08, 0x53, 0x56, 0x8B, 0x74, 0x24}) &&
        Matches(0x49D420, Entry{0x53, 0x56, 0x8B, 0x74, 0x24, 0x0C, 0x8B, 0xCE}) &&
        Matches(0x564A20, Entry{0x83, 0xEC, 0x18, 0x53, 0x55, 0x56, 0x57, 0x8B}) &&
        Matches(0x56A490, Entry{0x83, 0xEC, 0x5C, 0x66, 0x81, 0x3D, 0x78, 0xCD}) &&
        Matches(0x535300, Entry{0x8A, 0x41, 0x36, 0x24, 0x07, 0x3C, 0x02, 0x75}) &&
        Matches(0x4374D4, std::array<std::uint8_t, 4>{0x51, 0x52, 0x50, 0x56}) &&
        PatchCalls({0x4374D8}, 0x434900, reinterpret_cast<std::uintptr_t>(&OnTrafficSteering));
    Log(lanes ? "Installed: slow traffic passing and return on open lanes."
        : "Skipped: lane steering or collision query differs.");
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

bool SurfaceLayoutMatches() noexcept {
    return Matches(0x55E6F7, std::array<std::uint8_t, 7>{0x8B, 0x84, 0x81, 0x94, 0, 0, 0}) &&
        Matches(0x55E917, std::array<std::uint8_t, 7>{0x8B, 0x84, 0x81, 0x98, 0, 0, 0});
}

bool InstallPhysics() noexcept {
    const auto wheels = VehicleQueriesMatch() &&
        Matches(0x6D6C00, Entry{0x83, 0xEC, 0x48, 0xD9, 0x05, 0x50, 0x8B, 0x85}) &&
        PatchCalls({0x6A567B, 0x6A56E9, 0x6A5B49, 0x6A5BB7}, 0x6D6C00,
            reinterpret_cast<std::uintptr_t>(&OnWheel));
    s_CarContacts = wheels && SurfaceLayoutMatches() &&
        Matches(0x6A54D7, std::array<std::uint8_t, 7>{0x8D, 0x8C, 0x01, 0x24, 0x07, 0, 0});
    return wheels;
}

bool InstallMotorcyclePhysics() noexcept {
    const auto wheels = VehicleQueriesMatch() &&
        Matches(0x6B9250, Entry{0x6A, 0xFF, 0x68, 0xEB, 0x82, 0x84, 0x00, 0x64}) &&
        Matches(0x6D73B0, Entry{0x83, 0xEC, 0x40, 0xD9, 0x05, 0x50, 0x8B, 0x85}) &&
        PatchCalls({0x6BAF3D, 0x6BB4C1, 0x6BB855}, 0x6D73B0, reinterpret_cast<std::uintptr_t>(&OnBikeWheel));
    s_BikeContacts = wheels && SurfaceLayoutMatches() &&
        Matches(0x6BAB0C, Entry{0x0F, 0xB6, 0x8C, 0x30, 0x83, 0x06, 0, 0}) &&
        Matches(0x6BA8AE, std::array<std::uint8_t, 6>{0xD9, 0x86, 0x10, 0x07, 0, 0});
    return wheels;
}

void InstallRoadDynamics() noexcept {
    const bool springs = Matches(0x543C90, Entry{0xD9, 0x05, 0x24, 0x86, 0x85, 0, 0x83, 0xEC}) &&
        PatchCalls({0x6B007D}, 0x543C90, reinterpret_cast<std::uintptr_t>(&OnSpring));
    const bool alternate = Matches(0x543D60, Entry{0xD9, 0x05, 0x24, 0x86, 0x85, 0, 0x83, 0xEC}) &&
        PatchCalls({0x6B004D}, 0x543D60, reinterpret_cast<std::uintptr_t>(&OnSpringAlt));
    const bool damping = Matches(0x543E90, Entry{0x83, 0xEC, 0x28, 0x8B, 0x44, 0x24, 0x3C, 0xD9}) &&
        PatchCalls({0x6B027F}, 0x543E90, reinterpret_cast<std::uintptr_t>(&OnSpringDamping));
    const bool drive = Matches(0x6D05E0, Entry{0x8B, 0x44, 0x24, 0x10, 0x8B, 0x10, 0x89, 0x15}) &&
        Matches(0x6B27EA, std::array<std::uint8_t, 10>{0x8D, 0x96, 0x9C, 0x04, 0, 0, 0x52, 0x8D, 0x48, 0x2C}) &&
        PatchCalls({0x6B27F4}, 0x6D05E0, reinterpret_cast<std::uintptr_t>(&OnDriveAcceleration));
    Log(s_CarContacts ? "Installed: automobile surface contact and tyre temperature." : "Skipped: automobile contact layout differs.");
    Log(s_BikeContacts ? "Installed: motorcycle surface contact and tyre temperature." : "Skipped: motorcycle contact layout differs.");
    Log(springs && alternate && damping ? "Installed: progressive suspension and weight transfer."
        : "Suspension: one or more native force call sites differ.");
    Log(drive ? "Installed: class-specific throttle and shift points." : "Skipped: transmission call site differs.");
}

void InstallAirAndWaterDynamics() noexcept {
    const bool queries = VehicleQueriesMatch();
    const bool air = queries &&
        Matches(NativeFlight, Entry{0x90, 0xE9, 0xF2, 0xB6, 0xD2, 0xFF, 0x53, 0x55}) &&
        Matches(0x403CE8, Entry{0x81, 0xEC, 0x50, 0x02, 0, 0, 0xE9, 0x03}) &&
        Matches(0x5696C0, Entry{0x83, 0xEC, 0x38, 0x8B, 0x44, 0x24, 0x3C, 0x8B});
    const bool planes = air &&
        Matches(0x6CC475, std::array<std::uint8_t, 6>{0x50, 0x51, 0x6A, 0x03, 0x8B, 0xCE}) &&
        PatchCalls({0x6CC47B}, NativeFlight, reinterpret_cast<std::uintptr_t>(&OnFlyingControl));
    const bool helicopters = air &&
        Matches(0x6C5012, std::array<std::uint8_t, 6>{0x51, 0x52, 0x6A, 0x06, 0x8B, 0xCE}) &&
        PatchCalls({0x6C5018}, NativeFlight, reinterpret_cast<std::uintptr_t>(&OnFlyingControl));
    const bool boats = queries &&
        Matches(NativeBoat, Entry{0x6A, 0xFF, 0x68, 0xE3, 0x87, 0x84, 0x00, 0x64}) &&
        Matches(0x6F1F4C, std::array<std::uint8_t, 10>{0x8D, 0x96, 0x40, 0x06, 0, 0, 0x52, 0x50, 0x8B, 0xCE}) &&
        PatchCalls({0x6F1F56}, NativeBoat, reinterpret_cast<std::uintptr_t>(&OnBoatControl));
    Log(planes ? "Installed: fixed-wing flight handling."
        : "Skipped: fixed-wing flight call site differs.");
    Log(helicopters ? "Installed: helicopter flight handling."
        : "Skipped: helicopter flight call site differs.");
    Log(boats ? "Installed: boat hull resistance and planing."
        : "Skipped: boat physics call site differs.");
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
    s_CityVenues = attractors &&
        Matches(0x564A20, Entry{0x83, 0xEC, 0x18, 0x53, 0x55, 0x56, 0x57, 0x8B}) &&
        Matches(0x4C4C70, Entry{0x53, 0x55, 0x8B, 0xD9, 0x8B, 0x03, 0x0F, 0xB6}) &&
        Matches(0x4AF46B, Entry{0x8B, 0x3C, 0x95, 0xC8, 0xB0, 0xA9, 0, 0x8A}) &&
        Matches(0x5EE190, Entry{0x6A, 0xFF, 0x68, 0x8B, 0xDC, 0x83, 0, 0x64}) &&
        Matches(0x5EBB00, Entry{0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x74, 0x06}) &&
        Matches(0x6321F0, Entry{0x6A, 0xFF, 0x68, 0x38, 0x06, 0x84, 0, 0x64});
    Log(pedestrians ? "Installed: ambient pedestrian schedules." : "Skipped: population function differs.");
    Log(cars ? "Installed: ambient car schedules." : "Skipped: car generation function differs.");
    Log(attractors ? "Installed: neighborhood activity schedules." : "Skipped: pedestrian attractor function differs.");
}

bool InstallStreetPedestrians() noexcept {
    const bool routines = Matches(0x674C30, Entry{0x83, 0xEC, 0x0C, 0x55, 0x56, 0x8B, 0xF1, 0x8B}) &&
        PatchTaskMethod(StandardWanderVtable + 10 * sizeof(void*), 0x674C30,
            reinterpret_cast<std::uintptr_t>(&OnWanderControl));
    Log(routines ? "Installed: civilian walking routines." : "Skipped: civilian wander task differs.");
    s_CityTasks = routines && PedQueriesMatch() &&
        Matches(0x61A5A0, Entry{0x8B, 0x0D, 0xA8, 0x44, 0xB7, 0x00, 0xE9, 0x55}) &&
        Matches(0x65BD20, Entry{0x56, 0x8B, 0xF1, 0xE8, 0x88, 0xE6, 0xFB, 0xFF}) &&
        Matches(0x62F310, Entry{0x56, 0x8B, 0xF1, 0xE8, 0x78, 0xB0, 0xFE, 0xFF}) &&
        Matches(NativeWanderFirst, Entry{0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x8B, 0x87}) &&
        Matches(0x674C80, Entry{0x66, 0x89, 0x46, 0x1C, 0x66, 0x89, 0x46, 0x18}) &&
        Matches(NativeWanderNext, Entry{0x6A, 0xFF, 0x64, 0xA1, 0, 0, 0, 0}) &&
        PatchTaskMethod(StandardWanderVtable + 8 * sizeof(void*), NativeWanderNext,
            reinterpret_cast<std::uintptr_t>(&OnWanderNext));
    s_CityVenues = s_CityTasks && s_CityVenues;
    s_CityPhones = s_CityTasks &&
        Matches(0x6348A0, Entry{0x56, 0x8B, 0xF1, 0xE8, 0x08, 0x5B, 0xFE, 0xFF});
    Log(s_CityVenues ? "Installed: scheduled civilian visits to native venues." : "Skipped: civilian venue task differs.");
    Log(s_CityPhones ? "Installed: native civilian phone calls." : "Skipped: civilian phone task differs.");
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
    Log("Ultimate Living World 0.18.0");
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
        InstallRoadDynamics();
        InstallAirAndWaterDynamics();
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
