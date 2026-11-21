#pragma once

#include "StreetSystems.h"

namespace ulw {

enum class CityVenue : std::uint8_t { ATM = 0, Bench = 1, BusStop = 2, Food = 3, Shelter = 4, Display = 6, Park = 8, Steps = 9 };
enum class CrashReaction : std::uint8_t { Glance, Pause, Leave, Call };
enum class ResidentAction : std::uint8_t { Visit, Pause, Leave, Call };

bool IsCityVenue(unsigned effectType) noexcept;
std::optional<CityVenue> PreferredVenue(std::uint16_t seed, unsigned hour, unsigned weekday,
    unsigned region, float rain) noexcept;
CrashReaction ReactToStreetCrash(std::uint16_t seed, float distanceSquared) noexcept;
int RetailPrice(int nativePrice, unsigned region, unsigned section) noexcept;
int RepairPrice(unsigned region) noexcept;

struct VenueObservation {
    Point owner;
    Point point;
    std::uint16_t model;
    std::uint16_t region;
    CityVenue kind;
};

struct ResidentTask {
    std::uintptr_t parent;
    std::uintptr_t child;
    std::uint32_t started;
    ResidentAction action;
    Point focus;
};

class CityLife {
public:
    void ObserveVenue(const VenueObservation& venue, std::uint32_t now) noexcept;
    std::optional<VenueObservation> FindVenue(CityVenue kind, Point origin, unsigned region, std::uint32_t now) const noexcept;
    std::optional<Point> TaxiDestination(Point origin, unsigned region, std::uint32_t now) const noexcept;
    void ReserveVenue(const VenueObservation& venue, std::uint32_t now) noexcept;
    bool TakeVisitAttempt(std::uint32_t ped, std::uint32_t now, std::uint32_t frame) noexcept;
    bool CanCall(const CrashSite& crash, std::uint32_t now) const noexcept;
    void StartCall(const CrashSite& crash, std::uint32_t now) noexcept;
    void Start(std::uint32_t ped, const ResidentTask& task) noexcept;
    std::optional<ResidentTask> Current(std::uint32_t ped, std::uintptr_t parent, std::uintptr_t child) const noexcept;
    void Finish(std::uint32_t ped) noexcept;
    void Reset() noexcept;

private:
    struct Venue {
        VenueObservation observation{};
        std::uint32_t seenAt{};
        std::optional<std::uint32_t> reservedAt{};
        bool observed{};
    };
    struct Resident {
        std::uint32_t ped{};
        std::optional<std::uint32_t> lastOffer{};
        std::optional<ResidentTask> task{};
    };
    struct Call {
        std::uint32_t vehicle;
        std::uint32_t impact;
        std::uint32_t at;
    };
    std::array<Venue, 64> m_Venues{};
    std::array<Resident, 256> m_Residents{};
    std::optional<Call> m_LastCall{};
    std::size_t m_NextVenue{};
    std::uint32_t m_Frame{};
    unsigned m_Attempts{};
};

enum class TaxiStage : std::uint8_t { Pickup, Boarding, Driving, Dropoff, Leaving };
enum class TaxiAction : std::uint8_t { Wait, Board, Drive, FindStop, Unboard, Complete, Cancel };

struct TaxiSample {
    TaxiStage stage{};
    std::uint32_t elapsed{};
    bool passengerInCab{};
    bool stoppedAtCurb{};
    bool nearDestination{};
    bool ableToContinue{};
    bool exitClear{};
};

TaxiAction NextTaxiAction(const TaxiSample& sample) noexcept;

}
