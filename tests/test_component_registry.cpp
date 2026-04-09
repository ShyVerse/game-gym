#include "ecs/component_registry.h"

#include <gtest/gtest.h>

TEST(ComponentRegistryTest, ReturnsStableBuiltInOrder) {
    const auto components = gg::built_in_components();
    ASSERT_EQ(components.size(), 5u);
    EXPECT_EQ(components[0].stable_id, "transform");
    EXPECT_EQ(components[1].stable_id, "mesh_renderer");
    EXPECT_EQ(components[2].stable_id, "script");
    EXPECT_EQ(components[3].stable_id, "rigid_body");
    EXPECT_EQ(components[4].stable_id, "velocity");
}

TEST(ComponentRegistryTest, NameIsMetadataOnly) {
    EXPECT_FALSE(gg::is_addable_component("name"));
}
