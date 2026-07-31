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
};

struct AssetBakeResult {
    bool ok = false;
    std::string summary;
    std::string raw_json;
    std::vector<std::string> mesh_reloads;
    std::vector<EngineError> diagnostics;
};

/// Spawn `python tools/asset_bake.py` (TICKET-0245). Works offline (no live editor).
[[nodiscard]] EditorBridgeResponse apply_asset_bake_operation(const std::filesystem::path& project_root,
    const nlohmann::json& params);

[[nodiscard]] std::vector<AssetBakeTargetInfo> list_asset_bake_targets();
[[nodiscard]] AssetBakeResult run_asset_bake(const std::filesystem::path& project_root, const std::string& target,
    const std::string& source_override = {});

} // namespace engine
