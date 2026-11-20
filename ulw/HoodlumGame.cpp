#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "IntelligenceCore.h"
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
constexpr std::uintptr_t PedPool = 0xB74490;
constexpr std::uintptr_t TaskPool = 0xB744A8;
constexpr std::uintptr_t VehiclePool = 0xB74494;
constexpr std::uintptr_t PoliceHelicopters = 0xC1C964;
constexpr std::uintptr_t ActiveCamera = 0xB6F028;
constexpr std::uintptr_t ScriptSpace = 0xA49960;
constexpr std::uintptr_t MissionFlagOffset = 0xA476AC;
constexpr std::size_t MainScriptLimit = 200000;

using ShotResponse = void(__thiscall*)(void* handler, const void* shot, void* activeTask, void* simplestTask);
using PedReference = int(__cdecl*)(const void* ped);
using LineClear = bool(__cdecl*)(const ulw::Point&, const ulw::Point&, bool, bool, bool, bool, bool, bool, bool);
using AllocateTask = void*(__cdecl*)(std::size_t);
using TurnToPoint = void*(__thiscall*)(void*, const ulw::Point&, float, float);
using FleePoint = void*(__thiscall*)(void*, const ulw::Point&, bool, float, int);
using SeekPoint = ulw::Point*(__thiscall*)(void*, ulw::Point*, const void*);
using WantedPointer = const void*(__cdecl*)(int);
using TrafficSpeed = float(__cdecl*)(void*);
using CarAI = void(__cdecl*)(void*);
using FlyHelicopter = void(__cdecl*)(void*, float, float, bool);
using HeadingForOffset = float(__cdecl*)(float, float);
using AddSearchLight = void(__cdecl*)(const ulw::Point&, const ulw::Point&, float, float, std::uint32_t, std::uint8_t, std::uint8_t);
using RoadblockNodeInfo = bool(__cdecl*)(std::uint32_t, float*, ulw::Point*);
using PlayerVehicle = const void*(__cdecl*)(int, bool);
using ChaseNode = bool(__cdecl*)(void*, float, float, float, const void*);
using BlockSteering = void(__cdecl*)(void*, const void*, float, float, float, float, float*, float*, float*, bool*);
using PopulationUpdate = void(__cdecl*)(bool);
using RandomCars = void(__cdecl*)();
using AddAttractorPed = bool(__cdecl*)(int, const void*, ulw::Point, void*, int);
using FireWeapon = bool(__thiscall*)(void*, void*, const void*, const void*, const void*, const void*, const void*);
using ProcessWheel = void(__thiscall*)(void*, void*, void*, void*, void*, int, float, float, float,
    std::int8_t, float*, std::int32_t*, std::uint16_t);

ulw::IntelligenceCore s_Core{};
ulw::PoliceSearch s_Police{};
ulw::RoadblockAttempts s_Roadblocks{};
ulw::CrashScenes s_Crashes{};
ulw::RoadPhysics s_RoadPhysics{};
struct EmergencyCar {
    ulw::Point position;
    ulw::Point forward;
    std::uint32_t ref;
    std::uint32_t heardAt;
    bool active;
};
std::array<EmergencyCar, 16> s_Sirens{};
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
unsigned s_AllocationFailures{};
std::uint32_t s_LastLogTime{};
std::uint32_t s_LastGameTime{};
bool s_Diagnostics{};
bool s_WasActive{};
bool s_StreetActive{};
bool s_UseSightingForRoadblocks{};
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
    char message[384]{};
    std::snprintf(message, sizeof(message),
        "Look=%u Flee=%u Panic=%u taskPoolFailures=%u pursuitUpdates=%u reportUpdates=%u searchUpdates=%u patrolRoutes=%u airSearches=%u roadblockAttempts=%u limitedSpeedCalls=%u sirenSpeedCalls=%u crashSpeedCalls=%u attractorUses=%u",
        s_Reactions[1], s_Reactions[2], s_Reactions[3], s_AllocationFailures,
        s_SearchStages[static_cast<unsigned>(SearchPhase::Pursue)],
        s_SearchStages[static_cast<unsigned>(SearchPhase::Respond)],
        s_SearchStages[static_cast<unsigned>(SearchPhase::Search)],
        s_PatrolRoutes, s_AirSearches, s_RoadblockAttempts, s_RestrictedDrivers, s_YieldingDrivers, s_CrashSlowdowns, s_AttractorUses);
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

bool FreeRoam() noexcept {
    if (GameValue<std::uint8_t>(0xA444B1) || GameValue<std::uint8_t>(0xB5F852) ||
        GameValue<std::uint8_t>(0xB7CB48) || GameValue<std::uint8_t>(0xB7CB49)) {
        return false;
    }
    const auto offset = GameValue<std::uint32_t>(MissionFlagOffset);
    return offset != 0 && offset <= MainScriptLimit - sizeof(std::uint32_t) &&
        GameValue<std::uint32_t>(ScriptSpace + offset) == 0;
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

bool StreetActive(std::uint32_t now) noexcept {
    const auto active = FreeRoam();
    if ((s_StreetActive && !active) || now < s_StreetTime) {
        s_Police.Reset();
        s_Roadblocks.Reset();
        s_AirPatrols = {};
        s_RoadPhysics.Reset();
        s_Crashes.Reset();
        s_Sirens = {};
    }
    s_StreetActive = active;
    s_StreetTime = now;
    return active;
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

bool __fastcall OnWeaponFire(void* weapon, void*, void* ped, const void* start, const void* muzzle,
    const void* target, const void* aim, const void* alternate) noexcept {
    const auto fire = GameFunction<FireWeapon>(0x742300);
    const auto weaponType = Read<std::uint32_t>(weapon);
    if (!target || (Read<std::uint8_t>(target, 0x36) & 7) != 3 || Read<std::uint32_t>(target, 0x598) != 0 ||
        weaponType < 22 || weaponType > 34 || !StreetActive(GameValue<std::uint32_t>(0xB7CB84))) {
        return fire(weapon, ped, start, muzzle, target, aim, alternate);
    }
    const auto faction = Read<std::uint32_t>(ped, 0x598);
    // The engine marks free-roam territory attack waves as mission peds.
    const bool attackWave = faction >= 7 && faction <= 16 && (Read<std::uint8_t>(ped, 0x475) & 0x10);
    if ((!attackWave && Read<std::uint8_t>(ped, 0x484) != 1) ||
        Read<std::uint8_t>(ped, 0x2F) != 0 || (Read<std::uint8_t>(ped, 0x46D) & 0x07) ||
        faction < 6 || faction > 16) {
        return fire(weapon, ped, start, muzzle, target, aim, alternate);
    }
    auto& accuracy = *(static_cast<std::uint8_t*>(ped) + 0x71A);
    const auto native = accuracy;
    accuracy = ulw::CombatAccuracy(native, faction, Read<float>(ped, 0x540),
        ulw::DistanceSquared(EntityPosition(ped), EntityPosition(target)),
        GameValue<float>(0xC81308), Fog(), GameValue<std::uint8_t>(0xB70153));
    const auto fired = fire(weapon, ped, start, muzzle, target, aim, alternate);
    accuracy = native;
    return fired;
}

unsigned ActiveWantedLevel() noexcept {
    const auto* const wanted = GameFunction<WantedPointer>(0x56E230)(0);
    const auto level = wanted && !(Read<std::uint8_t>(wanted, 0x1E) & 7) ? Read<std::uint32_t>(wanted, 0x2C) : 0u;
    if (!level) {
        s_Police.Reset();
        s_AirPatrols = {};
        s_Roadblocks.Reset();
    }
    return level;
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
    const auto search = s_Police.Target(id, now, origin, mode);
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

bool PatrolReference(const void* car, std::uint32_t now, std::uint32_t& id) noexcept {
    if (!StreetActive(now) || !IsRoadCar(car) || !GameValue<void*>(VehiclePool) ||
        Read<std::uint8_t>(car, 0x4A4) == 2 || !(Read<std::uint8_t>(car, 0x428) & 1)) {
        return false;
    }
    const auto* const driver = Read<const void*>(car, 0x460);
    if (!driver || Read<std::uint32_t>(driver, 0x598) != 6 || !ActiveWantedLevel()) {
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
    if (GameValue<std::int32_t>(0xB72914) != 0 || !PatrolReference(car, now, id)) {
        return pick(car, x, y, z, targetCar);
    }
    const auto search = SearchForPlayer(car, EntityPosition(car), {x, y, z}, id, now, ulw::PatrolMode::Car);
    if (search.phase == ulw::SearchPhase::Pursue) {
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
    if (target && GameValue<std::int32_t>(0xB72914) == 0 && PatrolReference(car, now, id)) {
        const auto origin = EntityPosition(car);
        const auto search = SearchForPlayer(car, origin, {x, y, EntityPosition(target).z}, id, now, ulw::PatrolMode::Car);
        if (search.phase != ulw::SearchPhase::Pursue) {
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

bool __cdecl OnRoadblockNode(std::uint32_t address, float* width, ulw::Point* crossRoad) noexcept {
    const auto info = GameFunction<RoadblockNodeInfo>(0x460EE0);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!StreetActive(now) || GameValue<std::int32_t>(0xB72914) != 0) {
        return info(address, width, crossRoad);
    }
    const auto wanted = ActiveWantedLevel();
    const auto area = address & 0xffffu;
    const auto index = address >> 16;
    if (wanted < 3 || !s_Roadblocks.Ready(now, wanted) || area >= 64 ||
        index >= GameValue<std::uint32_t>(0x970114 + area * sizeof(std::uint32_t))) {
        return false;
    }
    const auto* const nodes = GameValue<const std::byte*>(0x96F854 + area * sizeof(void*));
    if (!nodes) {
        return false;
    }
    const auto* const node = nodes + index * 0x1C;
    const auto flags = Read<std::uint8_t>(node, 0x18);
    const auto links = flags & 0x0Fu;
    if (links < 2 || (flags & 0xB0)) {
        return false;
    }
    const ulw::Point road{
        static_cast<float>(Read<std::int16_t>(node, 0x08)) / 8.0f,
        static_cast<float>(Read<std::int16_t>(node, 0x0A)) / 8.0f,
        static_cast<float>(Read<std::int16_t>(node, 0x0C)) / 8.0f,
    };
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

void __cdecl OnCarAI(void* car) noexcept {
    GameFunction<CarAI>(0x41DA30)(car);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!StreetActive(now) || !IsRoadCar(car) || !GameValue<void*>(VehiclePool)) {
        return;
    }
    const bool emergency = (Read<std::uint8_t>(car, 0x428) & 7) != 0;
    const bool ambient = Read<std::uint8_t>(car, 0x4A4) == 1;
    if (!ambient && !emergency) {
        return;
    }
    const auto ref = GameFunction<PedReference>(0x54FFC0)(car);
    if (ref < 0 || (ref & 0x80)) {
        return;
    }
    const auto key = static_cast<std::uint32_t>(ref);
    if (ambient) {
        s_Crashes.Observe({key, now, Read<std::uint32_t>(car, 0x3C), EntityPosition(car),
            ulw::DistanceSquared(Read<ulw::Point>(car, 0x44), {}), Read<float>(car, 0xD8)});
    }
    if (!emergency) {
        return;
    }
    auto& entry = s_Sirens[(key >> 8) % s_Sirens.size()];
    if (!(Read<std::uint8_t>(car, 0x42D) & 0x80) || !Read<const void*>(car, 0x460)) {
        if (entry.ref == key) {
            entry.active = false;
        }
        return;
    }
    entry = {EntityPosition(car), EntityForward(car), key, now, true};
}

bool ApproachingSiren(ulw::Point position, ulw::Point forward, std::uint32_t now) noexcept {
    for (const auto& siren : s_Sirens) {
        if (!siren.active || now - siren.heardAt > 700 || std::abs(position.z - siren.position.z) > 3.0f) {
            continue;
        }
        const auto dx = position.x - siren.position.x;
        const auto dy = position.y - siren.position.y;
        const auto inFront = siren.forward.x * dx + siren.forward.y * dy;
        const auto lateral = siren.forward.x * dy - siren.forward.y * dx;
        if (inFront > 4.0f && inFront < 27.0f && std::abs(lateral) < 4.5f &&
            forward.x * siren.forward.x + forward.y * siren.forward.y > 0.75f) {
            return true;
        }
    }
    return false;
}

float __cdecl OnTrafficSpeed(void* car) noexcept {
    const auto limit = GameFunction<TrafficSpeed>(0x434400)(car);
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (!StreetActive(now) || !IsRoadCar(car) || Read<std::uint8_t>(car, 0x4A4) != 1 ||
        Read<std::uint8_t>(car, 0x3BA) != 1 || (Read<std::uint8_t>(car, 0x428) & 7)) {
        return limit;
    }
    const auto* const driver = Read<const void*>(car, 0x460);
    if (!driver || Read<std::uint8_t>(driver, 0x484) != 1) {
        return limit;
    }
    const auto collidedAt = Read<std::uint32_t>(car, 0x3C);
    const auto position = EntityPosition(car);
    const auto forward = EntityForward(car);
    const auto siren = ApproachingSiren(position, forward, now);
    const auto trafficIncident = s_Crashes.Ahead(position, forward, now);
    const auto adjusted = ulw::TrafficLimit({
        limit, static_cast<float>(Read<std::uint8_t>(car, 0x3D0)), GameValue<float>(0xC81308), Fog(),
        GameValue<std::uint8_t>(0xB70153), Read<std::uint16_t>(car, 0x20),
        collidedAt != 0 && now - collidedAt < 1350 && Read<float>(car, 0xD8) > 5.0f,
        siren, trafficIncident,
    });
    if (adjusted < limit) {
        ++s_RestrictedDrivers;
        s_YieldingDrivers += siren ? 1u : 0u;
        s_CrashSlowdowns += trafficIncident ? 1u : 0u;
    }
    LogActivity(now);
    return adjusted;
}

void __fastcall OnWheel(void* car, void*, void* forward, void* right, void* contactSpeed, void* contact,
    int wheels, float thrust, float brake, float grip, std::int8_t wheel, float* wheelSpeed,
    std::int32_t* wheelState, std::uint16_t status) noexcept {
    ulw::WheelForces forces{thrust, brake, grip};
    const auto now = GameValue<std::uint32_t>(0xB7CB84);
    if (StreetActive(now) && IsRoadCar(car) && Read<std::uint8_t>(car, 0x4A4) != 2 &&
        (Read<std::uint8_t>(car, 0x36) >> 3) == 0 && GameValue<void*>(VehiclePool)) {
        const auto ref = GameFunction<PedReference>(0x54FFC0)(car);
        if (ref >= 0 && !(ref & 0x80)) {
            const auto speed = Read<ulw::Point>(car, 0x44);
            forces = s_RoadPhysics.Tune(forces, {
                static_cast<std::uint32_t>(ref), now, Read<float>(car, 0x8C), Read<float>(car, 0x4C0),
                GameValue<float>(0xC81308), ulw::DistanceSquared(speed, {}), Read<float>(car, 0x4A0), status != 0,
            });
        }
    }
    GameFunction<ProcessWheel>(0x6D6C00)(car, forward, right, contactSpeed, contact, wheels,
        forces.thrust, forces.brake, forces.grip, wheel, wheelSpeed, wheelState, status);
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

void __fastcall OnShot(void* handler, void*, const void* shot, void* activeTask, void* simplestTask) noexcept {
    auto* const task = ChooseReaction(handler, shot);
    if (task) {
        // The native dispatcher takes ownership through this response slot.
        std::memcpy(static_cast<std::byte*>(handler) + 0x24, &task, sizeof(task));
    } else {
        GameFunction<ShotResponse>(NativeShotResponse)(handler, shot, activeTask, simplestTask);
    }
    LogActivity(GameValue<std::uint32_t>(0xB7CB84));
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

using Entry = std::array<std::uint8_t, 8>;

bool PedQueriesMatch() noexcept {
    return Matches(0x54FF60, Entry{0x8B, 0x4C, 0x24, 0x04, 0x56, 0x8B, 0x35, 0x90}) &&
        Matches(0x56A490, Entry{0x83, 0xEC, 0x5C, 0x66, 0x81, 0x3D, 0x78, 0xCD});
}

bool VehicleQueriesMatch() noexcept {
    return Matches(0x54FFC0, Entry{0x8B, 0x4C, 0x24, 0x04, 0x56, 0x8B, 0x35, 0x94});
}

bool InstallCivilian() noexcept {
    constexpr std::array<std::uint8_t, 15> dispatch{
        0x55, 0x53, 0x57, 0x8B, 0xCE, 0xE8, 0x5F, 0x8D, 0xFF, 0xFF, 0xE9, 0xFE, 0x02, 0x00, 0x00,
    };
    return Matches(0x4C39A7, dispatch) &&
        Matches(NativeShotResponse, Entry{0x64, 0xA1, 0, 0, 0, 0, 0x6A, 0xFF}) &&
        Matches(0x4BCB3A, std::array<std::uint8_t, 3>{0x89, 0x47, 0x24}) &&
        PedQueriesMatch() &&
        Matches(0x61A5A0, Entry{0x8B, 0x0D, 0xA8, 0x44, 0xB7, 0x00, 0xE9, 0x55}) &&
        Matches(0x65BD20, Entry{0x56, 0x8B, 0xF1, 0xE8, 0x88, 0xE6, 0xFB, 0xFF}) &&
        Matches(0x66B910, Entry{0x56, 0x8B, 0xF1, 0xE8, 0x98, 0xEA, 0xFA, 0xFF}) &&
        PatchCalls({ShotDispatchCall}, NativeShotResponse, reinterpret_cast<std::uintptr_t>(&OnShot));
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

bool InstallTraffic() noexcept {
    if (!Matches(0x434400, Entry{0x83, 0xEC, 0x24, 0x56, 0x8B, 0x74, 0x24, 0x2C}) ||
        !PatchCalls({0x4347C3, 0x435302}, 0x434400, reinterpret_cast<std::uintptr_t>(&OnTrafficSpeed))) {
        return false;
    }
    const bool sirens = VehicleQueriesMatch() &&
        Matches(0x41DA30, Entry{0x90, 0xE9, 0x12, 0x62, 0xFE, 0xFF, 0x6A, 0xFF}) &&
        PatchCalls({0x6B51CD, 0x6B52A2}, 0x41DA30, reinterpret_cast<std::uintptr_t>(&OnCarAI));
    Log(sirens ? "Installed: siren and collision observations." : "Skipped: car observations.");
    return true;
}

bool InstallPhysics() noexcept {
    return VehicleQueriesMatch() &&
        Matches(0x6D6C00, Entry{0x83, 0xEC, 0x48, 0xD9, 0x05, 0x50, 0x8B, 0x85}) &&
        PatchCalls({0x6A567B, 0x6A56E9, 0x6A5B49, 0x6A5BB7}, 0x6D6C00,
            reinterpret_cast<std::uintptr_t>(&OnWheel));
}

bool InstallCombat() noexcept {
    return Matches(0x742300, Entry{0x83, 0xEC, 0x3C, 0x53, 0x56, 0x57, 0x8B, 0xF1}) &&
        Matches(0x73FBFD, std::array<std::uint8_t, 7>{0x0F, 0xB6, 0x97, 0x1A, 0x07, 0, 0}) &&
        PatchCalls({0x61ECCD}, 0x742300, reinterpret_cast<std::uintptr_t>(&OnWeaponFire));
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
    Log("Ultimate Living World 0.5.0");
    if (GetPrivateProfileIntW(L"Modules", L"Civilian", 1, config.c_str())) {
        const auto civilian = InstallCivilian();
        Log(civilian ? "Installed: civilian gunshot reactions." : "Skipped: civilian dispatcher differs.");
        const bool fire = civilian &&
            Matches(NativeFireResponse, Entry{0x64, 0xA1, 0, 0, 0, 0, 0x6A, 0xFF}) &&
            PatchCalls({0x4C3C73}, NativeFireResponse, reinterpret_cast<std::uintptr_t>(&OnFire));
        Log(fire ? "Installed: civilian fire reactions." : "Skipped: fire dispatcher differs.");
    }
    if (GetPrivateProfileIntW(L"Modules", L"Police", 1, config.c_str())) {
        const auto foot = InstallPolice();
        Log(foot ? "Installed: foot patrol sight and search." : "Skipped: police seek function differs.");
        const auto cars = InstallPatrolRoutes();
        s_UseSightingForRoadblocks = foot || cars;
    }
    if (GetPrivateProfileIntW(L"Modules", L"AirSupport", 1, config.c_str())) {
        const auto air = InstallAirSupport();
        s_UseSightingForRoadblocks = s_UseSightingForRoadblocks || air;
    }
    if (GetPrivateProfileIntW(L"Modules", L"Roadblocks", 1, config.c_str())) {
        Log(InstallRoadblocks() ? "Installed: dynamic roadblock placement filter." : "Skipped: roadblock function differs.");
    }
    if (GetPrivateProfileIntW(L"Modules", L"Traffic", 1, config.c_str())) {
        Log(InstallTraffic() ? "Installed: traffic speed limits." : "Skipped: traffic speed function differs.");
    }
    if (GetPrivateProfileIntW(L"Modules", L"VehiclePhysics", 1, config.c_str())) {
        Log(InstallPhysics() ? "Installed: road grip and braking." : "Skipped: wheel call sites differ.");
    }
    if (GetPrivateProfileIntW(L"Modules", L"Combat", 1, config.c_str())) {
        Log(InstallCombat() ? "Installed: police and gang aiming profiles." : "Skipped: weapon function differs.");
    }
    if (GetPrivateProfileIntW(L"Modules", L"City", 1, config.c_str())) {
        InstallCity();
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
