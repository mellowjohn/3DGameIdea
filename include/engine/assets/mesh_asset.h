#pragma once

#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace engine {
struct MeshVertex { float x=0,y=0,z=0,r=.45f,g=.32f,b=.18f,u=0,v=0; };
struct MeshBounds {
    float min_x = -0.5f;
    float min_y = -0.5f;
    float min_z = -0.5f;
    float max_x = 0.5f;
    float max_y = 0.5f;
    float max_z = 0.5f;
};

/** Up to four joint influences per expanded vertex (glTF JOINTS_0 / WEIGHTS_0). */
struct MeshJointInfluence {
    std::array<std::uint16_t, 4> joints{};
    std::array<float, 4> weights{};
};

/** Rest-pose local TRS for one skin joint (glTF node transform). */
struct ImportedJointRestLocal {
    std::array<float, 3> translation{0.0f, 0.0f, 0.0f};
    /** glTF quaternion xyzw. */
    std::array<float, 4> rotation{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
    /** Parent index within this skin's joints array, or -1 for a skin root. */
    std::int32_t parent_joint = -1;
};

/** One glTF skin: joint node indices, names, and column-major inverse-bind matrices. */
struct ImportedSkin {
    std::string name;
    std::vector<std::uint32_t> joint_node_indices;
    std::vector<std::string> joint_names;
    std::vector<std::array<float, 16>> inverse_bind_matrices;
    /** Parallel to `joint_node_indices` — rest locals + hierarchy for CPU skinning. */
    std::vector<ImportedJointRestLocal> joint_rest_locals;
    std::int32_t skeleton_root = -1;
};

struct ImportedMesh {
    std::vector<MeshVertex> vertices;
    MeshBounds aabb;
    std::vector<ImportedSkin> skins;
    /** Parallel to `vertices` when the mesh carries JOINTS_0/WEIGHTS_0; otherwise empty. */
    std::vector<MeshJointInfluence> influences;
    /** Optional engine-owned base-color texture (RGBA8, row-major top-down) from the first used material. */
    std::vector<std::uint8_t> albedo_rgba;
    std::uint32_t albedo_width = 0;
    std::uint32_t albedo_height = 0;
    [[nodiscard]] bool has_skinning() const noexcept { return !skins.empty() || !influences.empty(); }
    [[nodiscard]] bool has_albedo() const noexcept {
        return albedo_width > 0 && albedo_height > 0
            && albedo_rgba.size() == static_cast<std::size_t>(albedo_width) * albedo_height * 4;
    }
    [[nodiscard]] Result<void> validate() const;
};
[[nodiscard]] Result<ImportedMesh> import_gltf_mesh(const std::filesystem::path& path);
[[nodiscard]] Result<ImportedMesh> generate_primitive_mesh(const std::string& primitive, const std::array<float, 3>& color);
[[nodiscard]] Result<ImportedMesh> import_project_mesh(const std::filesystem::path& path);
}
