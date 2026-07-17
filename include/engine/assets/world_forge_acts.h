#pragma once

#include "engine/core/error.h"
#include "engine/core/result.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

/// Canonical campaign act ids for World Forge Act lens ([DEC-0036](../../context/decisions/index.md)).
/// Acts are narrative arcs on the seamless world (DEC-0021), not load chapters.
inline constexpr const char* k_world_forge_act_ids[] = {"act0", "act1", "act2", "act3", "act4"};
inline constexpr const char* k_world_forge_act_labels[] = {"Act 0", "Act 1", "Act 2", "Act 3", "Act 4"};
inline constexpr int k_world_forge_act_count = 5;

[[nodiscard]] inline bool is_world_forge_act_id(std::string_view value) noexcept {
    for (const char* id : k_world_forge_act_ids) {
        if (value == id) return true;
    }
    return false;
}

[[nodiscard]] inline const char* world_forge_act_label(std::string_view act_id) noexcept {
    for (int i = 0; i < k_world_forge_act_count; ++i) {
        if (act_id == k_world_forge_act_ids[i]) return k_world_forge_act_labels[i];
    }
    return "Act";
}

/// Resolve act membership: explicit `acts` plus legacy `actN` tags (backward compatible).
[[nodiscard]] inline std::vector<std::string> resolve_world_forge_acts(const std::vector<std::string>& acts,
    const std::vector<std::string>& tags) {
    std::vector<std::string> out;
    out.reserve(acts.size() + 2);
    auto push_unique = [&](const std::string& value) {
        if (!is_world_forge_act_id(value)) return;
        if (std::find(out.begin(), out.end(), value) != out.end()) return;
        out.push_back(value);
    };
    for (const auto& act : acts) push_unique(act);
    for (const auto& tag : tags) push_unique(tag);
    return out;
}

/// Empty `filter` = All. Empty membership = campaign-wide (always visible under a specific act).
[[nodiscard]] inline bool matches_world_forge_act_filter(const std::vector<std::string>& acts,
    const std::vector<std::string>& tags, const std::string& filter) {
    if (filter.empty()) return true;
    const auto resolved = resolve_world_forge_acts(acts, tags);
    if (resolved.empty()) return true;
    return std::find(resolved.begin(), resolved.end(), filter) != resolved.end();
}

[[nodiscard]] inline Result<void> validate_world_forge_acts(const std::vector<std::string>& acts,
    const std::string& owner_kind, const std::string& owner_id) {
    for (const auto& act : acts) {
        if (!is_world_forge_act_id(act)) {
            return Result<void>::failure(EngineError{"WORLD-FORGE-ACT-ID", Severity::Error,
                ErrorCategory::Validation, "world_forge_acts",
                "Unknown act id '" + act + "' on " + owner_kind + " '" + owner_id + "'", ENGINE_SOURCE_CONTEXT, {},
                "Use act0..act4 (campaign beat sheet acts).", make_correlation_id()});
        }
    }
    return Result<void>::success();
}

} // namespace engine
