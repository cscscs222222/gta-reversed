#include "StreetSystems.h"

#include <algorithm>
#include <cmath>

namespace ulw {
namespace {

constexpr std::uint32_t CrashLifetime = 8000;
constexpr std::uint32_t SuppressionTime = 2400;
constexpr std::uint32_t GangLossLifetime = 10000;

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
    const bool continuous = record.visible && record.hasSeen;
    const auto observedFor = now - record.seenAt;
    record.sampled = true;
    record.sampledAt = now;
    record.visible = visible;
    if (visible) {
        record.motion = {};
        if (continuous && observedFor >= 100 && observedFor <= 850) {
            const auto scale = 1000.0f / static_cast<float>(observedFor);
            const Point motion{(point.x - record.lastSeen.x) * scale, (point.y - record.lastSeen.y) * scale, 0.0f};
            if (DistanceSquared({}, motion) <= 44.0f * 44.0f && std::abs(point.z - record.lastSeen.z) < 4.0f) {
                record.motion = motion;
            }
        }
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
        return {SearchPhase::Pursue, record.lastSeen, record.motion};
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

PatrolAssignment PatrolTeam::Assign(std::uint32_t unit, std::uint32_t now, unsigned wanted, Point unitPosition,
    const SearchTarget& sighting) noexcept {
    constexpr float RadioDistanceSquared = 95.0f * 95.0f;
    constexpr unsigned RoleExpiry = 3200;
    const auto sameSlot = [unit](const Unit& entry) { return entry.active && (entry.ref >> 8) == (unit >> 8); };
    auto own = std::find_if(m_Units.begin(), m_Units.end(), sameSlot);
    if (wanted < 3 || wanted > 6 || sighting.phase != SearchPhase::Pursue) {
        if (own != m_Units.end() && own->ref == unit) {
            *own = {};
        }
        return {PatrolRole::Pursuit, sighting.point};
    }
    if (own == m_Units.end()) {
        own = std::find_if(m_Units.begin(), m_Units.end(), [now](const Unit& entry) {
            return !entry.active || now - entry.at >= RoleExpiry;
        });
        if (own == m_Units.end()) {
            return {PatrolRole::Support, sighting.point};
        }
    }
    const auto speedSquared = DistanceSquared({}, sighting.motion);
    const auto unitDistance = DistanceSquared(unitPosition, sighting.point);
    const bool canCutOff = speedSquared >= 6.0f * 6.0f && speedSquared <= 44.0f * 44.0f &&
        unitDistance >= 14.0f * 14.0f && unitDistance <= RadioDistanceSquared;
    bool hasLead{};
    bool hasInterceptor{};
    bool hasNearLead{};
    for (const auto& other : m_Units) {
        if (!other.active || (other.ref >> 8) == (unit >> 8) || now - other.at >= RoleExpiry ||
            !(DistanceSquared(other.contact, sighting.point) <= 70.0f * 70.0f) ||
            !(DistanceSquared(other.position, unitPosition) <= RadioDistanceSquared) ||
            std::abs(other.position.z - unitPosition.z) > 6.0f) {
            continue;
        }
        hasLead |= other.role == PatrolRole::Pursuit;
        hasInterceptor |= other.role == PatrolRole::Intercept;
        hasNearLead |= other.role == PatrolRole::Pursuit && DistanceSquared(other.position, sighting.point) < unitDistance;
    }
    auto role = PatrolRole::Pursuit;
    auto target = sighting.point;
    if (hasLead) {
        const bool currentLead = own->active && own->ref == unit && own->role == PatrolRole::Pursuit &&
            now - own->at < RoleExpiry && !hasNearLead;
        if (!currentLead) {
            role = PatrolRole::Support;
            if (!hasInterceptor && canCutOff) {
                const auto speed = std::sqrt(speedSquared);
                const auto leadDistance = std::clamp(speed * (wanted >= 5 ? 1.8f : 1.3f), 11.0f, 37.0f);
                const auto projection = leadDistance / speed;
                target.x += sighting.motion.x * projection;
                target.y += sighting.motion.y * projection;
                role = PatrolRole::Intercept;
            }
        }
    }
    *own = {unit, now, unitPosition, sighting.point, role, true};
    return {role, target};
}

void PatrolTeam::Reset() noexcept {
    m_Units = {};
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

DriverProfile TrafficDriver(std::uint16_t seed, std::uint16_t model, float mass) noexcept {
    if (model == 420 || model == 438) {
        return DriverProfile::Taxi;
    }
    if (mass >= 3100.0f) {
        return DriverProfile::Truck;
    }
    constexpr std::array<std::uint16_t, 7> sports{411, 415, 429, 451, 477, 506, 541};
    if (std::find(sports.begin(), sports.end(), model) != sports.end()) {
        return DriverProfile::Sport;
    }
    constexpr std::array<DriverProfile, 10> drivers{
        DriverProfile::Careful, DriverProfile::Average, DriverProfile::Hurried, DriverProfile::Average,
        DriverProfile::Aggressive, DriverProfile::Learner, DriverProfile::Experienced, DriverProfile::Average,
        DriverProfile::Careful, DriverProfile::Experienced};
    return drivers[(seed * 0x9e3779b9u >> 24) % drivers.size()];
}

float TrafficBendScale(Point forward, Point nextDirection, float distance, bool signals, float wet,
    DriverProfile driver) noexcept {
    if (!(distance >= 0.0f && distance < 32.0f)) {
        return 1.0f;
    }
    const auto proximity = 1.0f - Unit((distance - 5.0f) / 27.0f);
    const auto dot = forward.x * nextDirection.x + forward.y * nextDirection.y;
    const auto turn = Unit((0.92f - dot) / 0.98f);
    const auto extra = driver == DriverProfile::Truck ? 0.17f : driver == DriverProfile::Learner ? 0.10f : 0.0f;
    return 1.0f - proximity * std::min(0.61f, turn * (0.37f + extra + 0.12f * Unit(wet)) +
        (signals ? 0.08f : 0.0f));
}

float FollowingSpeed(const FollowingTraffic& traffic) noexcept {
    const auto dx = traffic.leader.x - traffic.follower.x;
    const auto dy = traffic.leader.y - traffic.follower.y;
    const auto distance = dx * traffic.forward.x + dy * traffic.forward.y;
    const auto side = dx * traffic.forward.y - dy * traffic.forward.x;
    const auto aligned = traffic.forward.x * traffic.leaderForward.x + traffic.forward.y * traffic.leaderForward.y;
    if (!std::isfinite(traffic.nativeLimit) ||
        !(traffic.nativeLimit > 0.0f && distance > 0.0f && distance <= 37.0f && aligned >= 0.66f) ||
        !(std::abs(side) < 2.5f && std::abs(traffic.follower.z - traffic.leader.z) < 2.7f)) {
        return traffic.nativeLimit;
    }
    constexpr std::array<float, 9> headways{1.42f, 1.20f, 1.05f, 0.92f, 1.66f, 1.29f, 1.15f, 1.78f, 1.12f};
    const auto driver = static_cast<unsigned>(traffic.driver);
    const auto headway = (driver < headways.size() ? headways[driver] : 1.20f) + 0.52f * Unit(traffic.wet);
    const auto standstill = 4.6f + (traffic.heavyLeader ? 2.4f : 0.0f) +
        (traffic.driver == DriverProfile::Truck ? 1.7f : 0.0f);
    const auto followerSpeed = traffic.followerVelocity.x * traffic.forward.x + traffic.followerVelocity.y * traffic.forward.y;
    const auto leaderSpeed = traffic.leaderVelocity.x * traffic.forward.x + traffic.leaderVelocity.y * traffic.forward.y;
    if (!std::isfinite(followerSpeed) || !std::isfinite(leaderSpeed)) {
        return traffic.nativeLimit;
    }
    const auto speed = std::max(0.0f, followerSpeed) * 60.0f;
    const auto lead = std::max(0.0f, leaderSpeed) * 60.0f;
    const auto closing = std::max(0.0f, speed - lead);
    const auto desired = (distance - standstill - 0.26f * closing) / headway;
    const auto cap = lead + desired;
    return std::clamp(cap, 0.0f, traffic.nativeLimit);
}

bool TrafficHorns::Allow(std::uint32_t vehicle, std::uint32_t now, DriverProfile driver) noexcept {
    const auto interval = driver == DriverProfile::Aggressive || driver == DriverProfile::Hurried ? 3000u : 6200u;
    auto& request = m_Requests[(vehicle >> 8) % m_Requests.size()];
    if (request.vehicle == vehicle && request.at && now - *request.at < interval) {
        return false;
    }
    request = {vehicle, now};
    return true;
}

void TrafficHorns::Reset() noexcept {
    m_Requests = {};
}

bool TrafficLaneChanges::BeginProbe(std::uint32_t vehicle, std::uint32_t now, std::uint32_t frame) noexcept {
    if (m_Frame != frame) {
        m_Frame = frame;
        m_Probes = 0;
    }
    auto& driver = m_Drivers[(vehicle >> 8) % m_Drivers.size()];
    if (m_Probes >= 2 || (driver.sampled && driver.vehicle == vehicle && now - driver.probed < 340)) {
        return false;
    }
    if (!driver.sampled || driver.vehicle != vehicle || now - driver.probed > 8500) {
        driver = {};
    }
    driver.vehicle = vehicle;
    driver.probed = now;
    driver.sampled = true;
    ++m_Probes;
    return true;
}

std::optional<LaneChoice> TrafficLaneChanges::Decide(const LaneOpportunity& road,
    std::span<const LaneBody> nearby) noexcept {
    auto& driver = m_Drivers[(road.vehicle >> 8) % m_Drivers.size()];
    if (!driver.sampled || driver.vehicle != road.vehicle || driver.probed != road.now ||
        road.lane < 0 || road.lane >= road.count || road.count < 2 ||
        !(road.laneWidth >= 3.2f && road.laneWidth <= 6.0f && std::abs(road.laneCenter) <= 1.65f &&
            road.spaceAhead >= 22.0f && road.speed >= -0.2f && road.speed <= 20.0f && road.cruise >= 5.0f &&
            road.halfLength > 0.0f && road.halfLength <= 4.0f && road.halfWidth > 0.0f && road.halfWidth <= 1.55f &&
            road.wet >= 0.0f && road.wet < 0.82f)) {
        return std::nullopt;
    }

    if (driver.original >= 0 && (driver.original >= road.count || driver.target >= road.count ||
        (road.lane != driver.original && road.lane != driver.target) || road.now - driver.changed > 16000)) {
        driver.original = -1;
        driver.target = -1;
        driver.blocker.reset();
    }
    const bool returning = driver.original >= 0;
    const auto target = returning ? driver.original : road.lane - 1;
    if (target < 0 || (returning && (road.lane != driver.target || std::abs(road.lane - target) != 1)) ||
        (driver.moved && road.now - driver.changed < (returning ? 2750u : 5200u))) {
        return std::nullopt;
    }

    const auto center = road.laneCenter + static_cast<float>(target - road.lane) * road.laneWidth;
    const auto targetWidth = road.halfWidth + 0.42f + 0.27f * Unit(road.wet);
    const auto ahead = std::max(12.0f, 8.0f + road.speed * 0.75f);
    const LaneBody* blocker = nullptr;
    bool occupied = false;
    bool passedBlocker = true;
    for (const auto& body : nearby) {
        if (!(std::abs(body.elevation) < 2.45f)) {
            continue;
        }
        const auto bumper = road.halfLength + body.halfLength;
        const bool sameLane = std::abs(body.lateral - road.laneCenter) < road.halfWidth + body.halfWidth + 0.45f;
        if (sameLane && body.alignedVehicle && body.along > 0.0f && body.along < 24.0f &&
            (!blocker || blocker->along > body.along)) {
            blocker = &body;
        }
        if (returning && driver.blocker == body.vehicle && body.alignedVehicle && body.along > -bumper - 5.0f) {
            passedBlocker = false;
        }
        const auto futureLateral = body.lateral + body.lateralSpeed * 1.25f;
        if (std::min(body.lateral, futureLateral) > center + targetWidth + body.halfWidth ||
            std::max(body.lateral, futureLateral) < center - targetWidth - body.halfWidth) {
            continue;
        }
        const auto horizon = 1.5f + 0.38f * Unit(road.wet);
        const auto gapAhead = bumper + ahead + horizon * std::max(0.0f, road.speed - body.forwardSpeed);
        const auto gapBehind = bumper + 6.0f + horizon * std::max(0.0f, body.forwardSpeed - road.speed);
        occupied |= body.along <= gapAhead && body.along >= -gapBehind;
    }

    if (!returning) {
        if (road.speed > 15.0f || !blocker || !(blocker->forwardSpeed >= -0.5f && blocker->forwardSpeed <= road.cruise - 3.7f) ||
            road.spaceAhead < blocker->along + 6.0f) {
            driver.blocker.reset();
            return std::nullopt;
        }
        if (driver.blocker != blocker->vehicle) {
            driver.blocker = blocker->vehicle;
            driver.waitingSince = road.now;
            return std::nullopt;
        }
        const bool impatient = road.driver == DriverProfile::Hurried || road.driver == DriverProfile::Sport ||
            road.driver == DriverProfile::Aggressive;
        const auto delay = impatient ? 1050u : road.driver == DriverProfile::Careful ||
            road.driver == DriverProfile::Learner || road.driver == DriverProfile::Truck ? 2400u : 1650u;
        if (road.now - driver.waitingSince < delay ||
            blocker->along < road.halfLength + blocker->halfLength + 2.1f + 0.30f * road.speed) {
            return std::nullopt;
        }
    }
    if (occupied || !passedBlocker) {
        return std::nullopt;
    }

    const Point targetCenter{road.position.x + road.right.x * center, road.position.y + road.right.y * center,
        road.position.z};
    for (const auto& reservation : m_Reservations) {
        if (reservation.active && reservation.vehicle != road.vehicle && road.now - reservation.at < 1600 &&
            DistanceSquared(reservation.center, targetCenter) < 14.0f * 14.0f) {
            return std::nullopt;
        }
    }
    return LaneChoice{target, returning};
}

void TrafficLaneChanges::Commit(const LaneOpportunity& road, LaneChoice choice) noexcept {
    auto& driver = m_Drivers[(road.vehicle >> 8) % m_Drivers.size()];
    const auto center = road.laneCenter + static_cast<float>(choice.lane - road.lane) * road.laneWidth;
    const Point targetCenter{road.position.x + road.right.x * center, road.position.y + road.right.y * center,
        road.position.z};
    m_Reservations[m_NextReservation++ % m_Reservations.size()] = {targetCenter, road.vehicle, road.now, true};
    driver.changed = road.now;
    driver.moved = true;
    driver.original = choice.returning ? -1 : road.lane;
    driver.target = choice.returning ? -1 : choice.lane;
    if (choice.returning) {
        driver.blocker.reset();
    }
}

void TrafficLaneChanges::Reset() noexcept {
    m_Drivers = {};
    m_Reservations = {};
    m_Frame = 0;
    m_Probes = 0;
    m_NextReservation = 0;
}

DrivePedals PatrolFollowing(DrivePedals native, unsigned wanted, Point offset, Point forward,
    Point policeVelocity, Point targetVelocity, float wet) noexcept {
    const auto ahead = offset.x * forward.x + offset.y * forward.y;
    const auto lateral = offset.x * forward.y - offset.y * forward.x;
    const auto closing = (policeVelocity.x - targetVelocity.x) * forward.x +
        (policeVelocity.y - targetVelocity.y) * forward.y;
    if (!(wanted >= 1 && wanted <= 4 && native.gas >= 0.0f && ahead > 0.0f && ahead < 25.0f &&
            std::abs(lateral) < 3.0f && std::abs(offset.z) < 3.0f && closing > 0.025f)) {
        return native;
    }
    const auto gap = (wanted == 1 ? 10.5f : wanted == 2 ? 8.5f : 6.0f) + 2.5f * Unit(wet);
    const auto ease = Unit((gap + 27.0f * closing - ahead) / 11.0f);
    return {native.gas * (1.0f - ease), std::max(native.brake, 0.48f * ease)};
}

float TrafficLimit(const TrafficInput& input) noexcept {
    if (!std::isfinite(input.nativeLimit) || !std::isfinite(input.cruiseSpeed) ||
        input.nativeLimit <= 0.0f || input.cruiseSpeed <= 0.0f) {
        return input.nativeLimit;
    }
    constexpr std::array<float, 9> temperament{0.87f, 0.95f, 0.99f, 1.0f, 0.83f, 0.96f, 0.95f, 0.86f, 1.0f};
    const auto driver = static_cast<unsigned>(input.driver);
    auto cap = input.cruiseSpeed * (driver < temperament.size() ? temperament[driver] : 0.95f);
    cap *= std::clamp(std::isfinite(input.bendScale) ? input.bendScale : 1.0f, 0.39f, 1.0f);
    cap *= 1.0f - 0.17f * Unit(input.wet) - 0.18f * Unit(input.fog);
    if (IsNight(input.hour)) {
        cap *= 0.96f;
    }
    if (input.recentCollision) {
        cap *= 0.68f;
    }
    if (std::isfinite(input.emergencyScale)) {
        cap *= std::clamp(input.emergencyScale, 0.48f, 1.0f);
    }
    if (input.crashAhead) {
        cap *= 0.58f;
    }
    return std::min(input.nativeLimit, cap);
}

float EmergencyYieldScale(Point position, Point forward, Point velocity, std::uint32_t now,
    const EmergencyUnit& unit) noexcept {
    if (unit.state == EmergencyState::Inactive || std::abs(position.z - unit.position.z) > 3.25f) {
        return 1.0f;
    }
    const auto x = position.x - unit.position.x;
    const auto y = position.y - unit.position.y;
    const auto distanceSquared = x * x + y * y;
    const auto age = now - unit.observedAt;
    if (unit.state == EmergencyState::OnScene) {
        const auto ahead = -x * forward.x - y * forward.y;
        const auto lateral = x * forward.y - y * forward.x;
        if (age < 2000 && ahead > 1.0f && ahead < 23.0f && std::abs(lateral) <= 6.5f) {
            return 0.56f + 0.28f * Unit(ahead / 23.0f);
        }
        return 1.0f;
    }

    const auto unitSpeedSq = unit.velocity.x * unit.velocity.x + unit.velocity.y * unit.velocity.y;
    if (!(unitSpeedSq >= 0.075f * 0.075f && distanceSquared <= 58.0f * 58.0f) || age >= 900) {
        return 1.0f;
    }
    const auto unitSpeed = std::sqrt(unitSpeedSq);
    const auto fx = unit.velocity.x / unitSpeed;
    const auto fy = unit.velocity.y / unitSpeed;
    const auto ahead = x * fx + y * fy;
    if (ahead <= 2.5f) {
        return 1.0f;
    }
    const auto lateral = x * fy - y * fx;
    const auto alignment = fx * forward.x + fy * forward.y;
    if (alignment > 0.72f && std::abs(lateral) < 5.0f) {
        const auto closingSpeed = unitSpeed - (velocity.x * fx + velocity.y * fy);
        const auto range = std::min(54.0f, 27.0f + unitSpeed * 34.0f);
        if (closingSpeed >= 0.035f && ahead <= range) {
            return 0.69f + 0.23f * Unit(ahead / range);
        }
        return 1.0f;
    }

    // Compare where the vehicles will be on their current courses.
    const auto rx = velocity.x - unit.velocity.x;
    const auto ry = velocity.y - unit.velocity.y;
    const auto relativeSpeedSq = rx * rx + ry * ry;
    if (!(relativeSpeedSq > 0.015f) || std::abs(lateral) > 18.0f) {
        return 1.0f;
    }
    const auto timeToPass = -(x * rx + y * ry) / relativeSpeedSq;
    const auto missX = x + rx * timeToPass;
    const auto missY = y + ry * timeToPass;
    if (timeToPass >= 0.0f && timeToPass <= 65.0f && missX * missX + missY * missY < 5.0f * 5.0f) {
        return 0.53f + 0.27f * Unit(timeToPass / 65.0f);
    }
    return 1.0f;
}

DrivePedals EmergencyApproach(DrivePedals native, float distanceSquared, float speedSquared, float wet, float fog) noexcept {
    if (!(distanceSquared >= 0.0f && distanceSquared <= 64.0f * 64.0f) || !std::isfinite(speedSquared) ||
        speedSquared < 0.0f || !(native.gas >= 0.0f && native.gas <= 1.0f) ||
        !(native.brake >= 0.0f && native.brake <= 1.0f)) {
        return native;
    }
    const auto distance = std::sqrt(distanceSquared);
    const auto weather = 1.0f - 0.22f * Unit(wet) - 0.13f * Unit(fog);
    const auto targetSpeed = (0.105f + 0.006f * distance) * weather;
    const auto speed = std::sqrt(speedSquared);
    if (speed <= targetSpeed) {
        return native;
    }
    const auto excess = Unit((speed - targetSpeed) / 0.38f);
    const auto brake = std::min(0.64f, 0.12f + 0.57f * excess);
    return {native.gas * (1.0f - excess), std::max(native.brake, brake)};
}

void CrashScenes::Observe(const CrashObservation& crash) noexcept {
    auto& scene = m_Scenes[(crash.vehicle >> 8) % m_Scenes.size()];
    const bool sameVehicle = scene.status != Status::Empty && (scene.vehicle >> 8) == (crash.vehicle >> 8);
    if (sameVehicle && scene.vehicle != crash.vehicle) {
        scene = {};
    }
    if (crash.impactAt && crash.now - crash.impactAt < 900 && crash.impact >= 28.0f &&
        (scene.status == Status::Empty || scene.vehicle != crash.vehicle || scene.impactAt != crash.impactAt)) {
        scene = {crash.vehicle, crash.impactAt, crash.position, Status::Observed};
    }
    if (scene.vehicle != crash.vehicle) {
        return;
    }
    if (scene.status == Status::Stopped &&
        (crash.speedSquared > 0.12f * 0.12f || DistanceSquared(scene.position, crash.position) > 16.0f)) {
        scene.status = Status::Cleared;
        return;
    }
    if (scene.status == Status::Observed && crash.now - scene.impactAt < 2200 &&
        crash.speedSquared >= 0.0f && crash.speedSquared <= 0.08f * 0.08f) {
        scene.position = crash.position;
        scene.status = Status::Stopped;
    }
}

std::optional<std::uint32_t> CrashScenes::RecentImpact(std::uint32_t vehicle, std::uint32_t now) const noexcept {
    const auto& scene = m_Scenes[(vehicle >> 8) % m_Scenes.size()];
    if (scene.status != Status::Empty && scene.vehicle == vehicle && now - scene.impactAt < 4200) {
        return scene.impactAt;
    }
    return std::nullopt;
}

bool ShouldPullOver(const BreakdownEvidence& evidence) noexcept {
    return evidence.health >= 255.0f && evidence.health <= 590.0f &&
        (evidence.health <= 445.0f || evidence.engineDamage >= 145) &&
        evidence.speed >= 0.0f && evidence.speed <= 8.5f && evidence.impactAge < 4200;
}

bool RoadsideSurface(unsigned surface, float slope, float heightDifference) noexcept {
    const bool firm = (surface >= 1 && surface <= 3) || surface == 6 || surface == 26 || surface == 27 ||
        surface == 85 || surface == 86;
    return firm && slope >= 0.93f && slope <= 1.01f && std::abs(heightDifference) <= 0.42f;
}

RoadsideControl RoadsideResponse(const RoadsideInput& input) noexcept {
    if (!input.clear || !(input.health >= 250.0f && input.health <= 610.0f) || input.elapsed > 13000 ||
        !(input.speed >= -0.35f && input.speed < 12.0f) ||
        !(input.ahead >= -1.8f && input.ahead < 24.0f && std::abs(input.sideways) <= 6.4f) ||
        !std::isfinite(input.headingError)) {
        return {};
    }
    if (input.ahead < 1.6f && std::abs(input.sideways) < 0.85f) {
        if (input.stopped >= 4600) {
            return {};
        }
        const bool severe = input.health <= 385.0f || input.engineDamage >= 180;
        if (severe && input.stopped >= 1700) {
            return {RoadsideAction::Leave, 0.0f, 0.0f, 0.60f};
        }
        return {RoadsideAction::Hold, 0.0f, 0.0f, 0.38f};
    }
    return ShoulderApproach(input.ahead, input.headingError, input.speed);
}

RoadsideControl ShoulderApproach(float ahead, float headingError, float speed) noexcept {
    if (!(ahead >= -1.8f && ahead <= 24.0f) || !std::isfinite(headingError) || !std::isfinite(speed)) {
        return {};
    }
    const auto travel = std::max(ahead, 0.0f);
    const auto targetSpeed = std::clamp(travel * 0.48f, 1.05f, 5.2f);
    const auto difference = speed - targetSpeed;
    const auto steer = std::clamp(headingError * 0.95f, -0.34f, 0.34f);
    return {RoadsideAction::Approach, steer, 0.25f * Unit(1.0f - difference * 0.27f),
        0.55f * Unit(difference / 3.4f)};
}

bool CrashScenes::Ahead(Point position, Point forward, std::uint32_t now) const noexcept {
    for (const auto& scene : m_Scenes) {
        if (scene.status != Status::Stopped || now - scene.impactAt >= CrashLifetime || std::abs(scene.position.z - position.z) > 2.8f) {
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

std::optional<CrashSite> CrashScenes::Closest(Point position, float radius, std::uint32_t now) const noexcept {
    std::optional<CrashSite> best;
    auto limit = radius * radius;
    for (const auto& scene : m_Scenes) {
        if (scene.status != Status::Stopped || now - scene.impactAt >= CrashLifetime || std::abs(scene.position.z - position.z) > 2.5f) {
            continue;
        }
        const auto distance = DistanceSquared(scene.position, position);
        if (distance <= limit) {
            limit = distance;
            best = CrashSite{scene.position, scene.vehicle, scene.impactAt};
        }
    }
    return best;
}

bool CrashScenes::ObstructsFootpath(Point from, Point nextNode, std::uint32_t now) const noexcept {
    const auto dx = nextNode.x - from.x;
    const auto dy = nextNode.y - from.y;
    const auto lengthSquared = dx * dx + dy * dy;
    if (!(lengthSquared >= 0.25f && lengthSquared <= 9.0f * 9.0f) ||
        std::abs(nextNode.z - from.z) > 3.5f) {
        return false;
    }
    for (const auto& scene : m_Scenes) {
        if (scene.status != Status::Stopped || now - scene.impactAt >= CrashLifetime || std::abs(scene.position.z - from.z) > 2.5f) {
            continue;
        }
        const auto x = scene.position.x - from.x;
        const auto y = scene.position.y - from.y;
        const auto along = (x * dx + y * dy) / lengthSquared;
        if (!(along > 0.05f && along <= 1.15f)) {
            continue;
        }
        const auto lateralX = x - dx * along;
        const auto lateralY = y - dy * along;
        if (lateralX * lateralX + lateralY * lateralY < 2.5f * 2.5f) {
            return true;
        }
    }
    return false;
}

void CrashScenes::Reset() noexcept {
    m_Scenes = {};
}

PedestrianRoutine PickPedestrianRoutine(std::uint16_t seed, unsigned hour, unsigned weekday, unsigned region,
    float rain) noexcept {
    if (hour >= 24 || weekday < 1 || weekday > 7 || region > 4) {
        return {PedestrianPace::Walk, true};
    }
    const auto personality = ((static_cast<std::uint32_t>(seed) * 0x45d9f3bu) >> 16) % 100;
    const bool weekdayRush = weekday < 6 && ((hour >= 7 && hour < 10) || (hour >= 16 && hour < 19));
    const bool outdoorRun = hour >= 6 && hour < 9 &&
        (region == 1 || region == 2) && personality >= 80 && personality < 88 && Unit(rain) < 0.15f;
    const bool hurried = (weekdayRush && personality < 20) || (Unit(rain) >= 0.25f && personality < 34) || outdoorRun;
    const bool casualAtNight = region == 3 && (hour >= 20 || hour < 3);
    return {hurried ? PedestrianPace::Jog : PedestrianPace::Walk,
        !hurried && (Unit(rain) < 0.25f || personality >= 65) && (casualAtNight || !IsNight(hour) || personality >= 50)};
}

bool CrashWitnesses::TakeScan(std::uint32_t ped, std::uint32_t now, std::uint32_t frame) noexcept {
    if (m_Frame != frame) {
        m_Frame = frame;
        m_Scans = 0;
    }
    if (m_Scans >= 5) {
        return false;
    }
    auto& witness = m_Witnesses[(ped >> 8) % m_Witnesses.size()];
    if (witness.used && witness.ped == ped && now - witness.scannedAt < 460) {
        return false;
    }
    if (!witness.used || witness.ped != ped) {
        witness = {ped, now, 0, 0, true};
    } else {
        witness.scannedAt = now;
    }
    ++m_Scans;
    return true;
}

bool CrashWitnesses::Noticed(std::uint32_t ped, const CrashSite& crash) const noexcept {
    const auto& witness = m_Witnesses[(ped >> 8) % m_Witnesses.size()];
    return witness.used && witness.ped == ped && witness.vehicle == crash.vehicle && witness.impactAt == crash.impactAt;
}

void CrashWitnesses::Remember(std::uint32_t ped, const CrashSite& crash) noexcept {
    auto& witness = m_Witnesses[(ped >> 8) % m_Witnesses.size()];
    if (witness.used && witness.ped == ped) {
        witness.vehicle = crash.vehicle;
        witness.impactAt = crash.impactAt;
    }
}

void CrashWitnesses::Reset() noexcept {
    m_Witnesses = {};
    m_Frame = 0;
    m_Scans = 0;
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
    float wet, float fog, unsigned hour, bool suppressed) noexcept {
    if (native == 0 || native > 100 || faction < 6 || faction > 16 || !(health > 0.0f) ||
        !std::isfinite(distanceSquared) || distanceSquared < 0.0f) {
        return native;
    }
    // Distinct aiming skill for police and the ten gang ped types.
    constexpr std::array<float, 11> training{18, 15, 13, 11, 10, 14, 20, 17, 12, 18, 12};
    const auto distant = Unit((std::sqrt(distanceSquared) - 8.0f) / 37.0f);
    const auto injured = Unit((65.0f - health) / 55.0f);
    const auto weather = Unit(wet) * 6.0f + Unit(fog) * 17.0f + (IsNight(hour) ? 5.0f : 0.0f);
    const auto penalty = suppressed ? 14.0f : 0.0f;
    const auto adjustment = training[faction - 6] * (1.0f - 0.45f * distant) -
        injured * 18.0f - distant * weather - penalty;
    return static_cast<std::uint8_t>(std::clamp(static_cast<float>(native) + adjustment, 1.0f, 100.0f));
}

std::uint16_t CombatBurstLength(std::uint16_t native, unsigned clip, unsigned faction, float health,
    bool suppressed) noexcept {
    if (native == 0 || native > 12 || clip == 0 || faction < 6 || faction > 16 ||
        !std::isfinite(health) || health <= 0.0f) {
        return native;
    }
    if (suppressed || health < 35.0f) {
        return static_cast<std::uint16_t>(std::min((native + 1u) / 2u, clip));
    }
    const auto maximum = faction == 6 ? 12u : 10u;
    const auto extra = faction == 6 ? std::max(2u, native / 2u) : 1u + native / 4u;
    const auto extended = std::min({native + extra, maximum, clip});
    return static_cast<std::uint16_t>(std::max<unsigned>(native, extended));
}

bool CombatCoverInRange(Point fighter, Point threat) noexcept {
    const auto distance = DistanceSquared(fighter, threat);
    return distance >= 5.0f * 5.0f && distance <= 38.0f * 38.0f && std::abs(fighter.z - threat.z) < 3.0f;
}

bool CombatCoverApproach(Point fighter, Point threat, Point destination) noexcept {
    const auto travel = DistanceSquared(fighter, destination);
    const auto advance = (destination.x - fighter.x) * (threat.x - fighter.x) +
        (destination.y - fighter.y) * (threat.y - fighter.y);
    const auto threatRange = std::hypot(threat.x - fighter.x, threat.y - fighter.y);
    return travel >= 1.5f * 1.5f && travel <= 12.0f * 12.0f &&
        std::abs(destination.z - fighter.z) <= 1.4f && advance <= 1.5f * threatRange &&
        DistanceSquared(threat, destination) >= 4.0f * 4.0f;
}

unsigned CombatCoverHold(unsigned faction, unsigned clip, unsigned total, unsigned reloadRemaining) noexcept {
    // Police, Ballas, Grove Street and Vagos return fire at different tempos.
    constexpr std::array<unsigned, 11> wait{620, 420, 740, 900, 690, 760, 560, 610, 710, 590, 720};
    const auto base = faction >= 6 && faction <= 16 ? wait[faction - 6] : 700u;
    return total > clip && (clip <= 2 || reloadRemaining != 0)
        ? std::clamp(reloadRemaining, base + 250, 1900u)
        : base;
}

bool CombatShouldWithdraw(unsigned faction, float health, unsigned totalAmmo) noexcept {
    constexpr std::array<float, 11> reserve{19, 16, 23, 29, 25, 23, 18, 19, 24, 18, 23};
    return faction >= 6 && faction <= 16 && health > 0.0f && std::isfinite(health) &&
        (health <= reserve[faction - 6] || totalAmmo == 0);
}

void CombatManeuvers::Start(std::uint32_t ped, CombatManeuver move) noexcept {
    m_Entries[(ped >> 8) % m_Entries.size()] = {ped, move};
}

std::optional<CombatManeuver> CombatManeuvers::Current(std::uint32_t ped, std::uintptr_t parent,
    std::uintptr_t child) const noexcept {
    const auto& entry = m_Entries[(ped >> 8) % m_Entries.size()];
    if (entry.ped != ped || !entry.move || entry.move->parent != parent || entry.move->child != child) {
        return std::nullopt;
    }
    return entry.move;
}

std::optional<CombatManeuver> CombatManeuvers::TakeInterrupted(std::uint32_t ped, std::uintptr_t parent,
    std::uintptr_t child) noexcept {
    auto& entry = m_Entries[(ped >> 8) % m_Entries.size()];
    if (entry.ped != ped || !entry.move || (entry.move->parent == parent && entry.move->child == child)) {
        return std::nullopt;
    }
    const auto interrupted = entry.move;
    entry.move.reset();
    return interrupted;
}

bool CombatManeuvers::AllyMoving(std::uint32_t ped, unsigned faction, Point origin, std::uint32_t now) const noexcept {
    return std::any_of(m_Entries.begin(), m_Entries.end(), [&](const auto& entry) {
        return (entry.ped >> 8) != (ped >> 8) && entry.move && entry.move->faction == faction &&
            now - entry.move->started < 2100 && DistanceSquared(origin, entry.move->destination) <= 12.0f * 12.0f;
    });
}

void CombatManeuvers::Clear(std::uint32_t ped) noexcept {
    auto& entry = m_Entries[(ped >> 8) % m_Entries.size()];
    if (entry.ped == ped) {
        entry.move.reset();
    }
}

void CombatManeuvers::Reset() noexcept {
    m_Entries = {};
}

bool GangLosses::Record(std::uint32_t ped, unsigned faction, Point position, std::uint32_t now) noexcept {
    if (faction < 7 || faction > 16) {
        return false;
    }
    for (const auto& loss : m_Recent) {
        if (loss.faction && loss.ped == ped && now - loss.at < GangLossLifetime) {
            return false;
        }
    }
    m_Recent[m_Next] = {ped, faction, position, now};
    m_Next = (m_Next + 1) % m_Recent.size();
    return true;
}

bool GangLosses::UnderStrength(unsigned faction, Point position, std::uint32_t now) const noexcept {
    if (faction < 7 || faction > 16) {
        return false;
    }
    // Ballas, Mafia and Triads keep their position after two nearby losses.
    const unsigned limit = faction == 7 || faction == 12 || faction == 13 ? 3 : 2;
    unsigned losses = 0;
    for (const auto& loss : m_Recent) {
        if (loss.faction == faction && now - loss.at < GangLossLifetime &&
            std::abs(loss.position.z - position.z) < 3.0f &&
            DistanceSquared(loss.position, position) <= 22.0f * 22.0f && ++losses >= limit) {
            return true;
        }
    }
    return false;
}

void GangLosses::Reset() noexcept {
    m_Recent = {};
    m_Next = 0;
}

bool CombatPressure::RecordHit(std::uint32_t ped, std::uint32_t hitAt, std::uint32_t now) noexcept {
    if (now - hitAt > 250) {
        return false;
    }
    auto& previous = m_Hits[(ped >> 8) % m_Hits.size()];
    const bool samePed = previous.present && previous.ped == ped;
    if (samePed && previous.at >= hitAt) {
        return false;
    }
    const auto interval = previous.repeated ? SuppressionTime : 1450u;
    const bool repeated = samePed && hitAt - previous.at < interval;
    previous = {ped, hitAt, true, repeated, samePed ? previous.coverSearchAt : std::nullopt};
    return repeated;
}

bool CombatPressure::Suppressed(std::uint32_t ped, std::uint32_t now) const noexcept {
    const auto& hit = m_Hits[(ped >> 8) % m_Hits.size()];
    return hit.present && hit.ped == ped && hit.repeated && now - hit.at < SuppressionTime;
}

bool CombatPressure::TakeCoverSearch(std::uint32_t ped, std::uint32_t now, std::uint32_t frame) noexcept {
    if (!Suppressed(ped, now)) {
        return false;
    }
    auto& hit = m_Hits[(ped >> 8) % m_Hits.size()];
    if (hit.coverSearchAt && now - *hit.coverSearchAt < 4800) {
        return false;
    }
    if (m_Frame != frame) {
        m_Frame = frame;
        m_CoverSearches = 0;
    }
    if (m_CoverSearches >= 2) {
        return false;
    }
    ++m_CoverSearches;
    hit.coverSearchAt = now;
    return true;
}

void CombatPressure::Reset() noexcept {
    m_Hits = {};
    m_Frame = 0;
    m_CoverSearches = 0;
}

float PanelWearAtWheel(std::uint32_t panels, int wheel) noexcept {
    constexpr std::array<unsigned, 4> panelForWheel{0, 2, 1, 3};
    if (wheel < 0 || wheel >= 4) {
        return 0.0f;
    }
    const auto wing = (panels >> (4 * panelForWheel[static_cast<unsigned>(wheel)])) & 0xFu;
    const auto bumper = (panels >> ((wheel % 2 == 0) ? 20 : 24)) & 0xFu;
    return 0.82f * Unit((static_cast<float>(wing) - 1.0f) / 3.0f) +
        0.18f * Unit((static_cast<float>(bumper) - 1.0f) / 3.0f);
}

WheelForces RoadPhysics::Tune(WheelForces forces, const RoadConditions& conditions) noexcept {
    if (!(conditions.mass > 0.0f) || conditions.speedSquared < 0.0f ||
        !std::isfinite(conditions.speedSquared) || !std::isfinite(conditions.health)) {
        return forces;
    }
    auto& heat = m_Heat[(conditions.vehicle >> 8) % m_Heat.size()];
    if (!heat.occupied || heat.vehicle != conditions.vehicle || conditions.now - heat.at > 30000 ||
        (conditions.health >= 850.0f && conditions.health - heat.lastHealth > 90.0f)) {
        heat = {conditions.vehicle, conditions.now, 0.0f, 0.0f, conditions.health, true};
    }
    const auto elapsed = conditions.now - heat.at;
    const auto interval = std::min(elapsed, 100u);
    const auto idle = static_cast<float>(elapsed - interval);
    const auto tick = static_cast<float>(interval);
    const auto severity = Unit(conditions.brakePedal) * Unit(conditions.speedSquared / 0.45f);
    const auto heavy = Unit((conditions.mass - 1350.0f) / 3000.0f);
    const auto wear = Unit((650.0f - conditions.health) / 400.0f);
    const auto damage = std::max(wear, Unit((static_cast<float>(conditions.engineDamage) - 75.0f) / 150.0f));
    const auto airflow = Unit(conditions.speedSquared / 0.5f);
    const auto load = Unit((std::abs(conditions.throttle) - 0.55f) / 0.45f);
    heat.brakeHeat = Unit(heat.brakeHeat + tick * (severity * (0.00026f + heavy * 0.00020f) - 0.000075f) - idle * 0.00012f);
    const auto heating = load * (0.00011f + damage * 0.00021f) * (1.0f - 0.67f * airflow);
    const auto cooling = 0.000058f + airflow * 0.000055f + (1.0f - load) * 0.000036f;
    heat.engineHeat = Unit(heat.engineHeat + tick * (heating - cooling) - idle * 0.000075f);
    heat.at = conditions.now;
    heat.lastHealth = conditions.health;

    const auto panel = Unit(conditions.panelWear);
    const auto wet = Unit(conditions.wet);
    const auto rainGrip = conditions.motorcycle ? 0.16f : 0.11f + 0.035f * heavy;
    if (!conditions.burstTyre) {
        forces.grip *= 1.0f - wet * rainGrip - 0.065f * wear - 0.10f * panel;
    }
    forces.brake *= 1.0f - heat.brakeHeat * (0.075f + 0.055f * heavy) - 0.07f * wear - 0.06f * panel;
    const auto overheat = Unit((heat.engineHeat - 0.48f) / 0.52f);
    forces.thrust *= 1.0f - 0.20f * damage - 0.26f * overheat;
    if (conditions.contact.wheel >= 0 && conditions.contact.wheel < static_cast<int>(heat.tyres.size())) {
        auto& tyre = heat.tyres[static_cast<unsigned>(conditions.contact.wheel)];
        const auto temperature = UpdateTyreTemperature(tyre, conditions.now, conditions.contact, wet, conditions.tyreSliding);
        forces = DynamicWheelResponse(forces, conditions.contact, wet, conditions.throttle, conditions.brakePedal,
            temperature, conditions.burstTyre);
    }
    return forces;
}

void RoadPhysics::Reset() noexcept {
    m_Heat = {};
}

}
