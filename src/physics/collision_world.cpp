#include "engine/physics/collision_world.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <utility>
namespace engine {
namespace {
using namespace JPH;
namespace L { constexpr ObjectLayer Static = 0, Dynamic = 1, Character = 2, Trigger = 3, Count = 4; }
namespace BP { constexpr BroadPhaseLayer Static(0), Moving(1), Trigger(2); constexpr uint Count = 3; }
ObjectLayer layer(CollisionLayer v) { return static_cast<ObjectLayer>(v); }
CollisionLayer from_layer(ObjectLayer v) { return static_cast<CollisionLayer>(v); }
EngineError error(std::string code, std::string message) {
    return {std::move(code), Severity::Error, ErrorCategory::Physics, "collision", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, "Check shape dimensions, body capacity, and collision diagnostics.",
            make_correlation_id()};
}
void init_jolt() {
    static std::once_flag once;
    std::call_once(once, [] {
        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();
    });
}
struct BodyInfo {
    CollisionLayer layer = CollisionLayer::StaticWorld;
    CollisionDebugShape shape = CollisionDebugShape::Box;
    LocalPosition half_extent{};
    float radius = 0.0f;
    bool sensor = false;
};
std::pair<std::uint32_t, std::uint32_t> contact_key(std::uint32_t a, std::uint32_t b) {
    return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
}
class BPMap final : public BroadPhaseLayerInterface {
public:
    BPMap() {
        m[L::Static] = BP::Static;
        m[L::Dynamic] = BP::Moving;
        m[L::Character] = BP::Moving;
        m[L::Trigger] = BP::Trigger;
    }
    uint GetNumBroadPhaseLayers() const override { return BP::Count; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer v) const override { return m[v]; }
    BroadPhaseLayer m[L::Count];
};
class PairFilter final : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
        if (a == L::Static) return b == L::Dynamic || b == L::Character;
        if (a == L::Trigger) return b == L::Dynamic || b == L::Character;
        return true;
    }
};
class BPFilter final : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override {
        if (a == L::Static) return b == BP::Moving;
        if (a == L::Trigger) return b == BP::Moving;
        return true;
    }
};
class QueryObjectLayerFilter final : public ObjectLayerFilter {
public:
    explicit QueryObjectLayerFilter(std::optional<CollisionLayer> layer) : layer_(layer) {}
    bool ShouldCollide(ObjectLayer inLayer) const override {
        return !layer_ || from_layer(inLayer) == *layer_;
    }
private:
    std::optional<CollisionLayer> layer_;
};
RMat44 overlap_transform(WorldPosition center, const std::array<float, 4>& rotation) {
    return RMat44::sRotationTranslation(Quat(rotation[0], rotation[1], rotation[2], rotation[3]),
        RVec3(static_cast<Real>(center.x), static_cast<Real>(center.y), static_cast<Real>(center.z)));
}
} // namespace

using namespace JPH;

struct CollisionWorld::Impl {
    BPMap bp;
    BPFilter bp_filter;
    PairFilter pair_filter;
    TempAllocatorImpl temp{16 * 1024 * 1024};
    JobSystemThreadPool jobs{cMaxPhysicsJobs, cMaxPhysicsBarriers, std::max(1u, std::thread::hardware_concurrency() - 1)};
    PhysicsSystem physics;
    std::set<std::uint32_t> bodies;
    std::map<CellCoord, std::vector<BodyID>> cells;
    std::map<std::uint32_t, BodyInfo> body_info;
    std::set<std::pair<std::uint32_t, std::uint32_t>> active_contacts;
    std::vector<ContactEvent> pending_events;
    std::mutex event_mutex;
    class ContactBridge final : public ContactListener {
    public:
        explicit ContactBridge(Impl* owner_in) : owner(owner_in) {}
        ValidateResult OnContactValidate(const Body& inBody1, const Body& inBody2, RVec3Arg, const CollideShapeResult&) override {
            (void)inBody1;
            (void)inBody2;
            return ValidateResult::AcceptAllContactsForThisBodyPair;
        }
        void OnContactAdded(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold,
            ContactSettings&) override;
        void OnContactRemoved(const SubShapeIDPair& inSubShapePair) override;
    private:
        Impl* owner;
    } contact_bridge;
    Impl() : contact_bridge(this) {
        physics.Init(65536, 0, 65536, 16384, bp, bp_filter, pair_filter);
        physics.SetContactListener(&contact_bridge);
    }
    void queue_contact_event(ContactEventType type, std::uint32_t a, std::uint32_t b,
        const std::optional<WorldPosition>& point = std::nullopt);
    void emit_exit_for_body(std::uint32_t body);
    const BodyInfo* info_for(std::uint32_t body) const;
    std::vector<OverlapHit> collect_overlap_hits(const AllHitCollisionCollector<CollideShapeCollector>& collector,
        const CollisionQueryFilter& filter) const;
};
void CollisionWorld::Impl::ContactBridge::OnContactAdded(const Body& inBody1, const Body& inBody2,
    const ContactManifold& inManifold, ContactSettings&) {
    if (!owner) return;
    if (!inBody1.IsSensor() && !inBody2.IsSensor()) return;
    const auto a = inBody1.GetID().GetIndexAndSequenceNumber();
    const auto b = inBody2.GetID().GetIndexAndSequenceNumber();
    std::optional<WorldPosition> point;
    if (!inManifold.mRelativeContactPointsOn1.empty()) {
        const RVec3 world = inBody1.GetCenterOfMassTransform() * inManifold.mRelativeContactPointsOn1[0];
        point = WorldPosition{world.GetX(), world.GetY(), world.GetZ()};
    }
    std::lock_guard lock(owner->event_mutex);
    const auto key = contact_key(a, b);
    if (owner->active_contacts.insert(key).second)
        owner->queue_contact_event(ContactEventType::Enter, a, b, point);
}
void CollisionWorld::Impl::ContactBridge::OnContactRemoved(const SubShapeIDPair& inSubShapePair) {
    if (!owner) return;
    const auto a = inSubShapePair.GetBody1ID().GetIndexAndSequenceNumber();
    const auto b = inSubShapePair.GetBody2ID().GetIndexAndSequenceNumber();
    std::lock_guard lock(owner->event_mutex);
    const auto key = contact_key(a, b);
    if (owner->active_contacts.erase(key))
        owner->queue_contact_event(ContactEventType::Exit, a, b, std::nullopt);
}
void CollisionWorld::Impl::queue_contact_event(ContactEventType type, std::uint32_t a, std::uint32_t b,
    const std::optional<WorldPosition>& point) {
    const auto* info_a = info_for(a);
    const auto* info_b = info_for(b);
    ContactEvent event;
    event.type = type;
    event.body_a = {a};
    event.body_b = {b};
    event.layer_a = info_a ? info_a->layer : CollisionLayer::StaticWorld;
    event.layer_b = info_b ? info_b->layer : CollisionLayer::StaticWorld;
    event.contact_point = point;
    pending_events.push_back(event);
}
void CollisionWorld::Impl::emit_exit_for_body(std::uint32_t body) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> to_remove;
    for (const auto& pair : active_contacts) {
        if (pair.first == body || pair.second == body) to_remove.push_back(pair);
    }
    for (const auto& pair : to_remove) {
        active_contacts.erase(pair);
        queue_contact_event(ContactEventType::Exit, pair.first, pair.second, std::nullopt);
    }
}
const BodyInfo* CollisionWorld::Impl::info_for(std::uint32_t body) const {
    const auto found = body_info.find(body);
    return found == body_info.end() ? nullptr : &found->second;
}
std::vector<OverlapHit> CollisionWorld::Impl::collect_overlap_hits(
    const AllHitCollisionCollector<CollideShapeCollector>& collector, const CollisionQueryFilter& filter) const {
    std::vector<OverlapHit> hits;
    if (!collector.HadHit()) return hits;
    hits.reserve(collector.mHits.size());
    for (const auto& hit : collector.mHits) {
        const auto token = hit.mBodyID2.GetIndexAndSequenceNumber();
        if (!bodies.count(token)) continue;
        const auto* info = info_for(token);
        OverlapHit entry;
        entry.body = {token};
        entry.layer = info ? info->layer : CollisionLayer::StaticWorld;
        if (filter.layer && entry.layer != *filter.layer) continue;
        entry.contact_point = {hit.mContactPointOn2.GetX(), hit.mContactPointOn2.GetY(), hit.mContactPointOn2.GetZ()};
        entry.normal = {hit.mPenetrationAxis.GetX(), hit.mPenetrationAxis.GetY(), hit.mPenetrationAxis.GetZ()};
        hits.push_back(entry);
    }
    return hits;
}

CollisionWorld::CollisionWorld() {
    init_jolt();
    impl_ = std::make_unique<Impl>();
}
CollisionWorld::~CollisionWorld() {
    if (!impl_) return;
    auto& bi = impl_->physics.GetBodyInterface();
    for (auto v : impl_->bodies) {
        BodyID id(v);
        if (bi.IsAdded(id)) bi.RemoveBody(id);
        bi.DestroyBody(id);
    }
}
Result<CollisionBody> CollisionWorld::add_box(WorldPosition p, LocalPosition h, CollisionLayer l, bool dynamic,
    std::optional<CellCoord> owner, const std::array<float, 4>& rotation) {
    if (h.x <= 0 || h.y <= 0 || h.z <= 0)
        return Result<CollisionBody>::failure(error("COLLISION-SHAPE-INVALID", "Box extents must be positive"));
    auto& bi = impl_->physics.GetBodyInterface();
    BodyCreationSettings s(new BoxShape(Vec3(h.x, h.y, h.z)),
        RVec3(static_cast<Real>(p.x), static_cast<Real>(p.y), static_cast<Real>(p.z)),
        Quat(rotation[0], rotation[1], rotation[2], rotation[3]), dynamic ? EMotionType::Dynamic : EMotionType::Static,
        layer(l));
    if (l == CollisionLayer::Trigger) s.mIsSensor = true;
    BodyID id = bi.CreateAndAddBody(s, dynamic ? EActivation::Activate : EActivation::DontActivate);
    if (id.IsInvalid()) return Result<CollisionBody>::failure(error("COLLISION-BODY-CAPACITY", "Could not create box body"));
    const auto token = id.GetIndexAndSequenceNumber();
    impl_->bodies.insert(token);
    impl_->body_info[token] = {l, CollisionDebugShape::Box, h, 0.0f, l == CollisionLayer::Trigger};
    if (owner) impl_->cells[*owner].push_back(id);
    return Result<CollisionBody>::success({token});
}
Result<CollisionBody> CollisionWorld::add_sphere(WorldPosition p, float radius, CollisionLayer l, bool dynamic,
    std::optional<CellCoord> owner, const std::array<float, 4>& rotation) {
    if (!(radius > 0)) return Result<CollisionBody>::failure(error("COLLISION-SHAPE-INVALID", "Sphere radius must be positive"));
    auto& bi = impl_->physics.GetBodyInterface();
    BodyCreationSettings s(new SphereShape(radius),
        RVec3(static_cast<Real>(p.x), static_cast<Real>(p.y), static_cast<Real>(p.z)),
        Quat(rotation[0], rotation[1], rotation[2], rotation[3]), dynamic ? EMotionType::Dynamic : EMotionType::Static,
        layer(l));
    if (l == CollisionLayer::Trigger) s.mIsSensor = true;
    BodyID id = bi.CreateAndAddBody(s, dynamic ? EActivation::Activate : EActivation::DontActivate);
    if (id.IsInvalid()) return Result<CollisionBody>::failure(error("COLLISION-BODY-CAPACITY", "Could not create sphere body"));
    const auto token = id.GetIndexAndSequenceNumber();
    impl_->bodies.insert(token);
    impl_->body_info[token] = {l, CollisionDebugShape::Sphere, {}, radius, l == CollisionLayer::Trigger};
    if (owner) impl_->cells[*owner].push_back(id);
    return Result<CollisionBody>::success({token});
}
Result<CollisionBody> CollisionWorld::add_heightfield(const TerrainMesh& terrain, const PhysicalMaterialProperties& material,
    std::optional<CellCoord> owner) {
    if (terrain.resolution < 3 || terrain.heights.size() != static_cast<std::size_t>(terrain.resolution) * terrain.resolution ||
        !(terrain.cell_size > 0))
        return Result<CollisionBody>::failure(error("COLLISION-HEIGHTFIELD-INVALID", "Heightfield dimensions or samples are invalid"));
    if (!std::isfinite(material.friction) || material.friction < 0 || material.friction > 2 ||
        !std::isfinite(material.restitution) || material.restitution < 0 || material.restitution > 1)
        return Result<CollisionBody>::failure(error("COLLISION-MATERIAL-INVALID", "Friction or restitution is outside its supported range"));
    const float step = terrain.cell_size / static_cast<float>(terrain.resolution - 1);
    const float ox = static_cast<float>(terrain.coordinate.x) * terrain.cell_size - terrain.cell_size * 0.5f;
    const float oz = static_cast<float>(terrain.coordinate.z) * terrain.cell_size - terrain.cell_size * 0.5f;
    HeightFieldShapeSettings settings(terrain.heights.data(), Vec3(ox, 0, oz), Vec3(step, 1, step), terrain.resolution);
    auto shape = settings.Create();
    if (shape.HasError()) return Result<CollisionBody>::failure(error("COLLISION-HEIGHTFIELD-CREATE", shape.GetError().c_str()));
    auto& bi = impl_->physics.GetBodyInterface();
    BodyCreationSettings body_settings(shape.Get(), RVec3::sZero(), Quat::sIdentity(), EMotionType::Static,
        layer(CollisionLayer::StaticWorld));
    body_settings.mFriction = material.friction;
    body_settings.mRestitution = material.restitution;
    BodyID id = bi.CreateAndAddBody(body_settings, EActivation::DontActivate);
    if (id.IsInvalid()) return Result<CollisionBody>::failure(error("COLLISION-BODY-CAPACITY", "Could not create heightfield body"));
    const auto token = id.GetIndexAndSequenceNumber();
    impl_->bodies.insert(token);
    LocalPosition half{terrain.cell_size * 0.5f, 20.0f, terrain.cell_size * 0.5f};
    impl_->body_info[token] = {CollisionLayer::StaticWorld, CollisionDebugShape::Heightfield, half, 0.0f, false};
    if (owner) impl_->cells[*owner].push_back(id);
    return Result<CollisionBody>::success({token});
}
Result<void> CollisionWorld::remove(CollisionBody body) {
    if (!body.valid() || !impl_->bodies.erase(body.value))
        return Result<void>::failure(error("COLLISION-BODY-NOT-FOUND", "Body does not exist"));
    {
        std::lock_guard lock(impl_->event_mutex);
        impl_->emit_exit_for_body(body.value);
    }
    BodyID id(body.value);
    auto& bi = impl_->physics.GetBodyInterface();
    if (bi.IsAdded(id)) bi.RemoveBody(id);
    bi.DestroyBody(id);
    impl_->body_info.erase(body.value);
    for (auto& c : impl_->cells)
        c.second.erase(std::remove(c.second.begin(), c.second.end(), id), c.second.end());
    return Result<void>::success();
}
Result<WorldPosition> CollisionWorld::position(CollisionBody body) const {
    if (!body.valid() || !impl_->bodies.count(body.value))
        return Result<WorldPosition>::failure(error("COLLISION-BODY-NOT-FOUND", "Body does not exist"));
    auto p = impl_->physics.GetBodyInterface().GetPosition(BodyID(body.value));
    return Result<WorldPosition>::success({p.GetX(), p.GetY(), p.GetZ()});
}
void CollisionWorld::unload_cell(CellCoord cell) {
    auto found = impl_->cells.find(cell);
    if (found == impl_->cells.end()) return;
    auto ids = found->second;
    impl_->cells.erase(found);
    for (auto id : ids) (void)remove({id.GetIndexAndSequenceNumber()});
}
Result<void> CollisionWorld::step(float seconds) {
    if (!(seconds > 0) || seconds > 0.25f)
        return Result<void>::failure(error("PHYSICS-STEP-INVALID", "Step must be within (0, 0.25] seconds"));
    auto result = impl_->physics.Update(seconds, 1, &impl_->temp, &impl_->jobs);
    if (result != EPhysicsUpdateError::None)
        return Result<void>::failure(error("PHYSICS-UPDATE-FAILED", "Jolt reported an update error"));
    return Result<void>::success();
}
Result<std::optional<RayHit>> CollisionWorld::ray_cast(WorldPosition o, LocalPosition d) const {
    const float length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (!(length > 0)) return Result<std::optional<RayHit>>::failure(error("COLLISION-RAY-INVALID", "Ray direction must be non-zero"));
    RRayCast ray(RVec3(static_cast<Real>(o.x), static_cast<Real>(o.y), static_cast<Real>(o.z)), Vec3(d.x, d.y, d.z));
    RayCastResult hit;
    if (!impl_->physics.GetNarrowPhaseQuery().CastRay(ray, hit))
        return Result<std::optional<RayHit>>::success(std::nullopt);
    auto point = ray.GetPointOnRay(hit.mFraction);
    return Result<std::optional<RayHit>>::success(
        RayHit{{hit.mBodyID.GetIndexAndSequenceNumber()}, hit.mFraction, {point.GetX(), point.GetY(), point.GetZ()}});
}
Result<std::vector<OverlapHit>> CollisionWorld::overlap_sphere(WorldPosition center, float radius,
    const CollisionQueryFilter& filter) const {
    if (!(radius > 0))
        return Result<std::vector<OverlapHit>>::failure(error("COLLISION-OVERLAP-INVALID", "Sphere radius must be positive"));
    SphereShape shape(radius);
    const RMat44 transform = RMat44::sTranslation(RVec3(static_cast<Real>(center.x), static_cast<Real>(center.y),
        static_cast<Real>(center.z)));
    CollideShapeSettings settings;
    AllHitCollisionCollector<CollideShapeCollector> collector;
    QueryObjectLayerFilter layer_filter(filter.layer);
    impl_->physics.GetNarrowPhaseQuery().CollideShape(&shape, Vec3::sReplicate(1.0f), transform, settings,
        RVec3(static_cast<Real>(center.x), static_cast<Real>(center.y), static_cast<Real>(center.z)), collector, {},
        layer_filter);
    return Result<std::vector<OverlapHit>>::success(impl_->collect_overlap_hits(collector, filter));
}
Result<std::vector<OverlapHit>> CollisionWorld::overlap_box(WorldPosition center, LocalPosition half_extent,
    const CollisionQueryFilter& filter, const std::array<float, 4>& rotation) const {
    if (half_extent.x <= 0 || half_extent.y <= 0 || half_extent.z <= 0)
        return Result<std::vector<OverlapHit>>::failure(error("COLLISION-OVERLAP-INVALID", "Box half extents must be positive"));
    BoxShape shape(Vec3(half_extent.x, half_extent.y, half_extent.z));
    const auto transform = overlap_transform(center, rotation);
    CollideShapeSettings settings;
    AllHitCollisionCollector<CollideShapeCollector> collector;
    QueryObjectLayerFilter layer_filter(filter.layer);
    impl_->physics.GetNarrowPhaseQuery().CollideShape(&shape, Vec3::sReplicate(1.0f), transform, settings,
        transform.GetTranslation(), collector, {}, layer_filter);
    return Result<std::vector<OverlapHit>>::success(impl_->collect_overlap_hits(collector, filter));
}
Result<std::optional<SweepHit>> CollisionWorld::sweep_sphere(WorldPosition origin, LocalPosition direction, float radius,
    const CollisionQueryFilter& filter) const {
    if (!(radius > 0))
        return Result<std::optional<SweepHit>>::failure(error("COLLISION-SWEEP-INVALID", "Sphere radius must be positive"));
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (!(length > 0))
        return Result<std::optional<SweepHit>>::failure(error("COLLISION-SWEEP-INVALID", "Sweep direction must be non-zero"));
    SphereShape shape(radius);
    const RMat44 start =
        RMat44::sTranslation(RVec3(static_cast<Real>(origin.x), static_cast<Real>(origin.y), static_cast<Real>(origin.z)));
    const Vec3 sweep(direction.x, direction.y, direction.z);
    RShapeCast cast(&shape, Vec3::sReplicate(1.0f), start, sweep);
    ShapeCastSettings settings;
    ClosestHitCollisionCollector<CastShapeCollector> collector;
    QueryObjectLayerFilter layer_filter(filter.layer);
    impl_->physics.GetNarrowPhaseQuery().CastShape(cast, settings, start.GetTranslation(), collector, {}, layer_filter);
    if (!collector.HadHit()) return Result<std::optional<SweepHit>>::success(std::nullopt);
    const auto& hit = collector.mHit;
    const auto token = hit.mBodyID2.GetIndexAndSequenceNumber();
    if (!impl_->bodies.count(token)) return Result<std::optional<SweepHit>>::success(std::nullopt);
    const auto* info = impl_->info_for(token);
    SweepHit entry;
    entry.body = {token};
    entry.layer = info ? info->layer : CollisionLayer::StaticWorld;
    if (filter.layer && entry.layer != *filter.layer) return Result<std::optional<SweepHit>>::success(std::nullopt);
    entry.fraction = hit.mFraction;
    entry.contact_point = {hit.mContactPointOn2.GetX(), hit.mContactPointOn2.GetY(), hit.mContactPointOn2.GetZ()};
    entry.normal = {hit.mPenetrationAxis.GetX(), hit.mPenetrationAxis.GetY(), hit.mPenetrationAxis.GetZ()};
    return Result<std::optional<SweepHit>>::success(entry);
}
std::vector<ContactEvent> CollisionWorld::drain_contact_events() {
    std::lock_guard lock(impl_->event_mutex);
    auto events = std::move(impl_->pending_events);
    impl_->pending_events.clear();
    return events;
}
std::vector<CollisionDebugBody> CollisionWorld::debug_bodies() const {
    std::vector<CollisionDebugBody> out;
    out.reserve(impl_->bodies.size());
    const auto& bi = impl_->physics.GetBodyInterface();
    for (const auto token : impl_->bodies) {
        const auto* info = impl_->info_for(token);
        if (!info) continue;
        const auto p = bi.GetPosition(BodyID(token));
        CollisionDebugBody entry;
        entry.body = {token};
        entry.layer = info->layer;
        entry.shape = info->shape;
        entry.position = {p.GetX(), p.GetY(), p.GetZ()};
        entry.half_extent = info->half_extent;
        entry.radius = info->radius;
        entry.sensor = info->sensor;
        out.push_back(entry);
    }
    return out;
}
std::size_t CollisionWorld::body_count() const { return impl_->bodies.size(); }
CollisionWorld::CharacterPhysicsContext CollisionWorld::character_physics_context() const {
    return {&impl_->physics, &impl_->temp};
}
} // namespace engine
