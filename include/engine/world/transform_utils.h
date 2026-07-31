#pragma once

#include "engine/world/components.h"

#include <array>

namespace engine {

[[nodiscard]] TransformComponent multiply_transforms(const TransformComponent& parent, const TransformComponent& child);
[[nodiscard]] TransformComponent inverse_transform(const TransformComponent& transform);
/// Decompose a column-major affine matrix (cpu-skinning / HLSL layout) into TRS.
[[nodiscard]] TransformComponent transform_from_column_major(const std::array<float, 16>& matrix);

} // namespace engine
