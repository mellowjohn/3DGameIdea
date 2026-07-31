#pragma once

#include "engine/assets/animation_clip_asset.h"
#include "engine/assets/mesh_asset.h"
#include "engine/animation/animator_runtime.h"
#include "engine/core/result.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace engine {

struct JointLocalPose {
    std::array<float, 3> translation{0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation{0.0f, 0.0f, 0.0f, 1.0f}; // xyzw
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
};

/**
 * Sample weighted animator clips into per-joint local poses (rest pose for unkeyed joints).
 * Clip sources resolve through `library` using project-relative `clip_source` paths when needed.
 */
[[nodiscard]] Result<std::vector<JointLocalPose>> sample_skinned_local_poses(
    const ImportedSkin& skin, const AnimationClipLibrary& library,
    const std::filesystem::path& project_root, const std::vector<AnimatorClipWeight>& clips);

/** Compose local poses into column-major joint global matrices (model space, one per joint). */
[[nodiscard]] Result<std::vector<std::array<float, 16>>> build_joint_global_matrices(
    const ImportedSkin& skin, const std::vector<JointLocalPose>& locals);

/** Compose local poses with IBM into column-major skin matrices (one per joint). */
[[nodiscard]] Result<std::vector<std::array<float, 16>>> build_skin_matrices(
    const ImportedSkin& skin, const std::vector<JointLocalPose>& locals);

/** Index of `joint_name` in `skin.joint_names`, or nullopt if missing. */
[[nodiscard]] std::optional<std::size_t> find_skin_joint_index(const ImportedSkin& skin,
    const std::string& joint_name);

/**
 * CPU linear-blend skinning of bind-pose mesh positions into `out_positions` (xyz triples).
 * Color/UV are not written — callers copy those from the bind mesh.
 */
[[nodiscard]] Result<void> cpu_skin_positions(const ImportedMesh& mesh, std::size_t skin_index,
    const std::vector<std::array<float, 16>>& skin_matrices, std::vector<std::array<float, 3>>& out_positions);

} // namespace engine
