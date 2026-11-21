#include "PoliceInteractions.h"
#include "StreetSystems.h"

#include <algorithm>
#include <cmath>

namespace ulw {
namespace {

constexpr std::uint32_t IncidentLifetime = 6300;

bool FinitePoint(Point point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

}

bool PoliceIncidents::TakeProbe(std::uint32_t observer, std::uint32_t now, std::uint32_t frame) noexcept {
    if (m_Frame != frame) {
        m_Frame = frame;
        m_ProbeCount = 0;
    }
    auto& probe = m_Probes[((observer >> 8) + (observer >> 31) * 31u) % m_Probes.size()];
    if (m_ProbeCount >= 4 || (probe.taken && probe.observer == observer && now - probe.at < 340)) {
        return false;
    }
    probe = {observer, now, true};
    ++m_ProbeCount;
    return true;
}

bool PoliceIncidents::Record(IncidentKind kind, std::uint32_t observer, Point point, std::uint32_t now,
    bool playerRelated) noexcept {
    if (!FinitePoint(point)) {
        return false;
    }
    if (kind == IncidentKind::GunshotSound) {
        // Hearing supplies a street segment to check, with a coarser location than eyesight.
        point.x = std::round(point.x / 7.0f) * 7.0f;
        point.y = std::round(point.y / 7.0f) * 7.0f;
    }
    for (const auto& report : m_Reports) {
        if (report.present && report.lead.kind == kind && report.playerRelated == playerRelated &&
            now - report.at < 1400 && DistanceSquared(report.lead.point, point) <= 5.0f * 5.0f) {
            return false;
        }
    }
    const auto delay = kind == IncidentKind::Witness ? 750u + ((observer >> 8) % 5u) * 120u : 0u;
    m_Reports[m_NextReport] = {{point, kind, observer}, now, delay, playerRelated, true};
    m_NextReport = (m_NextReport + 1) % m_Reports.size();
    return true;
}

std::optional<IncidentLead> PoliceIncidents::Nearby(Point officer, std::uint32_t now,
    bool playerRelatedOnly) const noexcept {
    const Report* selected{};
    float best = 0.0f;
    for (const auto& report : m_Reports) {
        const auto age = now - report.at;
        const auto distance = DistanceSquared(officer, report.lead.point);
        if (!report.present || age < report.delay || age >= IncidentLifetime ||
            (playerRelatedOnly && !report.playerRelated) || !(distance <= 58.0f * 58.0f) ||
            !(std::abs(officer.z - report.lead.point.z) < 5.0f)) {
            continue;
        }
        const auto reliability = report.lead.kind == IncidentKind::OfficerSight ? 1.4f :
            report.lead.kind == IncidentKind::Witness ? 1.2f : 0.9f;
        const auto score = reliability - static_cast<float>(age) / 8000.0f - distance / (58.0f * 58.0f);
        if (!selected || score > best) {
            selected = &report;
            best = score;
        }
    }
    return selected ? std::optional{selected->lead} : std::nullopt;
}

void PoliceIncidents::Reset() noexcept {
    *this = {};
}

unsigned DrivingEvidence(float speedSquared, bool severeCollision) noexcept {
    if (!std::isfinite(speedSquared) || speedSquared < 0.0f) {
        return 0;
    }
    return (speedSquared > 0.46f * 0.46f ? 2u : speedSquared > 0.36f * 0.36f ? 1u : 0u) +
        (severeCollision ? 2u : 0u);
}

bool DrivingSuspicion::Observe(std::uint32_t officer, std::uint32_t now, Point point, unsigned evidence) noexcept {
    auto& record = m_Officers[(officer >> 8) % m_Officers.size()];
    if (!record.present || record.officer != officer || now - record.at >= 3600) {
        const auto lastWarning = record.present && record.officer == officer ? record.warnedAt : std::nullopt;
        record = {officer, now, 0, point, lastWarning, true};
    }
    record.at = now;
    if (!evidence || !FinitePoint(point)) {
        record.amount -= std::min(record.amount, 2u);
        return false;
    }
    record.point = point;
    record.amount = std::min(8u, record.amount + std::min(evidence, 4u));
    if (record.amount < 4 || (record.warnedAt && now - *record.warnedAt < 14000)) {
        return false;
    }
    record.warnedAt = now;
    return true;
}

std::optional<Point> DrivingSuspicion::Follow(std::uint32_t officer, std::uint32_t now) const noexcept {
    const auto& record = m_Officers[(officer >> 8) % m_Officers.size()];
    return record.present && record.officer == officer && record.amount >= 6 && now - record.at < 2400
        ? std::optional{record.point} : std::nullopt;
}

void DrivingSuspicion::Reset() noexcept {
    m_Officers = {};
}

bool SurrenderInput::Update(std::uint32_t now, bool held, bool eligible) noexcept {
    if (!held || !eligible) {
        Reset();
        return false;
    }
    if (!m_HeldAt || now - m_LastSample > 360) {
        Reset();
        m_HeldAt = now;
    }
    m_LastSample = now;
    return !m_Committed && now - *m_HeldAt >= 700;
}

bool SurrenderInput::Claim(std::uint32_t officer, std::uint32_t now) noexcept {
    if (m_Committed || !m_HeldAt || now - *m_HeldAt < 700 || now - m_LastSample > 360 ||
        (m_Officer && *m_Officer != officer && now - m_ClaimAt < 760)) {
        return false;
    }
    m_Officer = officer;
    m_ClaimAt = now;
    return true;
}

void SurrenderInput::Commit() noexcept {
    m_Committed = true;
}

void SurrenderInput::Reset() noexcept {
    *this = {};
}

}
