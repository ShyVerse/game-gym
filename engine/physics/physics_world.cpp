#include "physics/physics_world.h"
#include "physics/physics_layers.h"
#include "ecs/components.h"

#include <Jolt/Jolt.h>

JPH_SUPPRESS_WARNINGS

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>

#include <flecs.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <thread>

using namespace JPH;
using namespace JPH::literals;

namespace gg {

// Jolt global init / shutdown (reference counted for multiple PhysicsWorld instances)
static int s_jolt_ref_count = 0;

static void jolt_trace_impl(const char* inFMT, ...) {
    va_list args;
    va_start(args, inFMT);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), inFMT, args);
    va_end(args);
    // Intentionally silent — suppress log spam in tests
    (void)buf;
}

static void jolt_global_init() {
    if (s_jolt_ref_count++ == 0) {
        RegisterDefaultAllocator();
        // Install a no-op Trace handler so DummyTrace's assert is never reached
        Trace = jolt_trace_impl;
        Factory::sInstance = new Factory();
        RegisterTypes();
    }
}

static void jolt_global_shutdown() {
    if (--s_jolt_ref_count == 0) {
        UnregisterTypes();
        delete Factory::sInstance;
        Factory::sInstance = nullptr;
    }
}

// Contact listener — buffers events, thread-safe
class ContactListenerImpl final : public ContactListener {
public:
    ValidateResult OnContactValidate(
        const Body& /*body1*/, const Body& /*body2*/,
        RVec3Arg /*baseOffset*/,
        const CollideShapeResult& /*result*/) override
    {
        return ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(
        const Body& body1, const Body& body2,
        const ContactManifold& manifold,
        ContactSettings& /*settings*/) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ContactEvent ev{};
        ev.body_id_a = body1.GetID().GetIndexAndSequenceNumber();
        ev.body_id_b = body2.GetID().GetIndexAndSequenceNumber();
        ev.type      = ContactType::Begin;
        ev.point     = {
            static_cast<float>(manifold.mBaseOffset.GetX()),
            static_cast<float>(manifold.mBaseOffset.GetY()),
            static_cast<float>(manifold.mBaseOffset.GetZ())
        };
        ev.normal = {
            manifold.mWorldSpaceNormal.GetX(),
            manifold.mWorldSpaceNormal.GetY(),
            manifold.mWorldSpaceNormal.GetZ()
        };
        events_.push_back(ev);
    }

    void OnContactPersisted(
        const Body& body1, const Body& body2,
        const ContactManifold& /*manifold*/,
        ContactSettings& /*settings*/) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ContactEvent ev{};
        ev.body_id_a = body1.GetID().GetIndexAndSequenceNumber();
        ev.body_id_b = body2.GetID().GetIndexAndSequenceNumber();
        ev.type      = ContactType::Persist;
        events_.push_back(ev);
    }

    void OnContactRemoved(const SubShapeIDPair& pair) override {
        std::lock_guard<std::mutex> lock(mutex_);
        ContactEvent ev{};
        ev.body_id_a = pair.GetBody1ID().GetIndexAndSequenceNumber();
        ev.body_id_b = pair.GetBody2ID().GetIndexAndSequenceNumber();
        ev.type      = ContactType::End;
        events_.push_back(ev);
    }

    std::vector<ContactEvent> drain() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto result = std::move(events_);
        events_.clear();
        return result;
    }

private:
    std::mutex                mutex_;
    std::vector<ContactEvent> events_;
};

// PhysicsWorld::Impl — owns all Jolt objects
struct PhysicsWorld::Impl {
    PhysicsConfig config;
    std::unique_ptr<TempAllocatorImpl>   temp_allocator;
    std::unique_ptr<JobSystemThreadPool> job_system;
    BPLayerInterfaceImpl       bp_layer_interface;
    ObjectVsBPLayerFilterImpl  obj_vs_bp_filter;
    ObjectLayerPairFilterImpl  obj_pair_filter;
    ContactListenerImpl        contact_listener;
    std::unique_ptr<PhysicsSystem> system;
    std::vector<ContactEvent>  frame_events;

    static EMotionType to_jolt_motion(MotionType mt) {
        switch (mt) {
            case MotionType::Static:    return EMotionType::Static;
            case MotionType::Dynamic:   return EMotionType::Dynamic;
            case MotionType::Kinematic: return EMotionType::Kinematic;
        }
        return EMotionType::Static;
    }

    static ObjectLayer to_jolt_layer(PhysicsLayer pl) {
        return static_cast<ObjectLayer>(pl);
    }
};

PhysicsWorld::PhysicsWorld() : impl_(std::make_unique<Impl>()) {}

PhysicsWorld::~PhysicsWorld() {
    impl_->system.reset();
    impl_->job_system.reset();
    impl_->temp_allocator.reset();
    jolt_global_shutdown();
}

std::unique_ptr<PhysicsWorld> PhysicsWorld::create(const PhysicsConfig& config) {
    jolt_global_init();

    auto pw = std::unique_ptr<PhysicsWorld>(new PhysicsWorld());
    auto& impl = *pw->impl_;
    impl.config = config;

    impl.temp_allocator = std::make_unique<TempAllocatorImpl>(10 * 1024 * 1024);

    const auto num_threads = std::max(1u, std::thread::hardware_concurrency() - 1);
    impl.job_system = std::make_unique<JobSystemThreadPool>(
        cMaxPhysicsJobs, cMaxPhysicsBarriers, static_cast<int>(num_threads));

    const uint32_t max_bodies              = config.max_bodies;
    const uint32_t num_body_mutexes        = 0;
    const uint32_t max_body_pairs          = max_bodies * 2;
    const uint32_t max_contact_constraints = max_bodies;

    impl.system = std::make_unique<PhysicsSystem>();
    impl.system->Init(
        max_bodies, num_body_mutexes, max_body_pairs, max_contact_constraints,
        impl.bp_layer_interface, impl.obj_vs_bp_filter, impl.obj_pair_filter);

    impl.system->SetGravity(JPH::Vec3(config.gravity.x, config.gravity.y, config.gravity.z));
    impl.system->SetContactListener(&impl.contact_listener);

    return pw;
}

void PhysicsWorld::step(float dt) {
    const int collision_steps = impl_->config.substeps;
    impl_->system->Update(dt, collision_steps,
        impl_->temp_allocator.get(), impl_->job_system.get());
    impl_->frame_events = impl_->contact_listener.drain();
}

void PhysicsWorld::step_with_ecs(float dt, flecs::world& ecs) {
    // Pre-step: sync dirty ECS transforms -> Jolt bodies
    ecs.each([this](flecs::entity, const Transform& t, RigidBody& rb) {
        if (rb.sync_to_physics && rb.body_id != UINT32_MAX) {
            auto& bi = impl_->system->GetBodyInterface();
            bi.SetPositionAndRotation(
                BodyID(rb.body_id),
                RVec3(t.position.x, t.position.y, t.position.z),
                JPH::Quat(t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w),
                EActivation::Activate);
            rb.sync_to_physics = false;
        }
    });

    step(dt);

    // Post-step: sync Jolt -> ECS for dynamic/kinematic bodies
    auto& bi = impl_->system->GetBodyInterface();
    ecs.each([&bi](flecs::entity, Transform& t, const RigidBody& rb) {
        if (rb.body_id == UINT32_MAX) return;
        BodyID jolt_id(rb.body_id);
        if (bi.GetMotionType(jolt_id) == EMotionType::Static) return;

        RVec3        pos = bi.GetPosition(jolt_id);
        JPH::Quat    rot = bi.GetRotation(jolt_id);

        t.position = {
            static_cast<float>(pos.GetX()),
            static_cast<float>(pos.GetY()),
            static_cast<float>(pos.GetZ())
        };
        t.rotation = {rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW()};
    });
}

uint32_t PhysicsWorld::add_body(const Vec3& position, const gg::Quat& rotation, const BodyDef& def) {
    RefConst<Shape> shape;
    if (const auto* box = std::get_if<BoxShapeDesc>(&def.shape)) {
        shape = new BoxShape(JPH::Vec3(box->half_x, box->half_y, box->half_z));
    } else if (const auto* sphere = std::get_if<SphereShapeDesc>(&def.shape)) {
        shape = new SphereShape(sphere->radius);
    } else if (const auto* capsule = std::get_if<CapsuleShapeDesc>(&def.shape)) {
        shape = new CapsuleShape(capsule->half_height, capsule->radius);
    }

    BodyCreationSettings settings(
        shape,
        RVec3(position.x, position.y, position.z),
        JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
        Impl::to_jolt_motion(def.motion_type),
        Impl::to_jolt_layer(def.layer));

    settings.mFriction    = def.friction;
    settings.mRestitution = def.restitution;
    settings.mIsSensor    = def.is_sensor;

    auto& bi    = impl_->system->GetBodyInterface();
    Body* body  = bi.CreateBody(settings);
    if (!body) return UINT32_MAX;

    const EActivation activation = (def.motion_type == MotionType::Static)
        ? EActivation::DontActivate
        : EActivation::Activate;

    bi.AddBody(body->GetID(), activation);
    return body->GetID().GetIndexAndSequenceNumber();
}

void PhysicsWorld::remove_body(uint32_t body_id) {
    if (body_id == UINT32_MAX) return;
    auto& bi     = impl_->system->GetBodyInterface();
    BodyID jolt_id(body_id);
    bi.RemoveBody(jolt_id);
    bi.DestroyBody(jolt_id);
}

void PhysicsWorld::set_position(uint32_t body_id, const Vec3& pos) {
    if (body_id == UINT32_MAX) return;
    impl_->system->GetBodyInterface().SetPosition(
        BodyID(body_id), RVec3(pos.x, pos.y, pos.z), EActivation::Activate);
}

Vec3 PhysicsWorld::get_position(uint32_t body_id) const {
    if (body_id == UINT32_MAX) return {};
    RVec3 p = impl_->system->GetBodyInterface().GetPosition(BodyID(body_id));
    return {
        static_cast<float>(p.GetX()),
        static_cast<float>(p.GetY()),
        static_cast<float>(p.GetZ())
    };
}

gg::Quat PhysicsWorld::get_rotation(uint32_t body_id) const {
    if (body_id == UINT32_MAX) return {};
    JPH::Quat q = impl_->system->GetBodyInterface().GetRotation(BodyID(body_id));
    return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()};
}

bool PhysicsWorld::raycast(const Vec3& origin, const Vec3& direction,
                           float max_distance, RayHit& out_hit) const
{
    RRayCast ray(
        RVec3(origin.x, origin.y, origin.z),
        JPH::Vec3(direction.x, direction.y, direction.z) * max_distance);

    RayCastResult result;
    const bool hit = impl_->system->GetNarrowPhaseQuery().CastRay(ray, result);

    if (hit) {
        out_hit.body_id  = result.mBodyID.GetIndexAndSequenceNumber();
        out_hit.fraction = result.mFraction;
        RVec3 hit_point  = ray.mOrigin + ray.mDirection * result.mFraction;
        out_hit.point = {
            static_cast<float>(hit_point.GetX()),
            static_cast<float>(hit_point.GetY()),
            static_cast<float>(hit_point.GetZ())
        };
    }

    return hit;
}

const std::vector<ContactEvent>& PhysicsWorld::contact_events() const {
    return impl_->frame_events;
}

} // namespace gg
