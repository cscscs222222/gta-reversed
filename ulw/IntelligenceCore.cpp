#include "IntelligenceCore.h"

#include <cmath>

namespace ulw {

bool IntelligenceCore::TakeBudget(std::uint32_t frame) noexcept {
    if (frame != m_Frame) {
        m_Frame = frame;
        m_Decisions = 0;
    }
    if (m_Decisions >= DecisionsPerFrame) {
        return false;
    }
    ++m_Decisions;
    return true;
}

Decision IntelligenceCore::ReactToShot(const Shot& shot) noexcept {
    constexpr std::uint32_t recallMs = 3000;
    constexpr std::uint32_t cooldownMs = 500;
    const auto distance = shot.distanceSquared;
    if (!std::isfinite(distance) || distance < 0.0f || distance > 42.0f * 42.0f) {
        return {Response::Vanilla, {}};
    }

    // The reference includes the pool generation. Use the slot to spread
    // records, and compare the full reference when reading one back.
    const auto poolSlot = shot.pedRef >> 8;
    auto& memory = m_Memory[poolSlot % m_Memory.size()];
    const auto age = shot.now - memory.seenAt;
    const bool recent = memory.occupied && memory.pedRef == shot.pedRef && age < recallMs;
    if (recent && age < cooldownMs && distance > 8.0f * 8.0f) {
        return {Response::Vanilla, {}};
    }

    // Derive a stable temperament for the lifetime of this character.
    const auto temperament = ((shot.pedRef ^ (shot.pedRef >> 11)) * 0x45d9f3bu >> 24) % 3;
    const float nearDistance = temperament == 0 ? 22.0f : 14.0f;
    const bool close = distance <= nearDistance * nearDistance;
    auto response = Response::Flee;
    if (close && (shot.visible || distance <= 10.0f * 10.0f || temperament == 0)) {
        response = Response::Panic;
    } else if (!close && temperament != 0 && (!shot.visible || temperament == 2)) {
        response = Response::Look;
    }
    if (recent && response == Response::Look) {
        response = memory.state == Response::Panic ? Response::Panic : Response::Flee;
    }

    memory = {shot.pedRef, shot.now, shot.source, response, true};
    return {memory.state, memory.focus};
}

Decision IntelligenceCore::ReactToHazard(const Shot& hazard) noexcept {
    if (!(hazard.distanceSquared >= 0.0f && hazard.distanceSquared <= 18.0f * 18.0f) ||
        (!hazard.visible && hazard.distanceSquared > 6.0f * 6.0f)) {
        return {Response::Vanilla, {}};
    }
    auto& memory = m_Memory[(hazard.pedRef >> 8) % m_Memory.size()];
    if (memory.occupied && memory.pedRef == hazard.pedRef && hazard.now - memory.seenAt < 850) {
        return {Response::Vanilla, {}};
    }
    const auto response = hazard.distanceSquared <= 7.0f * 7.0f ? Response::Panic : Response::Flee;
    memory = {hazard.pedRef, hazard.now, hazard.source, response, true};
    return {response, hazard.source};
}

void IntelligenceCore::Reset() noexcept {
    m_Memory = {};
    m_Frame = 0;
    m_Decisions = 0;
}

}
