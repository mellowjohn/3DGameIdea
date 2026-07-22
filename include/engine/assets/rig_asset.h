#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace engine {

/** One IK effector hook (metadata only — no solver in TICKET-0106). */
struct RigIkHook {
    std::string id;          // e.g. left_hand, right_foot
    std::string tip_joint;   // joint display name on the skin
    std::string root_joint;  // optional chain root; empty = unspecified
    std::string pole_joint;  // optional pole/hint joint; empty = none
    int chain_length = 0;    // 0 = unspecified; otherwise positive bone count from tip toward root
};

/** Humanoid (or custom) role → skeleton joint name for retargeting. */
struct RigBoneRole {
    std::string role;       // e.g. hips, left_hand, head
    std::string joint_name; // matches ImportedSkin::joint_names entry when skinned
};

/**
 * Authorable rig metadata (*.rig.json): IK effector hooks + retarget bone-role map.
 * DEC-0041 / TICKET-0106 — schema and validation only; full IK solve is follow-on.
 */
struct RigAsset {
    std::uint32_t schema_version = 1;
    std::string id; // stable slug, usually filename stem
    std::string display_name;
    std::string mesh; // optional project-relative glTF this rig describes
    std::vector<RigIkHook> ik_hooks;
    std::vector<RigBoneRole> bone_roles;

    [[nodiscard]] Result<void> validate() const;
    /** When joint_names is non-empty, every tip/root/pole/role joint must appear in the list. */
    [[nodiscard]] Result<void> validate_against_joint_names(const std::vector<std::string>& joint_names) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<RigAsset> from_json(const std::string& text);
    [[nodiscard]] static Result<RigAsset> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save(const std::filesystem::path& path) const;
};

} // namespace engine
