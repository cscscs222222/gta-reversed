#include "CityLife.h"

#include <algorithm>
#include <cmath>

namespace ulw {

int RepairPrice(unsigned region) noexcept {
    constexpr std::array prices{85, 100, 112, 124};
    return region < prices.size() ? prices[region] : 100;
}

int RetailPrice(int nativePrice, unsigned region, unsigned section) noexcept {
    if (nativePrice <= 0 || nativePrice > 50000 || region > 3) {
        return nativePrice;
    }
    constexpr std::array<int, 4> vehicle{93, 100, 109, 115};
    constexpr std::array<int, 4> personal{92, 100, 108, 119};
    constexpr std::array<int, 4> meals{88, 100, 109, 117};
    int percent = 100;
    if (section == 1 || section == 2) {
        percent = vehicle[region];
    } else if (section >= 4 && section <= 7) {
        percent = personal[region];
    } else if (section == 8) {
        percent = meals[region];
    }
    return std::max(1, (nativePrice * percent + 50) / 100);
}

namespace {

unsigned Personality(std::uint16_t seed) noexcept {
    return ((static_cast<std::uint32_t>(seed) * 0x45d9f3bu) >> 16) % 100;
}

bool FinitePoint(Point point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool SameVenue(const VenueObservation& left, const VenueObservation& right) noexcept {
    return left.model == right.model && left.region == right.region && left.kind == right.kind &&
        DistanceSquared(left.owner, right.owner) < 0.6f * 0.6f &&
        DistanceSquared(left.point, right.point) < 0.8f * 0.8f;
}

}

bool IsCityVenue(unsigned effectType) noexcept {
    return effectType <= 4 || effectType == 6 || effectType == 8 || effectType == 9;
}

std::optional<CityVenue> PreferredVenue(std::uint16_t seed, unsigned hour, unsigned weekday,
    unsigned region, float rain) noexcept {
    if (hour >= 24 || weekday < 1 || weekday > 7 || region > 4) {
        return std::nullopt;
    }
    const auto temperament = Personality(seed);
    if (rain >= 0.25f) {
        return temperament < 48 ? std::optional{CityVenue::Shelter} : std::nullopt;
    }
    if (weekday < 6 && ((hour >= 7 && hour < 10) || (hour >= 16 && hour < 19)) && temperament < 40) {
        return CityVenue::BusStop;
    }
    if (((hour >= 11 && hour < 15) || (hour >= 17 && hour < 21) || (region == 3 && hour < 3)) &&
        temperament < 27) {
        return CityVenue::Food;
    }
    if (hour >= 8 && hour < 18 && temperament >= 75 && temperament < 85) {
        return temperament >= 81 ? CityVenue::ATM : CityVenue::Park;
    }
    if (hour >= 14 && hour < 20 && temperament < 44) {
        return temperament >= 33 ? CityVenue::Steps : CityVenue::Bench;
    }
    if (hour >= 10 && hour < 21 && temperament >= 90 && region != 4) {
        return CityVenue::Display;
    }
    return std::nullopt;
}

CrashReaction ReactToStreetCrash(std::uint16_t seed, float distanceSquared) noexcept {
    const auto temperament = Personality(seed);
    if (distanceSquared < 3.0f * 3.0f || temperament < 18) {
        return CrashReaction::Leave;
    }
    if (temperament >= 68 && temperament < 84 && distanceSquared >= 6.0f * 6.0f) {
        return CrashReaction::Call;
    }
    return temperament >= 37 && temperament < 62 ? CrashReaction::Pause : CrashReaction::Glance;
}

void CityLife::ObserveVenue(const VenueObservation& venue, std::uint32_t now) noexcept {
    if (!FinitePoint(venue.point) || !FinitePoint(venue.owner) ||
        !IsCityVenue(static_cast<unsigned>(venue.kind)) || venue.region > 4) {
        return;
    }
    for (auto& entry : m_Venues) {
        if (entry.observed && SameVenue(entry.observation, venue)) {
            entry.seenAt = now;
            return;
        }
    }
    m_Venues[m_NextVenue] = {venue, now, std::nullopt, true};
    m_NextVenue = (m_NextVenue + 1) % m_Venues.size();
}

std::optional<VenueObservation> CityLife::FindVenue(CityVenue kind, Point origin, unsigned region, std::uint32_t now) const noexcept {
    auto nearest = 34.0f * 34.0f;
    std::optional<VenueObservation> found;
    for (const auto& entry : m_Venues) {
        const auto& candidate = entry.observation;
        if (!entry.observed || candidate.kind != kind || candidate.region != region || now - entry.seenAt >= 30000 ||
            (entry.reservedAt && now - *entry.reservedAt < 10000) || std::abs(candidate.point.z - origin.z) > 2.8f) {
            continue;
        }
        const auto distance = DistanceSquared(candidate.point, origin);
        if (distance >= 4.0f * 4.0f && distance < nearest) {
            found = candidate;
            nearest = distance;
        }
    }
    return found;
}

std::optional<Point> CityLife::TaxiDestination(Point origin, unsigned region, std::uint32_t now) const noexcept {
    float nearest = 160.0f * 160.0f;
    std::optional<Point> result;
    for (const auto& venue : m_Venues) {
        if (!venue.observed || venue.observation.region != region || now - venue.seenAt >= 30000 ||
            !(std::abs(venue.observation.point.z - origin.z) < 5.0f)) {
            continue;
        }
        const auto kind = venue.observation.kind;
        if (kind != CityVenue::BusStop && kind != CityVenue::Food && kind != CityVenue::ATM && kind != CityVenue::Park) {
            continue;
        }
        const auto distance = DistanceSquared(origin, venue.observation.point);
        if (distance >= 44.0f * 44.0f && distance < nearest) {
            nearest = distance;
            result = venue.observation.point;
        }
    }
    return result;
}

TaxiAction NextTaxiAction(const TaxiSample& sample) noexcept {
    if (!sample.ableToContinue) {
        return TaxiAction::Cancel;
    }
    switch (sample.stage) {
    case TaxiStage::Pickup:
        if (sample.elapsed > 10000) {
            return TaxiAction::Cancel;
        }
        return sample.stoppedAtCurb && !sample.passengerInCab ? TaxiAction::Board : TaxiAction::Wait;
    case TaxiStage::Boarding:
        if (sample.passengerInCab && sample.stoppedAtCurb) {
            return TaxiAction::Drive;
        }
        return sample.elapsed > 7200 ? TaxiAction::Cancel : TaxiAction::Wait;
    case TaxiStage::Driving:
        if (!sample.passengerInCab || sample.elapsed > 44500) {
            return TaxiAction::Cancel;
        }
        return sample.nearDestination || sample.elapsed > 32000 ? TaxiAction::FindStop : TaxiAction::Wait;
    case TaxiStage::Dropoff:
        if (!sample.passengerInCab || sample.elapsed > 10000) {
            return TaxiAction::Cancel;
        }
        return sample.stoppedAtCurb ? TaxiAction::Unboard : TaxiAction::Wait;
    case TaxiStage::Leaving:
        if (!sample.passengerInCab && sample.exitClear) {
            return TaxiAction::Complete;
        }
        return sample.elapsed > 7200 ? TaxiAction::Cancel : TaxiAction::Wait;
    }
    return TaxiAction::Cancel;
}

void CityLife::ReserveVenue(const VenueObservation& venue, std::uint32_t now) noexcept {
    for (auto& entry : m_Venues) {
        if (entry.observed && SameVenue(entry.observation, venue)) {
            entry.reservedAt = now;
            return;
        }
    }
}

bool CityLife::TakeVisitAttempt(std::uint32_t ped, std::uint32_t now, std::uint32_t frame) noexcept {
    if (m_Frame != frame) {
        m_Frame = frame;
        m_Attempts = 0;
    }
    if (m_Attempts >= 2) {
        return false;
    }
    auto& resident = m_Residents[(ped >> 8) % m_Residents.size()];
    if (resident.ped != ped) {
        resident = {ped, std::nullopt, std::nullopt};
    }
    if (resident.task || (resident.lastOffer && now - *resident.lastOffer < 22000)) {
        return false;
    }
    resident.lastOffer = now;
    ++m_Attempts;
    return true;
}

bool CityLife::CanCall(const CrashSite& crash, std::uint32_t now) const noexcept {
    return !m_LastCall || ((m_LastCall->vehicle != crash.vehicle || m_LastCall->impact != crash.impactAt) &&
        now - m_LastCall->at >= 12000);
}

void CityLife::StartCall(const CrashSite& crash, std::uint32_t now) noexcept {
    m_LastCall = Call{crash.vehicle, crash.impactAt, now};
}

void CityLife::Start(std::uint32_t ped, const ResidentTask& task) noexcept {
    auto& resident = m_Residents[(ped >> 8) % m_Residents.size()];
    if (resident.ped != ped) {
        resident = {ped, task.started, task};
    } else {
        resident.task = task;
    }
}

std::optional<ResidentTask> CityLife::Current(std::uint32_t ped, std::uintptr_t parent, std::uintptr_t child) const noexcept {
    const auto& resident = m_Residents[(ped >> 8) % m_Residents.size()];
    return resident.ped == ped && resident.task && resident.task->parent == parent && resident.task->child == child
        ? resident.task : std::nullopt;
}

void CityLife::Finish(std::uint32_t ped) noexcept {
    auto& resident = m_Residents[(ped >> 8) % m_Residents.size()];
    if (resident.ped == ped) {
        resident.task.reset();
    }
}

void CityLife::Reset() noexcept {
    *this = {};
}

}
