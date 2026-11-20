#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "IntelligenceCore.h"
#include "MissionState.h"
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
    using ulw::PatrolMode;
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

    constexpr std::uint32_t cruiser = 0x80000101;
    const ulw::Point acrossStreet{62, 10, 0};
    police.Observe(lead, 1000, start, lastSeen, true);
    Check(police.Target(cruiser, 1250, acrossStreet, PatrolMode::Car).phase == SearchPhase::Respond,
        "an unsampled cruiser should act on a nearby foot patrol report");
    const auto sweep = police.Target(cruiser, 3500, lastSeen, PatrolMode::Car);
    Check(sweep.phase == SearchPhase::Search && ulw::DistanceSquared(sweep.point, lastSeen) == 400,
        "cruiser sweep should be wide enough to hand off to the road graph");
    police.Observe(cruiser, 3550, acrossStreet, {62, 16, 0}, true);
    police.Observe(cruiser, 3800, acrossStreet, {100, 110, 0}, false);
    const auto lostSight = police.Target(cruiser, 3850, acrossStreet, PatrolMode::Car);
    Check(lostSight.phase == SearchPhase::LastSeen && lostSight.point.x == 62 && lostSight.point.y == 16,
        "an obscured player's live coordinates must not overwrite the patrol route");
    Check(police.Target(0x401, 3850, acrossStreet).phase == SearchPhase::Respond,
        "a foot patrol should get the cruiser report");
}

void TestCrashScenes() {
    ulw::CrashScenes scenes;
    constexpr ulw::Point scene{0, 15, 0}, north{0, 1, 0};
    scenes.Observe({0x301, 1000, 900, scene, 0.0f, 5.0f});
    Check(!scenes.Ahead({}, north, 1100), "a minor bump must not start a crash scene");
    scenes.Observe({0x301, 1000, 900, scene, 0.08f, 45.0f});
    Check(!scenes.Ahead({}, north, 1100), "a moving car must not mark the road obstructed");
    scenes.Observe({0x301, 1000, 900, scene, 0.0f, 45.0f});
    Check(scenes.Ahead({}, north, 1100), "a stopped crash should affect approaching traffic");
    Check(!scenes.Ahead({0, 0, 6}, north, 1100) && !scenes.Ahead({}, {0, -1, 0}, 1100),
        "traffic travelling away or on another elevation must ignore the incident");
    Check(!scenes.Ahead({}, north, 8900), "crash scene should expire");
    scenes.Observe({0x302, 1350, 0, {50, 50, 0}, 0.0f, 0.0f});
    Check(!scenes.Ahead({}, north, 1350), "a recycled vehicle slot should clear the scene");
    scenes.Observe({0x302, 1600, 1510, scene, 0.0f, 45.0f});
    scenes.Observe({0x302, 2010, 1510, scene, 0.05f, 0.0f});
    Check(!scenes.Ahead({}, north, 2020), "a crash scene should clear once the car drives away");
    ulw::TrafficInput input{50, 40, 0, 0, 12, 12, false, false};
    const auto normal = ulw::TrafficLimit(input);
    input.crashAhead = true;
    Check(ulw::TrafficLimit(input) < normal, "the incident must lower the speed cap");
    input.nativeLimit = 0;
    Check(ulw::TrafficLimit(input) == 0, "crash scene must preserve the red-light stop");
}

void TestAirSupportAndRoadblocks() {
    using ulw::PatrolMode;
    using ulw::SearchPhase;
    constexpr std::uint32_t heli = 0xc0000101;
    constexpr ulw::Point building{32, 45, 5}, helicopter{36, 46, 70}, forward{0, 1, 0};
    ulw::PoliceSearch police;
    police.Observe(0x201, 1000, building, building, true);
    Check(police.Target(heli, 1100, helicopter, PatrolMode::Air).phase == SearchPhase::Respond,
        "air patrol should receive a sighting below its flight altitude");
    const auto sweep = police.Target(heli, 3400, helicopter, PatrolMode::Air);
    Check(sweep.phase == SearchPhase::Search && ulw::DistanceSquared(sweep.point, building) == 34 * 34,
        "the air patrol should start a wider search when it is above the last sighting");
    police.Observe(heli, 3600, helicopter, building, true, PatrolMode::Air);
    police.Observe(heli, 3820, helicopter, {60, 80, 2}, false, PatrolMode::Air);
    Check(police.Target(0x301, 3900, building).phase == SearchPhase::Respond,
        "ground patrol should receive a report from the helicopter");
    const auto airborneSearch = police.Target(heli, 3900, helicopter, PatrolMode::Air);
    Check(airborneSearch.phase == SearchPhase::LastSeen && airborneSearch.point.y == building.y,
        "a concealed target must not refresh the helicopter report");
    const auto holding = ulw::AirSearchPoint({SearchPhase::Lost, {200, 200, 200}}, helicopter, forward, 5);
    Check(holding.x == helicopter.x && holding.y == helicopter.y + 28 && holding.z == 5,
        "lost aircraft contact should keep searching without steering to hidden coordinates");
    const auto aim = ulw::AirSearchPoint(sweep, helicopter, forward, 10);
    Check(ulw::DistanceSquared(aim, sweep.point) == 0 && !ulw::HasSearchContact(SearchPhase::Lost),
        "a searchlight should use the sweep point only while a report is active");
    Check(police.HasRecentReport({100, 45, 5}, 4000), "roadblocks should see nearby flight reports");
    Check(!police.HasRecentReport({100, 45, 25}, 4000) && !police.HasRecentReport({100, 45, 5}, 7800),
        "roadblocks should reject sightings at the wrong elevation or after expiry");

    ulw::RoadblockCandidate site{{0, 64, 0}, {}, 15, 0.05f, 4, 2, false, true, true, true};
    Check(ulw::SuitableRoadblock(site), "a wide, nearby road with a fresh sighting should be eligible");
    site.recentlySeen = false;
    Check(!ulw::SuitableRoadblock(site), "a roadblock should require recent observation while patrol scanning is enabled");
    site.sightingRequired = false;
    Check(ulw::SuitableRoadblock(site), "roadblock placement should work with the search modules turned off");
    site.speedSquared = 1.0f;
    Check(!ulw::SuitableRoadblock(site), "a fast chase needs more warning distance");
    site.road.y = 75;
    Check(ulw::SuitableRoadblock(site), "a fast chase should allow a distant roadblock");
    site.road.z = 10;
    Check(!ulw::SuitableRoadblock(site), "elevated roads need a separate search");
    site.road.z = 0;
    site.width = 8;
    Check(!ulw::SuitableRoadblock(site), "a narrow road is not safe for native placement");
    Check(ulw::InCameraCone({}, forward, site.road) && !ulw::InCameraCone({}, {0, -1, 0}, site.road),
        "candidate approach should distinguish where the camera points");
    ulw::RoadblockAttempts attempts;
    Check(attempts.Ready(0, 4), "the first roadblock should be eligible");
    attempts.Record(1000);
    Check(!attempts.Ready(11999, 5) && attempts.Ready(12000, 5) && !attempts.Ready(12000, 4),
        "roadblock retry timing should depend on wanted level");
    attempts.Reset();
    Check(attempts.Ready(1200, 4), "roadblock attempts should reset between searches");
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
    Check(ulw::AttractorUsePercent(2, 9, 3, 1, 0) > ulw::AttractorUsePercent(2, 9, 7, 1, 0),
        "bus stops should favor weekday commuting hours");
    Check(ulw::AttractorUsePercent(3, 1, 5, 3, 0) > ulw::AttractorUsePercent(3, 1, 5, 1, 0),
        "Las Venturas food stands should stay busier after midnight");
    Check(ulw::AttractorUsePercent(4, 11, 3, 1, 1) > ulw::AttractorUsePercent(4, 11, 3, 1, 0) &&
        ulw::AttractorUsePercent(1, 11, 3, 1, 1) < ulw::AttractorUsePercent(1, 11, 3, 1, 0),
        "rain should favor shelters instead of open benches");
    Check(ulw::AttractorUsePercent(5, 1, 5, 1, 1) == 100, "script triggers must retain native availability");
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

void TestMissionProtection() {
    using ulw::MissionPhase;
    using ulw::MissionProfile;

    ulw::MissionState state;
    Check(!state.AllowsWorld(), "world systems should wait for engine initialization");
    Check(state.Update({100, true}) && state.AllowsWorld(), "validated free roam should enable world systems");
    const auto train = ulw::NormalizeMissionName({'s', 'm', 'o', 'k', 'e', '3', 0, 0});
    const auto pursuit = ulw::NormalizeMissionName({'D', 'R', 'U', 'G', 'S', '1', 0, 0});
    const auto flight = ulw::NormalizeMissionName({'Z', 'E', 'R', 'O', '2', 0, 0, 0});
    const auto finale = ulw::NormalizeMissionName({'f', 'i', 'n', 'a', 'l', 'e', 'c', 0});
    Check(ulw::NormalizeMissionName({'n','o','n','a','m','e',0,0})[0] == 0 &&
        ulw::NormalizeMissionName({'a','\n','b',0,0,0,0,0})[1] == 0, "script identifiers need safe logging");

    state.Update({200, true, false, true, false, false, train});
    Check(state.Profile() == MissionProfile::TrainChase && !state.AllowsWorld(),
        "loaded train mission thread must protect the world without the global mission flag");
    state.Update({500, true, true, true, false, false, pursuit});
    Check(state.Profile() == MissionProfile::ScriptedPursuit, "Just Business should use scripted pursuit protection");
    state.Update({700, true, true, true, false, false, flight});
    Check(state.Profile() == MissionProfile::RemoteFlight, "Supply Lines should use remote flight protection");
    state.Update({900, true, true, true, true, false, finale});
    Check(state.Profile() == MissionProfile::Finale && state.Phase() == MissionPhase::Cutscene,
        "cutscenes must take precedence and retain the active finale profile");
    state.Update({1000, true});
    Check(state.Phase() == MissionPhase::Recovery && !state.AllowsWorld(),
        "cleanup protection should remain after the last active mission and cutscene disappear");
    state.Update({3499, true});
    Check(!state.AllowsWorld(), "the finale must get its full cleanup delay");
    state.Update({3500, true});
    Check(state.AllowsWorld() && !state.Name()[0], "free roam must resume and forget the old mission name");
    state.Update({3600, true, true});
    Check(state.Phase() == MissionPhase::Mission, "the native mission flag must protect unnamed missions");
    state.Update({3650, true, false, false, false, true});
    Check(state.Phase() == MissionPhase::Activity && !state.AllowsWorld(), "scripted activities must suspend ambient behavior");
    state.Update({200, false});
    Check(state.Phase() == MissionPhase::Loading && !state.AllowsWorld(), "a clock rewind should begin a fresh lifecycle");
    state.Update({240, true});
    Check(state.AllowsWorld(), "starting a new game must not inherit the old mission cooldown");
}

}

int wmain(int argc, wchar_t** argv) {
    try {
        TestDecisions();
        std::puts("Core: frame budget, perception, memory, slot reuse and reactions passed.");
        TestPoliceSearch();
        TestCrashScenes();
        TestTrafficAndPhysics();
        std::puts("Street systems: police search and reports, traffic limits, wet grip and brake fade passed.");
        TestAirSupportAndRoadblocks();
        std::puts("Police support: air and ground reports, search routes and roadblock placement passed.");
        TestMissionProtection();
        std::puts("Campaign: native and script mission signals, special profiles, cleanup, and reset passed.");
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
