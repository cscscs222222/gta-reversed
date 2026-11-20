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
    const auto& record = m_Officers[(officer >> 8) % m_Officers.size()];
    if (record.sampled && record.ref == officer && now - record.sampledAt < 210) {
        return false;
    }
    ++m_Casts;
    return true;
}

void PoliceSearch::Observe(std::uint32_t officer, std::uint32_t now,
    Point officerPosition, Point point, bool visible) noexcept {
    auto& record = m_Officers[(officer >> 8) % m_Officers.size()];
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
        m_Reports[(officer >> 8) % m_Reports.size()] = {officerPosition, point, now, officer, true};
    }
}

SearchTarget PoliceSearch::Target(std::uint32_t officer, std::uint32_t now, Point officerPosition) const noexcept {
    const auto& record = m_Officers[(officer >> 8) % m_Officers.size()];
    if (!record.sampled || record.ref != officer) {
        return {SearchPhase::Observe, officerPosition};
    }
    const bool hasLocalSighting = record.hasSeen && now - record.seenAt < 6500;
    if (record.visible && now - record.sampledAt < 420) {
        return {SearchPhase::Pursue, record.lastSeen};
    }
    const Report* nearby{};
    auto newestAge = hasLocalSighting ? std::min(3800u, now - record.seenAt) : 3800u;
    for (const auto& report : m_Reports) {
        const auto age = now - report.at;
        if (report.occupied && (report.officer >> 8) != (officer >> 8) &&
            age < newestAge && DistanceSquared(officerPosition, report.reporter) <= 38.0f * 38.0f &&
            std::abs(officerPosition.z - report.reporter.z) <= 8.0f) {
            nearby = &report;
            newestAge = age;
        }
    }
    if (!nearby && !hasLocalSighting) {
        return {SearchPhase::Lost, officerPosition};
    }
    // Each responding officer approaches the last confirmed location then
    // sweeps a different part of the immediate area.
    auto destination = nearby ? nearby->lastSeen : record.lastSeen;
    const auto age = nearby ? newestAge : now - record.seenAt;
    if (age < 2300 || DistanceSquared(officerPosition, destination) > 64.0f) {
        return {nearby ? SearchPhase::Respond : SearchPhase::LastSeen, destination};
    }
    const auto section = ((officer >> 8) + age / 1400) % 4;
    constexpr std::array<Point, 4> sweep{{{3, 0, 0}, {0, 3, 0}, {-3, 0, 0}, {0, -3, 0}}};
    destination.x += sweep[section].x;
    destination.y += sweep[section].y;
    return {SearchPhase::Search, destination};
}

void PoliceSearch::Reset() noexcept {
    m_Officers = {};
    m_Reports = {};
    m_Frame = 0;
    m_Casts = 0;
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
    return std::min(input.nativeLimit, cap);
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
