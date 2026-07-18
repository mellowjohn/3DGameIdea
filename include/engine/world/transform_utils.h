#pragma once

#include "engine/world/components.h"

namespace engine {

[[nodiscard]] TransformComponent multiply_transforms(const TransformComponent& parent, const TransformComponent& child);
[[nodiscard]] TransformComponent inverse_transform(const TransformComponent& transform);

} // namespace engine
