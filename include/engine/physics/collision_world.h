#pragma once
#include "engine/world/world_partition.h"
#include "engine/world/terrain.h"
#include "engine/assets/material_asset.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
namespace engine {
enum class CollisionLayer : std::uint8_t { StaticWorld, Dynamic, Character, Trigger };
struct CollisionBody { std::uint32_t value = 0xffffffffu; [[nodiscard]] bool valid() const { return value != 0xffffffffu; } };
struct RayHit { CollisionBody body; float fraction = 0; WorldPosition position; };
struct OverlapHit {
    CollisionBody body;
    CollisionLayer layer = CollisionLayer::StaticWorld;
    WorldPosition contact_point;
    LocalPosition normal;
};
struct SweepHit {
    CollisionBody body;
    CollisionLayer layer = CollisionLayer::StaticWorld;
    WorldPosition contact_point;
    LocalPosition normal;
    float fraction = 0.0f;
};
struct CollisionQueryFilter { std::optional<CollisionLayer> layer; };
enum class ContactEventType : std::uint8_t { Enter, Exit };
struct ContactEvent {
    ContactEventType type = ContactEventType::Enter;
    CollisionBody body_a;
    CollisionBody body_b;
    CollisionLayer layer_a = CollisionLayer::StaticWorld;
    CollisionLayer layer_b = CollisionLayer::StaticWorld;
    std::optional<WorldPosition> contact_point;
};
enum class CollisionDebugShape : std::uint8_t { Box, Sphere, Capsule, Heightfield };
struct CollisionDebugBody {
    CollisionBody body;
    CollisionLayer layer = CollisionLayer::StaticWorld;
    CollisionDebugShape shape = CollisionDebugShape::Box;
    WorldPosition position;
    LocalPosition half_extent;
    float radius = 0.0f;
    bool sensor = false;
};
class CollisionWorld final {
public:
    CollisionWorld();
    ~CollisionWorld();
    CollisionWorld(const CollisionWorld&) = delete;
    CollisionWorld& operator=(const CollisionWorld&) = delete;
    [[nodiscard]] Result<CollisionBody> add_box(WorldPosition position, LocalPosition half_extent, CollisionLayer layer,
        bool dynamic, std::optional<CellCoord> owner = {},
        const std::array<float, 4>& rotation = {0.0f, 0.0f, 0.0f, 1.0f});
    [[nodiscard]] Result<CollisionBody> add_sphere(WorldPosition position, float radius, CollisionLayer layer, bool dynamic,
        std::optional<CellCoord> owner = {},
        const std::array<float, 4>& rotation = {0.0f, 0.0f, 0.0f, 1.0f});
    [[nodiscard]] Result<CollisionBody> add_heightfield(const TerrainMesh& terrain, const PhysicalMaterialProperties& material = {},
        std::optional<CellCoord> owner = {});
    [[nodiscard]] Result<void> remove(CollisionBody body);
    [[nodiscard]] Result<WorldPosition> position(CollisionBody body) const;
    void unload_cell(CellCoord cell);
    [[nodiscard]] Result<void> step(float seconds);
    [[nodiscard]] Result<std::optional<RayHit>> ray_cast(WorldPosition origin, LocalPosition direction) const;
    [[nodiscard]] Result<std::vector<OverlapHit>> overlap_sphere(WorldPosition center, float radius,
        const CollisionQueryFilter& filter = {}) const;
    [[nodiscard]] Result<std::vector<OverlapHit>> overlap_box(WorldPosition center, LocalPosition half_extent,
        const CollisionQueryFilter& filter = {},
        const std::array<float, 4>& rotation = {0.0f, 0.0f, 0.0f, 1.0f}) const;
    [[nodiscard]] Result<std::optional<SweepHit>> sweep_sphere(WorldPosition origin, LocalPosition direction, float radius,
        const CollisionQueryFilter& filter = {}) const;
    [[nodiscard]] std::vector<ContactEvent> drain_contact_events();
    [[nodiscard]] std::vector<CollisionDebugBody> debug_bodies() const;
    [[nodiscard]] std::size_t body_count() const;
    struct CharacterPhysicsContext {
        void* physics_system = nullptr;
        void* temp_allocator = nullptr;
    };
    [[nodiscard]] CharacterPhysicsContext character_physics_context() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace engine
