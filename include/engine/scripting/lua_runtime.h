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
struct AnimatorFiredEvent;
struct EventTimelineEmitEvent;
struct StatusTickEvent;
class HudRuntime;
class UiCanvasStack;
class WorldUiBillboardRuntime;
class CombatTextRuntime;
class StatusEffectRuntime;
class QuestRuntime;
class StandingRuntime;
class FlagRuntime;
class InventoryRuntime;
class AnimatorRuntime;
class EventTimelineRuntime;
class DialogueRuntime;
class AudioEngine;
class GameSession;
struct DialogueUiSession;

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
    /// Explicit interact (e.g. Press E) for a currently prompted interaction id.
    void dispatch_interaction_use(const std::string& interaction_id);
    void dispatch_combat_hit(const CombatContactEvent& event);
    void dispatch_status_tick(const StatusTickEvent& event);
    void dispatch_ui_button(const std::string& bind_id, const std::string& canvas_id, const std::string& widget_id);
    void dispatch_animation_event(const AnimatorFiredEvent& event);
    void dispatch_event_timeline_emit(const EventTimelineEmitEvent& event);
    /// Optional global `on_update` during play-test (silent if missing).
    void dispatch_update(float dt_seconds);

    [[nodiscard]] Result<void> call_handler(const std::string& handler_name, const std::string& payload_json);

    [[nodiscard]] std::optional<ScriptBlackboardEntry> blackboard_get(const std::string& key) const;
    void blackboard_set_bool(const std::string& key, bool value);
    void blackboard_set_number(const std::string& key, double value);
    void blackboard_clear();
    /// Drop keys that start with `prefix` (e.g. `"combat."` on F5). Leaves other keys.
    void blackboard_erase_prefix(const std::string& prefix);

    void set_hud_runtime(HudRuntime* hud) noexcept;
    void set_ui_canvas_stack(UiCanvasStack* stack) noexcept;
    void set_world_ui_billboards(WorldUiBillboardRuntime* billboards) noexcept;
    void set_combat_text(CombatTextRuntime* combat_text) noexcept;
    void set_status_effects(StatusEffectRuntime* status_effects) noexcept;
    void set_quest_runtime(QuestRuntime* quest) noexcept;
    void set_standing_runtime(StandingRuntime* standing) noexcept;
    void set_flag_runtime(FlagRuntime* flags) noexcept;
    void set_inventory_runtime(InventoryRuntime* inventory) noexcept;
    void set_animator_runtime(AnimatorRuntime* animator) noexcept;
    void set_event_timeline_runtime(EventTimelineRuntime* timeline) noexcept;
    void set_dialogue_runtime(DialogueRuntime* dialogue) noexcept;
    void set_audio_engine(AudioEngine* audio) noexcept;
    void set_game_session(GameSession* session) noexcept;

    [[nodiscard]] DialogueUiSession& dialogue_ui_session() noexcept;
    [[nodiscard]] const DialogueUiSession& dialogue_ui_session() const noexcept;

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
