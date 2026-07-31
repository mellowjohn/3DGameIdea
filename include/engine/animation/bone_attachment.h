#pragma once

#include "engine/world/components.h"

#include <array>
#include <string>

namespace engine {

/// Authored weld from an animated skeleton joint to an attached (unskinned) mesh — the engine analogue of a
/// Roblox Motor6D C0. The socket frame comes from the joint; the weld adds a fixed local offset, rotation, and
/// scale on top of it.
struct BoneWeld {
    /// Skin joint the weld hangs from (e.g. "RightHand").
    std::string joint;
    /// Offset in joint-local space, before the socket chain's scale is applied.
    std::array<float, 3> offset{0.0f, 0.0f, 0.0f};
    /// XYZ degrees, composed with DirectXMath roll/pitch/yaw ordering (roll Z, then pitch X, then yaw Y).
    std::array<float, 3> euler_deg{0.0f, 0.0f, 0.0f};
    /// Extra scale for the attached mesh, on top of the character scale the socket already carries.
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
};

/// The three transforms that place an animated joint in world space. Dropping any one of them detaches the
/// weld from the visible body.
struct BoneSocketChain {
    /// Scene placement of the skinned entity.
    TransformComponent owner_world{};
    /// Prefab part local transform of the skinned mesh. Carries the authored character scale.
    TransformComponent visual_local{};
    /// Joint global from build_joint_global_matrices(), in skinned model space. Column-major / row-vector
    /// layout, identical to the GPU bone palette.
    std::array<float, 16> joint_model{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

/// Quaternion for `euler_deg`, matching XMQuaternionRotationRollPitchYaw ordering.
[[nodiscard]] std::array<float, 4> quaternion_from_euler_deg(const std::array<float, 3>& euler_deg);

/// Inverse of quaternion_from_euler_deg. ImGuizmo's own Euler decomposition uses a different axis order, so
/// manipulator write-back must come through here or authored rotations drift every drag.
[[nodiscard]] std::array<float, 3> euler_deg_from_quaternion(const std::array<float, 4>& rotation);

/// TRS of a weld relative to its socket.
[[nodiscard]] TransformComponent weld_local_transform(const BoneWeld& weld);

/// World transform of the joint itself — the frame a weld is authored against.
[[nodiscard]] TransformComponent bone_socket_world(const BoneSocketChain& chain);

/// World transform of the attached mesh.
[[nodiscard]] TransformComponent weld_world_transform(const TransformComponent& socket_world, const BoneWeld& weld);

/// Solve the weld that puts an attached mesh at `target_world` while hanging off `socket_world`. This is the
/// write-back path for a manipulator: drag in world space, persist joint-local values.
[[nodiscard]] BoneWeld weld_from_world_transform(const TransformComponent& socket_world,
    const TransformComponent& target_world, std::string joint);

} // namespace engine
