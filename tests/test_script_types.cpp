#include "ecs/components.h"
#include "physics/physics_components.h"
#include "script/script_types_manual.h"
#include "script_types_gen.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace gg;
using namespace gg::script_types;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Vec3 tests
// ---------------------------------------------------------------------------

TEST(ScriptTypesTest, Vec3ToJson) {
    const Vec3 v{1.0f, 2.0f, 3.0f};
    const auto j = to_json(v);

    EXPECT_FLOAT_EQ(j.at("x").get<float>(), 1.0f);
    EXPECT_FLOAT_EQ(j.at("y").get<float>(), 2.0f);
    EXPECT_FLOAT_EQ(j.at("z").get<float>(), 3.0f);
}

TEST(ScriptTypesTest, Vec3FromJson) {
    const auto j = json{{"x", 4.0f}, {"y", 5.0f}, {"z", 6.0f}};
    const auto v = vec3_from_json(j);

    EXPECT_FLOAT_EQ(v.x, 4.0f);
    EXPECT_FLOAT_EQ(v.y, 5.0f);
    EXPECT_FLOAT_EQ(v.z, 6.0f);
}

TEST(ScriptTypesTest, Vec3FromJsonMissingFieldsDefaultToZero) {
    const auto j = json{{"y", 7.0f}};
    const auto v = vec3_from_json(j);

    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 7.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(ScriptTypesTest, Vec3RoundTrip) {
    const Vec3 original{-1.5f, 0.0f, 99.9f};
    const auto restored = vec3_from_json(to_json(original));

    EXPECT_FLOAT_EQ(restored.x, original.x);
    EXPECT_FLOAT_EQ(restored.y, original.y);
    EXPECT_FLOAT_EQ(restored.z, original.z);
}

// ---------------------------------------------------------------------------
// Quat tests
// ---------------------------------------------------------------------------

TEST(ScriptTypesTest, QuatToJson) {
    const Quat q{0.1f, 0.2f, 0.3f, 0.9f};
    const auto j = to_json(q);

    EXPECT_FLOAT_EQ(j.at("x").get<float>(), 0.1f);
    EXPECT_FLOAT_EQ(j.at("y").get<float>(), 0.2f);
    EXPECT_FLOAT_EQ(j.at("z").get<float>(), 0.3f);
    EXPECT_FLOAT_EQ(j.at("w").get<float>(), 0.9f);
}

TEST(ScriptTypesTest, QuatFromJsonDefaultsToIdentity) {
    const auto j = json::object();
    const auto q = quat_from_json(j);

    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
    EXPECT_FLOAT_EQ(q.w, 1.0f);
}

// ---------------------------------------------------------------------------
// Transform test
// ---------------------------------------------------------------------------

TEST(ScriptTypesTest, TransformRoundTrip) {
    const Transform original{
        .position = {1.0f, 2.0f, 3.0f},
        .rotation = {0.0f, 0.707f, 0.0f, 0.707f},
        .scale = {2.0f, 2.0f, 2.0f},
    };
    const auto restored = transform_from_json(to_json(original));

    EXPECT_FLOAT_EQ(restored.position.x, original.position.x);
    EXPECT_FLOAT_EQ(restored.position.y, original.position.y);
    EXPECT_FLOAT_EQ(restored.position.z, original.position.z);

    EXPECT_FLOAT_EQ(restored.rotation.x, original.rotation.x);
    EXPECT_FLOAT_EQ(restored.rotation.y, original.rotation.y);
    EXPECT_FLOAT_EQ(restored.rotation.z, original.rotation.z);
    EXPECT_FLOAT_EQ(restored.rotation.w, original.rotation.w);

    EXPECT_FLOAT_EQ(restored.scale.x, original.scale.x);
    EXPECT_FLOAT_EQ(restored.scale.y, original.scale.y);
    EXPECT_FLOAT_EQ(restored.scale.z, original.scale.z);
}

// ---------------------------------------------------------------------------
// BodyDef tests
// ---------------------------------------------------------------------------

TEST(ScriptTypesTest, BodyDefBoxRoundTrip) {
    BodyDef original{};
    original.shape = BoxShapeDesc{1.0f, 2.0f, 3.0f};
    original.motion_type = MotionType::Dynamic;
    original.friction = 0.5f;
    original.restitution = 0.3f;
    original.is_sensor = true;

    const auto j = to_json(original);
    const auto restored = bodydef_from_json(j);

    ASSERT_TRUE(std::holds_alternative<BoxShapeDesc>(restored.shape));
    const auto& box = std::get<BoxShapeDesc>(restored.shape);
    EXPECT_FLOAT_EQ(box.half_x, 1.0f);
    EXPECT_FLOAT_EQ(box.half_y, 2.0f);
    EXPECT_FLOAT_EQ(box.half_z, 3.0f);
    EXPECT_EQ(restored.motion_type, MotionType::Dynamic);
    EXPECT_FLOAT_EQ(restored.friction, 0.5f);
    EXPECT_FLOAT_EQ(restored.restitution, 0.3f);
    EXPECT_TRUE(restored.is_sensor);
}

TEST(ScriptTypesTest, BodyDefSphereFromJson) {
    const auto j = json{
        {"shape", "sphere"},
        {"radius", 1.5f},
        {"motionType", "static"},
    };
    const auto bd = bodydef_from_json(j);

    ASSERT_TRUE(std::holds_alternative<SphereShapeDesc>(bd.shape));
    const auto& sphere = std::get<SphereShapeDesc>(bd.shape);
    EXPECT_FLOAT_EQ(sphere.radius, 1.5f);
    EXPECT_EQ(bd.motion_type, MotionType::Static);
}

// ---------------------------------------------------------------------------
// Malformed input safety
// ---------------------------------------------------------------------------

TEST(ScriptTypesTest, Vec3FromJsonNonObjectReturnsDefault) {
    auto v = gg::script_types::vec3_from_json(nlohmann::json(42));
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

// ---------------------------------------------------------------------------
// BodyDef capsule round-trip
// ---------------------------------------------------------------------------

TEST(ScriptTypesTest, BodyDefCapsuleRoundTrip) {
    BodyDef def;
    def.shape = CapsuleShapeDesc{1.0f, 0.5f};
    def.motion_type = MotionType::Kinematic;

    auto j = to_json(def);
    EXPECT_EQ(j["shape"].get<std::string>(), "capsule");
    EXPECT_EQ(j["motionType"].get<std::string>(), "kinematic");

    auto restored = bodydef_from_json(j);
    ASSERT_TRUE(std::holds_alternative<CapsuleShapeDesc>(restored.shape));
    EXPECT_FLOAT_EQ(std::get<CapsuleShapeDesc>(restored.shape).half_height, 1.0f);
    EXPECT_FLOAT_EQ(std::get<CapsuleShapeDesc>(restored.shape).radius, 0.5f);
    EXPECT_EQ(restored.motion_type, MotionType::Kinematic);
}

// ---------------------------------------------------------------------------
// BodyDef box with no halfExtents falls back to defaults
// ---------------------------------------------------------------------------

TEST(ScriptTypesTest, BodyDefDefaultBoxNoHalfExtents) {
    nlohmann::json j = {{"shape", "box"}, {"motionType", "dynamic"}};
    auto def = bodydef_from_json(j);
    ASSERT_TRUE(std::holds_alternative<BoxShapeDesc>(def.shape));
    EXPECT_FLOAT_EQ(std::get<BoxShapeDesc>(def.shape).half_x, 0.5f);
}

// ---------------------------------------------------------------------------
// ContactEvent test (output-only)
// ---------------------------------------------------------------------------

TEST(ScriptTypesTest, ContactEventToJson) {
    const ContactEvent ce{
        .body_id_a = 10,
        .body_id_b = 20,
        .type = ContactType::End,
        .point = {1.0f, 2.0f, 3.0f},
        .normal = {0.0f, 1.0f, 0.0f},
    };
    const auto j = to_json(ce);

    EXPECT_EQ(j.at("bodyIdA").get<uint32_t>(), 10u);
    EXPECT_EQ(j.at("bodyIdB").get<uint32_t>(), 20u);
    EXPECT_EQ(j.at("type").get<std::string>(), "end");
    EXPECT_FLOAT_EQ(j.at("point").at("x").get<float>(), 1.0f);
    EXPECT_FLOAT_EQ(j.at("normal").at("y").get<float>(), 1.0f);
}

// ---------------------------------------------------------------------------
// RayHit test (output-only)
// ---------------------------------------------------------------------------

TEST(ScriptTypesTest, RayHitToJson) {
    const RayHit hit{
        .body_id = 42,
        .fraction = 0.75f,
        .point = {5.0f, 0.0f, -3.0f},
        .normal = {0.0f, 0.0f, 1.0f},
    };
    const auto j = to_json(hit);

    EXPECT_EQ(j.at("bodyId").get<uint32_t>(), 42u);
    EXPECT_FLOAT_EQ(j.at("fraction").get<float>(), 0.75f);
    EXPECT_FLOAT_EQ(j.at("point").at("x").get<float>(), 5.0f);
    EXPECT_FLOAT_EQ(j.at("point").at("z").get<float>(), -3.0f);
    EXPECT_FLOAT_EQ(j.at("normal").at("z").get<float>(), 1.0f);
}
