#pragma once

#include "ecs/components.h"

#include <array>

namespace gg::gizmo {

static constexpr float SHAFT_LENGTH = 1.2f;
static constexpr float CONE_LENGTH = 0.3f;
static constexpr float ARROW_TOTAL_LENGTH = SHAFT_LENGTH + CONE_LENGTH;
static constexpr float SCREEN_RATIO = 0.25f;

static constexpr std::array<Vec3, 3> AXIS_DIRS = {{
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
}};

} // namespace gg::gizmo
