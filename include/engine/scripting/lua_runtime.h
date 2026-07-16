#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace engine {

struct InteractionEvent;
struct CombatContactEvent;
class HudRuntime;
class UiCanvasStack;
class QuestRuntime;
class StandingRuntime;

struct ScriptBindingEntry {
    std::string id;
    std::string handler;
    std::string script_path;
};

enum class ScriptBlackboardType : std::uint8_t { Bool, Number, String };

struct ScriptBlackboardEntry {
    ScriptBlackboardType type = ScriptBlackboardType::String;
    bool bool_value = false;
    double number_value = 0.0;
    std::string string_value;
};

class LuaRuntime final {
public:
    LuaRuntime();
    ~LuaRuntime();
    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;

    [[nodiscard]] Result<void> load_bindings(const std::filesystem::path& project_root,
        const std::filesystem::path& bindings_path);
    [[nodiscard]] Result<void> load_script(const std::filesystem::path& absolute_path);
    [[nodiscard]] Result<void> reload_script(const std::filesystem::path& absolute_path);
    [[nodiscard]] Result<void> validate_script(const std::filesystem::path& absolute_path) const;

    void dispatch_interaction(const InteractionEvent& event);
    void dispatch_combat_hit(const CombatContactEvent& event);
    void dispatch_ui_button(const std::string& bind_id, const std::string& canvas_id, const std::string& widget_id);

    [[nodiscard]] Result<void> call_handler(const std::string& handler_name, const std::string& payload_json);

    [[nodiscard]] std::optional<ScriptBlackboardEntry> blackboard_get(const std::string& key) const;
    void blackboard_clear();

    void set_hud_runtime(HudRuntime* hud) noexcept;
    void set_ui_canvas_stack(UiCanvasStack* stack) noexcept;
    void set_quest_runtime(QuestRuntime* quest) noexcept;
    void set_standing_runtime(StandingRuntime* standing) noexcept;

    [[nodiscard]] const std::vector<EngineError>& recent_errors() const noexcept { return recent_errors_; }
    void clear_recent_errors() { recent_errors_.clear(); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::filesystem::path project_root_;
    std::map<std::string, ScriptBindingEntry> interaction_bindings_;
    std::map<std::string, ScriptBindingEntry> combat_hit_bindings_;
    std::map<std::string, ScriptBindingEntry> combat_hurt_bindings_;
    std::map<std::string, ScriptBindingEntry> ui_button_bindings_;
    std::vector<EngineError> recent_errors_;
};

[[nodiscard]] Result<std::string> write_lua_script_atomic(const std::filesystem::path& absolute_path,
    const std::string& source);

} // namespace engine
