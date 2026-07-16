#include "engine/scripting/lua_runtime.h"

#include "engine/assets/script_bindings_asset.h"
#include "engine/diagnostics/logger.h"
#include "engine/quest/quest_runtime.h"
#include "engine/standing/standing_runtime.h"
#include "engine/ui/hud_runtime.h"
#include "engine/ui/ui_canvas_stack.h"
#include "engine/world/combat_volumes.h"
#include "engine/world/interaction_volumes.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace engine {
namespace {

constexpr const char* kHostRegistryKey = "engine.lua_host";

struct LuaHost {
    std::map<std::string, ScriptBlackboardEntry> blackboard;
    HudRuntime* hud = nullptr;
    UiCanvasStack* ui_stack = nullptr;
    QuestRuntime* quest = nullptr;
    StandingRuntime* standing = nullptr;
};

void sync_quest_hud(LuaHost* host) {
    if (!host || !host->hud) return;
    const std::string text = host->quest ? host->quest->primary_objective_text() : std::string{};
    host->hud->set_text("quest.objectiveText", text);
}

EngineError lua_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Scripting, "scripting", std::move(message),
                       ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

int sandbox_require(lua_State* state) {
    (void)state;
    return luaL_error(state, "require is disabled in sandbox");
}

LuaHost* host_from_state(lua_State* state) {
    lua_getfield(state, LUA_REGISTRYINDEX, kHostRegistryKey);
    auto* host = static_cast<LuaHost*>(lua_touserdata(state, -1));
    lua_pop(state, 1);
    return host;
}

void push_json_value(lua_State* state, const nlohmann::json& value) {
    if (value.is_null()) {
        lua_pushnil(state);
    } else if (value.is_boolean()) {
        lua_pushboolean(state, value.get<bool>() ? 1 : 0);
    } else if (value.is_number_integer()) {
        lua_pushinteger(state, static_cast<lua_Integer>(value.get<std::int64_t>()));
    } else if (value.is_number()) {
        lua_pushnumber(state, value.get<double>());
    } else if (value.is_string()) {
        lua_pushlstring(state, value.get_ref<const std::string&>().data(), value.get_ref<const std::string&>().size());
    } else if (value.is_array()) {
        lua_createtable(state, static_cast<int>(value.size()), 0);
        int index = 1;
        for (const auto& element : value) {
            push_json_value(state, element);
            lua_rawseti(state, -2, index++);
        }
    } else if (value.is_object()) {
        lua_createtable(state, 0, static_cast<int>(value.size()));
        for (const auto& [key, child] : value.items()) {
            push_json_value(state, child);
            lua_setfield(state, -2, key.c_str());
        }
    } else {
        lua_pushnil(state);
    }
}

int engine_log(lua_State* state) {
    const char* level = luaL_checkstring(state, 1);
    const char* message = luaL_checkstring(state, 2);
    Severity severity = Severity::Info;
    if (std::strcmp(level, "debug") == 0 || std::strcmp(level, "info") == 0) {
        severity = Severity::Info;
    } else if (std::strcmp(level, "warn") == 0) {
        severity = Severity::Warning;
    } else if (std::strcmp(level, "error") == 0) {
        severity = Severity::Error;
    } else {
        Logger::instance().write(Severity::Warning, "lua",
            std::string("invalid log level '") + level + "'; message ignored");
        return 0;
    }
    Logger::instance().write(severity, "lua", message);
    return 0;
}

int engine_json_decode(lua_State* state) {
    const char* json = luaL_checkstring(state, 1);
    try {
        const auto parsed = nlohmann::json::parse(json);
        push_json_value(state, parsed);
        return 1;
    } catch (const nlohmann::json::exception& ex) {
        lua_pushnil(state);
        lua_pushstring(state, ex.what());
        return 2;
    }
}

int engine_blackboard_set(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host) return luaL_error(state, "engine host is not available");
    const char* key = luaL_checkstring(state, 1);
    if (key == nullptr || key[0] == '\0') return luaL_error(state, "blackboard key must be a non-empty string");

    ScriptBlackboardEntry entry;
    const int value_type = lua_type(state, 2);
    if (value_type == LUA_TBOOLEAN) {
        entry.type = ScriptBlackboardType::Bool;
        entry.bool_value = lua_toboolean(state, 2) != 0;
    } else if (value_type == LUA_TNUMBER) {
        entry.type = ScriptBlackboardType::Number;
        entry.number_value = lua_tonumber(state, 2);
    } else if (value_type == LUA_TSTRING) {
        entry.type = ScriptBlackboardType::String;
        size_t length = 0;
        const char* text = lua_tolstring(state, 2, &length);
        entry.string_value.assign(text, length);
    } else {
        return luaL_error(state, "blackboard value must be bool, number, or string");
    }
    host->blackboard[key] = std::move(entry);
    return 0;
}

int engine_blackboard_get(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host) {
        lua_pushnil(state);
        return 1;
    }
    const char* key = luaL_checkstring(state, 1);
    const auto it = host->blackboard.find(key);
    if (it == host->blackboard.end()) {
        lua_pushnil(state);
        return 1;
    }
    switch (it->second.type) {
    case ScriptBlackboardType::Bool:
        lua_pushboolean(state, it->second.bool_value ? 1 : 0);
        break;
    case ScriptBlackboardType::Number:
        lua_pushnumber(state, it->second.number_value);
        break;
    case ScriptBlackboardType::String:
        lua_pushlstring(state, it->second.string_value.data(), it->second.string_value.size());
        break;
    }
    return 1;
}

int engine_hud_set_number(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) return 0;
    const char* key = luaL_checkstring(state, 1);
    const double value = luaL_checknumber(state, 2);
    host->hud->set_number(key, value);
    return 0;
}

int engine_hud_set_bool(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) return 0;
    const char* key = luaL_checkstring(state, 1);
    const bool value = lua_toboolean(state, 2) != 0;
    host->hud->set_bool(key, value);
    return 0;
}

int engine_hud_get_bool(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) {
        lua_pushboolean(state, 0);
        return 1;
    }
    const char* key = luaL_checkstring(state, 1);
    lua_pushboolean(state, host->hud->get_bool(key).value_or(false) ? 1 : 0);
    return 1;
}

int engine_hud_set_text(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) return 0;
    const char* key = luaL_checkstring(state, 1);
    const char* text = luaL_checkstring(state, 2);
    host->hud->set_text(key, text);
    return 0;
}

int engine_hud_set_visible(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) return 0;
    const char* widget_id = luaL_checkstring(state, 1);
    const bool visible = lua_toboolean(state, 2) != 0;
    host->hud->set_visible(widget_id, visible);
    return 0;
}

int engine_hud_set_enabled(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) return 0;
    const char* widget_id = luaL_checkstring(state, 1);
    const bool enabled = lua_toboolean(state, 2) != 0;
    host->hud->set_enabled(widget_id, enabled);
    return 0;
}

int engine_set_health(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) return 0;
    const double current = luaL_checknumber(state, 1);
    const double max = luaL_checknumber(state, 2);
    host->hud->set_health(current, max);
    return 0;
}

int engine_get_health(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) {
        lua_pushnumber(state, 0);
        lua_pushnumber(state, 0);
        return 2;
    }
    const double current = host->hud->get_number("player.health").value_or(0.0);
    const double max = host->hud->get_number("player.healthMax").value_or(0.0);
    lua_pushnumber(state, current);
    lua_pushnumber(state, max);
    return 2;
}

int engine_ui_push(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* id = luaL_checkstring(state, 1);
    const auto result = host->ui_stack->push(id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_ui_pop(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const auto result = host->ui_stack->pop();
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_ui_show(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* id = luaL_checkstring(state, 1);
    const auto result = host->ui_stack->show(id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_ui_hide(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* id = luaL_checkstring(state, 1);
    const auto result = host->ui_stack->hide(id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_ui_top(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) {
        lua_pushnil(state);
        return 1;
    }
    if (const auto top = host->ui_stack->top_modal()) {
        lua_pushlstring(state, top->data(), top->size());
    } else {
        lua_pushnil(state);
    }
    return 1;
}

void push_quest_status(lua_State* state, const QuestProgressStatus& status) {
    lua_createtable(state, 0, 5);
    lua_pushstring(state, status.quest_id.c_str());
    lua_setfield(state, -2, "questId");
    lua_pushstring(state, to_string(status.status));
    lua_setfield(state, -2, "status");
    lua_pushstring(state, status.current_objective_id.c_str());
    lua_setfield(state, -2, "currentObjectiveId");
    lua_pushstring(state, status.current_objective_summary.c_str());
    lua_setfield(state, -2, "currentObjectiveSummary");
    lua_createtable(state, static_cast<int>(status.completed_objective_ids.size()), 0);
    for (std::size_t i = 0; i < status.completed_objective_ids.size(); ++i) {
        lua_pushstring(state, status.completed_objective_ids[i].c_str());
        lua_rawseti(state, -2, static_cast<int>(i) + 1);
    }
    lua_setfield(state, -2, "completedObjectiveIds");
}

int engine_quest_start(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->quest) return luaL_error(state, "quest runtime is not available");
    const char* quest_id = luaL_checkstring(state, 1);
    const auto result = host->quest->start(quest_id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    sync_quest_hud(host);
    return 0;
}

int engine_quest_complete_objective(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->quest) return luaL_error(state, "quest runtime is not available");
    const char* quest_id = luaL_checkstring(state, 1);
    const char* objective_id = luaL_checkstring(state, 2);
    const auto result = host->quest->complete_objective(quest_id, objective_id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    sync_quest_hud(host);
    return 0;
}

int engine_quest_abandon(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->quest) return luaL_error(state, "quest runtime is not available");
    const char* quest_id = luaL_checkstring(state, 1);
    const auto result = host->quest->abandon(quest_id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    sync_quest_hud(host);
    return 0;
}

int engine_quest_status(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->quest) return luaL_error(state, "quest runtime is not available");
    const char* quest_id = luaL_checkstring(state, 1);
    const auto result = host->quest->status(quest_id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    push_quest_status(state, result.value());
    return 1;
}

int engine_quest_dialogue_hook(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->quest) return luaL_error(state, "quest runtime is not available");
    const char* quest_id = luaL_checkstring(state, 1);
    const char* stage_raw = luaL_checkstring(state, 2);
    std::string stage_key = stage_raw ? stage_raw : "";
    for (char& c : stage_key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    QuestDialogueStage stage = QuestDialogueStage::Start;
    if (stage_key == "start") stage = QuestDialogueStage::Start;
    else if (stage_key == "current" || stage_key == "currentobjective" || stage_key == "objective")
        stage = QuestDialogueStage::CurrentObjective;
    else if (stage_key == "complete") stage = QuestDialogueStage::Complete;
    else if (stage_key == "abandon") stage = QuestDialogueStage::Abandon;
    else return luaL_error(state, "unknown quest dialogue stage (use start|current|complete|abandon)");
    const auto result = host->quest->dialogue_for_stage(quest_id, stage);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    lua_pushstring(state, result.value().c_str());
    return 1;
}

int engine_standing_get(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->standing) return luaL_error(state, "standing runtime is not available");
    const char* faction_id = luaL_checkstring(state, 1);
    const auto result = host->standing->get(faction_id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    lua_pushnumber(state, result.value());
    return 1;
}

int engine_standing_set(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->standing) return luaL_error(state, "standing runtime is not available");
    const char* faction_id = luaL_checkstring(state, 1);
    const double score = luaL_checknumber(state, 2);
    const auto result = host->standing->set(faction_id, score);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_standing_adjust(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->standing) return luaL_error(state, "standing runtime is not available");
    const char* faction_id = luaL_checkstring(state, 1);
    const double delta = luaL_checknumber(state, 2);
    const auto result = host->standing->adjust(faction_id, delta);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_standing_rank(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->standing) return luaL_error(state, "standing runtime is not available");
    const char* faction_id = luaL_checkstring(state, 1);
    const auto result = host->standing->rank(faction_id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    lua_pushstring(state, result.value().c_str());
    return 1;
}

int engine_standing_meets(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->standing) return luaL_error(state, "standing runtime is not available");
    const char* faction_id = luaL_checkstring(state, 1);
    WorldForgeQuestStandingRequirement req;
    req.faction_id = faction_id;
    if (lua_type(state, 2) == LUA_TNUMBER) {
        req.min_score = lua_tonumber(state, 2);
    } else if (lua_type(state, 2) == LUA_TSTRING) {
        req.min_rank_id = lua_tostring(state, 2);
    } else {
        return luaL_error(state, "standing_meets expects minScore (number) or minRankId (string)");
    }
    if (lua_gettop(state) >= 3 && lua_type(state, 3) == LUA_TSTRING) {
        req.min_rank_id = lua_tostring(state, 3);
    }
    const auto result = host->standing->meets_requirement(req);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    lua_pushboolean(state, result.value() ? 1 : 0);
    return 1;
}

int engine_standing_lock_in(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->standing) return luaL_error(state, "standing runtime is not available");
    const auto result = host->standing->lock_in_faction();
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    if (result.value().empty()) lua_pushnil(state);
    else lua_pushstring(state, result.value().c_str());
    return 1;
}

void register_engine_api(lua_State* state, LuaHost* host) {
    lua_pushlightuserdata(state, host);
    lua_setfield(state, LUA_REGISTRYINDEX, kHostRegistryKey);

    lua_createtable(state, 0, 28);
    lua_pushcfunction(state, engine_log);
    lua_setfield(state, -2, "log");
    lua_pushcfunction(state, engine_json_decode);
    lua_setfield(state, -2, "json_decode");
    lua_pushcfunction(state, engine_blackboard_set);
    lua_setfield(state, -2, "blackboard_set");
    lua_pushcfunction(state, engine_blackboard_get);
    lua_setfield(state, -2, "blackboard_get");
    lua_pushcfunction(state, engine_hud_set_number);
    lua_setfield(state, -2, "hud_set_number");
    lua_pushcfunction(state, engine_hud_set_bool);
    lua_setfield(state, -2, "hud_set_bool");
    lua_pushcfunction(state, engine_hud_get_bool);
    lua_setfield(state, -2, "hud_get_bool");
    lua_pushcfunction(state, engine_hud_set_text);
    lua_setfield(state, -2, "hud_set_text");
    lua_pushcfunction(state, engine_hud_set_visible);
    lua_setfield(state, -2, "hud_set_visible");
    lua_pushcfunction(state, engine_hud_set_enabled);
    lua_setfield(state, -2, "hud_set_enabled");
    lua_pushcfunction(state, engine_set_health);
    lua_setfield(state, -2, "set_health");
    lua_pushcfunction(state, engine_get_health);
    lua_setfield(state, -2, "get_health");
    lua_pushcfunction(state, engine_ui_push);
    lua_setfield(state, -2, "ui_push");
    lua_pushcfunction(state, engine_ui_pop);
    lua_setfield(state, -2, "ui_pop");
    lua_pushcfunction(state, engine_ui_show);
    lua_setfield(state, -2, "ui_show");
    lua_pushcfunction(state, engine_ui_hide);
    lua_setfield(state, -2, "ui_hide");
    lua_pushcfunction(state, engine_ui_top);
    lua_setfield(state, -2, "ui_top");
    lua_pushcfunction(state, engine_quest_start);
    lua_setfield(state, -2, "quest_start");
    lua_pushcfunction(state, engine_quest_complete_objective);
    lua_setfield(state, -2, "quest_complete_objective");
    lua_pushcfunction(state, engine_quest_abandon);
    lua_setfield(state, -2, "quest_abandon");
    lua_pushcfunction(state, engine_quest_status);
    lua_setfield(state, -2, "quest_status");
    lua_pushcfunction(state, engine_quest_dialogue_hook);
    lua_setfield(state, -2, "quest_dialogue_hook");
    lua_pushcfunction(state, engine_standing_get);
    lua_setfield(state, -2, "standing_get");
    lua_pushcfunction(state, engine_standing_set);
    lua_setfield(state, -2, "standing_set");
    lua_pushcfunction(state, engine_standing_adjust);
    lua_setfield(state, -2, "standing_adjust");
    lua_pushcfunction(state, engine_standing_rank);
    lua_setfield(state, -2, "standing_rank");
    lua_pushcfunction(state, engine_standing_meets);
    lua_setfield(state, -2, "standing_meets");
    lua_pushcfunction(state, engine_standing_lock_in);
    lua_setfield(state, -2, "standing_lock_in");
    lua_setglobal(state, "engine");
}

void open_sandbox(lua_State* state, LuaHost* host) {
    luaL_openlibs(state);
    lua_pushcfunction(state, sandbox_require);
    lua_setglobal(state, "require");
    lua_pushnil(state);
    lua_setglobal(state, "dofile");
    lua_pushnil(state);
    lua_setglobal(state, "loadfile");
    register_engine_api(state, host);
}

} // namespace

struct LuaRuntime::Impl {
    lua_State* state = nullptr;
    LuaHost host;
    std::map<std::filesystem::path, std::string> loaded_sources;
};

LuaRuntime::LuaRuntime() : impl_(std::make_unique<Impl>()) {
    impl_->state = luaL_newstate();
    if (impl_->state) open_sandbox(impl_->state, &impl_->host);
}

LuaRuntime::~LuaRuntime() {
    if (impl_ && impl_->state) lua_close(impl_->state);
}

Result<void> LuaRuntime::load_bindings(const std::filesystem::path& project_root,
    const std::filesystem::path& bindings_path) {
    project_root_ = project_root;
    interaction_bindings_.clear();
    combat_hit_bindings_.clear();
    combat_hurt_bindings_.clear();
    ui_button_bindings_.clear();
    const auto loaded = ScriptBindingsAsset::load(bindings_path);
    if (!loaded) return Result<void>::failure(loaded.error());
    const auto ingest = [&](const std::vector<ScriptBindingEntry>& entries, std::map<std::string, ScriptBindingEntry>& out) {
        for (auto entry : entries) {
            if (!entry.script_path.empty()) {
                const auto loaded_script = load_script(project_root / entry.script_path);
                if (!loaded_script) recent_errors_.push_back(loaded_script.error());
            }
            out[entry.id] = std::move(entry);
        }
    };
    ingest(loaded.value().interactions, interaction_bindings_);
    ingest(loaded.value().combat_hits, combat_hit_bindings_);
    ingest(loaded.value().combat_hurts, combat_hurt_bindings_);
    ingest(loaded.value().ui_buttons, ui_button_bindings_);
    return Result<void>::success();
}

Result<void> LuaRuntime::validate_script(const std::filesystem::path& absolute_path) const {
    std::ifstream input(absolute_path);
    if (!input) {
        return Result<void>::failure(lua_error("SCRIPT-IO", "Script file not found: " + absolute_path.generic_string(),
            "Create the script under assets/scripts/."));
    }
    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    lua_State* temp = luaL_newstate();
    if (!temp) return Result<void>::failure(lua_error("SCRIPT-VM", "Failed to allocate Lua state", "Retry after restart."));
    LuaHost temp_host;
    open_sandbox(temp, &temp_host);
    const auto status = luaL_loadbuffer(temp, source.c_str(), source.size(), absolute_path.generic_string().c_str());
    if (status != LUA_OK) {
        const std::string message = lua_tostring(temp, -1) ? lua_tostring(temp, -1) : "unknown compile error";
        lua_close(temp);
        return Result<void>::failure(lua_error("SCRIPT-COMPILE", message, "Fix Lua syntax and reload."));
    }
    if (lua_pcall(temp, 0, 0, 0) != LUA_OK) {
        const std::string message = lua_tostring(temp, -1) ? lua_tostring(temp, -1) : "unknown runtime error";
        lua_close(temp);
        return Result<void>::failure(lua_error("SCRIPT-RUNTIME", message, "Fix Lua runtime errors and reload."));
    }
    lua_close(temp);
    return Result<void>::success();
}

Result<void> LuaRuntime::load_script(const std::filesystem::path& absolute_path) {
    const auto validated = validate_script(absolute_path);
    if (!validated) return validated;
    std::ifstream input(absolute_path);
    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (!impl_->state) return Result<void>::failure(lua_error("SCRIPT-VM", "Lua runtime is not initialized", "Restart editor."));
    const auto status = luaL_loadbuffer(impl_->state, source.c_str(), source.size(), absolute_path.generic_string().c_str());
    if (status != LUA_OK) {
        const std::string message = lua_tostring(impl_->state, -1) ? lua_tostring(impl_->state, -1) : "unknown compile error";
        return Result<void>::failure(lua_error("SCRIPT-COMPILE", message, "Fix Lua syntax and reload."));
    }
    if (lua_pcall(impl_->state, 0, 0, 0) != LUA_OK) {
        const std::string message = lua_tostring(impl_->state, -1) ? lua_tostring(impl_->state, -1) : "unknown runtime error";
        return Result<void>::failure(lua_error("SCRIPT-RUNTIME", message, "Fix Lua runtime errors and reload."));
    }
    impl_->loaded_sources[absolute_path] = source;
    return Result<void>::success();
}

Result<void> LuaRuntime::reload_script(const std::filesystem::path& absolute_path) {
    return load_script(absolute_path);
}

Result<void> LuaRuntime::call_handler(const std::string& handler_name, const std::string& payload_json) {
    if (!impl_->state) return Result<void>::failure(lua_error("SCRIPT-VM", "Lua runtime is not initialized", "Restart editor."));
    lua_getglobal(impl_->state, handler_name.c_str());
    if (!lua_isfunction(impl_->state, -1)) {
        lua_pop(impl_->state, 1);
        return Result<void>::failure(lua_error("SCRIPT-HANDLER-MISSING", "Handler not found: " + handler_name,
            "Define a global Lua function with this name."));
    }
    lua_pushstring(impl_->state, payload_json.c_str());
    if (lua_pcall(impl_->state, 1, 0, 0) != LUA_OK) {
        const std::string message = lua_tostring(impl_->state, -1) ? lua_tostring(impl_->state, -1) : "handler failed";
        lua_pop(impl_->state, 1);
        return Result<void>::failure(lua_error("SCRIPT-HANDLER-FAILED", message, "Fix handler implementation."));
    }
    return Result<void>::success();
}

std::optional<ScriptBlackboardEntry> LuaRuntime::blackboard_get(const std::string& key) const {
    if (!impl_) return std::nullopt;
    const auto it = impl_->host.blackboard.find(key);
    if (it == impl_->host.blackboard.end()) return std::nullopt;
    return it->second;
}

void LuaRuntime::blackboard_clear() {
    if (impl_) impl_->host.blackboard.clear();
}

void LuaRuntime::set_hud_runtime(HudRuntime* hud) noexcept {
    if (impl_) impl_->host.hud = hud;
}

void LuaRuntime::set_ui_canvas_stack(UiCanvasStack* stack) noexcept {
    if (impl_) impl_->host.ui_stack = stack;
}

void LuaRuntime::set_quest_runtime(QuestRuntime* quest) noexcept {
    if (!impl_) return;
    impl_->host.quest = quest;
    if (impl_->host.hud) {
        const std::string text = quest ? quest->primary_objective_text() : std::string{};
        impl_->host.hud->set_text("quest.objectiveText", text);
    }
}

void LuaRuntime::set_standing_runtime(StandingRuntime* standing) noexcept {
    if (impl_) impl_->host.standing = standing;
}

void LuaRuntime::dispatch_interaction(const InteractionEvent& event) {
    const auto binding = interaction_bindings_.find(event.interaction_id);
    if (binding == interaction_bindings_.end()) return;
    nlohmann::json payload;
    payload["type"] = event.type == InteractionEventType::Enter ? "enter" : "exit";
    payload["interactionId"] = event.interaction_id;
    payload["placementEntityId"] = event.placement_entity_id;
    payload["interactorId"] = event.interactor_id;
    payload["volumeIndex"] = event.volume_index;
    if (const auto result = call_handler(binding->second.handler, payload.dump()); !result)
        recent_errors_.push_back(result.error());
}

void LuaRuntime::dispatch_combat_hit(const CombatContactEvent& event) {
    const auto binding = combat_hurt_bindings_.find(event.hurt_combat_id);
    if (binding == combat_hurt_bindings_.end()) return;
    nlohmann::json payload;
    payload["attackerId"] = event.attacker_id;
    payload["hurtPlacementEntityId"] = event.hurt_placement_entity_id;
    payload["hurtCombatId"] = event.hurt_combat_id;
    payload["hurtVolumeIndex"] = event.hurt_volume_index;
    if (const auto result = call_handler(binding->second.handler, payload.dump()); !result)
        recent_errors_.push_back(result.error());
}

void LuaRuntime::dispatch_ui_button(const std::string& bind_id, const std::string& canvas_id,
    const std::string& widget_id) {
    nlohmann::json payload;
    payload["bind"] = bind_id;
    payload["canvas"] = canvas_id;
    payload["widget"] = widget_id;
    const auto payload_json = payload.dump();
    const auto binding = ui_button_bindings_.find(bind_id);
    if (binding != ui_button_bindings_.end()) {
        if (const auto result = call_handler(binding->second.handler, payload_json); !result)
            recent_errors_.push_back(result.error());
        return;
    }
    if (const auto result = call_handler("on_ui_button", payload_json); !result)
        recent_errors_.push_back(result.error());
}

Result<std::string> write_lua_script_atomic(const std::filesystem::path& absolute_path, const std::string& source) {
    const auto parent = absolute_path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    const auto temp = absolute_path.string() + ".tmp";
    const auto backup = absolute_path.string() + ".bak";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            return Result<std::string>::failure(lua_error("SCRIPT-IO", "Failed to open temp script file",
                "Check permissions."));
        }
        out << source;
    }
    if (std::filesystem::exists(absolute_path))
        std::filesystem::copy_file(absolute_path, backup, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::rename(temp, absolute_path);
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup);
    return Result<std::string>::success(absolute_path.generic_string());
}

} // namespace engine
