#pragma once

#include "engine/world/components.h"

namespace engine {

[[nodiscard]] TransformComponent multiply_transforms(const TransformComponent& parent, const TransformComponent& child);

} // namespace engine
