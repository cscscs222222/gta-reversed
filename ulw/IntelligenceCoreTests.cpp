#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "IntelligenceCore.h"
#include "CityLife.h"
#include "MissionState.h"
#include "PoliceInteractions.h"
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

void TestPatrolTeam() {
    using ulw::PatrolMode;
    using ulw::PatrolRole;
    using ulw::SearchPhase;
    constexpr auto lead = 0x80000201u;
    constexpr auto second = 0x80000301u;
    constexpr auto third = 0x80000401u;
    const ulw::Point street{0, 10, 1};
    const ulw::Point contact{0, 40, 1};
    ulw::PoliceSearch search;
    search.Observe(lead, 1000, street, {0, 35, 1}, true, PatrolMode::Car);
    search.Observe(lead, 1250, street, contact, true, PatrolMode::Car);
    const auto moving = search.Target(lead, 1300, street, PatrolMode::Car);
    Check(moving.phase == SearchPhase::Pursue && moving.motion.y == 20, "direct observations should record travel direction");
    search.Observe(lead, 1490, street, {90, 110, 1}, false, PatrolMode::Car);
    const auto hidden = search.Target(lead, 1500, street, PatrolMode::Car);
    Check(hidden.phase == SearchPhase::LastSeen && hidden.point.y == contact.y && hidden.motion.y == 0,
        "hidden motion must not feed interception decisions");
    search.Observe(lead, 1730, street, {100, 110, 1}, true, PatrolMode::Car);
    Check(search.Target(lead, 1740, street, PatrolMode::Car).motion.x == 0,
        "reacquisition must not treat unseen travel as observed motion");

    ulw::PatrolTeam team;
    Check(team.Assign(lead, 1330, 3, street, moving).role == PatrolRole::Pursuit,
        "the first cruiser should keep contact");
    const auto intercept = team.Assign(second, 1350, 3, {12, 10, 1}, moving);
    Check(intercept.role == PatrolRole::Intercept && intercept.destination.y > contact.y &&
        intercept.destination.y <= contact.y + 37 && intercept.destination.z == contact.z,
        "one extra unit may project a bounded route using observed movement");
    Check(team.Assign(third, 1370, 3, {-15, 10, 1}, moving).role == PatrolRole::Support,
        "the next cruiser must not copy the intercept route");
    Check(team.Assign(second, 1400, 3, {10, 10, 1}, hidden).role == PatrolRole::Pursuit &&
        team.Assign(third, 1440, 3, {-15, 10, 1}, moving).role == PatrolRole::Intercept,
        "loss of contact frees the interception role for a unit with a valid sighting");
    Check(team.Assign(second, 1470, 2, street, moving).role == PatrolRole::Pursuit, "low wanted levels need no intercept");
    Check(team.Assign(second, 1480, 3, {12, 10, 14}, moving).role == PatrolRole::Pursuit,
        "cruisers on another level must have independent roles");
    Check(team.Assign(third, 5000, 3, street, moving).role == PatrolRole::Pursuit, "old roles must expire");
    team.Reset();
    Check(team.Assign(second, 5020, 3, street, moving).role == PatrolRole::Pursuit, "reset must free team assignments");

    constexpr ulw::DrivePedals pedals{0.9f, 0.05f};
    const auto pursuit = ulw::PatrolFollowing(pedals, 1, {0, 8, 0}, {0, 1, 0}, {0, 0.27f, 0}, {0, 0.06f, 0}, 0);
    Check(pursuit.gas < pedals.gas && pursuit.brake > pedals.brake, "a closing patrol must brake for the gap");
    Check(ulw::PatrolFollowing(pedals, 6, {0, 8, 0}, {0, 1, 0}, {0, 0.27f, 0}, {0, 0.06f, 0}, 0).brake == pedals.brake &&
        ulw::PatrolFollowing(pedals, 2, {6, 8, 0}, {0, 1, 0}, {0, 0.27f, 0}, {0, 0.06f, 0}, 0).brake == pedals.brake,
        "army response and vehicles in another lane must keep native controls");
}

void TestPoliceInteractions() {
    using ulw::IncidentKind;
    constexpr ulw::Point corner{9, 10, 1};
    constexpr ulw::Point hood{13, 11, 1};
    ulw::PoliceIncidents incidents;
    Check(incidents.Record(IncidentKind::Witness, 0x101, corner, 1000, true) &&
        !incidents.Record(IncidentKind::Witness, 0x201, corner, 1050, true), "duplicate witness reports must coalesce");
    const auto witness = incidents.Nearby(hood, 1870, true);
    Check(!incidents.Nearby(hood, 1869, true) && witness && witness->kind == IncidentKind::Witness,
        "a civilian sighting should reach the police only after the reporting delay");
    Check(!incidents.Nearby({65, 65, 1}, 1900, true) && !incidents.Nearby({12, 10, 8}, 1900, true) &&
        !incidents.Nearby(hood, 7300, true), "reports must expire and stay local to their level");
    Check(incidents.Record(IncidentKind::GunshotSound, 0x201, corner, 7500, false) &&
        !incidents.Nearby(hood, 7501, true), "an unknown source must not replace a player sighting");
    const auto sound = incidents.Nearby(hood, 7501, false);
    Check(sound && sound->point.x == 7 && sound->point.y == 7, "heard shots supply coarse coordinates");
    Check(incidents.Record(IncidentKind::OfficerSight, 0x301, corner, 7700, true), "direct sighting must be recorded");
    const auto sight = incidents.Nearby(hood, 7701, false);
    Check(sight && sight->kind == IncidentKind::OfficerSight, "direct observation should win near a noisy event");
    for (unsigned i = 1; i <= 4; ++i) {
        Check(incidents.TakeProbe(i << 8, 8000, 10), "early observers should receive the frame budget");
    }
    Check(!incidents.TakeProbe(0x501, 8000, 10) && !incidents.TakeProbe(0x100, 8230, 11) &&
        incidents.TakeProbe(0x501, 8230, 11), "frame and actor sample limits should remain independent");
    incidents.Reset();
    Check(!incidents.Nearby(hood, 8200, false), "free-roam transitions must clear incident reports");

    Check(ulw::DrivingEvidence(0.2f * 0.2f, false) == 0 && ulw::DrivingEvidence(0.4f * 0.4f, false) == 1 &&
        ulw::DrivingEvidence(0.49f * 0.49f, false) == 2 && ulw::DrivingEvidence(0.3f * 0.3f, true) == 2,
        "driving warnings should distinguish clean, fast and collision behavior");
    ulw::DrivingSuspicion suspicion;
    constexpr auto cruiser = 0x601u;
    Check(!suspicion.Observe(cruiser, 1000, corner, 2) &&
        suspicion.Observe(cruiser, 1400, corner, 2) &&
        !suspicion.Follow(cruiser, 1401), "a warning should come before pursuit-like attention");
    Check(!suspicion.Observe(cruiser, 1800, hood, 2), "a patrol must respect the warning cooldown");
    const auto follow = suspicion.Follow(cruiser, 1801);
    Check(follow && follow->x == hood.x && !suspicion.Follow(0x602, 1801),
        "continued danger should store only the last observed location for the same patrol");
    Check(!suspicion.Observe(cruiser, 2200, corner, 0) && !suspicion.Follow(cruiser, 2201),
        "slowing down or losing sight should drop the brief follow decision");
    Check(!suspicion.Observe(cruiser, 2600, hood, 4) && !suspicion.Follow(cruiser, 5000),
        "warned patrols should respect the cooldown and forget stale locations");

    ulw::SurrenderInput surrender;
    Check(!surrender.Update(1000, true, false) && !surrender.Update(1100, true, true) &&
        !surrender.Update(1420, true, true) && !surrender.Update(1730, true, true) &&
        surrender.Update(1800, true, true), "holding surrender must require continuous eligible input");
    Check(surrender.Claim(cruiser, 1801) && !surrender.Claim(0x701, 1850),
        "only one pursuing officer should own the surrender approach");
    surrender.Commit();
    Check(!surrender.Update(1900, true, true) && !surrender.Claim(cruiser, 1900) &&
        !surrender.Update(2000, false, true), "committed or released input must not repeat the arrest");
    Check(!surrender.Update(2200, true, true) && !surrender.Update(3000, true, true),
        "a long interruption must restart the surrender timer");
}

void TestCityLife() {
    using ulw::CityVenue;
    using ulw::CrashReaction;
    using ulw::ResidentAction;
    constexpr std::uint16_t routineSeed = 0;
    Check(ulw::PreferredVenue(routineSeed, 8, 2, 1, 0) == CityVenue::BusStop &&
        ulw::PreferredVenue(routineSeed, 8, 2, 1, 0.7f) == CityVenue::Shelter,
        "weekday commuters should switch to a real shelter when raining");
    Check(ulw::PreferredVenue(routineSeed, 12, 3, 1, 0) == CityVenue::Food &&
        !ulw::PreferredVenue(routineSeed, 8, 7, 1, 0) && !ulw::PreferredVenue(routineSeed, 26, 3, 1, 0),
        "scheduled destinations should respect lunch, weekends and the valid clock");
    Check(ulw::IsCityVenue(4) && !ulw::IsCityVenue(5) && !ulw::IsCityVenue(7),
        "civilian visits should use world effects that do not activate scripted tasks");
    bool pauses{}, calls{};
    for (std::uint16_t seed = 0; seed < 256; ++seed) {
        const auto reaction = ulw::ReactToStreetCrash(seed, 100);
        pauses |= reaction == CrashReaction::Pause;
        calls |= reaction == CrashReaction::Call;
        Check(ulw::ReactToStreetCrash(seed, 4) == CrashReaction::Leave, "nearby wrecks should send witnesses back");
    }
    Check(pauses && calls, "some crash witnesses should wait or call when watching from a safe distance");

    ulw::CityLife city;
    constexpr ulw::Point pedestrian{0, 0, 1};
    constexpr ulw::VenueObservation stop{{6, 0, 1}, {7, 0, 1}, 1200, 1, CityVenue::BusStop};
    constexpr ulw::VenueObservation second{{9, 0, 1}, {10, 0, 1}, 1201, 1, CityVenue::BusStop};
    constexpr ulw::VenueObservation above{{4, 0, 8}, {5, 0, 8}, 1202, 1, CityVenue::BusStop};
    city.ObserveVenue(stop, 1000);
    city.ObserveVenue(second, 1000);
    city.ObserveVenue(above, 1000);
    const auto close = city.FindVenue(CityVenue::BusStop, pedestrian, 1, 1000);
    Check(close && close->model == stop.model && !city.FindVenue(CityVenue::Food, pedestrian, 1, 1000),
        "the route must use a live venue of the right kind and elevation");
    city.ReserveVenue(stop, 1050);
    city.ObserveVenue(stop, 1100);
    const auto available = city.FindVenue(CityVenue::BusStop, pedestrian, 1, 1150);
    Check(available && available->model == second.model && !city.FindVenue(CityVenue::BusStop, pedestrian, 2, 1150),
        "refreshing an observed venue should preserve its reservation and district");
    Check(city.FindVenue(CityVenue::BusStop, pedestrian, 1, 11100).has_value() &&
        !city.FindVenue(CityVenue::BusStop, pedestrian, 1, 31100), "reserved venues open later and unloaded venues expire");

    constexpr std::uint32_t walker = 0x101;
    Check(city.TakeVisitAttempt(walker, 1300, 20) && city.TakeVisitAttempt(0x301, 1300, 20) &&
        !city.TakeVisitAttempt(0x401, 1300, 20) && city.TakeVisitAttempt(0x401, 1310, 21) &&
        !city.TakeVisitAttempt(walker, 1310, 21), "no more than two visitors should scan in a frame");
    city.Start(walker, {200, 300, 1450, ResidentAction::Visit, stop.point});
    Check(city.Current(walker, 200, 300).has_value() && !city.Current(0x102, 200, 300) &&
        !city.Current(walker, 200, 400), "native action holders must match the resident generation and both tasks");
    city.Finish(0x102);
    Check(city.Current(walker, 200, 300).has_value(), "other generations cannot finish a visit");
    city.Finish(walker);
    Check(!city.Current(walker, 200, 300), "completion must free the resident's native task");

    constexpr ulw::CrashSite crash{{0, 2, 0}, 0x501, 1500};
    constexpr ulw::CrashSite later{{1, 2, 0}, 0x501, 12700};
    Check(city.CanCall(crash, 1600), "the first witness should be able to call for help");
    city.StartCall(crash, 1600);
    Check(!city.CanCall(crash, 16000) && !city.CanCall(later, 13599) && city.CanCall(later, 13600),
        "reporting must deduplicate the same crash and pace calls for different ones");
    city.Reset();
    Check(!city.FindVenue(CityVenue::BusStop, pedestrian, 1, 1000) && city.CanCall(crash, 1600),
        "mission or session changes must clear world activity memory");
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
    ulw::TrafficInput input{50, 40, 0, 0, 12, false};
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
    ulw::TrafficInput input{40, 30, 0, 0, 12, false};
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

void TestTrafficSpacing() {
    using ulw::DriverProfile;
    Check(ulw::TrafficDriver(12, 420, 1500) == DriverProfile::Taxi &&
        ulw::TrafficDriver(12, 414, 4600) == DriverProfile::Truck &&
        ulw::TrafficDriver(12, 411, 1100) == DriverProfile::Sport,
        "commercial, heavy and sports cars should use dedicated driver profiles");
    constexpr ulw::Point north{0, 1, 0};
    const auto turn = ulw::TrafficBendScale(north, {1, 0, 0}, 9, false, 0, DriverProfile::Average);
    Check(turn < 0.8f && ulw::TrafficBendScale(north, {1, 0, 0}, 9, false, 1, DriverProfile::Truck) < turn &&
        ulw::TrafficBendScale(north, {1, 0, 0}, 33, false, 1, DriverProfile::Truck) == 1.0f &&
        ulw::TrafficBendScale(north, north, 8, true, 0, DriverProfile::Taxi) < 1.0f,
        "close bends and signals should lower speed before arrival");

    ulw::FollowingTraffic passage{22, {0, 0, 0}, {0, 14, 0}, north, north,
        {0, 0.25f, 0}, {0, 0.10f, 0}, DriverProfile::Average, false, 0};
    const auto dry = ulw::FollowingSpeed(passage);
    Check(dry > 0 && dry < passage.nativeLimit, "a closing driver must ease off before reaching a queue");
    passage.wet = 1;
    passage.heavyLeader = true;
    Check(ulw::FollowingSpeed(passage) < dry, "wet roads and heavy leaders require a larger following gap");
    passage.leader = {4, 14, 0};
    Check(ulw::FollowingSpeed(passage) == passage.nativeLimit, "a neighboring lane must not stop the driver");
    passage.leader = {0, 14, 7};
    Check(ulw::FollowingSpeed(passage) == passage.nativeLimit, "bridge traffic must not slow vehicles underneath");
    passage.leader = {0, 14, 0};
    passage.leaderForward = {0, -1, 0};
    Check(ulw::FollowingSpeed(passage) == passage.nativeLimit, "head-on traffic must use native collision avoidance");
    passage.nativeLimit = 0;
    passage.leaderForward = north;
    Check(ulw::FollowingSpeed(passage) == 0, "car following must preserve a native stop or red light");

    ulw::TrafficHorns horns;
    Check(horns.Allow(0x501, 1000, DriverProfile::Careful) && !horns.Allow(0x501, 4100, DriverProfile::Careful) &&
        horns.Allow(0x501, 7200, DriverProfile::Careful), "careful drivers must wait longer to honk again in a queue");
    Check(horns.Allow(0x502, 7210, DriverProfile::Aggressive) && !horns.Allow(0x502, 10209, DriverProfile::Aggressive) &&
        horns.Allow(0x502, 10210, DriverProfile::Aggressive), "a new driver may honk and then has its own cooldown");
    horns.Reset();
    Check(horns.Allow(0x502, 10211, DriverProfile::Aggressive), "reset must discard queued horn state");
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

void TestCombatManeuvers() {
    constexpr unsigned Ballas = 7;
    constexpr unsigned Grove = 8;
    constexpr unsigned Vagos = 9;
    const auto ballas = ulw::CombatCoverHold(Ballas, 10, 50, 0);
    const auto grove = ulw::CombatCoverHold(Grove, 10, 50, 0);
    const auto vagos = ulw::CombatCoverHold(Vagos, 10, 50, 0);
    Check(ballas < grove && grove < vagos, "gangs should return fire at different tempos");
    Check(ulw::CombatCoverHold(Ballas, 0, 30, 1400) == 1400 &&
        ulw::CombatCoverHold(Ballas, 2, 30, 0) > ballas &&
        ulw::CombatCoverHold(Ballas, 0, 30, 6000) == 1900 &&
        ulw::CombatCoverHold(Ballas, 0, 0, 6000) == ballas,
        "available reloads must extend time behind cover within a fixed limit");
    Check(ulw::CombatShouldWithdraw(Vagos, 24, 13) && !ulw::CombatShouldWithdraw(Ballas, 24, 13) &&
        ulw::CombatShouldWithdraw(Grove, 75, 0) && !ulw::CombatShouldWithdraw(4, 15, 0) &&
        !ulw::CombatShouldWithdraw(Grove, 0, 0), "withdrawal should reflect faction, health and ammunition");

    ulw::CombatManeuvers memory;
    using ulw::CombatMove;
    constexpr std::uint32_t fighter = 0x301;
    constexpr std::uint32_t ally = 0x401;
    constexpr std::uint32_t replacement = 0x302;
    constexpr ulw::Point destination{9, 0, 1};
    memory.Start(fighter, {0x1000, 0x1100, 0x1200, destination, 1000, Grove, CombatMove::Approach});
    Check(memory.Current(fighter, 0x1000, 0x1100)->phase == CombatMove::Approach &&
        !memory.Current(fighter, 0x1000, 0x1110) && !memory.Current(fighter, 0x1020, 0x1100) &&
        !memory.Current(replacement, 0x1000, 0x1100), "cover transitions must match ped generation and both native tasks");
    Check(memory.AllyMoving(ally, Grove, {0, 0, 1}, 3000) &&
        !memory.AllyMoving(ally, Grove, {0, 0, 1}, 3100) &&
        !memory.AllyMoving(ally, Grove, {30, 0, 1}, 1000) &&
        !memory.AllyMoving(ally, Ballas, {0, 0, 1}, 1000) &&
        !memory.AllyMoving(replacement, Grove, {0, 0, 1}, 1000),
        "only a nearby, recent movement by another member of the same faction should stagger movement");
    memory.Start(fighter, {0x1000, 0x1300, 0x1200, destination, 2500, Grove, CombatMove::Hold});
    Check(!memory.Current(fighter, 0x1000, 0x1100) &&
        memory.Current(fighter, 0x1000, 0x1300)->cover == 0x1200, "the hold replaces the approach and retains its reservation");
    memory.Clear(replacement);
    Check(memory.Current(fighter, 0x1000, 0x1300).has_value(), "a stale clear must not delete another generation");
    Check(!memory.TakeInterrupted(replacement, 0x1000, 0x1400) &&
        !memory.TakeInterrupted(fighter, 0x1000, 0x1300), "unrelated and current tasks must keep their reservations");
    const auto abandoned = memory.TakeInterrupted(fighter, 0x1000, 0x1400);
    Check(abandoned && abandoned->cover == 0x1200 && !memory.AllyMoving(ally, Grove, destination, 2600),
        "a new subtask must return the interrupted reservation and free its local movement turn");
    memory.Start(fighter, {0x1000, 0x1400, 0, destination, 2600, Grove, CombatMove::Peek});
    memory.Reset();
    Check(!memory.Current(fighter, 0x1000, 0x1400), "mission transitions must discard tracked actions");

    ulw::GangLosses losses;
    Check(!losses.Record(0x201, 6, destination, 1000), "police must not follow gang morale");
    Check(losses.Record(fighter, Grove, destination, 1000) &&
        !losses.Record(fighter, Grove, destination, 1005) &&
        !losses.UnderStrength(Grove, destination, 1006), "duplicate death events must count once");
    Check(losses.Record(ally, Grove, {11, 0, 1}, 1900) && losses.UnderStrength(Grove, destination, 2000) &&
        !losses.UnderStrength(Grove, {9, 0, 5}, 2000) && !losses.UnderStrength(Grove, {35, 0, 1}, 2000) &&
        !losses.UnderStrength(Vagos, destination, 2000), "only recent casualties near the same faction should count");
    Check(losses.UnderStrength(Grove, destination, 10999) && !losses.UnderStrength(Grove, destination, 11000),
        "losing a nearby fight must have a bounded effect on morale");
    Check(losses.Record(0x601, Ballas, destination, 11000) && losses.Record(0x701, Ballas, destination, 11300) &&
        !losses.UnderStrength(Ballas, destination, 11300) && losses.Record(0x801, Ballas, destination, 11400) &&
        losses.UnderStrength(Ballas, destination, 11400), "Ballas should tolerate more losses before falling back");
    losses.Reset();
    Check(!losses.UnderStrength(Ballas, destination, 11500), "missions must clear gang casualty reports");
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

void TestVehicleDynamics() {
    using ulw::DrivenAxle;
    using ulw::RoadMaterial;
    using ulw::RoadVehicle;
    const auto paved = ulw::ClassifySurface(2, 0);
    const auto grass = ulw::ClassifySurface(3 | (2u << 23), 2);
    const auto sand = ulw::ClassifySurface(4, 0);
    const auto mud = ulw::ClassifySurface(3, 8);
    Check(paved.material == RoadMaterial::Paved && grass.material == RoadMaterial::Grass && grass.roughness == 2 &&
        sand.material == RoadMaterial::Sand && mud.material == RoadMaterial::Mud &&
        ulw::ClassifySurface(3, 4).material == RoadMaterial::Gravel &&
        ulw::ClassifySurface(3 | 0x10000u, 2).material == RoadMaterial::Wet,
        "native material and wheel flags should classify each contact");
    Check(ulw::ClassifyRoadVehicle(411, 1400, false, false) == RoadVehicle::Sport &&
        ulw::ClassifyRoadVehicle(470, 1550, false, false) == RoadVehicle::Utility &&
        ulw::ClassifyRoadVehicle(431, 3000, false, false) == RoadVehicle::Heavy &&
        ulw::ClassifyRoadVehicle(420, 1100, false, false) == RoadVehicle::Compact &&
        ulw::ClassifyRoadVehicle(521, 520, false, true) == RoadVehicle::Motorcycle,
        "handling classes should distinguish sports cars, heavy vehicles and motorbikes");
    Check(ulw::WheelDriven(DrivenAxle::Front, 0) && !ulw::WheelDriven(DrivenAxle::Front, 1) &&
        ulw::WheelDriven(DrivenAxle::Rear, 5) && !ulw::WheelDriven(DrivenAxle::Both, 6),
        "only valid wheels on a driven axle should receive engine braking");

    constexpr ulw::WheelForces base{0.15f, 0.20f, 0.25f};
    ulw::WheelContact road{paved, RoadVehicle::Standard, DrivenAxle::Rear, 0, 0.55f, 0.36f, 0.09f, false};
    const auto dry = ulw::DynamicWheelResponse(base, road, 0, 0.7f, 0.0f, 0.48f, false);
    road.surface = grass;
    const auto wet = ulw::DynamicWheelResponse(base, road, 1, 0.7f, 0.0f, 0.48f, false);
    Check(wet.grip < dry.grip && wet.thrust < dry.thrust, "wet grass should offer less grip and propulsion than paving");
    road.surface = sand;
    road.vehicle = RoadVehicle::Heavy;
    const auto truck = ulw::DynamicWheelResponse(base, road, 0, 0.7f, 0.0f, 0.48f, false);
    road.vehicle = RoadVehicle::Utility;
    Check(ulw::DynamicWheelResponse(base, road, 0, 0.7f, 0.0f, 0.48f, false).thrust > truck.thrust,
        "off-road handling should lose less propulsion on sand");
    road.surface = paved;
    const auto front = ulw::DynamicWheelResponse(base, road, 0, 0, 0.8f, 0.48f, false);
    road.wheel = 1;
    const auto rear = ulw::DynamicWheelResponse(base, road, 0, 0, 0.8f, 0.48f, false);
    Check(front.grip > rear.grip && front.brake > rear.brake, "braking should transfer load toward the front axle");
    constexpr ulw::WheelForces coasting{0.0f, 0.0f, 0.25f};
    Check(ulw::DynamicWheelResponse(coasting, road, 0, 0, 0, 0.48f, false).brake > 0,
        "driven wheels should supply a small closed-throttle braking effect");
    road.handbrake = true;
    Check(ulw::DynamicWheelResponse(coasting, road, 0, 0, 0, 0.48f, false).brake == 0 &&
        ulw::DynamicWheelResponse(base, road, 1, 0.7f, 0, 1, true).grip == base.grip,
        "handbrake response and burst tyre grip must retain native handling");

    ulw::TyreTemperature sliding, rolling;
    for (std::uint32_t tick = 500; tick <= 5000; tick += 100) {
        ulw::UpdateTyreTemperature(sliding, tick, road, 0, true);
        ulw::UpdateTyreTemperature(rolling, tick, road, 1, false);
    }
    Check(sliding.value > rolling.value && ulw::UpdateTyreTemperature(sliding, 5000, road, 1, false) == sliding.value,
        "sliding must build heat while several wheel callbacks at one time must not advance it");
    Check(ulw::UpdateTyreTemperature(sliding, 40000, road, 1, false) == 0.48f, "idle tyres should return to baseline");

    constexpr ulw::SuspensionSpring spring{1.0f, 0.55f};
    const auto relaxed = ulw::TuneSpring(spring, RoadVehicle::Sport, 0.7f, 0, 0, 0, true);
    const auto compressed = ulw::TuneSpring(spring, RoadVehicle::Sport, 0.1f, 0, 0.9f, 0, true);
    Check(relaxed.force == spring.force && compressed.force > spring.force && compressed.bias > spring.bias &&
        ulw::TuneSpring(spring, RoadVehicle::Sport, 0.1f, 0, 0.9f, 0, false).bias < spring.bias,
        "progressive springs should resist bottoming and distribute load under braking");
    Check(ulw::TuneDamping(0.2f, RoadVehicle::Sport, paved, 0.12f) >
        ulw::TuneDamping(0.2f, RoadVehicle::Sport, grass, 0.12f) &&
        ulw::TuneDamping(0.2f, RoadVehicle::Utility, grass, -0.12f) > 0.2f,
        "rough impacts and rebound should receive different damping");

    constexpr std::array<ulw::GearBand, 6> stock{{
        {-0.15f, -0.01f, -0.15f}, {0.36f, 0.24f, 0.0f}, {0.60f, 0.52f, 0.15f},
        {0.81f, 0.74f, 0.46f}, {1.10f, 1.10f, 0.68f}, {}
    }};
    auto sport = stock;
    auto heavy = stock;
    Check(ulw::TuneShiftBands(sport, 4, RoadVehicle::Sport, 1.0f, 0.17f) &&
        ulw::TuneShiftBands(heavy, 4, RoadVehicle::Heavy, 0.15f, 0),
        "valid gear bands should be adjustable for vehicle load");
    Check(sport[1].up > stock[1].up && heavy[1].up < stock[1].up && sport[4].up == stock[4].up &&
        sport[0].down == stock[0].down && sport[2].down < sport[1].up,
        "earlier and later upshifts must retain reverse, top speed and separation between shift points");
    auto damaged = stock;
    damaged[2].down = 0.7f;
    Check(!ulw::TuneShiftBands(damaged, 4, RoadVehicle::Sport, 1, 0) && damaged[1].up == stock[1].up,
        "malformed gear thresholds must remain intact");
    Check(ulw::ShapeThrottle(0.75f, RoadVehicle::Heavy, 0.20f) <
        ulw::ShapeThrottle(0.75f, RoadVehicle::Sport, 0) && ulw::ShapeThrottle(-0.7f, RoadVehicle::Heavy, 0.2f) == -0.7f,
        "inclines and class should shape positive gas while reverse stays native");
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
        TestPatrolTeam();
        std::puts("Patrols: observed motion, interception roles, sight loss, role expiry and following gaps passed.");
        TestPoliceInteractions();
        std::puts("Police interactions: delayed witnesses, evidence range, warnings and voluntary arrest input passed.");
        TestCrashScenes();
        TestPedestrianRoutines();
        TestTrafficAndPhysics();
        TestTrafficSpacing();
        std::puts("Traffic: driver profiles, bends, headway, level separation and native stops passed.");
        TestEmergencyServices();
        std::puts("Emergency services: closing and crossing traffic, working crews and arrival braking passed.");
        std::puts("Pedestrians: routine timing, witness memory, and crash-aware footpaths passed.");
        std::puts("Street systems: police search and reports, traffic limits, wet grip and brake fade passed.");
        TestVehicleDamageAndCooling();
        std::puts("Vehicles: engine load, cooling, repair, local damage and motorcycle grip passed.");
        TestVehicleDynamics();
        std::puts("Vehicle dynamics: surface grip, tyre heat, weight transfer, damping and bounded shift points passed.");
        TestAirSupportAndRoadblocks();
        std::puts("Police support: air and ground reports, search routes and roadblock placement passed.");
        TestMissionProtection();
        std::puts("Campaign: native and script mission signals, special profiles, cleanup, and reset passed.");
        TestCombat();
        std::puts("Combat: aiming profiles, burst limits, timed suppression and ped slot reuse passed.");
        TestCombatCover();
        std::puts("Combat cover: approach limits, search budgets, cooldown and slot reuse passed.");
        TestCombatManeuvers();
        std::puts("Combat maneuvers: faction tempo, ammunition, withdrawal, ally timing and gang losses passed.");
        TestCityActivity();
        std::puts("City: rush hour, regional nightlife and weather schedules passed.");
        TestCityLife();
        std::puts("City life: destination schedules, venue reservations, resident tasks and crash witness calls passed.");
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
