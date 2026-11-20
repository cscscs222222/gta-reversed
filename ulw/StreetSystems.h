#pragma once

#include "IntelligenceCore.h"

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
    bool sirenApproaching;
    bool crashAhead{};
};
float TrafficLimit(const TrafficInput& input) noexcept;

struct CrashObservation {
    std::uint32_t vehicle;
    std::uint32_t now;
    std::uint32_t impactAt;
    Point position;
    float speedSquared;
    float impact;
};

class CrashScenes {
public:
    void Observe(const CrashObservation& observation) noexcept;
    bool Ahead(Point position, Point forward, std::uint32_t now) const noexcept;
    void Reset() noexcept;

private:
    struct Scene {
        std::uint32_t vehicle{};
        std::uint32_t impactAt{};
        Point position{};
        bool occupied{};
    };
    std::array<Scene, 16> m_Scenes{};
};

struct CityActivity {
    float pedestrians;
    float cars;
};

CityActivity AmbientActivity(unsigned hour, unsigned weekday, unsigned region, float wet, float fog) noexcept;
unsigned AttractorUsePercent(unsigned type, unsigned hour, unsigned weekday, unsigned region, float rain) noexcept;

std::uint8_t CombatAccuracy(std::uint8_t native, unsigned faction, float health, float distanceSquared,
    float wet, float fog, unsigned hour) noexcept;

struct WheelForces {
    float thrust;
    float brake;
    float grip;
};

struct RoadConditions {
    std::uint32_t vehicle;
    std::uint32_t now;
    float mass;
    float health;
    float wet;
    float speedSquared;
    float brakePedal;
    bool burstTyre;
};

class RoadPhysics {
public:
    WheelForces Tune(WheelForces forces, const RoadConditions& conditions) noexcept;
    void Reset() noexcept;

private:
    struct Brakes {
        std::uint32_t vehicle{};
        std::uint32_t at{};
        float heat{};
        bool occupied{};
    };
    std::array<Brakes, 8> m_Brakes{};
};

}
