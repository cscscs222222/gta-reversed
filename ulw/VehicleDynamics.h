#pragma once

#include <array>
#include <cstdint>

namespace ulw {

enum class RoadMaterial : std::uint8_t { Unknown, Paved, Gravel, Grass, Mud, Sand, Wet };
enum class RoadVehicle : std::uint8_t { Compact, Standard, Sport, Utility, Heavy, Motorcycle };
enum class DrivenAxle : std::uint8_t { Unknown, Front, Rear, Both };

struct SurfaceContact {
    RoadMaterial material{RoadMaterial::Unknown};
    unsigned roughness{};
};

struct WheelForces {
    float thrust;
    float brake;
    float grip;
};

struct WheelContact {
    SurfaceContact surface{};
    RoadVehicle vehicle{RoadVehicle::Standard};
    DrivenAxle drive{DrivenAxle::Unknown};
    int wheel{-1}; // Four automobile wheels followed by the two motorcycle wheels.
    float compression{};
    float speed{};
    float slip{};
    bool handbrake{};
};

struct TyreTemperature {
    float value{0.48f};
    std::uint32_t at{};
    bool sampled{};
};

SurfaceContact ClassifySurface(std::uint32_t flags, std::uint32_t useFlags) noexcept;
RoadVehicle ClassifyRoadVehicle(std::uint16_t model, float mass, bool offRoad, bool motorcycle) noexcept;
bool FrontWheel(int wheel) noexcept;
bool WheelDriven(DrivenAxle axle, int wheel) noexcept;
float UpdateTyreTemperature(TyreTemperature& state, std::uint32_t now, const WheelContact& contact,
    float wet, bool sliding) noexcept;
WheelForces DynamicWheelResponse(WheelForces native, const WheelContact& contact, float wet, float throttle,
    float brakePedal, float temperature, bool burst) noexcept;

struct SuspensionSpring {
    float force;
    float bias;
};

SuspensionSpring TuneSpring(SuspensionSpring native, RoadVehicle vehicle, float travel, float wet,
    float brake, float throttle, bool front) noexcept;
float TuneDamping(float native, RoadVehicle vehicle, SurfaceContact surface, float compressionSpeed) noexcept;

struct GearBand {
    float maximum;
    float up;
    float down;
};

float ShapeThrottle(float throttle, RoadVehicle vehicle, float grade) noexcept;
bool TuneShiftBands(std::array<GearBand, 6>& gears, unsigned count, RoadVehicle vehicle,
    float throttle, float grade) noexcept;

enum class Aircraft : std::uint8_t { Light, Aerobatic, Jet, Transport, Helicopter, HeavyHelicopter };

struct FlightTuning {
    float yaw{1.0f};
    float roll{1.0f};
    float pitch{1.0f};
    float sideSlip{1.0f};
    float thrust{1.0f};
};

Aircraft ClassifyAircraft(std::uint16_t model, bool helicopter) noexcept;
FlightTuning TuneFlight(Aircraft aircraft, float forwardSpeed, float altitude, float clearance,
    bool groundContact) noexcept;

enum class Hull : std::uint8_t { Speedboat, Utility, Displacement };

struct HullTuning {
    float longitudinalDrag{1.0f};
    float lateralDrag{1.0f};
    float yawDrag{1.0f};
    float planing{1.0f};
};

Hull ClassifyHull(std::uint16_t model) noexcept;
HullTuning TuneHull(Hull hull, float forwardSpeed, float immersed, float verticalSpeed, float throttle) noexcept;
float TuneResistance(float nativeRetention, float dragScale) noexcept;

}
