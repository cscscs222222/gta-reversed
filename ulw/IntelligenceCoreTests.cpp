#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "IntelligenceCore.h"
#include "StreetSystems.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using ulw::IntelligenceCore;
using ulw::Response;

void Check(bool value, const char* reason) {
    if (!value) {
        throw std::runtime_error(reason);
    }
}

void TestDecisions() {
    IntelligenceCore core;
    for (unsigned i = 0; i < IntelligenceCore::DecisionsPerFrame; ++i) {
        Check(core.TakeBudget(42), "expected a budget slot");
    }
    Check(!core.TakeBudget(42), "frame budget exceeded");
    Check(core.TakeBudget(43), "budget did not reset next frame");

    const ulw::Point source{12.0f, 19.0f, 5.0f};
    Check(core.ReactToShot({0x101, 1000, source, 16.0f, false}).response == Response::Panic, "a nearby sound should cause panic");
    Check(core.ReactToShot({0x201, 1000, source, 43.0f * 43.0f, true}).response == Response::Vanilla, "far events should stay native");
    Check(core.ReactToShot({0x201, 1000, source, std::numeric_limits<float>::quiet_NaN(), true}).response == Response::Vanilla,
        "invalid coordinates should stay native");

    std::uint32_t cautious{};
    std::uint32_t nextLifetime{};
    for (std::uint32_t slot = 1; slot < 128 && !cautious; ++slot) {
        const auto before = (slot << 8) + 1;
        const auto after = (slot << 8) + 2;
        core.Reset();
        const auto first = core.ReactToShot({before, 1000, source, 900.0f, false});
        core.Reset();
        const auto second = core.ReactToShot({after, 1000, source, 900.0f, false});
        if (first.response == Response::Look && second.response == Response::Look) {
            cautious = before;
            nextLifetime = after;
        }
    }
    Check(cautious != 0, "expected differing civilian temperaments");
    core.Reset();
    const auto look = core.ReactToShot({cautious, 1000, source, 900.0f, false});
    Check(look.response == Response::Look && look.focus.x == source.x && look.focus.y == source.y, "the civilian should face the sound");
    Check(core.ReactToShot({cautious, 1200, source, 900.0f, false}).response == Response::Vanilla, "repeat cooldown failed");
    Check(core.ReactToShot({cautious, 1550, source, 900.0f, false}).response == Response::Flee, "repeated threat should escalate");
    Check(core.ReactToShot({nextLifetime, 1600, source, 900.0f, false}).response == Response::Look, "reused slot inherited old memory");
    Check(core.ReactToShot({nextLifetime, 4601, source, 900.0f, false}).response == Response::Look, "old event memory did not expire");

    core.Reset();
    Check(core.ReactToHazard({0x101, 2100, source, 16.0f, false}).response == Response::Panic, "close fire should trigger panic");
    Check(core.ReactToHazard({0x101, 2200, source, 16.0f, true}).response == Response::Vanilla, "fire cooldown failed");
    Check(core.ReactToHazard({0x201, 2200, source, 100.0f, false}).response == Response::Vanilla, "distant hidden fire should stay native");
    Check(core.ReactToHazard({0x201, 2250, source, 100.0f, true}).response == Response::Flee, "visible fire should trigger flight");
}

void TestPoliceSearch() {
    using ulw::SearchPhase;
    ulw::PoliceSearch police;
    const ulw::Point start{10, 10, 0}, lastSeen{14, 12, 0};
    constexpr std::uint32_t lead = 0x101, nearby = 0x201, distant = 0x301;
    Check(police.Target(lead, 1000, start).phase == SearchPhase::Observe, "officer should start in observation");
    Check(police.ShouldSample(lead, 1000, 30), "expected first police sight sample");
    police.Observe(lead, 1000, start, lastSeen, true);
    Check(!police.ShouldSample(lead, 1100, 31), "same officer sampled too often");
    Check(police.Target(lead, 1200, start).phase == SearchPhase::Pursue, "visible player should be pursued");
    police.Observe(lead, 1250, start, {20, 20, 0}, false);
    const auto hidden = police.Target(lead, 1280, start);
    Check(hidden.phase == SearchPhase::LastSeen && hidden.point.x == lastSeen.x, "occlusion leaked the current target");
    police.Observe(nearby, 1280, start, {}, false);
    Check(police.Target(nearby, 1300, start).phase == SearchPhase::Respond, "nearby officer missed the sight report");
    const ulw::Point farPosition{50, 50, 0};
    police.Observe(distant, 1280, farPosition, {}, false);
    Check(police.Target(distant, 1300, farPosition).phase == SearchPhase::Lost, "sight report travelled too far");
    const auto firstSweep = police.Target(lead, 3500, lastSeen);
    const auto secondSweep = police.Target(nearby, 3500, lastSeen);
    Check(firstSweep.phase == SearchPhase::Search && secondSweep.phase == SearchPhase::Search &&
        ulw::DistanceSquared(firstSweep.point, secondSweep.point) > 0, "officers should sweep different positions");
    Check(police.Target(0x102, 3600, start).phase == SearchPhase::Observe, "reused officer inherited a sighting");
    Check(police.Target(nearby, 4800, start).phase == SearchPhase::Lost, "shared report should expire");
    Check(police.Target(lead, 7500, start).phase == SearchPhase::Lost, "personal sighting should expire");
    for (std::uint32_t slot = 4; slot < 9; ++slot) {
        Check(police.ShouldSample(slot << 8, 7600, 32), "expected police sight sample");
    }
    Check(!police.ShouldSample(0x901, 7600, 32) && police.ShouldSample(0x901, 7616, 33), "LOS budget should reset each frame");
    police.Reset();
    Check(police.Target(lead, 1000, start).phase == SearchPhase::Observe, "reset retained the old chase");
}

void TestTrafficAndPhysics() {
    ulw::TrafficInput input{40, 30, 0, 0, 12, 505, false, false};
    const auto dry = ulw::TrafficLimit(input);
    Check(dry > 0 && dry <= input.cruiseSpeed, "traffic cap must use the cruise speed");
    input.wet = 1;
    input.fog = 1;
    input.hour = 23;
    const auto poorVisibility = ulw::TrafficLimit(input);
    Check(poorVisibility < dry && ulw::SightRange(1, 1, 23) < ulw::SightRange(0, 0, 12), "wet night fog should limit speed and sight");
    input.sirenApproaching = true;
    Check(ulw::TrafficLimit(input) < poorVisibility, "an approaching siren should slow traffic");
    input.recentCollision = true;
    Check(ulw::TrafficLimit(input) < poorVisibility * 0.7f, "traffic should slow after collision");
    input.nativeLimit = 0;
    Check(ulw::TrafficLimit(input) == 0, "the red-light limit must stay at zero");
    input.nativeLimit = 2;
    Check(ulw::TrafficLimit(input) == 2, "the native limit must never be increased");

    ulw::RoadPhysics road;
    const ulw::WheelForces native{0.18f, 0.3f, 0.2f};
    ulw::RoadConditions vehicle{0x401, 1000, 3950, 1000, 0, 1, 0, false};
    Check(road.Tune(native, vehicle).grip == native.grip, "healthy tyres on a dry road should keep native grip");
    vehicle.wet = 1;
    Check(road.Tune(native, vehicle).grip < native.grip, "wet roads should reduce grip");
    vehicle.burstTyre = true;
    Check(road.Tune(native, vehicle).grip == native.grip, "burst tyres should use the game's existing slip");
    vehicle.burstTyre = false;
    vehicle.brakePedal = 1;
    for (; vehicle.now <= 4000; vehicle.now += 80) {
        road.Tune(native, vehicle);
    }
    const auto hot = road.Tune(native, vehicle);
    Check(hot.brake < native.brake && road.Tune(native, vehicle).brake == hot.brake, "brakes should heat once per frame");
    vehicle.vehicle = 0x402;
    Check(road.Tune(native, vehicle).brake == native.brake, "reused vehicle inherited brake heat");
    vehicle.health = 250;
    const auto worn = road.Tune(native, vehicle);
    Check(worn.thrust < native.thrust && worn.brake < native.brake, "near-critical damage should reduce thrust and braking");
}

void TestCombat() {
    const auto closeCop = ulw::CombatAccuracy(60, 6, 100, 36, 0, 0, 12);
    const auto closeGang = ulw::CombatAccuracy(60, 7, 100, 36, 0, 0, 12);
    Check(closeCop > 60 && closeGang > 60 && closeGang != closeCop, "police and gangs should have distinct skill");
    Check(ulw::CombatAccuracy(60, 6, 20, 900, 1, 1, 23) < closeCop, "poor conditions should degrade aim");
    Check(ulw::CombatAccuracy(100, 6, 100, 36, 0, 0, 12) == 100, "combat accuracy must not exceed 100");
    Check(ulw::CombatAccuracy(60, 4, 100, 36, 0, 0, 12) == 60, "civilian accuracy must remain native");
    Check(ulw::CombatAccuracy(0, 7, 100, 36, 0, 0, 12) == 0, "intentional misses must remain native");
}

void TestCityActivity() {
    const auto laRush = ulw::AmbientActivity(8, 3, 1, 0, 0);
    const auto laLate = ulw::AmbientActivity(23, 3, 1, 0, 0);
    const auto vegasLate = ulw::AmbientActivity(23, 3, 3, 0, 0);
    const auto weekend = ulw::AmbientActivity(8, 7, 1, 0, 0);
    const auto foggyDesert = ulw::AmbientActivity(8, 3, 4, 0, 1);
    const auto wetRush = ulw::AmbientActivity(8, 3, 1, 1, 0);
    Check(laRush.cars > weekend.cars && laRush.cars > laLate.cars, "traffic schedules should distinguish rush hour");
    Check(vegasLate.pedestrians > laLate.pedestrians && vegasLate.cars > laLate.cars, "Las Venturas should stay busier at night");
    Check(foggyDesert.cars < laRush.cars && wetRush.pedestrians < laRush.pedestrians, "weather should reduce outdoor activity");
    Check(ulw::AmbientActivity(0, 0, 1, 1, 1).cars == 1, "uninitialized clock should use native activity");
}

void TestLoader(const std::filesystem::path& asi) {
    const auto logPath = asi.parent_path() / L"UltimateLivingWorld.log";
    std::error_code error;
    std::filesystem::remove(logPath, error);
    Check(!error, "could not clear the test log");
    auto* const module = LoadLibraryW(asi.c_str());
    Check(module != nullptr, "could not load UltimateLivingWorld.asi in a test process");
    using Initialize = void(__cdecl*)();
    const auto address = GetProcAddress(module, "InitializeASI");
    Check(address != nullptr, "the ASI loader entry point is missing");
    static_assert(sizeof(address) == sizeof(Initialize));
    Initialize initialize{};
    std::memcpy(&initialize, &address, sizeof(initialize));
    initialize();
    FreeLibrary(module);
    std::ifstream log{logPath};
    const std::string text{std::istreambuf_iterator<char>{log}, std::istreambuf_iterator<char>{}};
    Check(text.find("does not match the supported Hoodlum") != std::string::npos,
        "a non-game process should reject the hook before touching any game address");
}

}

int wmain(int argc, wchar_t** argv) {
    try {
        TestDecisions();
        std::puts("Core: frame budget, perception, memory, slot reuse and reactions passed.");
        TestPoliceSearch();
        TestTrafficAndPhysics();
        std::puts("Street systems: police search and reports, traffic limits, wet grip and brake fade passed.");
        TestCombat();
        std::puts("Combat: aiming profiles, injury, weather and native accuracy limits passed.");
        TestCityActivity();
        std::puts("City: rush hour, regional nightlife and weather schedules passed.");
        if (argc == 2) {
            TestLoader(argv[1]);
            std::puts("ASI: load and InitializeASI passed. The non-game process was rejected.");
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
