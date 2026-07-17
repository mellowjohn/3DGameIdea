#pragma once

#include "engine/automation/editor_bridge.h"

#include <nlohmann/json.hpp>

#include <filesystem>

namespace engine {

/// Command-backed project git sync (DEC-0037 / TICKET-0193).
/// Actions: status | fetch | pull | commit | push.
/// Wraps system `git` for the opened project root (supports monorepo nested projects).
[[nodiscard]] EditorBridgeResponse apply_project_git_operation(const std::filesystem::path& project_root,
    const nlohmann::json& params);

} // namespace engine
