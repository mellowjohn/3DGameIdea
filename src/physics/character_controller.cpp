#include "engine/physics/character_controller.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cmath>

namespace engine {
namespace {
using namespace JPH;

namespace L { constexpr ObjectLayer Static = 0, Dynamic = 1, Character = 2, Trigger = 3; }
namespace BP { constexpr BroadPhaseLayer Static(0), Moving(1), Trigger(2); }

EngineError character_error(std::string code, std::string message) {
    return {std::move(code), Severity::Error, ErrorCategory::Physics, "character-controller", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, "Check capsule dimensions, spawn position, and collision world state.",
            make_correlation_id()};
}

class CharacterBroadPhaseFilter final : public BroadPhaseLayerFilter {
public:
    bool ShouldCollide(BroadPhaseLayer inLayer) const override {
        return inLayer == BP::Static || inLayer == BP::Moving;
    }
};

class CharacterObjectLayerFilter final : public ObjectLayerFilter {
public:
    bool ShouldCollide(ObjectLayer inLayer) const override {
        return inLayer == L::Static || inLayer == L::Dynamic;
    }
};

class CharacterBodyFilter final : public BodyFilter {
public:
    bool ShouldCollide(const BodyID&) const override { return true; }
};

CharacterBroadPhaseFilter g_bp_filter;
CharacterObjectLayerFilter g_object_filter;
CharacterBodyFilter g_body_filter;
ShapeFilter g_shape_filter;

PhysicsSystem& physics_from(CollisionWorld& world) {
    return *static_cast<PhysicsSystem*>(world.character_physics_context().physics_system);
}

TempAllocatorImpl& temp_from(CollisionWorld& world) {
    return *static_cast<TempAllocatorImpl*>(world.character_physics_context().temp_allocator);
}
} // namespace

struct CharacterController::Impl {
    CharacterControllerConfig config;
    std::unique_ptr<CharacterVirtual> character;
    float capsule_radius = 0.35f;
    float capsule_half_height = 0.85f;
    bool jump_requested = false;
};

CharacterController::CharacterController(CollisionWorld& world) : world_(&world) {}
CharacterController::~CharacterController() = default;
CharacterController::CharacterController(CharacterController&&) noexcept = default;
CharacterController& CharacterController::operator=(CharacterController&&) noexcept = default;

Result<CharacterController> CharacterController::create(CollisionWorld& world, WorldPosition spawn,
    const CharacterControllerConfig& config) {
    if (!(config.capsule_radius > 0) || !(config.capsule_half_height > 0))
        return Result<CharacterController>::failure(
            character_error("CHARACTER-SHAPE-INVALID", "Capsule radius and half height must be positive"));
    if (!(config.max_slope_ratio > 0) || !(config.step_height > 0) || !(config.max_speed > 0) || !(config.gravity > 0) ||
        !(config.jump_velocity > 0))
        return Result<CharacterController>::failure(
            character_error("CHARACTER-CONFIG-INVALID", "Movement configuration values must be positive"));

    CharacterVirtualSettings settings;
    settings.mMaxSlopeAngle = std::atan(config.max_slope_ratio);
    settings.mInnerBodyLayer = L::Character;
    settings.mInnerBodyShape = new CapsuleShape(config.capsule_half_height, config.capsule_radius);
    settings.mShape = new CapsuleShape(config.capsule_half_height, config.capsule_radius);
    settings.mMass = 70.0f;

    CharacterController controller(world);
    controller.impl_ = std::make_unique<Impl>();
    controller.impl_->config = config;
    controller.impl_->capsule_radius = config.capsule_radius;
    controller.impl_->capsule_half_height = config.capsule_half_height;
    controller.impl_->character = std::make_unique<CharacterVirtual>(
        &settings, RVec3(static_cast<Real>(spawn.x), static_cast<Real>(spawn.y), static_cast<Real>(spawn.z)), Quat::sIdentity(),
        &physics_from(world));

    controller.impl_->character->RefreshContacts(g_bp_filter, g_object_filter, g_body_filter, g_shape_filter,
        temp_from(world));
    return Result<CharacterController>::success(std::move(controller));
}

Result<void> CharacterController::move(const LocalPosition& wish_velocity, float yaw_radians, float seconds) {
    if (!impl_ || !impl_->character)
        return Result<void>::failure(character_error("CHARACTER-NOT-READY", "Character controller is not initialized"));
    if (!world_)
        return Result<void>::failure(character_error("CHARACTER-NOT-READY", "Character controller lost collision world"));
    if (!(seconds > 0) || seconds > 0.25f)
        return Result<void>::failure(character_error("CHARACTER-STEP-INVALID", "Step must be within (0, 0.25] seconds"));

    const float forward_x = std::sin(yaw_radians);
    const float forward_z = std::cos(yaw_radians);
    const float right_x = std::cos(yaw_radians);
    const float right_z = -std::sin(yaw_radians);
    Vec3 desired{
        right_x * wish_velocity.x + forward_x * wish_velocity.z,
        wish_velocity.y,
        right_z * wish_velocity.x + forward_z * wish_velocity.z};

    const float horizontal = std::sqrt(desired.GetX() * desired.GetX() + desired.GetZ() * desired.GetZ());
    if (horizontal > 0.0f) {
        const float target_speed = horizontal <= 1.0f ? horizontal * impl_->config.max_speed : impl_->config.max_speed;
        const float scale = target_speed / horizontal;
        desired.SetX(desired.GetX() * scale);
        desired.SetZ(desired.GetZ() * scale);
    }

    Vec3 velocity = impl_->character->GetLinearVelocity();
    if (impl_->character->GetGroundState() == CharacterBase::EGroundState::OnGround &&
        impl_->character->GetGroundVelocity().Dot(impl_->character->GetUp()) <= 0.0f) {
        velocity = impl_->character->GetGroundVelocity();
    }
    velocity.SetX(desired.GetX());
    velocity.SetZ(desired.GetZ());
    if (impl_->character->GetGroundState() == CharacterBase::EGroundState::OnGround) {
        if (impl_->jump_requested) {
            velocity.SetY(impl_->config.jump_velocity);
            impl_->jump_requested = false;
        } else {
            velocity.SetY(desired.GetY());
        }
    } else {
        velocity.SetY(velocity.GetY() - impl_->config.gravity * seconds);
        impl_->jump_requested = false;
    }
    impl_->character->SetLinearVelocity(velocity);

    CharacterVirtual::ExtendedUpdateSettings update_settings;
    update_settings.mStickToFloorStepDown = Vec3(0, -0.5f, 0);
    update_settings.mWalkStairsStepUp = Vec3(0, impl_->config.step_height, 0);
    update_settings.mWalkStairsMinStepForward = 0.02f;
    update_settings.mWalkStairsStepForwardTest = 0.15f;

    impl_->character->ExtendedUpdate(seconds, Vec3(0, -impl_->config.gravity, 0), update_settings, g_bp_filter,
        g_object_filter, g_body_filter, g_shape_filter, temp_from(*world_));
    return Result<void>::success();
}

Result<bool> CharacterController::jump() {
    if (!impl_ || !impl_->character)
        return Result<bool>::failure(character_error("CHARACTER-NOT-READY", "Character controller is not initialized"));
    if (!on_ground()) return Result<bool>::success(false);
    impl_->jump_requested = true;
    return Result<bool>::success(true);
}

WorldPosition CharacterController::position() const {
    if (!impl_ || !impl_->character) return {};
    const RVec3 p = impl_->character->GetPosition();
    return {p.GetX(), p.GetY(), p.GetZ()};
}

CellCoord CharacterController::owner_cell(const WorldPartition& partition) const {
    const auto cell = partition.cell_for(position());
    return cell ? cell.value() : CellCoord{};
}

bool CharacterController::on_ground() const {
    return impl_ && impl_->character &&
           impl_->character->GetGroundState() == CharacterBase::EGroundState::OnGround;
}

bool CharacterController::on_steep_ground() const {
    return impl_ && impl_->character &&
           impl_->character->GetGroundState() == CharacterBase::EGroundState::OnSteepGround;
}

CollisionDebugBody CharacterController::debug_body() const {
    CollisionDebugBody entry;
    entry.layer = CollisionLayer::Character;
    entry.shape = CollisionDebugShape::Capsule;
    entry.position = position();
    entry.radius = impl_ ? impl_->capsule_radius : 0.35f;
    entry.half_extent = {impl_ ? impl_->capsule_half_height : 0.85f, 0.0f, 0.0f};
    entry.sensor = false;
    return entry;
}

CharacterControllerConfig CharacterController::config() const {
    return impl_ ? impl_->config : CharacterControllerConfig{};
}

std::array<float, 3> CharacterController::linear_velocity() const {
    if (!impl_ || !impl_->character) return {0.0f, 0.0f, 0.0f};
    const Vec3 velocity = impl_->character->GetLinearVelocity();
    return {velocity.GetX(), velocity.GetY(), velocity.GetZ()};
}

void CharacterController::set_position(WorldPosition position) {
    if (!impl_ || !impl_->character || !world_) return;
    impl_->character->SetPosition(
        RVec3(static_cast<Real>(position.x), static_cast<Real>(position.y), static_cast<Real>(position.z)));
    impl_->character->RefreshContacts(g_bp_filter, g_object_filter, g_body_filter, g_shape_filter, temp_from(*world_));
}

} // namespace engine
