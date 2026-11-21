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

struct TrafficInput {
    float nativeLimit;
    float cruiseSpeed;
    float wet;
    float fog;
    unsigned hour;
    std::uint32_t seed;
    bool recentCollision;
    float emergencyScale{1.0f};
    bool crashAhead{};
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

class CombatPressure {
public:
    bool RecordHit(std::uint32_t ped, std::uint32_t hitAt, std::uint32_t now) noexcept;
    bool Suppressed(std::uint32_t ped, std::uint32_t now) const noexcept;
    void Reset() noexcept;

private:
    struct Hit {
        std::uint32_t ped{};
        std::uint32_t at{};
        bool present{};
        bool repeated{};
    };
    std::array<Hit, 256> m_Hits{};
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
