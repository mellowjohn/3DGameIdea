#pragma once

#include "engine/core/result.h"
#include "engine/scripting/lua_runtime.h"

#include <filesystem>
#include <vector>

namespace engine {

struct ScriptBindingsAsset {
    int schema_version = 1;
    std::vector<ScriptBindingEntry> interactions;
    std::vector<ScriptBindingEntry> combat_hits;
    std::vector<ScriptBindingEntry> combat_hurts;
    std::vector<ScriptBindingEntry> ui_buttons;

    [[nodiscard]] static Result<ScriptBindingsAsset> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;

    /// Resolve a scriptBinding kind + binding id to the project-relative Lua `script` path.
    [[nodiscard]] Result<std::string> resolve_script_path(const std::string& kind, const std::string& binding_id) const;
};

[[nodiscard]] std::filesystem::path default_script_bindings_path(const std::filesystem::path& project_root);

/// Load `bindings.script.json` under the project and resolve kind + binding id to a script path.
[[nodiscard]] Result<std::string> resolve_script_binding_path(const std::filesystem::path& project_root,
    const std::string& kind, const std::string& binding_id);

} // namespace engine
