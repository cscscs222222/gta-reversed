#pragma once

#include "IntelligenceCore.h"

#include <optional>

namespace ulw {

enum class IncidentKind : std::uint8_t { GunshotSound, Witness, OfficerSight, Crash };

struct IncidentLead {
    Point point;
    IncidentKind kind;
    std::uint32_t observer;
};

class PoliceIncidents {
public:
    bool TakeProbe(std::uint32_t observer, std::uint32_t now, std::uint32_t frame) noexcept;
    bool Record(IncidentKind kind, std::uint32_t observer, Point point, std::uint32_t now, bool playerRelated) noexcept;
    std::optional<IncidentLead> Nearby(Point officer, std::uint32_t now, bool playerRelatedOnly) const noexcept;
    void Reset() noexcept;

private:
    struct Probe {
        std::uint32_t observer{};
        std::uint32_t at{};
        bool taken{};
    };
    struct Report {
        IncidentLead lead{};
        std::uint32_t at{};
        std::uint32_t delay{};
        bool playerRelated{};
        bool present{};
    };
    std::array<Probe, 128> m_Probes{};
    std::array<Report, 24> m_Reports{};
    std::size_t m_NextReport{};
    std::uint32_t m_Frame{};
    unsigned m_ProbeCount{};
};

struct FootInspection {
    std::uintptr_t parent{}, child{};
    Point scene{};
    std::uint32_t started{};
    bool looking{};
};

class FootPatrols {
public:
    bool CanInspect(std::uint32_t officer, Point origin, const IncidentLead& lead, std::uint32_t now) const noexcept;
    void Assign(std::uint32_t officer, FootInspection inspection) noexcept;
    std::optional<FootInspection> Current(std::uint32_t officer, std::uintptr_t parent, std::uintptr_t child) const noexcept;
    void Clear(std::uint32_t officer) noexcept;
    void Reset() noexcept;

private:
    struct Patrol {
        std::uint32_t officer{};
        FootInspection inspection{};
        std::optional<std::uint32_t> assignedAt{};
    };
    std::array<Patrol, 48> m_Patrols{};
};

unsigned DrivingEvidence(float speedSquared, bool severeCollision) noexcept;

class DrivingSuspicion {
public:
    bool Observe(std::uint32_t officer, std::uint32_t now, Point point, unsigned evidence) noexcept;
    std::optional<Point> Follow(std::uint32_t officer, std::uint32_t now) const noexcept;
    void Reset() noexcept;

private:
    struct Observation {
        std::uint32_t officer{};
        std::uint32_t at{};
        unsigned amount{};
        Point point{};
        std::optional<std::uint32_t> warnedAt{};
        bool present{};
    };
    std::array<Observation, 64> m_Officers{};
};

class SurrenderInput {
public:
    bool Update(std::uint32_t now, bool held, bool eligible) noexcept;
    bool Claim(std::uint32_t officer, std::uint32_t now) noexcept;
    void Commit() noexcept;
    void Reset() noexcept;

private:
    std::optional<std::uint32_t> m_HeldAt{};
    std::uint32_t m_LastSample{};
    std::optional<std::uint32_t> m_Officer{};
    std::uint32_t m_ClaimAt{};
    bool m_Committed{};
};

}
