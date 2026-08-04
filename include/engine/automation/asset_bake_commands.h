#pragma once

#include "engine/automation/editor_bridge.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace engine {

struct AssetBakeTargetInfo {
    std::string id;
    std::string kind;
    std::string default_source;
    std::string baker;
    /// Project-relative bake outputs, e.g. `assets/models/player.gltf`.
    std::string mesh_output;
    std::string atlas_output;
};

struct AssetBakeResult {
    bool ok = false;
    std::string summary;
    std::string raw_json;
    std::vector<std::string> mesh_reloads;
    std::vector<EngineError> diagnostics;
};

struct AssetBakeToolResult {
    int exit_code = 1;
    std::string raw_stdout;
    std::string raw_stderr;
    /// First JSON object found on stdout (empty when the tool produced none).
    nlohmann::json payload;
};

/// Spawn `python tools/asset_bake.py` (TICKET-0245). Works offline (no live editor).
[[nodiscard]] EditorBridgeResponse apply_asset_bake_operation(const std::filesystem::path& project_root,
    const nlohmann::json& params);

[[nodiscard]] std::vector<AssetBakeTargetInfo> list_asset_bake_targets();
[[nodiscard]] AssetBakeResult run_asset_bake(const std::filesystem::path& project_root, const std::string& target,
    const std::string& source_override = {});

/// Run `tools/asset_bake.py --project <root> --json <args...>` and return the parsed stdout payload.
/// Shared with the in-editor import flow so process spawning lives in one place.
[[nodiscard]] AssetBakeToolResult run_asset_bake_tool(const std::filesystem::path& project_root,
    const std::vector<std::string>& args);

[[nodiscard]] EngineError asset_bake_error(const std::string& code, const std::string& message,
    const std::string& remediation = {});

} // namespace engine
