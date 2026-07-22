#include "engine/assets/rig_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

namespace engine {
namespace {

EngineError rig_error(std::string code, std::string message,
    std::string remedy = "Correct the *.rig.json schema, unique ids/roles, and joint names.") {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "rig-assets", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

bool nonempty_trimmed(const std::string& value) {
    return std::any_of(value.begin(), value.end(),
        [](unsigned char c) { return !std::isspace(c); });
}

std::string normalize_token(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool joint_list_contains(const std::vector<std::string>& joint_names, const std::string& name) {
    const auto needle = normalize_token(name);
    for (const auto& joint : joint_names) {
        if (normalize_token(joint) == needle) return true;
    }
    return false;
}

} // namespace

Result<void> RigAsset::validate() const {
    if (schema_version != 1)
        return Result<void>::failure(
            rig_error("RIG-SCHEMA-UNSUPPORTED", "Only rig schema version 1 is supported"));
    if (!nonempty_trimmed(id))
        return Result<void>::failure(rig_error("RIG-ID-MISSING", "id must be a non-empty slug"));

    std::set<std::string> hook_ids;
    for (const auto& hook : ik_hooks) {
        if (!nonempty_trimmed(hook.id))
            return Result<void>::failure(rig_error("RIG-IK-ID-MISSING", "Each ikHooks[].id must be non-empty"));
        if (!nonempty_trimmed(hook.tip_joint))
            return Result<void>::failure(
                rig_error("RIG-IK-TIP-MISSING", "ikHooks[].tipJoint is required for id '" + hook.id + "'"));
        if (hook.chain_length < 0)
            return Result<void>::failure(
                rig_error("RIG-IK-CHAIN-INVALID", "ikHooks[].chainLength must be >= 0 for id '" + hook.id + "'"));
        const auto normalized_id = normalize_token(hook.id);
        if (!hook_ids.insert(normalized_id).second)
            return Result<void>::failure(
                rig_error("RIG-IK-ID-DUPLICATE", "Duplicate ikHooks id '" + hook.id + "'"));
    }

    std::set<std::string> roles;
    for (const auto& role : bone_roles) {
        if (!nonempty_trimmed(role.role))
            return Result<void>::failure(rig_error("RIG-ROLE-MISSING", "Each boneRoles[].role must be non-empty"));
        if (!nonempty_trimmed(role.joint_name))
            return Result<void>::failure(
                rig_error("RIG-ROLE-JOINT-MISSING", "boneRoles[].jointName is required for role '" + role.role + "'"));
        const auto normalized_role = normalize_token(role.role);
        if (!roles.insert(normalized_role).second)
            return Result<void>::failure(
                rig_error("RIG-ROLE-DUPLICATE", "Duplicate boneRoles role '" + role.role + "'"));
    }
    return Result<void>::success();
}

Result<void> RigAsset::validate_against_joint_names(const std::vector<std::string>& joint_names) const {
    if (const auto base = validate(); !base) return base;
    if (joint_names.empty()) return Result<void>::success();

    auto require_joint = [&](const std::string& joint, const std::string& context) -> Result<void> {
        if (!nonempty_trimmed(joint)) return Result<void>::success();
        if (joint_list_contains(joint_names, joint)) return Result<void>::success();
        return Result<void>::failure(rig_error("RIG-JOINT-UNKNOWN",
            context + " references unknown joint '" + joint + "'",
            "Use a joint name from the skinned mesh ImportedSkin::joint_names list."));
    };

    for (const auto& hook : ik_hooks) {
        if (const auto tip = require_joint(hook.tip_joint, "ikHooks[" + hook.id + "].tipJoint"); !tip) return tip;
        if (const auto root = require_joint(hook.root_joint, "ikHooks[" + hook.id + "].rootJoint"); !root)
            return root;
        if (const auto pole = require_joint(hook.pole_joint, "ikHooks[" + hook.id + "].poleJoint"); !pole)
            return pole;
    }
    for (const auto& role : bone_roles) {
        if (const auto joint = require_joint(role.joint_name, "boneRoles[" + role.role + "]"); !joint) return joint;
    }
    return Result<void>::success();
}

std::string RigAsset::to_json() const {
    nlohmann::ordered_json hooks = nlohmann::ordered_json::array();
    for (const auto& hook : ik_hooks) {
        nlohmann::ordered_json entry{{"id", hook.id}, {"tipJoint", hook.tip_joint}};
        if (!hook.root_joint.empty()) entry["rootJoint"] = hook.root_joint;
        if (!hook.pole_joint.empty()) entry["poleJoint"] = hook.pole_joint;
        if (hook.chain_length > 0) entry["chainLength"] = hook.chain_length;
        hooks.push_back(std::move(entry));
    }
    nlohmann::ordered_json roles = nlohmann::ordered_json::array();
    for (const auto& role : bone_roles) {
        roles.push_back({{"role", role.role}, {"jointName", role.joint_name}});
    }
    nlohmann::ordered_json root{{"schemaVersion", schema_version}, {"id", id}, {"ikHooks", std::move(hooks)},
        {"boneRoles", std::move(roles)}};
    if (!display_name.empty()) root["displayName"] = display_name;
    if (!mesh.empty()) root["mesh"] = mesh;
    return root.dump(2) + "\n";
}

Result<RigAsset> RigAsset::from_json(const std::string& text) {
    try {
        const auto root = nlohmann::json::parse(text);
        RigAsset value;
        value.schema_version = root.at("schemaVersion").get<std::uint32_t>();
        value.id = root.at("id").get<std::string>();
        value.display_name = root.value("displayName", std::string{});
        value.mesh = root.value("mesh", std::string{});
        if (root.contains("ikHooks") && root["ikHooks"].is_array()) {
            for (const auto& entry : root["ikHooks"]) {
                RigIkHook hook;
                hook.id = entry.at("id").get<std::string>();
                hook.tip_joint = entry.at("tipJoint").get<std::string>();
                hook.root_joint = entry.value("rootJoint", std::string{});
                hook.pole_joint = entry.value("poleJoint", std::string{});
                hook.chain_length = entry.value("chainLength", 0);
                value.ik_hooks.push_back(std::move(hook));
            }
        }
        if (root.contains("boneRoles") && root["boneRoles"].is_array()) {
            for (const auto& entry : root["boneRoles"]) {
                RigBoneRole role;
                role.role = entry.at("role").get<std::string>();
                role.joint_name = entry.at("jointName").get<std::string>();
                value.bone_roles.push_back(std::move(role));
            }
        }
        if (const auto valid = value.validate(); !valid) return Result<RigAsset>::failure(valid.error());
        return Result<RigAsset>::success(std::move(value));
    } catch (const std::exception& exception) {
        auto error = rig_error("RIG-PARSE-FAILED", "Rig asset JSON is malformed");
        error.causes.push_back(exception.what());
        return Result<RigAsset>::failure(std::move(error));
    }
}

Result<RigAsset> RigAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return Result<RigAsset>::failure(
            rig_error("RIG-READ-FAILED", "Could not read rig asset: " + path.generic_string()));
    std::ostringstream text;
    text << input.rdbuf();
    return from_json(text.str());
}

Result<void> RigAsset::save(const std::filesystem::path& path) const {
    if (const auto valid = validate(); !valid) return valid;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
            return Result<void>::failure(
                rig_error("RIG-WRITE-FAILED", "Could not write rig asset: " + path.generic_string()));
        output << to_json();
    }
    std::error_code ec;
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(temporary);
        return Result<void>::failure(
            rig_error("RIG-WRITE-FAILED", "Could not replace rig asset: " + path.generic_string()));
    }
    return Result<void>::success();
}

} // namespace engine
