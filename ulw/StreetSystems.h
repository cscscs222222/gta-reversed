#pragma once

#include "IntelligenceCore.h"

namespace ulw {

float DistanceSquared(Point a, Point b) noexcept;
float SightRange(float wet, float fog, unsigned hour) noexcept;

enum class SearchPhase : std::uint8_t { Observe, Pursue, LastSeen, Respond, Search, Lost };

struct SearchTarget {
    SearchPhase phase;
    Point point;
};

class PoliceSearch {
public:
    bool ShouldSample(std::uint32_t officer, std::uint32_t now, std::uint32_t frame) noexcept;
    void Observe(std::uint32_t officer, std::uint32_t now, Point officerPosition, Point point, bool visible) noexcept;
    SearchTarget Target(std::uint32_t officer, std::uint32_t now, Point officerPosition) const noexcept;
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

struct TrafficInput {
    float nativeLimit;
    float cruiseSpeed;
    float wet;
    float fog;
    unsigned hour;
    std::uint32_t seed;
    bool recentCollision;
    bool sirenApproaching;
};
float TrafficLimit(const TrafficInput& input) noexcept;

struct CityActivity {
    float pedestrians;
    float cars;
};

CityActivity AmbientActivity(unsigned hour, unsigned weekday, unsigned region, float wet, float fog) noexcept;

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
