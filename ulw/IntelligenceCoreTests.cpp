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
    scenes.Observe({0x301, 1900, 900, scene, 0.0f, 0.0f});
    Check(scenes.Ahead({}, north, 2000), "recent impact should still be known when the car stops later");
    Check(!scenes.Ahead({0, 0, 6}, north, 1100) && !scenes.Ahead({}, {0, -1, 0}, 1100),
        "traffic travelling away or on another elevation must ignore the incident");
    const auto witness = scenes.Closest({0, 7, 0}, 12, 2000);
    Check(witness && witness->vehicle == 0x301 && witness->impactAt == 900 &&
        !scenes.Closest({0, 7, 6}, 12, 2000), "only a nearby crash at the same level should attract attention");
    Check(scenes.ObstructsFootpath({0, 10, 0}, {0, 17, 0}, 2000) &&
        !scenes.ObstructsFootpath({0, 10, 0}, {0, 5, 0}, 2000) &&
        !scenes.ObstructsFootpath({0, 10, 6}, {0, 17, 6}, 2000) &&
        !scenes.ObstructsFootpath({5, 10, 0}, {5, 17, 0}, 2000),
        "only footpaths towards the same-level obstacle should be marked blocked");
    Check(!scenes.Ahead({}, north, 8900), "crash scene should expire");
    scenes.Observe({0x302, 1350, 0, {50, 50, 0}, 0.0f, 0.0f});
    Check(!scenes.Ahead({}, north, 1350), "a recycled vehicle slot should clear the scene");
    scenes.Observe({0x302, 1600, 1510, scene, 0.0f, 45.0f});
    scenes.Observe({0x302, 2010, 1510, scene, 0.05f, 0.0f});
    Check(!scenes.Ahead({}, north, 2020), "a crash scene should clear once the car drives away");
    scenes.Observe({0x302, 2020, 1510, scene, 0.0f, 0.0f});
    Check(!scenes.Ahead({}, north, 2030), "stopping again must not revive an old impact");
    ulw::TrafficInput input{50, 40, 0, 0, 12, 12, false};
    const auto normal = ulw::TrafficLimit(input);
    input.crashAhead = true;
    Check(ulw::TrafficLimit(input) < normal, "the incident must lower the speed cap");
    input.nativeLimit = 0;
    Check(ulw::TrafficLimit(input) == 0, "crash scene must preserve the red-light stop");
}

void TestPedestrianRoutines() {
    using ulw::PedestrianPace;
    bool rainyRush{};
    bool weekdayRush{};
    bool nightlife{};
    bool sports{};
    for (std::uint16_t seed = 0; seed < 512; ++seed) {
        const auto dry = ulw::PickPedestrianRoutine(seed, 13, 3, 1, 0);
        const auto rain = ulw::PickPedestrianRoutine(seed, 13, 3, 1, 1);
        rainyRush |= dry.pace == PedestrianPace::Walk && rain.pace == PedestrianPace::Jog && !rain.chat;
        weekdayRush |= ulw::PickPedestrianRoutine(seed, 17, 3, 1, 0).pace == PedestrianPace::Jog &&
            ulw::PickPedestrianRoutine(seed, 17, 7, 1, 0).pace == PedestrianPace::Walk;
        nightlife |= !ulw::PickPedestrianRoutine(seed, 23, 5, 1, 0).chat && ulw::PickPedestrianRoutine(seed, 23, 5, 3, 0).chat;
        sports |= ulw::PickPedestrianRoutine(seed, 7, 7, 1, 0).pace == PedestrianPace::Jog &&
            ulw::PickPedestrianRoutine(seed, 10, 7, 1, 0).pace == PedestrianPace::Walk;
    }
    Check(rainyRush && weekdayRush && nightlife && sports, "civilian routines should vary with rain, day and place");
    const auto invalidClock = ulw::PickPedestrianRoutine(10, 24, 0, 1, 1);
    Check(invalidClock.pace == PedestrianPace::Walk && invalidClock.chat, "an unset clock should keep native walking and social behavior");

    ulw::CrashWitnesses witnesses;
    constexpr ulw::CrashSite collision{{0, 2, 0}, 0x301, 700};
    Check(witnesses.TakeScan(0x101, 1000, 20) && !witnesses.Noticed(0x101, collision), "a new witness should inspect the scene");
    witnesses.Remember(0x101, collision);
    Check(witnesses.Noticed(0x101, collision) && !witnesses.TakeScan(0x101, 1100, 21),
        "a witness should remember the same incident and respect the scan interval");
    Check(witnesses.TakeScan(0x102, 1120, 22) && !witnesses.Noticed(0x102, collision),
        "reused pedestrian slots must not inherit old witness memory");
    for (std::uint32_t slot = 2; slot < 6; ++slot) {
        Check(witnesses.TakeScan(slot << 8, 1120, 22), "a nearby pedestrian should have a witness budget slot");
    }
    Check(!witnesses.TakeScan(0x601, 1120, 22) && witnesses.TakeScan(0x601, 1135, 23),
        "witness checks need a per-frame limit");
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
    ulw::TrafficInput input{40, 30, 0, 0, 12, 505, false};
    const auto dry = ulw::TrafficLimit(input);
    Check(dry > 0 && dry <= input.cruiseSpeed, "traffic cap must use the cruise speed");
    input.wet = 1;
    input.fog = 1;
    input.hour = 23;
    const auto poorVisibility = ulw::TrafficLimit(input);
    Check(poorVisibility < dry && ulw::SightRange(1, 1, 23) < ulw::SightRange(0, 0, 12), "wet night fog should limit speed and sight");
    input.emergencyScale = 0.74f;
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

void TestEmergencyServices() {
    using ulw::EmergencyState;
    constexpr ulw::Point north{0, 1, 0}, east{1, 0, 0};
    ulw::EmergencyUnit ambulance{{}, {0, 0.4f, 0}, 0x301, 1000, EmergencyState::Responding};
    Check(ulw::EmergencyYieldScale({0, 20, 0}, north, {0, 0.15f, 0}, 1100, ambulance) < 1,
        "a closing ambulance should cause cars ahead to yield");
    Check(ulw::EmergencyYieldScale({0, 20, 0}, north, {0, 0.45f, 0}, 1100, ambulance) == 1 &&
        ulw::EmergencyYieldScale({0, -10, 0}, north, {0, 0.15f, 0}, 1100, ambulance) == 1,
        "cars already pulling away or behind the response should maintain their pace");
    Check(ulw::EmergencyYieldScale({-5, 10, 0}, east, {0.2f, 0, 0}, 1100, ambulance) < 1,
        "crossing traffic should yield when both vehicles would reach the same spot");
    Check(ulw::EmergencyYieldScale({-12, 8, 0}, {-1, 0, 0}, {-0.2f, 0, 0}, 1100, ambulance) == 1 &&
        ulw::EmergencyYieldScale({-5, 10, 6}, east, {0.2f, 0, 0}, 1100, ambulance) == 1,
        "diverging paths and overpasses should stay native");
    Check(ulw::EmergencyYieldScale({0, 20, 0}, north, {0, 0.15f, 0}, 1900, ambulance) == 1,
        "old siren reports must expire");
    ambulance.state = EmergencyState::OnScene;
    ambulance.velocity = {};
    Check(ulw::EmergencyYieldScale({0, -10, 0}, north, {0, 0.16f, 0}, 2000, ambulance) < 1 &&
        ulw::EmergencyYieldScale({0, -10, 0}, {0, -1, 0}, {0, -0.16f, 0}, 2000, ambulance) == 1,
        "a parked response should only slow cars approaching the scene");
    Check(ulw::EmergencyYieldScale({0, -10, 0}, north, {0, 0.16f, 0}, 3000, ambulance) == 1,
        "an unobserved work zone must expire");

    constexpr ulw::DrivePedals native{0.8f, 0.0f};
    const auto dry = ulw::EmergencyApproach(native, 26.0f * 26.0f, 0.17f, 0.0f, 0.0f);
    const auto wet = ulw::EmergencyApproach(native, 26.0f * 26.0f, 0.17f, 1.0f, 1.0f);
    Check(dry.brake > 0 && dry.gas < native.gas && wet.brake > dry.brake && wet.gas < dry.gas,
        "emergency crews should slow earlier on wet foggy approaches");
    Check(ulw::EmergencyApproach(native, 70.0f * 70.0f, 0.17f, 1, 1).gas == native.gas &&
        ulw::EmergencyApproach(native, 20.0f * 20.0f, 0.001f, 1, 1).gas == native.gas,
        "normal driving must continue while far away or already crawling");
    Check(ulw::EmergencyApproach({0.6f, 0.9f}, 12.0f * 12.0f, 0.4f, 1, 1).brake == 0.9f &&
        ulw::EmergencyApproach({-0.5f, 0.2f}, 12.0f * 12.0f, 0.4f, 1, 1).gas == -0.5f,
        "emergency braking must not weaken native brakes or change reverse behavior");
}

void TestCombat() {
    const auto closeCop = ulw::CombatAccuracy(60, 6, 100, 36, 0, 0, 12);
    const auto closeGang = ulw::CombatAccuracy(60, 7, 100, 36, 0, 0, 12);
    Check(closeCop > 60 && closeGang > 60 && closeGang != closeCop, "police and gangs should have distinct skill");
    Check(ulw::CombatAccuracy(60, 6, 20, 900, 1, 1, 23) < closeCop, "poor conditions should degrade aim");
    Check(ulw::CombatAccuracy(100, 6, 100, 36, 0, 0, 12) == 100, "combat accuracy must not exceed 100");
    Check(ulw::CombatAccuracy(60, 4, 100, 36, 0, 0, 12) == 60, "civilian accuracy must remain native");
    Check(ulw::CombatAccuracy(0, 7, 100, 36, 0, 0, 12) == 0, "intentional misses must remain native");
    Check(ulw::CombatAccuracy(60, 6, 100, 36, 0, 0, 12, true) < closeCop,
        "repeated player hits should temporarily weaken aim");

    Check(ulw::CombatBurstLength(4, 30, 6, 100, false) == 6 &&
        ulw::CombatBurstLength(3, 30, 7, 100, false) == 4, "healthy fighters should extend automatic bursts");
    Check(ulw::CombatBurstLength(3, 4, 6, 100, false) == 4 &&
        ulw::CombatBurstLength(4, 2, 6, 100, false) == 4, "added rounds must fit the current clip");
    Check(ulw::CombatBurstLength(11, 30, 6, 100, false) == 12 &&
        ulw::CombatBurstLength(9, 30, 16, 100, false) == 10, "burst extensions should respect faction caps");
    Check(ulw::CombatBurstLength(5, 30, 6, 100, true) == 3 &&
        ulw::CombatBurstLength(5, 2, 7, 25, false) == 2, "pressure and serious injuries should shorten bursts");
    Check(ulw::CombatBurstLength(0, 30, 6, 100, false) == 0 &&
        ulw::CombatBurstLength(13, 30, 6, 100, true) == 13 &&
        ulw::CombatBurstLength(4, 0, 6, 100, true) == 4 &&
        ulw::CombatBurstLength(4, 30, 5, 100, false) == 4,
        "native reloads, special bursts and outside factions must be preserved");

    ulw::CombatPressure pressure;
    constexpr std::uint32_t officer = 0xA01;
    constexpr std::uint32_t replacement = 0xA02;
    Check(!pressure.RecordHit(officer, 1000, 1004) && !pressure.RecordHit(officer, 1000, 1010) &&
        !pressure.Suppressed(officer, 1010), "a hit and its duplicate must not start suppression");
    Check(pressure.RecordHit(officer, 2000, 2005) && pressure.Suppressed(officer, 2005) &&
        !pressure.Suppressed(replacement, 2005), "consecutive hits should suppress only the matching ped generation");
    Check(!pressure.RecordHit(officer, 1999, 2005) && !pressure.RecordHit(officer, 2250, 2501),
        "out-of-order and delayed events must not renew suppression");
    Check(pressure.RecordHit(officer, 3840, 3850) && pressure.Suppressed(officer, 6239) &&
        !pressure.Suppressed(officer, 6240), "a recent hit may renew suppression, which must expire on time");
    Check(!pressure.RecordHit(officer, 6800, 6801) && pressure.RecordHit(officer, 7600, 7601),
        "a late hit should start a fresh sequence");
    Check(!pressure.RecordHit(replacement, 7700, 7700) && !pressure.Suppressed(officer, 7700) &&
        !pressure.Suppressed(replacement, 7700), "reusing the game slot must start with an empty history");
    Check(pressure.RecordHit(replacement, 7800, 7800), "the replacement should accumulate its own hits");
    pressure.Reset();
    Check(!pressure.Suppressed(replacement, 7801), "leaving street mode must discard hit history");
}

void TestCombatCover() {
    constexpr ulw::Point fighter{0, 0, 1};
    constexpr ulw::Point threat{0, 18, 1};
    Check(ulw::CombatCoverInRange(fighter, threat) &&
        !ulw::CombatCoverInRange(fighter, {0, 3, 1}) &&
        !ulw::CombatCoverInRange(fighter, {0, 39, 1}) &&
        !ulw::CombatCoverInRange(fighter, {0, 18, 5}), "only nearby gunfights on similar elevations can divert to cover");
    Check(ulw::CombatCoverApproach(fighter, threat, {4, -1, 1}) &&
        ulw::CombatCoverApproach(fighter, threat, {3, 1, 1}), "lateral and retreating routes should qualify");
    Check(!ulw::CombatCoverApproach(fighter, threat, {2, 3, 1}) &&
        !ulw::CombatCoverApproach(fighter, threat, {0, -13, 1}) &&
        !ulw::CombatCoverApproach(fighter, threat, {0, -5, -2}) &&
        !ulw::CombatCoverApproach(fighter, threat, {0, -1, 1}),
        "cover routes must avoid advancing into fire, distant retreats, drops and points already occupied");

    ulw::CombatPressure pressure;
    constexpr std::array<std::uint32_t, 3> fighters{0x301, 0x401, 0x501};
    for (const auto ped : fighters) {
        pressure.RecordHit(ped, 1000, 1000);
        Check(!pressure.TakeCoverSearch(ped, 1000, 1), "the first hit should not start a cover search");
        pressure.RecordHit(ped, 1500, 1500);
    }
    Check(pressure.TakeCoverSearch(fighters[0], 1500, 2) && pressure.TakeCoverSearch(fighters[1], 1500, 2) &&
        !pressure.TakeCoverSearch(fighters[2], 1500, 2) && pressure.TakeCoverSearch(fighters[2], 1516, 3),
        "cover searches must fit the frame budget without penalizing deferred NPCs");
    pressure.RecordHit(fighters[0], 2000, 2000);
    Check(!pressure.TakeCoverSearch(fighters[0], 2000, 4), "additional hits must preserve the cover cooldown");
    pressure.RecordHit(fighters[0], 5800, 5800);
    pressure.RecordHit(fighters[0], 6200, 6200);
    Check(!pressure.TakeCoverSearch(fighters[0], 6299, 5) && pressure.TakeCoverSearch(fighters[0], 6300, 6),
        "a later pressure episode may trigger cover after the cooldown");
    constexpr std::uint32_t replacement = 0x302;
    pressure.RecordHit(replacement, 6500, 6500);
    pressure.RecordHit(replacement, 6700, 6700);
    Check(pressure.TakeCoverSearch(replacement, 6700, 7), "a reused pool slot must not inherit a cover cooldown");
    pressure.Reset();
    Check(!pressure.TakeCoverSearch(replacement, 6701, 8), "mission transitions must clear pending cover searches");
}

void TestVehicleDamageAndCooling() {
    const ulw::WheelForces native{0.2f, 0.3f, 0.2f};
    ulw::RoadPhysics road;
    ulw::RoadConditions vehicle{0x501, 1000, 1300, 430, 0, 0.03f, 0, false};
    vehicle.throttle = 1.0f;
    vehicle.engineDamage = 160;
    const auto cold = road.Tune(native, vehicle);
    for (vehicle.now += 50; vehicle.now <= 12000; vehicle.now += 50) {
        road.Tune(native, vehicle);
    }
    const auto hot = road.Tune(native, vehicle);
    Check(hot.thrust < cold.thrust * 0.9f, "a damaged engine should deliver less power under prolonged low-speed load");
    Check(road.Tune(native, vehicle).thrust == hot.thrust, "engine heat must advance only once at the same timestamp");
    auto repaired = vehicle;
    repaired.health = 1000.0f;
    repaired.engineDamage = 0;
    const auto restored = road.Tune(native, repaired);
    Check(restored.thrust == native.thrust, "repairing the car should reset its heat and power loss");

    road.Reset();
    vehicle.now = 1000;
    for (; vehicle.now <= 12000; vehicle.now += 50) {
        road.Tune(native, vehicle);
    }
    vehicle.throttle = 0;
    vehicle.speedSquared = 0.55f;
    for (; vehicle.now < 22000; vehicle.now += 50) {
        road.Tune(native, vehicle);
    }
    Check(road.Tune(native, vehicle).thrust == cold.thrust, "light throttle and airflow should recover heat-related performance");

    vehicle.health = 1000;
    vehicle.engineDamage = 0;
    vehicle.wet = 1;
    const auto wetCar = road.Tune(native, vehicle);
    vehicle.vehicle = 0x502;
    vehicle.motorcycle = true;
    const auto wetBike = road.Tune(native, vehicle);
    Check(wetBike.grip < wetCar.grip, "intact motorcycle tyres should be more sensitive to wet roads");
    vehicle.burstTyre = true;
    Check(road.Tune(native, vehicle).grip == native.grip, "a burst motorcycle tyre must retain native slide behavior");

    constexpr std::uint32_t brokenLeftWing = 4;
    constexpr std::uint32_t brokenRearBumper = 4u << 24;
    Check(ulw::PanelWearAtWheel(brokenLeftWing, 0) > 0 && ulw::PanelWearAtWheel(brokenLeftWing, 2) == 0,
        "left wing damage should affect only the nearby wheel");
    Check(ulw::PanelWearAtWheel(brokenRearBumper, 1) == ulw::PanelWearAtWheel(brokenRearBumper, 3) &&
        ulw::PanelWearAtWheel(brokenRearBumper, 3) > 0 && ulw::PanelWearAtWheel(brokenRearBumper, 0) == 0 &&
        ulw::PanelWearAtWheel(brokenRearBumper, 4) == 0, "rear bumper damage should only affect the rear axle");
    vehicle.vehicle = 0x503;
    vehicle.motorcycle = false;
    vehicle.burstTyre = false;
    vehicle.wet = 0;
    vehicle.panelWear = ulw::PanelWearAtWheel(brokenLeftWing, 0);
    const auto localDamage = road.Tune(native, vehicle);
    Check(localDamage.grip < native.grip && localDamage.brake < native.brake && localDamage.thrust == native.thrust,
        "regional body damage should limit local braking and grip without directly reducing engine power");
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
        TestPedestrianRoutines();
        TestTrafficAndPhysics();
        TestEmergencyServices();
        std::puts("Emergency services: closing and crossing traffic, working crews and arrival braking passed.");
        std::puts("Pedestrians: routine timing, witness memory, and crash-aware footpaths passed.");
        std::puts("Street systems: police search and reports, traffic limits, wet grip and brake fade passed.");
        TestVehicleDamageAndCooling();
        std::puts("Vehicles: engine load, cooling, repair, local damage and motorcycle grip passed.");
        TestAirSupportAndRoadblocks();
        std::puts("Police support: air and ground reports, search routes and roadblock placement passed.");
        TestMissionProtection();
        std::puts("Campaign: native and script mission signals, special profiles, cleanup, and reset passed.");
        TestCombat();
        std::puts("Combat: aiming profiles, burst limits, timed suppression and ped slot reuse passed.");
        TestCombatCover();
        std::puts("Combat cover: approach limits, search budgets, cooldown and slot reuse passed.");
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
