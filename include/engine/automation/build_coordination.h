#pragma once

#include "engine/automation/editor_bridge.h"

#include <nlohmann/json.hpp>

#include <filesystem>

namespace engine {

/// Same-machine agent rebuild coordination (TICKET-0228).
///
/// Agents acquire a timed, token-protected lease on the shared engine rebuild slot before
/// running MSBuild, wait in a FIFO queue while another agent holds it, and release when the
/// kill -> rebuild -> restart loop finishes. State is a git-ignored JSON file at the
/// coordination root (the checkout that owns `context/planning/epics.md`), serialized by a
/// path-keyed named mutex so concurrent engine processes cannot interleave read-modify-write.
///
/// Actions (params.action): status | acquire | wait | release | heartbeat | clear-stale.
/// acquire/wait require agentId + ticketId (validated against epics.md) + summary.
/// Direct manual MSBuild runs are not intercepted; this coordinates the documented agent flow.

/// Coordination root: nearest ancestor with context/planning/epics.md, else the project root.
[[nodiscard]] std::filesystem::path build_coordination_root(const std::filesystem::path& project_root);

/// State file under the coordination root: `.engine/build-coordinator.json` (generated; git-ignored).
[[nodiscard]] std::filesystem::path build_coordination_state_path(const std::filesystem::path& project_root);

[[nodiscard]] EditorBridgeResponse apply_build_coordination_operation(const std::filesystem::path& project_root,
    const nlohmann::json& params);

} // namespace engine
