#pragma once

#include "IntelligenceCore.h"

#include <optional>

namespace ulw {

float DistanceSquared(Point a, Point b) noexcept;
float SightRange(float wet, float fog, unsigned hour) noexcept;

enum class SearchPhase : std::uint8_t { Observe, Pursue, LastSeen, Respond, Search, Lost };
enum class PatrolMode : std::uint8_t { Foot, Car, Air };

struct SearchTarget {
    SearchPhase phase;
    Point point;
    Point motion{};
};

class PoliceSearch {
public:
    bool ShouldSample(std::uint32_t officer, std::uint32_t now, std::uint32_t frame) noexcept;
    void Observe(std::uint32_t officer, std::uint32_t now, Point officerPosition, Point point, bool visible,
        PatrolMode mode = PatrolMode::Foot) noexcept;
    SearchTarget Target(std::uint32_t officer, std::uint32_t now, Point officerPosition,
        PatrolMode mode = PatrolMode::Foot) const noexcept;
    bool HasRecentReport(Point position, std::uint32_t now) const noexcept;
    void Reset() noexcept;

private:
    struct Record {
        std::uint32_t ref{};
        std::uint32_t sampledAt{};
        std::uint32_t seenAt{};
        Point lastSeen{};
        Point motion{};
        bool sampled{};
        bool hasSeen{};
        bool visible{};
    };
    struct Report {
        Point reporter{};
        Point lastSeen{};
        std::uint32_t at{};
        std::uint32_t officer{};
        bool occupied{};
    };
    std::array<Record, 128> m_Officers{};
    std::array<Report, 16> m_Reports{};
    std::uint32_t m_Frame{};
    unsigned m_Casts{};
};

bool HasSearchContact(SearchPhase phase) noexcept;
Point AirSearchPoint(SearchTarget target, Point helicopter, Point forward, float flightReferenceZ) noexcept;

enum class PatrolRole : std::uint8_t { Pursuit, Intercept, Support };

struct PatrolAssignment {
    PatrolRole role;
    Point destination;
};

class PatrolTeam {
public:
    PatrolAssignment Assign(std::uint32_t unit, std::uint32_t now, unsigned wanted,
        Point unitPosition, const SearchTarget& sighting) noexcept;
    void Reset() noexcept;

private:
    struct Unit {
        std::uint32_t ref{};
        std::uint32_t at{};
        Point position{};
        Point contact{};
        PatrolRole role{};
        bool active{};
    };
    std::array<Unit, 24> m_Units{};
};

struct RoadblockCandidate {
    Point road;
    Point player;
    float width;
    float speedSquared;
    unsigned wanted;
    unsigned links;
    bool highway;
    bool activeRoad;
    bool sightingRequired;
    bool recentlySeen;
};

bool SuitableRoadblock(const RoadblockCandidate& candidate) noexcept;
bool InCameraCone(Point camera, Point forward, Point target) noexcept;

class RoadblockAttempts {
public:
    bool Ready(std::uint32_t now, unsigned wanted) const noexcept;
    void Record(std::uint32_t now) noexcept;
    void Reset() noexcept;

private:
    std::uint32_t m_Last{};
    bool m_HasAttempt{};
};

enum class DriverProfile : std::uint8_t { Careful, Average, Hurried, Aggressive, Learner, Experienced, Taxi, Truck, Sport };

DriverProfile TrafficDriver(std::uint16_t seed, std::uint16_t model, float mass) noexcept;
float TrafficBendScale(Point forward, Point nextDirection, float distance, bool signals, float wet,
    DriverProfile driver) noexcept;

struct FollowingTraffic {
    float nativeLimit;
    Point follower;
    Point leader;
    Point forward;
    Point leaderForward;
    Point followerVelocity;
    Point leaderVelocity;
    DriverProfile driver;
    bool heavyLeader;
    float wet;
};
float FollowingSpeed(const FollowingTraffic& traffic) noexcept;

class TrafficHorns {
public:
    bool Allow(std::uint32_t vehicle, std::uint32_t now, DriverProfile driver) noexcept;
    void Reset() noexcept;

private:
    struct Request {
        std::uint32_t vehicle{};
        std::optional<std::uint32_t> at{};
    };
    std::array<Request, 256> m_Requests{};
};

struct TrafficInput {
    float nativeLimit;
    float cruiseSpeed;
    float wet;
    float fog;
    unsigned hour;
    bool recentCollision;
    float emergencyScale{1.0f};
    bool crashAhead{};
    DriverProfile driver{DriverProfile::Average};
    float bendScale{1.0f};
};
float TrafficLimit(const TrafficInput& input) noexcept;

enum class EmergencyState : std::uint8_t { Inactive, Responding, OnScene };

struct EmergencyUnit {
    Point position{};
    Point velocity{};
    std::uint32_t vehicle{};
    std::uint32_t observedAt{};
    EmergencyState state{};
};

float EmergencyYieldScale(Point position, Point forward, Point velocity, std::uint32_t now,
    const EmergencyUnit& unit) noexcept;

struct DrivePedals {
    float gas;
    float brake;
};

DrivePedals EmergencyApproach(DrivePedals native, float distanceSquared, float speedSquared, float wet, float fog) noexcept;
DrivePedals PatrolFollowing(DrivePedals native, unsigned wanted, Point offset, Point forward,
    Point policeVelocity, Point targetVelocity, float wet) noexcept;

struct CrashObservation {
    std::uint32_t vehicle;
    std::uint32_t now;
    std::uint32_t impactAt;
    Point position;
    float speedSquared;
    float impact;
};

struct CrashSite {
    Point position;
    std::uint32_t vehicle;
    std::uint32_t impactAt;
};

class CrashScenes {
public:
    void Observe(const CrashObservation& observation) noexcept;
    bool Ahead(Point position, Point forward, std::uint32_t now) const noexcept;
    std::optional<CrashSite> Closest(Point position, float radius, std::uint32_t now) const noexcept;
    bool ObstructsFootpath(Point from, Point nextNode, std::uint32_t now) const noexcept;
    void Reset() noexcept;

private:
    enum class Status : std::uint8_t { Empty, Observed, Stopped, Cleared };
    struct Scene {
        std::uint32_t vehicle{};
        std::uint32_t impactAt{};
        Point position{};
        Status status{};
    };
    std::array<Scene, 16> m_Scenes{};
};

enum class PedestrianPace : std::uint8_t { Walk, Jog };

struct PedestrianRoutine {
    PedestrianPace pace;
    bool chat;
};

PedestrianRoutine PickPedestrianRoutine(std::uint16_t seed, unsigned hour, unsigned weekday, unsigned region,
    float rain) noexcept;

class CrashWitnesses {
public:
    bool TakeScan(std::uint32_t ped, std::uint32_t now, std::uint32_t frame) noexcept;
    bool Noticed(std::uint32_t ped, const CrashSite& crash) const noexcept;
    void Remember(std::uint32_t ped, const CrashSite& crash) noexcept;
    void Reset() noexcept;

private:
    struct Witness {
        std::uint32_t ped{};
        std::uint32_t scannedAt{};
        std::uint32_t vehicle{};
        std::uint32_t impactAt{};
        bool used{};
    };
    std::array<Witness, 128> m_Witnesses{};
    std::uint32_t m_Frame{};
    unsigned m_Scans{};
};

struct CityActivity {
    float pedestrians;
    float cars;
};

CityActivity AmbientActivity(unsigned hour, unsigned weekday, unsigned region, float wet, float fog) noexcept;
unsigned AttractorUsePercent(unsigned type, unsigned hour, unsigned weekday, unsigned region, float rain) noexcept;

std::uint8_t CombatAccuracy(std::uint8_t native, unsigned faction, float health, float distanceSquared,
    float wet, float fog, unsigned hour, bool suppressed = false) noexcept;
std::uint16_t CombatBurstLength(std::uint16_t native, unsigned clip, unsigned faction, float health,
    bool suppressed) noexcept;
bool CombatCoverInRange(Point fighter, Point threat) noexcept;
bool CombatCoverApproach(Point fighter, Point threat, Point destination) noexcept;
unsigned CombatCoverHold(unsigned faction, unsigned clip, unsigned total, unsigned reloadRemaining) noexcept;
bool CombatShouldWithdraw(unsigned faction, float health, unsigned totalAmmo) noexcept;

enum class CombatMove : std::uint8_t { Approach, Hold, Peek, Withdraw };

struct CombatManeuver {
    std::uintptr_t parent;
    std::uintptr_t child;
    std::uintptr_t cover;
    Point destination;
    std::uint32_t started;
    unsigned faction;
    CombatMove phase;
};

class CombatManeuvers {
public:
    void Start(std::uint32_t ped, CombatManeuver move) noexcept;
    std::optional<CombatManeuver> Current(std::uint32_t ped, std::uintptr_t parent,
        std::uintptr_t child) const noexcept;
    std::optional<CombatManeuver> TakeInterrupted(std::uint32_t ped, std::uintptr_t parent,
        std::uintptr_t child) noexcept;
    bool AllyMoving(std::uint32_t ped, unsigned faction, Point origin, std::uint32_t now) const noexcept;
    void Clear(std::uint32_t ped) noexcept;
    void Reset() noexcept;

private:
    struct Slot {
        std::uint32_t ped{};
        std::optional<CombatManeuver> move{};
    };
    std::array<Slot, 256> m_Entries{};
};

class GangLosses {
public:
    bool Record(std::uint32_t ped, unsigned faction, Point position, std::uint32_t now) noexcept;
    bool UnderStrength(unsigned faction, Point position, std::uint32_t now) const noexcept;
    void Reset() noexcept;

private:
    struct Loss {
        std::uint32_t ped{};
        unsigned faction{};
        Point position{};
        std::uint32_t at{};
    };
    std::array<Loss, 32> m_Recent{};
    std::size_t m_Next{};
};

class CombatPressure {
public:
    bool RecordHit(std::uint32_t ped, std::uint32_t hitAt, std::uint32_t now) noexcept;
    bool Suppressed(std::uint32_t ped, std::uint32_t now) const noexcept;
    bool TakeCoverSearch(std::uint32_t ped, std::uint32_t now, std::uint32_t frame) noexcept;
    void Reset() noexcept;

private:
    struct Hit {
        std::uint32_t ped{};
        std::uint32_t at{};
        bool present{};
        bool repeated{};
        std::optional<std::uint32_t> coverSearchAt{};
    };
    std::array<Hit, 256> m_Hits{};
    std::uint32_t m_Frame{};
    unsigned m_CoverSearches{};
};

struct WheelForces {
    float thrust;
    float brake;
    float grip;
};

float PanelWearAtWheel(std::uint32_t panels, int wheel) noexcept;

struct RoadConditions {
    std::uint32_t vehicle;
    std::uint32_t now;
    float mass;
    float health;
    float wet;
    float speedSquared;
    float brakePedal;
    bool burstTyre;
    float throttle{};
    float panelWear{};
    std::uint8_t engineDamage{};
    bool motorcycle{};
};

class RoadPhysics {
public:
    WheelForces Tune(WheelForces forces, const RoadConditions& conditions) noexcept;
    void Reset() noexcept;

private:
    struct VehicleHeat {
        std::uint32_t vehicle{};
        std::uint32_t at{};
        float brakeHeat{};
        float engineHeat{};
        float lastHealth{};
        bool occupied{};
    };
    std::array<VehicleHeat, 8> m_Heat{};
};

}
