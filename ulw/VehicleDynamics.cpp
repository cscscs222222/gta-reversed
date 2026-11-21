#include "VehicleDynamics.h"

#include <algorithm>
#include <cmath>

namespace ulw {
namespace {

float Unit(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

struct GripProfile {
    float loss;
    float wetLoss;
    float drag;
};

constexpr std::array<GripProfile, 7> GripBySurface{{
    {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.10f, 0.06f, 0.065f},
    {0.10f, 0.13f, 0.08f}, {0.19f, 0.15f, 0.20f}, {0.17f, 0.05f, 0.24f},
    {0.16f, 0.12f, 0.14f}
}};

}

SurfaceContact ClassifySurface(std::uint32_t flags, std::uint32_t useFlags) noexcept {
    // These are the material, wheel-particle and audio flags of the contacted surface.
    const auto adhesion = flags & 7u;
    const auto roughness = (flags >> 23) & 3u;
    if (adhesion == 5 || (flags & 0x18000u) || (useFlags & 0x8000u)) {
        return {RoadMaterial::Wet, roughness};
    }
    if (adhesion == 4 || (flags & 0x4000u) || (useFlags & 0x1020u)) {
        return {RoadMaterial::Sand, roughness};
    }
    if (useFlags & 8u) {
        return {RoadMaterial::Mud, roughness};
    }
    if (useFlags & 0x20802u) {
        return {RoadMaterial::Grass, roughness};
    }
    if (adhesion == 3 || (useFlags & 0x2004u)) {
        return {RoadMaterial::Gravel, roughness};
    }
    return {adhesion == 1 || adhesion == 2 ? RoadMaterial::Paved : RoadMaterial::Unknown, roughness};
}

RoadVehicle ClassifyRoadVehicle(std::uint16_t model, float mass, bool offRoad, bool motorcycle) noexcept {
    if (motorcycle) {
        return RoadVehicle::Motorcycle;
    }
    if (model == 431 || model == 437 || mass >= 3100.0f) {
        return RoadVehicle::Heavy;
    }
    if (offRoad || model == 422 || model == 470 || model == 489 || model == 495 || model == 554 || model == 579) {
        return RoadVehicle::Utility;
    }
    constexpr std::array<std::uint16_t, 8> sports{411, 415, 429, 451, 477, 506, 541, 565};
    if (std::find(sports.begin(), sports.end(), model) != sports.end()) {
        return RoadVehicle::Sport;
    }
    return mass < 1250.0f ? RoadVehicle::Compact : RoadVehicle::Standard;
}

bool FrontWheel(int wheel) noexcept {
    return wheel == 0 || wheel == 2 || wheel == 4;
}

bool WheelDriven(DrivenAxle axle, int wheel) noexcept {
    return wheel >= 0 && wheel < 6 &&
        (axle == DrivenAxle::Both || (axle == DrivenAxle::Front && FrontWheel(wheel)) ||
            (axle == DrivenAxle::Rear && !FrontWheel(wheel)));
}

float UpdateTyreTemperature(TyreTemperature& state, std::uint32_t now, const WheelContact& contact,
    float wet, bool sliding) noexcept {
    if (!state.sampled || now - state.at > 30000) {
        state = {0.48f, now, true};
    }
    const auto water = Unit(wet);
    const auto elapsed = now - state.at;
    const auto milliseconds = static_cast<float>(std::min(elapsed, 100u));
    const auto working = Unit(std::abs(contact.speed) / 0.45f);
    const auto slidingHeat = sliding ? 0.00020f : 0.000075f * Unit(std::abs(contact.slip) / 0.16f);
    const auto cooling = (state.value - (0.44f - 0.24f * water)) * (0.00020f + 0.00012f * water);
    const auto idle = static_cast<float>(elapsed - std::min(elapsed, 100u));
    state.value = Unit(state.value + milliseconds * (working * 0.000034f + slidingHeat - cooling) -
        idle * (state.value - 0.38f) * 0.000035f);
    state.at = now;
    return state.value;
}

WheelForces DynamicWheelResponse(WheelForces native, const WheelContact& contact, float wet, float throttle,
    float brakePedal, float temperature, bool burst) noexcept {
    if (contact.wheel < 0 || contact.wheel >= 6 ||
        !std::isfinite(contact.speed) || !std::isfinite(contact.slip)) {
        return native;
    }
    const auto index = static_cast<unsigned>(contact.surface.material);
    const auto material = index < GripBySurface.size() ? GripBySurface[index] : GripBySurface.front();
    const auto utility = contact.vehicle == RoadVehicle::Utility ? 0.72f : 1.0f;
    const auto heavy = contact.vehicle == RoadVehicle::Heavy ? 1.18f : 1.0f;
    const auto speed = Unit(std::abs(contact.speed) / 0.65f);
    const auto load = Unit(brakePedal) - 0.52f * Unit(throttle);
    const auto front = FrontWheel(contact.wheel);
    const auto water = Unit(wet);
    const auto tyreLoad = 0.78f + 0.22f * Unit((1.0f - contact.compression) * 1.5f);
    const auto transfer = load * (front ? 0.06f : -0.06f) * speed * tyreLoad;
    if (!burst) {
        const auto lossToSlip = material.loss * 0.30f * Unit(std::abs(contact.slip) / 0.20f);
        const auto heatLoss = 0.15f * Unit((temperature - 0.80f) / 0.20f) +
            0.055f * Unit((0.34f - temperature) / 0.34f);
        const auto puddles = (contact.surface.material == RoadMaterial::Wet ? 0.10f : 0.044f) * water * speed;
        const auto response = 1.0f + transfer - utility * (material.loss + water * material.wetLoss + lossToSlip) -
            heatLoss - puddles;
        native.grip *= std::clamp(response, 0.60f, 1.07f);
    }
    native.thrust *= 1.0f - material.drag * utility * heavy * (1.0f - 0.62f * speed);
    if (native.brake > 0.0f) {
        native.brake *= 1.0f + 0.60f * transfer - 0.22f * material.loss;
    }
    if (!contact.handbrake && !burst && WheelDriven(contact.drive, contact.wheel) && contact.speed > 0.10f &&
        std::abs(throttle) < 0.04f && brakePedal < 0.12f && native.grip > 0.0f) {
        const auto drag = contact.vehicle == RoadVehicle::Heavy ? 0.11f : 0.065f;
        native.brake += native.grip * drag * Unit((contact.speed - 0.10f) / 0.30f);
    }
    return native;
}

SuspensionSpring TuneSpring(SuspensionSpring native, RoadVehicle vehicle, float travel, float wet,
    float brake, float throttle, bool front) noexcept {
    if (!(native.force > 0.0f && native.bias > 0.0f && native.bias <= 1.0f && travel >= 0.0f && travel <= 1.0f)) {
        return native;
    }
    const auto bump = Unit((0.46f - travel) / 0.46f);
    const auto firmness = vehicle == RoadVehicle::Sport ? 0.20f : vehicle == RoadVehicle::Utility ? 0.16f : 0.13f;
    const auto pitch = (Unit(brake) - 0.5f * Unit(throttle)) * (front ? 0.08f : -0.08f);
    return {native.force * (1.0f + bump * bump * firmness),
        std::clamp(native.bias * (1.0f + pitch * (1.0f - 0.2f * Unit(wet))), 0.01f, 1.0f)};
}

float TuneDamping(float native, RoadVehicle vehicle, SurfaceContact surface, float compressionSpeed) noexcept {
    if (!(native > 0.0f) || !std::isfinite(compressionSpeed)) {
        return native;
    }
    const auto impact = Unit(std::abs(compressionSpeed) / 0.14f);
    const auto rough = static_cast<float>(std::min(surface.roughness, 3u)) / 3.0f;
    const auto model = vehicle == RoadVehicle::Sport ? 0.13f : vehicle == RoadVehicle::Utility ? 0.04f : 0.08f;
    const auto response = compressionSpeed >= 0.0f ? 1.0f + impact * (model - rough * 0.12f)
        : 1.0f + impact * (0.12f + 0.045f * rough);
    return native * response;
}

float ShapeThrottle(float throttle, RoadVehicle vehicle, float grade) noexcept {
    if (!(throttle > 0.0f && throttle <= 1.0f) || !std::isfinite(grade)) {
        return throttle;
    }
    const auto climb = Unit(grade / 0.26f);
    const auto progressive = vehicle == RoadVehicle::Sport ? 0.20f : vehicle == RoadVehicle::Heavy ? 0.16f : 0.09f;
    const auto hillLoad = vehicle == RoadVehicle::Heavy ? 0.13f : vehicle == RoadVehicle::Compact ? 0.09f : 0.055f;
    return throttle * (1.0f - (1.0f - throttle) * progressive - hillLoad * climb);
}

bool TuneShiftBands(std::array<GearBand, 6>& gears, unsigned count, RoadVehicle vehicle,
    float throttle, float grade) noexcept {
    if (count < 2 || count >= gears.size() || !(throttle >= 0.0f && throttle <= 1.0f) || !std::isfinite(grade)) {
        return false;
    }
    const auto late = vehicle == RoadVehicle::Sport ? 0.11f : 0.06f;
    const auto early = vehicle == RoadVehicle::Heavy ? 0.09f : 0.055f;
    const auto scale = 1.0f - early * (1.0f - throttle) + late * throttle + 0.04f * Unit(grade / 0.20f);
    auto adjusted = gears;
    for (unsigned i = 1; i < count; ++i) {
        const auto& current = gears[i];
        const auto& next = gears[i + 1];
        if (!(current.maximum > current.up && current.up > next.down && next.down >= 0.0f &&
                next.up > current.up && current.up > current.down) ||
            !std::isfinite(current.maximum) || !std::isfinite(next.up)) {
            return false;
        }
        const auto margin = (current.up - next.down) * 0.40f;
        adjusted[i].up = std::clamp(current.up * scale, next.down + margin, current.maximum);
        adjusted[i + 1].down = std::min(next.down * scale, adjusted[i].up - margin);
    }
    for (unsigned i = 1; i < count; ++i) {
        if (!(adjusted[i].up < adjusted[i + 1].up && adjusted[i].up > adjusted[i].down)) {
            return false;
        }
    }
    gears = adjusted;
    return true;
}

Aircraft ClassifyAircraft(std::uint16_t model, bool helicopter) noexcept {
    if (helicopter) {
        return model == 417 || model == 425 || model == 548 ? Aircraft::HeavyHelicopter : Aircraft::Helicopter;
    }
    switch (model) {
    case 476: // Rustler
    case 513: // Stuntplane
        return Aircraft::Aerobatic;
    case 519: // Shamal
    case 520: // Hydra in conventional flight
        return Aircraft::Jet;
    case 553: // Nevada
    case 577: // AT-400
    case 592: // Andromada
        return Aircraft::Transport;
    default:
        return Aircraft::Light;
    }
}

FlightTuning TuneFlight(Aircraft aircraft, float forwardSpeed, float altitude, float clearance,
    bool groundContact) noexcept {
    if (groundContact || !std::isfinite(forwardSpeed) || !std::isfinite(altitude) ||
        !(clearance >= 0.0f) || !std::isfinite(clearance)) {
        return {};
    }
    const bool rotary = aircraft == Aircraft::Helicopter || aircraft == Aircraft::HeavyHelicopter;
    // Blend out near the runway or helipad, including low-speed lift-off and hover.
    const auto airborne = Unit(clearance / (rotary ? 12.0f : 19.0f));
    const auto speed = Unit((forwardSpeed - (rotary ? 0.02f : 0.23f)) / (rotary ? 0.46f : 0.62f));
    const auto high = Unit((altitude - 180.0f) / 280.0f);
    if (rotary) {
        const bool heavy = aircraft == Aircraft::HeavyHelicopter;
        const auto weight = heavy ? 1.0f : 0.66f;
        const auto control = airborne * (0.45f + 0.55f * speed) * weight;
        return {1.0f - 0.18f * control, 1.0f - 0.15f * control, 1.0f - 0.13f * control,
            1.0f + 0.16f * airborne * speed, 1.0f - airborne * speed * weight * (0.025f + 0.025f * high)};
    }
    const auto weight = airborne * speed;
    const auto sweep = Unit((forwardSpeed - 0.72f) / 0.78f);
    FlightTuning result{};
    switch (aircraft) {
    case Aircraft::Aerobatic:
        result = {1.0f + weight * (0.09f - 0.10f * sweep), 1.0f + weight * (0.13f - 0.14f * sweep),
            1.0f + weight * (0.07f - 0.16f * sweep), 1.0f + 0.07f * weight};
        break;
    case Aircraft::Jet:
        result = {1.0f - weight * (0.11f + 0.10f * sweep), 1.0f - weight * 0.09f * sweep,
            1.0f - weight * 0.15f * sweep, 1.0f + weight * 0.10f * sweep};
        break;
    case Aircraft::Transport:
        result = {1.0f - weight * 0.22f, 1.0f - weight * 0.19f,
            1.0f - weight * (0.11f + 0.07f * sweep), 1.0f + 0.14f * weight};
        break;
    default:
        result = {1.0f + weight * (0.04f - 0.13f * sweep), 1.0f - weight * 0.09f * sweep,
            1.0f - weight * 0.11f * sweep, 1.0f + 0.08f * weight};
        break;
    }
    result.pitch -= 0.075f * high * weight;
    return result;
}

Hull ClassifyHull(std::uint16_t model) noexcept {
    switch (model) {
    case 446: // Squalo
    case 452: // Speeder
    case 493: // Jetmax
        return Hull::Speedboat;
    case 453: // Reefer
    case 454: // Tropic
    case 484: // Marquis
        return Hull::Displacement;
    default:
        return Hull::Utility;
    }
}

HullTuning TuneHull(Hull hull, float forwardSpeed, float immersed, float verticalSpeed, float throttle) noexcept {
    if (!(forwardSpeed > 0.04f) || !std::isfinite(forwardSpeed) || !(immersed > 0.015f && immersed <= 1.20f) ||
        !std::isfinite(verticalSpeed) || !std::isfinite(throttle)) {
        return {};
    }
    const auto contact = Unit((immersed - 0.015f) / 0.28f);
    const auto motion = Unit((forwardSpeed - 0.04f) / 0.65f);
    const auto wave = Unit(std::abs(verticalSpeed) / 0.16f);
    const auto load = contact * motion * (1.0f - 0.58f * wave);
    const auto planing = contact * Unit((forwardSpeed - 0.30f) / 0.52f) * Unit(throttle) * (1.0f - wave);
    switch (hull) {
    case Hull::Speedboat:
        return {1.0f - 0.09f * load, 1.0f - 0.13f * load, 1.0f + 0.11f * load,
            1.0f + 0.085f * planing};
    case Hull::Displacement:
        return {1.0f + 0.18f * load, 1.0f + 0.16f * load, 1.0f + 0.19f * load,
            1.0f - 0.08f * planing};
    default:
        return {1.0f + 0.05f * load, 1.0f + 0.09f * load, 1.0f + 0.08f * load,
            1.0f + 0.025f * planing};
    }
}

float TuneResistance(float nativeRetention, float dragScale) noexcept {
    if (!(nativeRetention > 0.0f && nativeRetention < 1.0f && dragScale >= 0.75f && dragScale <= 1.25f) ||
        dragScale == 1.0f) {
        return nativeRetention;
    }
    return std::pow(nativeRetention, dragScale);
}

}
