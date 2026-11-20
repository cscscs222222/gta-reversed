#pragma once

#include <array>
#include <cstdint>

namespace ulw {

struct Point {
    float x;
    float y;
    float z;
};

enum class Response : std::uint8_t {
    Vanilla,
    Look,
    Flee,
    Panic,
};

struct Shot {
    std::uint32_t pedRef;
    std::uint32_t now;
    Point source;
    float distanceSquared;
    bool visible;
};

struct Decision {
    Response response;
    Point focus;
};

class IntelligenceCore {
public:
    static constexpr unsigned DecisionsPerFrame = 6;

    bool TakeBudget(std::uint32_t frame) noexcept;
    Decision ReactToShot(const Shot& shot) noexcept;
    void Reset() noexcept;

private:
    struct Memory {
        std::uint32_t pedRef{};
        std::uint32_t seenAt{};
        Point focus{};
        Response state{};
        bool occupied{};
    };

    std::array<Memory, 128> m_Memory{};
    std::uint32_t m_Frame{};
    unsigned m_Decisions{};
};

}
