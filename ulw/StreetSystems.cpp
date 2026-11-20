#include "StreetSystems.h"

#include <algorithm>
#include <cmath>

namespace ulw {
namespace {

float Unit(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

bool IsNight(unsigned hour) noexcept {
    return hour >= 21 || hour < 6;
}

std::uint32_t PatrolSlot(std::uint32_t id) noexcept {
    // Keep pedestrian, patrol-car and helicopter references in different
    // parts of the cache even when their native pool indices coincide.
    return ((id & 0x3fffffffu) >> 8) + ((id >> 31) * 47u) + (((id >> 30) & 1u) * 91u);
}

}

float DistanceSquared(Point a, Point b) noexcept {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

float SightRange(float wet, float fog, unsigned hour) noexcept {
    return 62.0f * (IsNight(hour) ? 0.76f : 1.0f) * (1.0f - Unit(wet) * 0.10f - Unit(fog) * 0.32f);
}

bool PoliceSearch::ShouldSample(std::uint32_t officer, std::uint32_t now, std::uint32_t frame) noexcept {
    if (m_Frame != frame) {
        m_Frame = frame;
        m_Casts = 0;
    }
    if (m_Casts >= 5) {
        return false;
    }
    const auto& record = m_Officers[PatrolSlot(officer) % m_Officers.size()];
    if (record.sampled && record.ref == officer && now - record.sampledAt < 210) {
        return false;
    }
    ++m_Casts;
    return true;
}

void PoliceSearch::Observe(std::uint32_t officer, std::uint32_t now,
    Point officerPosition, Point point, bool visible, PatrolMode mode) noexcept {
    auto& record = m_Officers[PatrolSlot(officer) % m_Officers.size()];
    if (!record.sampled || record.ref != officer) {
        record = {};
        record.ref = officer;
    }
    record.sampled = true;
    record.sampledAt = now;
    record.visible = visible;
    if (visible) {
        record.lastSeen = point;
        record.seenAt = now;
        record.hasSeen = true;
        const auto reportOrigin = mode == PatrolMode::Air ? point : officerPosition;
        m_Reports[PatrolSlot(officer) % m_Reports.size()] = {reportOrigin, point, now, officer, true};
    }
}

SearchTarget PoliceSearch::Target(std::uint32_t officer, std::uint32_t now,
    Point officerPosition, PatrolMode mode) const noexcept {
    const bool airborne = mode == PatrolMode::Air;
    const bool driving = mode == PatrolMode::Car;
    const auto& record = m_Officers[PatrolSlot(officer) % m_Officers.size()];
    const bool ownRecord = record.sampled && record.ref == officer;
    const bool hasLocalSighting = ownRecord && record.hasSeen && now - record.seenAt < 6500;
    if (ownRecord && record.visible && now - record.sampledAt < 420) {
        return {SearchPhase::Pursue, record.lastSeen};
    }
    const Report* nearby{};
    auto newestAge = hasLocalSighting ? std::min(3800u, now - record.seenAt) : 3800u;
    for (const auto& report : m_Reports) {
        const auto age = now - report.at;
        const auto radioRange = airborne ? 155.0f : driving ? 65.0f : 38.0f;
        if (report.occupied && (report.officer >> 8) != (officer >> 8) &&
            age < newestAge && DistanceSquared(officerPosition, report.reporter) <= radioRange * radioRange &&
            std::abs(officerPosition.z - report.reporter.z) <= (airborne ? 125.0f : 8.0f)) {
            nearby = &report;
            newestAge = age;
        }
    }
    if (!nearby && !hasLocalSighting) {
        return {ownRecord ? SearchPhase::Lost : SearchPhase::Observe, officerPosition};
    }
    // Each responding officer approaches the last confirmed location then
    // sweeps a different part of the immediate area.
    auto destination = nearby ? nearby->lastSeen : record.lastSeen;
    const auto age = nearby ? newestAge : now - record.seenAt;
    const auto arrivalRadius = airborne ? 32.0f : driving ? 18.0f : 8.0f;
    const auto altitude = officerPosition.z - destination.z;
    const auto rangeSquared = DistanceSquared(officerPosition, destination) - (airborne ? altitude * altitude : 0.0f);
    if (age < 2300 || rangeSquared > arrivalRadius * arrivalRadius) {
        return {nearby ? SearchPhase::Respond : SearchPhase::LastSeen, destination};
    }
    const auto section = (PatrolSlot(officer) + age / 1400) % 4;
    constexpr std::array<Point, 4> sweep{{{1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, -1, 0}}};
    const float radius = airborne ? 34.0f : driving ? 20.0f : 3.0f;
    destination.x += radius * sweep[section].x;
    destination.y += radius * sweep[section].y;
    return {SearchPhase::Search, destination};
}

bool PoliceSearch::HasRecentReport(Point position, std::uint32_t now) const noexcept {
    for (const auto& report : m_Reports) {
        if (report.occupied && now - report.at < 4200 && std::abs(report.lastSeen.z - position.z) <= 12.0f &&
            DistanceSquared(position, report.lastSeen) <= 105.0f * 105.0f) {
            return true;
        }
    }
    return false;
}

void PoliceSearch::Reset() noexcept {
    m_Officers = {};
    m_Reports = {};
    m_Frame = 0;
    m_Casts = 0;
}

bool HasSearchContact(SearchPhase phase) noexcept {
    return phase != SearchPhase::Observe && phase != SearchPhase::Lost;
}

Point AirSearchPoint(SearchTarget target, Point helicopter, Point forward, float flightReferenceZ) noexcept {
    auto point = target.point;
    const auto dx = point.x - helicopter.x;
    const auto dy = point.y - helicopter.y;
    if (!HasSearchContact(target.phase) || !std::isfinite(dx) || !std::isfinite(dy) || dx * dx + dy * dy < 64.0f) {
        point = {helicopter.x + forward.x * 28.0f, helicopter.y + forward.y * 28.0f, flightReferenceZ};
    }
    return point;
}

bool SuitableRoadblock(const RoadblockCandidate& site) noexcept {
    if (site.wanted < 3 || !site.activeRoad || site.links < 2 || !std::isfinite(site.width) ||
        !std::isfinite(site.speedSquared) || (site.sightingRequired && !site.recentlySeen)) {
        return false;
    }
    const auto minWidth = site.highway || (site.links >= 3 && site.wanted >= 4) ? 10.0f : 15.0f;
    const auto minDistance = 45.0f + Unit(site.speedSquared / 0.8f) * 23.0f;
    const auto distance = DistanceSquared(site.road, site.player);
    return site.width >= minWidth && std::abs(site.road.z - site.player.z) <= 7.5f &&
        distance >= minDistance * minDistance && distance <= 92.0f * 92.0f;
}

bool InCameraCone(Point camera, Point forward, Point target) noexcept {
    const auto dx = target.x - camera.x;
    const auto dy = target.y - camera.y;
    const auto dz = target.z - camera.z;
    const auto dot = forward.x * dx + forward.y * dy + forward.z * dz;
    return dot > 0.0f && dot * dot >= 0.36f * (dx * dx + dy * dy + dz * dz);
}

bool RoadblockAttempts::Ready(std::uint32_t now, unsigned wanted) const noexcept {
    return !m_HasAttempt || now - m_Last >= (wanted >= 5 ? 11000u : 16000u);
}

void RoadblockAttempts::Record(std::uint32_t now) noexcept {
    m_Last = now;
    m_HasAttempt = true;
}

void RoadblockAttempts::Reset() noexcept {
    m_Last = 0;
    m_HasAttempt = false;
}

float TrafficLimit(const TrafficInput& input) noexcept {
    if (!std::isfinite(input.nativeLimit) || !std::isfinite(input.cruiseSpeed) ||
        input.nativeLimit <= 0.0f || input.cruiseSpeed <= 0.0f) {
        return input.nativeLimit;
    }
    constexpr std::array<float, 4> temperament{0.86f, 0.94f, 0.98f, 1.0f};
    auto cap = input.cruiseSpeed * temperament[(input.seed * 0x9e3779b9u >> 26) % temperament.size()];
    cap *= 1.0f - 0.17f * Unit(input.wet) - 0.18f * Unit(input.fog);
    if (IsNight(input.hour)) {
        cap *= 0.96f;
    }
    if (input.recentCollision) {
        cap *= 0.68f;
    }
    if (input.sirenApproaching) {
        cap *= 0.74f;
    }
    if (input.crashAhead) {
        cap *= 0.58f;
    }
    return std::min(input.nativeLimit, cap);
}

void CrashScenes::Observe(const CrashObservation& crash) noexcept {
    auto& scene = m_Scenes[(crash.vehicle >> 8) % m_Scenes.size()];
    const bool sameVehicle = scene.occupied && (scene.vehicle >> 8) == (crash.vehicle >> 8);
    if (sameVehicle && (scene.vehicle != crash.vehicle || crash.speedSquared > 0.12f * 0.12f ||
        DistanceSquared(scene.position, crash.position) > 16.0f)) {
        scene = {};
    }
    if (crash.impactAt && crash.now - crash.impactAt < 900 && crash.impact >= 28.0f &&
        crash.speedSquared >= 0.0f && crash.speedSquared <= 0.08f * 0.08f) {
        scene = {crash.vehicle, crash.impactAt, crash.position, true};
    }
}

bool CrashScenes::Ahead(Point position, Point forward, std::uint32_t now) const noexcept {
    for (const auto& scene : m_Scenes) {
        if (!scene.occupied || now - scene.impactAt >= 8000 || std::abs(scene.position.z - position.z) > 2.8f) {
            continue;
        }
        const auto dx = scene.position.x - position.x;
        const auto dy = scene.position.y - position.y;
        const auto ahead = dx * forward.x + dy * forward.y;
        const auto lateral = dx * forward.y - dy * forward.x;
        if (ahead > 2.0f && ahead < 30.0f && std::abs(lateral) < 5.0f) {
            return true;
        }
    }
    return false;
}

void CrashScenes::Reset() noexcept {
    m_Scenes = {};
}

CityActivity AmbientActivity(unsigned hour, unsigned weekday, unsigned region, float wet, float fog) noexcept {
    if (hour >= 24 || weekday < 1 || weekday > 7 || region > 4) {
        return {1.0f, 1.0f};
    }
    const bool weekend = weekday >= 6;
    const bool rushHour = !weekend && ((hour >= 7 && hour < 10) || (hour >= 16 && hour < 19));
    const bool sanFierro = region == 2;
    const bool lasVenturas = region == 3;
    const bool desert = region == 4;
    const bool night = hour >= 22 || hour < 6;
    const bool nightlife = lasVenturas && (hour >= 19 || hour < 3);
    float pedestrians = night && !nightlife ? 0.62f : 1.0f;
    float cars = night && !nightlife ? 0.73f : rushHour ? 1.0f : 0.9f;
    if (desert) {
        pedestrians *= 0.78f;
        cars *= rushHour ? 0.94f : 0.88f;
    }
    if (sanFierro && weekend && hour < 10) {
        pedestrians *= 0.88f;
    }
    pedestrians *= 1.0f - 0.29f * Unit(wet);
    cars *= 1.0f - 0.12f * Unit(fog);
    return {pedestrians, cars};
}

unsigned AttractorUsePercent(unsigned type, unsigned hour, unsigned weekday, unsigned region, float rain) noexcept {
    if (hour >= 24 || weekday < 1 || weekday > 7 || region > 4) {
        return 100;
    }
    const bool weekend = weekday >= 6;
    const bool raining = Unit(rain) >= 0.15f;
    switch (type) {
    case 1: // Seats
    case 8: // Parks
    case 9: // Steps
        return raining ? 35 : hour >= 11 && hour < 18 ? 100 : 65;
    case 2: // Stops
        return !weekend && ((hour >= 7 && hour < 10) || (hour >= 16 && hour < 19)) ? 100 : 60;
    case 3: // Food
        return (hour >= 11 && hour < 15) || (hour >= 17 && hour < 22) ||
            (region == 3 && hour < 3) ? 100 : 55;
    case 4: // Shelters
        return raining ? 100 : 10;
    default:
        return 100;
    }
}

std::uint8_t CombatAccuracy(std::uint8_t native, unsigned faction, float health, float distanceSquared,
    float wet, float fog, unsigned hour) noexcept {
    if (native == 0 || native > 100 || faction < 6 || faction > 16 || !(health > 0.0f) ||
        !std::isfinite(distanceSquared) || distanceSquared < 0.0f) {
        return native;
    }
    // Distinct aiming skill for police and the ten gang ped types.
    constexpr std::array<float, 11> training{18, 15, 13, 11, 10, 14, 20, 17, 12, 18, 12};
    const auto distant = Unit((std::sqrt(distanceSquared) - 8.0f) / 37.0f);
    const auto injured = Unit((65.0f - health) / 55.0f);
    const auto weather = Unit(wet) * 6.0f + Unit(fog) * 17.0f + (IsNight(hour) ? 5.0f : 0.0f);
    const auto adjustment = training[faction - 6] * (1.0f - 0.45f * distant) - injured * 18.0f - distant * weather;
    return static_cast<std::uint8_t>(std::clamp(static_cast<float>(native) + adjustment, 1.0f, 100.0f));
}

WheelForces RoadPhysics::Tune(WheelForces forces, const RoadConditions& conditions) noexcept {
    if (!(conditions.mass > 0.0f) || conditions.speedSquared < 0.0f ||
        !std::isfinite(conditions.speedSquared) || !std::isfinite(conditions.health)) {
        return forces;
    }
    auto& brakes = m_Brakes[(conditions.vehicle >> 8) % m_Brakes.size()];
    if (!brakes.occupied || brakes.vehicle != conditions.vehicle || conditions.now - brakes.at > 3000) {
        brakes = {conditions.vehicle, conditions.now, 0.0f, true};
    }
    const auto interval = std::min(conditions.now - brakes.at, 100u);
    const auto severity = Unit(conditions.brakePedal) * Unit(conditions.speedSquared / 0.45f);
    const auto heavy = Unit((conditions.mass - 1350.0f) / 3000.0f);
    const auto wear = Unit((650.0f - conditions.health) / 400.0f);
    brakes.heat = Unit(brakes.heat + static_cast<float>(interval) *
        (severity * (0.00026f + heavy * 0.00020f) - 0.000075f));
    brakes.at = conditions.now;
    const auto wet = Unit(conditions.wet);
    if (!conditions.burstTyre) {
        forces.grip *= 1.0f - wet * (0.11f + 0.035f * heavy) - 0.065f * wear;
    }
    forces.brake *= 1.0f - brakes.heat * (0.075f + 0.055f * heavy) - 0.07f * wear;
    forces.thrust *= 1.0f - 0.20f * wear;
    return forces;
}

void RoadPhysics::Reset() noexcept {
    m_Brakes = {};
}

}
