#pragma once

#include "engine/automation/editor_bridge.h"

#include <nlohmann/json.hpp>

#include <filesystem>

namespace engine {

/// MCP / editor-bridge World Forge ops for factions, relationships, and map assets.
/// Actions: get | validate | apply (aliases: read, write). Offline-capable (file + validate).
[[nodiscard]] EditorBridgeResponse apply_world_forge_operation(const std::filesystem::path& project_root,
    const nlohmann::json& params);

} // namespace engine
