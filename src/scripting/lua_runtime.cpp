#include "engine/scripting/lua_runtime.h"

#include "engine/animation/animator_runtime.h"
#include "engine/audio/audio_engine.h"
#include "engine/assets/script_bindings_asset.h"
#include "engine/diagnostics/logger.h"
#include "engine/dialogue/dialogue_runtime.h"
#include "engine/dialogue/dialogue_ui.h"
#include "engine/event/event_timeline_runtime.h"
#include "engine/quest/quest_runtime.h"
#include "engine/inventory/inventory_runtime.h"
#include "engine/assets/item_catalog_asset.h"
#include "engine/flag/flag_runtime.h"
#include "engine/standing/standing_runtime.h"
#include "engine/flag/flag_runtime.h"
#include "engine/session/game_session.h"
#include "engine/ui/hud_runtime.h"
#include "engine/ui/ui_canvas_stack.h"
#include "engine/assets/ui_theme_asset.h"
#include "engine/ui/combat_text.h"
#include "engine/combat/status_effect_runtime.h"
#include "engine/ui/world_ui_billboard.h"
#include "engine/world/combat_volumes.h"
#include "engine/world/interaction_volumes.h"

#include <nlohmann/json.hpp>

#include <cmath>
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
    WorldUiBillboardRuntime* world_ui = nullptr;
    CombatTextRuntime* combat_text = nullptr;
    StatusEffectRuntime* status_effects = nullptr;
    QuestRuntime* quest = nullptr;
    StandingRuntime* standing = nullptr;
    FlagRuntime* flags = nullptr;
    InventoryRuntime* inventory = nullptr;
    AnimatorRuntime* animator = nullptr;
    EventTimelineRuntime* event_timeline = nullptr;
    DialogueRuntime* dialogue = nullptr;
    DialogueUiSession dialogue_ui{};
    AudioEngine* audio = nullptr;
    GameSession* game_session = nullptr;
};

void sync_quest_hud(LuaHost* host) {
    if (!host || !host->hud) return;
    const std::string text = host->quest ? host->quest->primary_objective_text() : std::string{};
    host->hud->set_text("quest.objectiveText", text);
    const bool show = !text.empty();
    host->hud->set_visible("quest_objective_text", show);
    host->hud->set_visible("quest_objective_panel", show);
}

void sync_dialogue_ui(LuaHost* host) {
    if (!host) return;
    sync_dialogue_canvas(host->ui_stack, host->dialogue, host->dialogue_ui);
}

void push_dialogue_present(lua_State* state, const DialoguePresent& present) {
    lua_createtable(state, 0, 7);
    lua_pushstring(state, present.tree_id.c_str());
    lua_setfield(state, -2, "treeId");
    lua_pushstring(state, present.node_id.c_str());
    lua_setfield(state, -2, "nodeId");
    lua_pushstring(state, present.speaker_id.c_str());
    lua_setfield(state, -2, "speakerId");
    lua_pushstring(state, present.line.c_str());
    lua_setfield(state, -2, "line");
    lua_pushboolean(state, present.complete ? 1 : 0);
    lua_setfield(state, -2, "complete");
    lua_createtable(state, static_cast<int>(present.choices.size()), 0);
    for (std::size_t i = 0; i < present.choices.size(); ++i) {
        const auto& choice = present.choices[i];
        lua_createtable(state, 0, 5);
        lua_pushstring(state, choice.id.c_str());
        lua_setfield(state, -2, "id");
        lua_pushstring(state, choice.text.c_str());
        lua_setfield(state, -2, "text");
        lua_pushstring(state, choice.next_node_id.c_str());
        lua_setfield(state, -2, "nextId");
        lua_pushstring(state, choice.tone.c_str());
        lua_setfield(state, -2, "tone");
        lua_createtable(state, static_cast<int>(choice.standing_adjust.size()), 0);
        for (std::size_t j = 0; j < choice.standing_adjust.size(); ++j) {
            lua_createtable(state, 0, 2);
            lua_pushstring(state, choice.standing_adjust[j].faction_id.c_str());
            lua_setfield(state, -2, "factionId");
            lua_pushnumber(state, choice.standing_adjust[j].delta);
            lua_setfield(state, -2, "delta");
            lua_rawseti(state, -2, static_cast<int>(j) + 1);
        }
        lua_setfield(state, -2, "standingAdjust");
        lua_rawseti(state, -2, static_cast<int>(i) + 1);
    }
    lua_setfield(state, -2, "choices");
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

int engine_hud_set_image(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) return 0;
    const char* key = luaL_checkstring(state, 1);
    const char* path = luaL_optstring(state, 2, "");
    host->hud->set_image(key, path ? path : "");
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

int engine_hud_set_color(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) return 0;
    const char* widget_id = luaL_checkstring(state, 1);
    const float r = static_cast<float>(luaL_checknumber(state, 2));
    const float g = static_cast<float>(luaL_checknumber(state, 3));
    const float b = static_cast<float>(luaL_checknumber(state, 4));
    const float a = static_cast<float>(luaL_optnumber(state, 5, 255.0));
    host->hud->set_color(widget_id, r, g, b, a);
    return 0;
}

int engine_hud_clear_color(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) return 0;
    const char* widget_id = luaL_checkstring(state, 1);
    host->hud->clear_color(widget_id);
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

int engine_set_resource(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) return 0;
    const double current = luaL_checknumber(state, 1);
    const double max = luaL_checknumber(state, 2);
    host->hud->set_resource(current, max);
    return 0;
}

int engine_get_resource(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) {
        lua_pushnumber(state, 0);
        lua_pushnumber(state, 0);
        return 2;
    }
    const double current = host->hud->get_number("player.resource").value_or(0.0);
    const double max = host->hud->get_number("player.resourceMax").value_or(0.0);
    lua_pushnumber(state, current);
    lua_pushnumber(state, max);
    return 2;
}

int engine_apply_archetype_hud(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud) return 0;
    const char* archetype_id = luaL_checkstring(state, 1);
    host->hud->apply_archetype_hud(archetype_id);
    return 0;
}

int engine_world_ui_upsert(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->world_ui) return 0;
    const char* id = luaL_checkstring(state, 1);
    luaL_checktype(state, 2, LUA_TTABLE);
    WorldUiBillboard billboard;
    billboard.id = id;
    lua_getfield(state, 2, "x");
    if (lua_isnumber(state, -1)) billboard.world_x = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 2, "y");
    if (lua_isnumber(state, -1)) billboard.world_y = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 2, "z");
    if (lua_isnumber(state, -1)) billboard.world_z = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 2, "text");
    if (lua_isstring(state, -1)) billboard.text = lua_tostring(state, -1);
    lua_pop(state, 1);
    lua_getfield(state, 2, "visible");
    if (lua_isboolean(state, -1)) billboard.visible = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    lua_getfield(state, 2, "barCurrent");
    const bool has_bar_current = lua_isnumber(state, -1) != 0;
    const double bar_current = has_bar_current ? lua_tonumber(state, -1) : 0.0;
    lua_pop(state, 1);
    lua_getfield(state, 2, "barMax");
    const bool has_bar_max = lua_isnumber(state, -1) != 0;
    const double bar_max = has_bar_max ? lua_tonumber(state, -1) : 100.0;
    lua_pop(state, 1);
    if (has_bar_current || has_bar_max) {
        billboard.has_bar = true;
        billboard.bar_max = bar_max > 0.0 ? bar_max : 1.0;
        billboard.bar_current = bar_current;
    }
    lua_getfield(state, 2, "hitJuice");
    if (lua_isboolean(state, -1)) billboard.request_hit_juice = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    if (const auto existing = host->world_ui->get(id)) {
        if (!has_bar_current && !has_bar_max) {
            billboard.has_bar = existing->has_bar;
            billboard.bar_current = existing->bar_current;
            billboard.bar_max = existing->bar_max;
            billboard.bar_color = existing->bar_color;
        }
        if (billboard.text.empty()) billboard.text = existing->text;
        billboard.panel_color = existing->panel_color;
        billboard.border_color = existing->border_color;
        billboard.text_color = existing->text_color;
        billboard.font_size = existing->font_size;
    }
    host->world_ui->upsert(std::move(billboard));
    return 0;
}

int engine_world_ui_clear(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->world_ui) return 0;
    if (lua_gettop(state) >= 1 && lua_isstring(state, 1)) {
        host->world_ui->remove(lua_tostring(state, 1));
    } else {
        host->world_ui->clear();
    }
    return 0;
}

int engine_combat_text(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->combat_text) return 0;
    luaL_checktype(state, 1, LUA_TTABLE);
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool crit = false;
    CombatTextKind kind = CombatTextKind::Hit;
    lua_getfield(state, 1, "x");
    if (lua_isnumber(state, -1)) x = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 1, "y");
    if (lua_isnumber(state, -1)) y = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 1, "z");
    if (lua_isnumber(state, -1)) z = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 1, "crit");
    if (lua_isboolean(state, -1)) crit = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    lua_getfield(state, 1, "kind");
    if (lua_isstring(state, -1)) kind = combat_text_kind_from_id(lua_tostring(state, -1));
    lua_pop(state, 1);
    if (crit) kind = CombatTextKind::Crit;
    lua_getfield(state, 1, "text");
    if (lua_isstring(state, -1)) {
        host->combat_text->spawn_text(x, y, z, lua_tostring(state, -1), kind);
        lua_pop(state, 1);
        return 0;
    }
    lua_pop(state, 1);
    lua_getfield(state, 1, "amount");
    if (lua_isnumber(state, -1)) {
        host->combat_text->spawn(x, y, z, lua_tonumber(state, -1), kind);
    }
    lua_pop(state, 1);
    return 0;
}

int engine_status_apply(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->status_effects) return 0;
    luaL_checktype(state, 1, LUA_TTABLE);
    std::string target_id;
    std::string status = "bleed";
    float damage = 1.0f;
    float duration = 6.0f;
    float interval = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool has_anchor = false;
    lua_getfield(state, 1, "targetId");
    if (lua_isstring(state, -1)) target_id = lua_tostring(state, -1);
    lua_pop(state, 1);
    lua_getfield(state, 1, "status");
    if (lua_isstring(state, -1)) status = lua_tostring(state, -1);
    lua_pop(state, 1);
    lua_getfield(state, 1, "damagePerTick");
    if (lua_isnumber(state, -1)) damage = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 1, "duration");
    if (lua_isnumber(state, -1)) duration = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 1, "tickInterval");
    if (lua_isnumber(state, -1)) interval = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 1, "x");
    if (lua_isnumber(state, -1)) {
        x = static_cast<float>(lua_tonumber(state, -1));
        has_anchor = true;
    }
    lua_pop(state, 1);
    lua_getfield(state, 1, "y");
    if (lua_isnumber(state, -1)) y = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 1, "z");
    if (lua_isnumber(state, -1)) z = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    if (target_id.empty() || !is_known_status_effect_id(status)) return 0;
    if (has_anchor) host->status_effects->set_target_anchor(target_id, x, y, z);
    host->status_effects->apply(target_id, status_effect_kind_from_id(status), damage, duration, interval);
    return 0;
}

int engine_status_clear(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->status_effects) return 0;
    if (lua_gettop(state) >= 1 && lua_isstring(state, 1)) {
        host->status_effects->clear_target(lua_tostring(state, 1));
    } else {
        host->status_effects->clear();
    }
    return 0;
}

int engine_status_set_anchor(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->status_effects) return 0;
    const char* id = luaL_checkstring(state, 1);
    luaL_checktype(state, 2, LUA_TTABLE);
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    lua_getfield(state, 2, "x");
    if (lua_isnumber(state, -1)) x = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 2, "y");
    if (lua_isnumber(state, -1)) y = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    lua_getfield(state, 2, "z");
    if (lua_isnumber(state, -1)) z = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
    host->status_effects->set_target_anchor(id, x, y, z);
    return 0;
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

void sync_coop_ready_room_ui(LuaHost* host) {
    if (!host || !host->ui_stack || !host->game_session) return;
    auto* canvas = host->ui_stack->find_canvas("coop_ready_room");
    if (!canvas) return;
    const bool host_ready = host->game_session->is_ready(0);
    const bool guest_ready = host->game_session->is_ready(1);
    canvas->set_text("coop_ready_room.host_ready", host_ready ? "Host: Ready" : "Host: Not Ready");
    canvas->set_text("coop_ready_room.guest_ready", guest_ready ? "Guest: Ready" : "Guest: Not Ready");
    canvas->set_enabled("ready_start", host->game_session->can_host_start());
    if (auto* host_lobby = host->ui_stack->find_canvas("coop_lobby_host")) {
        host_lobby->set_text("coop_lobby_host.invite",
            std::string("Invite code: ") + host->game_session->invite_code());
        host_lobby->set_text("coop_lobby_host.status",
            host->game_session->slot(1).connected ? "Guest connected — open ready room" : "Waiting for guest…");
    }
}

int engine_ui_canvas_set_enabled(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* canvas_id = luaL_checkstring(state, 1);
    const char* widget_id = luaL_checkstring(state, 2);
    const bool enabled = lua_toboolean(state, 3) != 0;
    if (auto* canvas = host->ui_stack->find_canvas(canvas_id)) canvas->set_enabled(widget_id, enabled);
    return 0;
}

int engine_ui_canvas_set_text(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* canvas_id = luaL_checkstring(state, 1);
    const char* bind = luaL_checkstring(state, 2);
    const char* text = luaL_checkstring(state, 3);
    if (auto* canvas = host->ui_stack->find_canvas(canvas_id)) canvas->set_text(bind, text);
    return 0;
}

int engine_ui_canvas_set_text_typed(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* canvas_id = luaL_checkstring(state, 1);
    const char* bind = luaL_checkstring(state, 2);
    const char* text = luaL_checkstring(state, 3);
    const float cps = static_cast<float>(luaL_optnumber(state, 4, 42.0));
    if (auto* canvas = host->ui_stack->find_canvas(canvas_id)) canvas->set_text_typed(bind, text, cps);
    return 0;
}

int engine_ui_canvas_skip_typewriter(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* canvas_id = luaL_checkstring(state, 1);
    const char* bind = luaL_checkstring(state, 2);
    bool skipped = false;
    if (auto* canvas = host->ui_stack->find_canvas(canvas_id)) skipped = canvas->skip_typewriter(bind);
    lua_pushboolean(state, skipped ? 1 : 0);
    return 1;
}

int engine_ui_canvas_set_image(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* canvas_id = luaL_checkstring(state, 1);
    const char* bind = luaL_checkstring(state, 2);
    const char* path = luaL_optstring(state, 3, "");
    if (auto* canvas = host->ui_stack->find_canvas(canvas_id)) canvas->set_image(bind, path ? path : "");
    return 0;
}

int engine_ui_canvas_set_visible(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* canvas_id = luaL_checkstring(state, 1);
    const char* widget_id = luaL_checkstring(state, 2);
    const bool visible = lua_toboolean(state, 3) != 0;
    if (auto* canvas = host->ui_stack->find_canvas(canvas_id)) canvas->set_visible(widget_id, visible);
    return 0;
}

int engine_ui_canvas_set_color(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* canvas_id = luaL_checkstring(state, 1);
    const char* widget_id = luaL_checkstring(state, 2);
    const float r = static_cast<float>(luaL_checknumber(state, 3));
    const float g = static_cast<float>(luaL_checknumber(state, 4));
    const float b = static_cast<float>(luaL_checknumber(state, 5));
    const float a = static_cast<float>(luaL_optnumber(state, 6, 255.0));
    if (auto* canvas = host->ui_stack->find_canvas(canvas_id)) canvas->set_color(widget_id, r, g, b, a);
    return 0;
}

int engine_ui_canvas_clear_color(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* canvas_id = luaL_checkstring(state, 1);
    const char* widget_id = luaL_checkstring(state, 2);
    if (auto* canvas = host->ui_stack->find_canvas(canvas_id)) canvas->clear_color(widget_id);
    return 0;
}

int engine_ui_theme_set_token(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* name = luaL_checkstring(state, 1);
    const float r = static_cast<float>(luaL_checknumber(state, 2));
    const float g = static_cast<float>(luaL_checknumber(state, 3));
    const float b = static_cast<float>(luaL_checknumber(state, 4));
    const float a = static_cast<float>(luaL_optnumber(state, 5, 255.0));
    host->ui_stack->theme().set_token(name, {r, g, b, a});
    return 0;
}

int engine_ui_theme_get_token(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* name = luaL_checkstring(state, 1);
    const auto token = host->ui_stack->theme().token(name);
    if (!token) return 0;
    lua_pushnumber(state, static_cast<lua_Number>((*token)[0]));
    lua_pushnumber(state, static_cast<lua_Number>((*token)[1]));
    lua_pushnumber(state, static_cast<lua_Number>((*token)[2]));
    lua_pushnumber(state, static_cast<lua_Number>((*token)[3]));
    return 4;
}

int engine_ui_theme_set_role(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* name = luaL_checkstring(state, 1);
    UiThemeRole role;
    role.fill = luaL_optstring(state, 2, "");
    role.text = luaL_optstring(state, 3, "");
    role.border = luaL_optstring(state, 4, "");
    host->ui_stack->theme().set_role(name, std::move(role));
    return 0;
}

int engine_ui_theme_save(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return luaL_error(state, "ui canvas stack is not available");
    const auto saved = host->ui_stack->save_theme();
    if (!saved) return luaL_error(state, "%s", saved.error().message.c_str());
    return 0;
}

int engine_ui_theme_reload(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return luaL_error(state, "ui canvas stack is not available");
    const auto path = host->ui_stack->theme_path();
    const auto loaded = host->ui_stack->load_theme(path);
    if (!loaded) return luaL_error(state, "%s", loaded.error().message.c_str());
    return 0;
}

int engine_ui_canvas_set_offset(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* canvas_id = luaL_checkstring(state, 1);
    const char* widget_id = luaL_checkstring(state, 2);
    const float x = static_cast<float>(luaL_checknumber(state, 3));
    const float y = static_cast<float>(luaL_checknumber(state, 4));
    if (auto* canvas = host->ui_stack->find_canvas(canvas_id)) canvas->set_offset(widget_id, x, y);
    return 0;
}

int engine_ui_canvas_set_size(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* canvas_id = luaL_checkstring(state, 1);
    const char* widget_id = luaL_checkstring(state, 2);
    const float w = static_cast<float>(luaL_checknumber(state, 3));
    const float h = static_cast<float>(luaL_checknumber(state, 4));
    if (auto* canvas = host->ui_stack->find_canvas(canvas_id)) canvas->set_size(widget_id, w, h);
    return 0;
}

int engine_ui_canvas_set_opacity(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->ui_stack) return 0;
    const char* canvas_id = luaL_checkstring(state, 1);
    const char* widget_id = luaL_checkstring(state, 2);
    const float opacity = static_cast<float>(luaL_checknumber(state, 3));
    if (auto* canvas = host->ui_stack->find_canvas(canvas_id)) canvas->set_opacity(widget_id, opacity);
    return 0;
}

int engine_coop_begin_host_lobby(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->game_session) return luaL_error(state, "game session is not available");
    host->game_session->reset_to_menu();
    host->game_session->bind_quest_runtime(host->quest);
    host->game_session->bind_standing_runtime(host->standing);
    host->game_session->bind_flag_runtime(host->flags);
    if (const auto begin = host->game_session->begin_coop_lobby(); !begin)
        return luaL_error(state, "%s", begin.error().message.c_str());
    if (const auto connected = host->game_session->set_slot_connected(0, true, 0); !connected)
        return luaL_error(state, "%s", connected.error().message.c_str());
    sync_coop_ready_room_ui(host);
    return 0;
}

int engine_coop_mock_guest_join(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->game_session) return luaL_error(state, "game session is not available");
    const char* code = luaL_optstring(state, 1, "COOP-LOCAL");
    if (const auto joined = host->game_session->mock_guest_join(code); !joined)
        return luaL_error(state, "%s", joined.error().message.c_str());
    sync_coop_ready_room_ui(host);
    return 0;
}

int engine_coop_set_ready(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->game_session) return luaL_error(state, "game session is not available");
    const int slot = static_cast<int>(luaL_checkinteger(state, 1));
    const bool ready = lua_toboolean(state, 2) != 0;
    if (const auto result = host->game_session->set_ready(slot, ready); !result)
        return luaL_error(state, "%s", result.error().message.c_str());
    sync_coop_ready_room_ui(host);
    lua_pushboolean(state, host->game_session->can_host_start() ? 1 : 0);
    return 1;
}

int engine_coop_toggle_ready(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->game_session) return luaL_error(state, "game session is not available");
    const int slot = static_cast<int>(luaL_checkinteger(state, 1));
    const bool next = !host->game_session->is_ready(slot);
    if (const auto result = host->game_session->set_ready(slot, next); !result)
        return luaL_error(state, "%s", result.error().message.c_str());
    sync_coop_ready_room_ui(host);
    lua_pushboolean(state, next ? 1 : 0);
    return 1;
}

int engine_coop_host_start(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->game_session) return luaL_error(state, "game session is not available");
    if (!host->game_session->can_host_start())
        return luaL_error(state, "both players must be ready and connected before start");
    if (const auto started = host->game_session->start_playing(); !started)
        return luaL_error(state, "%s", started.error().message.c_str());
    return 0;
}

int engine_coop_end_session(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->game_session) return luaL_error(state, "game session is not available");
    if (host->game_session->state() == GameSessionState::Menu) return 0;
    if (const auto ended = host->game_session->end_session(); !ended)
        return luaL_error(state, "%s", ended.error().message.c_str());
    return 0;
}

int engine_coop_can_host_start(lua_State* state) {
    auto* host = host_from_state(state);
    lua_pushboolean(state, host && host->game_session && host->game_session->can_host_start() ? 1 : 0);
    return 1;
}

int engine_coop_invite_code(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->game_session) {
        lua_pushstring(state, "COOP-LOCAL");
        return 1;
    }
    lua_pushstring(state, host->game_session->invite_code().c_str());
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

void push_stack_table(lua_State* state, const InventoryStack& stack) {
    lua_createtable(state, 0, 2);
    lua_pushstring(state, stack.item_id.c_str());
    lua_setfield(state, -2, "itemId");
    lua_pushinteger(state, stack.count);
    lua_setfield(state, -2, "count");
}

void decorate_item_def_fields(lua_State* state, const ItemDef& def) {
    lua_pushstring(state, def.display_name.c_str());
    lua_setfield(state, -2, "displayName");
    lua_pushstring(state, def.kind.c_str());
    lua_setfield(state, -2, "kind");
    lua_pushstring(state, def.icon.c_str());
    lua_setfield(state, -2, "icon");
    lua_pushstring(state, def.notes.c_str());
    lua_setfield(state, -2, "notes");
    lua_pushstring(state, format_item_stat_text(def).c_str());
    lua_setfield(state, -2, "statsText");
    lua_createtable(state, 0, 5);
    lua_pushnumber(state, static_cast<lua_Number>(def.stats.damage_min));
    lua_setfield(state, -2, "damageMin");
    lua_pushnumber(state, static_cast<lua_Number>(def.stats.damage_max));
    lua_setfield(state, -2, "damageMax");
    lua_pushnumber(state, static_cast<lua_Number>(def.stats.attacks_per_second));
    lua_setfield(state, -2, "attacksPerSecond");
    lua_pushnumber(state, static_cast<lua_Number>(def.stats.dps()));
    lua_setfield(state, -2, "dps");
    lua_pushnumber(state, static_cast<lua_Number>(def.stats.heal));
    lua_setfield(state, -2, "heal");
    lua_createtable(state, 0, static_cast<int>(def.stats.modifiers.size()));
    for (const auto& mod : def.stats.modifiers) {
        lua_pushnumber(state, static_cast<lua_Number>(mod.value));
        lua_setfield(state, -2, mod.id.c_str());
    }
    lua_setfield(state, -2, "modifiers");
    lua_createtable(state, static_cast<int>(def.stats.on_hit.size()), 0);
    for (std::size_t i = 0; i < def.stats.on_hit.size(); ++i) {
        const auto& hit = def.stats.on_hit[i];
        lua_createtable(state, 0, 4);
        lua_pushstring(state, hit.status.c_str());
        lua_setfield(state, -2, "status");
        lua_pushnumber(state, static_cast<lua_Number>(hit.damage_per_tick));
        lua_setfield(state, -2, "damagePerTick");
        lua_pushnumber(state, static_cast<lua_Number>(hit.duration_seconds));
        lua_setfield(state, -2, "duration");
        lua_pushnumber(state, static_cast<lua_Number>(hit.tick_interval_seconds));
        lua_setfield(state, -2, "tickInterval");
        lua_rawseti(state, -2, static_cast<int>(i) + 1);
    }
    lua_setfield(state, -2, "onHit");
    lua_setfield(state, -2, "stats");
}

void decorate_stack_if_known(lua_State* state, const ItemCatalogAsset* catalog, const InventoryStack& stack) {
    if (!catalog || stack.empty()) return;
    if (const ItemDef* def = catalog->find(stack.item_id)) {
        decorate_item_def_fields(state, *def);
    }
}

void push_inventory_status(lua_State* state, const InventoryStatusSnapshot& snap, const ItemCatalogAsset* catalog) {
    lua_createtable(state, 0, 14);
    lua_pushinteger(state, snap.bag_capacity);
    lua_setfield(state, -2, "bagCapacity");
    lua_pushinteger(state, snap.selected_hotbar);
    lua_setfield(state, -2, "selectedHotbar");
    lua_pushinteger(state, snap.gold);
    lua_setfield(state, -2, "gold");

    lua_createtable(state, static_cast<int>(snap.bag.size()), 0);
    for (std::size_t i = 0; i < snap.bag.size(); ++i) {
        push_stack_table(state, snap.bag[i]);
        decorate_stack_if_known(state, catalog, snap.bag[i]);
        lua_rawseti(state, -2, static_cast<int>(i) + 1);
    }
    lua_setfield(state, -2, "bag");

    lua_createtable(state, static_cast<int>(snap.hotbar.size()), 0);
    for (std::size_t i = 0; i < snap.hotbar.size(); ++i) {
        push_stack_table(state, snap.hotbar[i]);
        decorate_stack_if_known(state, catalog, snap.hotbar[i]);
        lua_rawseti(state, -2, static_cast<int>(i) + 1);
    }
    lua_setfield(state, -2, "hotbar");

    lua_createtable(state, 0, 7);
    auto push_equip = [&](const char* key, const InventoryStack& stack) {
        push_stack_table(state, stack);
        decorate_stack_if_known(state, catalog, stack);
        lua_setfield(state, -2, key);
    };
    push_equip("head", snap.equipped.head);
    push_equip("chest", snap.equipped.chest);
    push_equip("legs", snap.equipped.legs);
    for (int t = 0; t < kInventoryTrinketSlots; ++t) {
        const std::string key = "trinket" + std::to_string(t);
        push_equip(key.c_str(), snap.equipped.trinkets[static_cast<std::size_t>(t)]);
    }
    lua_setfield(state, -2, "equipped");

    lua_createtable(state, static_cast<int>(snap.ammo.size()), 0);
    for (std::size_t i = 0; i < snap.ammo.size(); ++i) {
        push_stack_table(state, snap.ammo[i]);
        decorate_stack_if_known(state, catalog, snap.ammo[i]);
        lua_rawseti(state, -2, static_cast<int>(i) + 1);
    }
    lua_setfield(state, -2, "ammo");

    lua_pushstring(state, snap.container_id.c_str());
    lua_setfield(state, -2, "containerId");
    lua_pushinteger(state, snap.container_capacity);
    lua_setfield(state, -2, "containerCapacity");
    lua_createtable(state, static_cast<int>(snap.container.size()), 0);
    for (std::size_t i = 0; i < snap.container.size(); ++i) {
        push_stack_table(state, snap.container[i]);
        if (catalog && !snap.container[i].empty()) {
            decorate_stack_if_known(state, catalog, snap.container[i]);
        }
        lua_rawseti(state, -2, static_cast<int>(i) + 1);
    }
    lua_setfield(state, -2, "container");

    lua_createtable(state, 0, 3);
    lua_pushstring(state, snap.ui_selection.region.c_str());
    lua_setfield(state, -2, "region");
    lua_pushinteger(state, snap.ui_selection.index);
    lua_setfield(state, -2, "index");
    lua_pushstring(state, snap.ui_selection.equip_slot.c_str());
    lua_setfield(state, -2, "equipSlot");
    lua_setfield(state, -2, "selection");

    const auto& stats = snap.player_stats;
    lua_createtable(state, 0, 24);
    lua_pushnumber(state, static_cast<lua_Number>(stats.max_health));
    lua_setfield(state, -2, "maxHealth");
    lua_pushnumber(state, static_cast<lua_Number>(stats.max_stamina));
    lua_setfield(state, -2, "maxStamina");
    lua_pushnumber(state, static_cast<lua_Number>(stats.max_magicka));
    lua_setfield(state, -2, "maxMagicka");
    lua_pushnumber(state, static_cast<lua_Number>(stats.armor));
    lua_setfield(state, -2, "armor");
    lua_pushnumber(state, static_cast<lua_Number>(stats.strength));
    lua_setfield(state, -2, "strength");
    lua_pushnumber(state, static_cast<lua_Number>(stats.agility));
    lua_setfield(state, -2, "agility");
    lua_pushnumber(state, static_cast<lua_Number>(stats.intellect));
    lua_setfield(state, -2, "intellect");
    lua_pushnumber(state, static_cast<lua_Number>(stats.magic_resist));
    lua_setfield(state, -2, "magicResist");
    lua_pushnumber(state, static_cast<lua_Number>(stats.poison_resist));
    lua_setfield(state, -2, "poisonResist");
    lua_pushnumber(state, static_cast<lua_Number>(stats.blight_resist));
    lua_setfield(state, -2, "blightResist");
    lua_pushnumber(state, static_cast<lua_Number>(stats.holy_resist));
    lua_setfield(state, -2, "holyResist");
    lua_pushnumber(state, static_cast<lua_Number>(stats.shadow_resist));
    lua_setfield(state, -2, "shadowResist");
    lua_pushnumber(state, static_cast<lua_Number>(stats.crit_chance));
    lua_setfield(state, -2, "critChance");
    lua_pushinteger(state, stats.crit_marbles);
    lua_setfield(state, -2, "critMarbles");
    lua_pushinteger(state, stats.hit_marbles);
    lua_setfield(state, -2, "hitMarbles");
    lua_pushnumber(state, static_cast<lua_Number>(stats.damage_min));
    lua_setfield(state, -2, "damageMin");
    lua_pushnumber(state, static_cast<lua_Number>(stats.damage_max));
    lua_setfield(state, -2, "damageMax");
    lua_pushnumber(state, static_cast<lua_Number>(stats.attacks_per_second));
    lua_setfield(state, -2, "attacksPerSecond");
    lua_pushnumber(state, static_cast<lua_Number>(stats.dps));
    lua_setfield(state, -2, "dps");
    lua_pushnumber(state, static_cast<lua_Number>(stats.damage_multiplier));
    lua_setfield(state, -2, "damageMultiplier");
    lua_pushboolean(state, stats.holding_magic_weapon ? 1 : 0);
    lua_setfield(state, -2, "holdingMagicWeapon");
    lua_pushstring(state, weapon_school_id(stats.weapon_school));
    lua_setfield(state, -2, "weaponSchool");
    lua_setfield(state, -2, "playerStats");
}

int engine_inventory_grant(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const char* item_id = luaL_checkstring(state, 1);
    const int count = static_cast<int>(luaL_optinteger(state, 2, 1));
    const auto result = host->inventory->grant(item_id, count);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_inventory_remove(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const char* item_id = luaL_checkstring(state, 1);
    const int count = static_cast<int>(luaL_optinteger(state, 2, 1));
    const auto result = host->inventory->remove(item_id, count);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_inventory_set_hotbar(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const int slot = static_cast<int>(luaL_checkinteger(state, 1));
    const char* item_id = luaL_checkstring(state, 2);
    const int count = static_cast<int>(luaL_optinteger(state, 3, 1));
    const auto result = host->inventory->set_hotbar(slot, item_id, count);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_inventory_set_equip(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const char* slot = luaL_checkstring(state, 1);
    const char* item_id = luaL_checkstring(state, 2);
    const auto result = host->inventory->set_equip(slot, item_id, 1);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_inventory_select_hotbar(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const int slot = static_cast<int>(luaL_checkinteger(state, 1));
    const auto result = host->inventory->select_hotbar(slot);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_inventory_select(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const char* region = luaL_checkstring(state, 1);
    const int index = static_cast<int>(luaL_optinteger(state, 2, -1));
    const char* equip_slot = luaL_optstring(state, 3, "");
    const auto result = host->inventory->select_ui(region, index, equip_slot ? equip_slot : "");
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_inventory_equip_selected(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const auto result = host->inventory->equip_selected();
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_inventory_unequip_selected(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const auto result = host->inventory->unequip_selected();
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_inventory_move(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const char* from_region = luaL_checkstring(state, 1);
    const int from_index = static_cast<int>(luaL_optinteger(state, 2, -1));
    const char* from_equip = luaL_optstring(state, 3, "");
    const char* to_region = luaL_checkstring(state, 4);
    const int to_index = static_cast<int>(luaL_optinteger(state, 5, -1));
    const char* to_equip = luaL_optstring(state, 6, "");
    const auto result = host->inventory->move_to(from_region, from_index, from_equip ? from_equip : "", to_region,
        to_index, to_equip ? to_equip : "");
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_inventory_status(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    push_inventory_status(state, host->inventory->status(), host->inventory->catalog());
    return 1;
}

int engine_apply_player_stats(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->hud || !host->inventory) return 0;
    apply_player_stats_to_hud(*host->hud, host->inventory->compute_player_stats());
    return 0;
}

int engine_roll_player_attack(lua_State* state) {
    auto* host = host_from_state(state);
    int combo_step = 0;
    if (lua_gettop(state) >= 1 && lua_istable(state, 1)) {
        lua_getfield(state, 1, "comboStep");
        if (lua_isnumber(state, -1)) combo_step = static_cast<int>(lua_tointeger(state, -1));
        lua_pop(state, 1);
    } else if (lua_gettop(state) >= 1 && lua_isnumber(state, 1)) {
        combo_step = static_cast<int>(lua_tointeger(state, 1));
    }
    if (!host || !host->inventory) {
        lua_createtable(state, 0, 4);
        lua_pushnumber(state, 0);
        lua_setfield(state, -2, "damage");
        lua_pushboolean(state, 0);
        lua_setfield(state, -2, "crit");
        lua_pushstring(state, "none");
        lua_setfield(state, -2, "school");
        lua_pushinteger(state, combo_step);
        lua_setfield(state, -2, "comboStep");
        return 1;
    }
    const auto roll = host->inventory->roll_player_attack(combo_step);
    lua_createtable(state, 0, 4);
    lua_pushnumber(state, static_cast<lua_Number>(roll.damage));
    lua_setfield(state, -2, "damage");
    lua_pushboolean(state, roll.crit ? 1 : 0);
    lua_setfield(state, -2, "crit");
    lua_pushstring(state, weapon_school_id(roll.school));
    lua_setfield(state, -2, "school");
    lua_pushinteger(state, roll.combo_step);
    lua_setfield(state, -2, "comboStep");
    return 1;
}

int engine_inventory_def(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const char* item_id = luaL_checkstring(state, 1);
    const ItemDef* def = host->inventory->find_def(item_id);
    if (!def) {
        lua_pushnil(state);
        return 1;
    }
    lua_createtable(state, 0, 8);
    lua_pushstring(state, def->id.c_str());
    lua_setfield(state, -2, "id");
    decorate_item_def_fields(state, *def);
    lua_pushboolean(state, def->icon_only ? 1 : 0);
    lua_setfield(state, -2, "iconOnly");
    return 1;
}

int engine_inventory_open_container(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const char* container_id = luaL_checkstring(state, 1);
    const auto result = host->inventory->open_container(container_id ? container_id : "");
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_inventory_close_container(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    host->inventory->close_container();
    return 0;
}

int engine_inventory_grant_container(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->inventory) return luaL_error(state, "inventory runtime is not available");
    const char* item_id = luaL_checkstring(state, 1);
    const int count = static_cast<int>(luaL_optinteger(state, 2, 1));
    const auto result = host->inventory->grant_container(item_id, count);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
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

int engine_quest_resolve_fork(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->quest) return luaL_error(state, "quest runtime is not available");
    if (!host->flags) return luaL_error(state, "flag runtime is not available");
    const char* quest_id = luaL_checkstring(state, 1);
    const char* fork_id = luaL_checkstring(state, 2);
    const char* outcome_flag = luaL_checkstring(state, 3);
    const auto result = host->quest->resolve_fork(quest_id, fork_id, outcome_flag, *host->flags);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_flag_set(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->flags) return luaL_error(state, "flag runtime is not available");
    const char* flag_id = luaL_checkstring(state, 1);
    const auto result = host->flags->set(flag_id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_flag_clear(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->flags) return luaL_error(state, "flag runtime is not available");
    const char* flag_id = luaL_checkstring(state, 1);
    const auto result = host->flags->clear(flag_id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_flag_has(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->flags) {
        lua_pushboolean(state, 0);
        return 1;
    }
    const char* flag_id = luaL_checkstring(state, 1);
    lua_pushboolean(state, host->flags->has(flag_id) ? 1 : 0);
    return 1;
}

int engine_flag_list(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->flags) {
        lua_createtable(state, 0, 0);
        return 1;
    }
    const auto listed = host->flags->list();
    lua_createtable(state, static_cast<int>(listed.size()), 0);
    for (std::size_t i = 0; i < listed.size(); ++i) {
        lua_pushstring(state, listed[i].c_str());
        lua_rawseti(state, -2, static_cast<int>(i + 1));
    }
    return 1;
}

int engine_dialogue_start(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->dialogue) return luaL_error(state, "dialogue runtime is not available");
    const char* tree_id = luaL_checkstring(state, 1);
    const auto result = host->dialogue->start(tree_id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    sync_dialogue_ui(host);
    return 0;
}

int engine_dialogue_present(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->dialogue) return luaL_error(state, "dialogue runtime is not available");
    const auto result = host->dialogue->present();
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    sync_dialogue_ui(host);
    push_dialogue_present(state, result.value());
    return 1;
}

int engine_dialogue_choose(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->dialogue) return luaL_error(state, "dialogue runtime is not available");
    const char* choice_id = luaL_checkstring(state, 1);
    const auto result =
        dialogue_choose_with_ui(host->ui_stack, host->dialogue, host->dialogue_ui, host->standing, host->flags,
            choice_id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_dialogue_active(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->dialogue) {
        lua_pushboolean(state, 0);
        return 1;
    }
    const bool active = !host->dialogue->tree_id().empty() && !host->dialogue->is_complete();
    lua_pushboolean(state, active ? 1 : 0);
    return 1;
}

int engine_dialogue_reset(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->dialogue) return 0;
    host->dialogue->reset();
    sync_dialogue_ui(host);
    return 0;
}

int engine_start_event_timeline(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->event_timeline) return luaL_error(state, "event timeline runtime is not available");
    const char* sequence_id = luaL_checkstring(state, 1);
    const auto result = host->event_timeline->start(sequence_id);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_cancel_event_timeline(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->event_timeline) return luaL_error(state, "event timeline runtime is not available");
    host->event_timeline->cancel();
    return 0;
}

int engine_event_timeline_control_locked(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->event_timeline) {
        lua_pushboolean(state, 0);
        return 1;
    }
    lua_pushboolean(state, host->event_timeline->control_locked() ? 1 : 0);
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

int engine_animator_set_float(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->animator) return luaL_error(state, "animator runtime is not available");
    const char* entity_id = luaL_checkstring(state, 1);
    const char* name = luaL_checkstring(state, 2);
    const float value = static_cast<float>(luaL_checknumber(state, 3));
    const auto result = host->animator->set_float(entity_id, name, value);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_animator_set_bool(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->animator) return luaL_error(state, "animator runtime is not available");
    const char* entity_id = luaL_checkstring(state, 1);
    const char* name = luaL_checkstring(state, 2);
    const bool value = lua_toboolean(state, 3) != 0;
    const auto result = host->animator->set_bool(entity_id, name, value);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_animator_set_trigger(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->animator) return luaL_error(state, "animator runtime is not available");
    const char* entity_id = luaL_checkstring(state, 1);
    const char* name = luaL_checkstring(state, 2);
    const auto result = host->animator->set_trigger(entity_id, name);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_animator_crossfade(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->animator) return luaL_error(state, "animator runtime is not available");
    const char* entity_id = luaL_checkstring(state, 1);
    const char* state_name = luaL_checkstring(state, 2);
    const float duration = lua_gettop(state) >= 3 ? static_cast<float>(luaL_optnumber(state, 3, 0.15)) : 0.15f;
    const char* layer = lua_gettop(state) >= 4 && lua_isstring(state, 4) ? lua_tostring(state, 4) : "";
    const auto result = host->animator->crossfade(entity_id, state_name, duration, layer ? layer : "");
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_animator_get_state(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->animator) return luaL_error(state, "animator runtime is not available");
    const char* entity_id = luaL_checkstring(state, 1);
    const char* layer = lua_gettop(state) >= 2 && lua_isstring(state, 2) ? lua_tostring(state, 2) : "";
    const auto result = host->animator->current_state(entity_id, layer ? layer : "");
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    lua_pushstring(state, result.value().c_str());
    return 1;
}

int engine_play_sound(lua_State* state) {
    auto* host = host_from_state(state);
    const char* path = luaL_checkstring(state, 1);
    const bool loop = lua_gettop(state) >= 2 ? lua_toboolean(state, 2) != 0 : false;
    // Soft-fail when audio is unavailable so interaction scripts still run in headless suites.
    if (!host || !host->audio || !host->audio->is_initialized()) {
        Logger::instance().write(Severity::Warning, "lua", "play_sound skipped: audio engine is not available");
        return 0;
    }
    const auto result = host->audio->play_project_sound(path, loop);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_play_sound_at(lua_State* state) {
    auto* host = host_from_state(state);
    const char* path = luaL_checkstring(state, 1);
    const float x = static_cast<float>(luaL_checknumber(state, 2));
    const float y = static_cast<float>(luaL_checknumber(state, 3));
    const float z = static_cast<float>(luaL_checknumber(state, 4));
    const bool loop = lua_gettop(state) >= 5 ? lua_toboolean(state, 5) != 0 : false;
    if (!host || !host->audio || !host->audio->is_initialized()) {
        Logger::instance().write(Severity::Warning, "lua", "play_sound_at skipped: audio engine is not available");
        return 0;
    }
    const auto result = host->audio->play_project_sound_at(path, x, y, z, loop);
    if (!result) return luaL_error(state, "%s", result.error().message.c_str());
    return 0;
}

int engine_set_master_volume(lua_State* state) {
    auto* host = host_from_state(state);
    if (!host || !host->audio) return luaL_error(state, "audio engine is not available");
    const float volume = static_cast<float>(luaL_checknumber(state, 1));
    host->audio->set_master_volume(volume);
    return 0;
}

void register_engine_api(lua_State* state, LuaHost* host) {
    lua_pushlightuserdata(state, host);
    lua_setfield(state, LUA_REGISTRYINDEX, kHostRegistryKey);

    lua_createtable(state, 0, 48);
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
    lua_pushcfunction(state, engine_hud_set_image);
    lua_setfield(state, -2, "hud_set_image");
    lua_pushcfunction(state, engine_hud_set_visible);
    lua_setfield(state, -2, "hud_set_visible");
    lua_pushcfunction(state, engine_hud_set_enabled);
    lua_setfield(state, -2, "hud_set_enabled");
    lua_pushcfunction(state, engine_hud_set_color);
    lua_setfield(state, -2, "hud_set_color");
    lua_pushcfunction(state, engine_hud_clear_color);
    lua_setfield(state, -2, "hud_clear_color");
    lua_pushcfunction(state, engine_set_health);
    lua_setfield(state, -2, "set_health");
    lua_pushcfunction(state, engine_get_health);
    lua_setfield(state, -2, "get_health");
    lua_pushcfunction(state, engine_set_resource);
    lua_setfield(state, -2, "set_resource");
    lua_pushcfunction(state, engine_get_resource);
    lua_setfield(state, -2, "get_resource");
    lua_pushcfunction(state, engine_apply_archetype_hud);
    lua_setfield(state, -2, "apply_archetype_hud");
    lua_pushcfunction(state, engine_world_ui_upsert);
    lua_setfield(state, -2, "world_ui_upsert");
    lua_pushcfunction(state, engine_world_ui_clear);
    lua_setfield(state, -2, "world_ui_clear");
    lua_pushcfunction(state, engine_combat_text);
    lua_setfield(state, -2, "combat_text");
    lua_pushcfunction(state, engine_status_apply);
    lua_setfield(state, -2, "status_apply");
    lua_pushcfunction(state, engine_status_clear);
    lua_setfield(state, -2, "status_clear");
    lua_pushcfunction(state, engine_status_set_anchor);
    lua_setfield(state, -2, "status_set_anchor");
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
    lua_pushcfunction(state, engine_ui_canvas_set_enabled);
    lua_setfield(state, -2, "ui_canvas_set_enabled");
    lua_pushcfunction(state, engine_ui_canvas_set_text);
    lua_setfield(state, -2, "ui_canvas_set_text");
    lua_pushcfunction(state, engine_ui_canvas_set_text_typed);
    lua_setfield(state, -2, "ui_canvas_set_text_typed");
    lua_pushcfunction(state, engine_ui_canvas_skip_typewriter);
    lua_setfield(state, -2, "ui_canvas_skip_typewriter");
    lua_pushcfunction(state, engine_ui_canvas_set_image);
    lua_setfield(state, -2, "ui_canvas_set_image");
    lua_pushcfunction(state, engine_ui_canvas_set_visible);
    lua_setfield(state, -2, "ui_canvas_set_visible");
    lua_pushcfunction(state, engine_ui_canvas_set_color);
    lua_setfield(state, -2, "ui_canvas_set_color");
    lua_pushcfunction(state, engine_ui_canvas_clear_color);
    lua_setfield(state, -2, "ui_canvas_clear_color");
    lua_pushcfunction(state, engine_ui_theme_set_token);
    lua_setfield(state, -2, "ui_theme_set_token");
    lua_pushcfunction(state, engine_ui_theme_get_token);
    lua_setfield(state, -2, "ui_theme_get_token");
    lua_pushcfunction(state, engine_ui_theme_set_role);
    lua_setfield(state, -2, "ui_theme_set_role");
    lua_pushcfunction(state, engine_ui_theme_save);
    lua_setfield(state, -2, "ui_theme_save");
    lua_pushcfunction(state, engine_ui_theme_reload);
    lua_setfield(state, -2, "ui_theme_reload");
    lua_pushcfunction(state, engine_ui_canvas_set_offset);
    lua_setfield(state, -2, "ui_canvas_set_offset");
    lua_pushcfunction(state, engine_ui_canvas_set_size);
    lua_setfield(state, -2, "ui_canvas_set_size");
    lua_pushcfunction(state, engine_ui_canvas_set_opacity);
    lua_setfield(state, -2, "ui_canvas_set_opacity");
    lua_pushcfunction(state, engine_coop_begin_host_lobby);
    lua_setfield(state, -2, "coop_begin_host_lobby");
    lua_pushcfunction(state, engine_coop_mock_guest_join);
    lua_setfield(state, -2, "coop_mock_guest_join");
    lua_pushcfunction(state, engine_coop_set_ready);
    lua_setfield(state, -2, "coop_set_ready");
    lua_pushcfunction(state, engine_coop_toggle_ready);
    lua_setfield(state, -2, "coop_toggle_ready");
    lua_pushcfunction(state, engine_coop_host_start);
    lua_setfield(state, -2, "coop_host_start");
    lua_pushcfunction(state, engine_coop_end_session);
    lua_setfield(state, -2, "coop_end_session");
    lua_pushcfunction(state, engine_coop_can_host_start);
    lua_setfield(state, -2, "coop_can_host_start");
    lua_pushcfunction(state, engine_coop_invite_code);
    lua_setfield(state, -2, "coop_invite_code");
    lua_pushcfunction(state, engine_inventory_grant);
    lua_setfield(state, -2, "inventory_grant");
    lua_pushcfunction(state, engine_inventory_remove);
    lua_setfield(state, -2, "inventory_remove");
    lua_pushcfunction(state, engine_inventory_set_hotbar);
    lua_setfield(state, -2, "inventory_set_hotbar");
    lua_pushcfunction(state, engine_inventory_set_equip);
    lua_setfield(state, -2, "inventory_set_equip");
    lua_pushcfunction(state, engine_inventory_select_hotbar);
    lua_setfield(state, -2, "inventory_select_hotbar");
    lua_pushcfunction(state, engine_inventory_select);
    lua_setfield(state, -2, "inventory_select");
    lua_pushcfunction(state, engine_inventory_equip_selected);
    lua_setfield(state, -2, "inventory_equip_selected");
    lua_pushcfunction(state, engine_inventory_unequip_selected);
    lua_setfield(state, -2, "inventory_unequip_selected");
    lua_pushcfunction(state, engine_inventory_move);
    lua_setfield(state, -2, "inventory_move");
    lua_pushcfunction(state, engine_inventory_status);
    lua_setfield(state, -2, "inventory_status");
    lua_pushcfunction(state, engine_apply_player_stats);
    lua_setfield(state, -2, "apply_player_stats");
    lua_pushcfunction(state, engine_roll_player_attack);
    lua_setfield(state, -2, "roll_player_attack");
    lua_pushcfunction(state, engine_inventory_def);
    lua_setfield(state, -2, "inventory_def");
    lua_pushcfunction(state, engine_inventory_open_container);
    lua_setfield(state, -2, "inventory_open_container");
    lua_pushcfunction(state, engine_inventory_close_container);
    lua_setfield(state, -2, "inventory_close_container");
    lua_pushcfunction(state, engine_inventory_grant_container);
    lua_setfield(state, -2, "inventory_grant_container");
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
    lua_pushcfunction(state, engine_quest_resolve_fork);
    lua_setfield(state, -2, "quest_resolve_fork");
    lua_pushcfunction(state, engine_flag_set);
    lua_setfield(state, -2, "flag_set");
    lua_pushcfunction(state, engine_flag_clear);
    lua_setfield(state, -2, "flag_clear");
    lua_pushcfunction(state, engine_flag_has);
    lua_setfield(state, -2, "flag_has");
    lua_pushcfunction(state, engine_flag_list);
    lua_setfield(state, -2, "flag_list");
    lua_pushcfunction(state, engine_dialogue_start);
    lua_setfield(state, -2, "dialogue_start");
    lua_pushcfunction(state, engine_dialogue_present);
    lua_setfield(state, -2, "dialogue_present");
    lua_pushcfunction(state, engine_dialogue_choose);
    lua_setfield(state, -2, "dialogue_choose");
    lua_pushcfunction(state, engine_dialogue_active);
    lua_setfield(state, -2, "dialogue_active");
    lua_pushcfunction(state, engine_dialogue_reset);
    lua_setfield(state, -2, "dialogue_reset");
    lua_pushcfunction(state, engine_start_event_timeline);
    lua_setfield(state, -2, "start_event_timeline");
    lua_pushcfunction(state, engine_cancel_event_timeline);
    lua_setfield(state, -2, "cancel_event_timeline");
    lua_pushcfunction(state, engine_event_timeline_control_locked);
    lua_setfield(state, -2, "event_timeline_control_locked");
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
    lua_pushcfunction(state, engine_animator_set_float);
    lua_setfield(state, -2, "animator_set_float");
    lua_pushcfunction(state, engine_animator_set_bool);
    lua_setfield(state, -2, "animator_set_bool");
    lua_pushcfunction(state, engine_animator_set_trigger);
    lua_setfield(state, -2, "animator_set_trigger");
    lua_pushcfunction(state, engine_animator_crossfade);
    lua_setfield(state, -2, "animator_crossfade");
    lua_pushcfunction(state, engine_animator_get_state);
    lua_setfield(state, -2, "animator_get_state");
    lua_pushcfunction(state, engine_play_sound);
    lua_setfield(state, -2, "play_sound");
    lua_pushcfunction(state, engine_play_sound_at);
    lua_setfield(state, -2, "play_sound_at");
    lua_pushcfunction(state, engine_set_master_volume);
    lua_setfield(state, -2, "set_master_volume");
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

void LuaRuntime::blackboard_set_bool(const std::string& key, bool value) {
    if (!impl_) return;
    ScriptBlackboardEntry entry;
    entry.type = ScriptBlackboardType::Bool;
    entry.bool_value = value;
    impl_->host.blackboard[key] = std::move(entry);
}

void LuaRuntime::blackboard_set_number(const std::string& key, double value) {
    if (!impl_) return;
    ScriptBlackboardEntry entry;
    entry.type = ScriptBlackboardType::Number;
    entry.number_value = value;
    impl_->host.blackboard[key] = std::move(entry);
}

void LuaRuntime::blackboard_clear() {
    if (impl_) impl_->host.blackboard.clear();
}

void LuaRuntime::blackboard_erase_prefix(const std::string& prefix) {
    if (!impl_ || prefix.empty()) return;
    auto& board = impl_->host.blackboard;
    for (auto it = board.begin(); it != board.end();) {
        if (it->first.compare(0, prefix.size(), prefix) == 0)
            it = board.erase(it);
        else
            ++it;
    }
}

void LuaRuntime::set_hud_runtime(HudRuntime* hud) noexcept {
    if (impl_) impl_->host.hud = hud;
}

void LuaRuntime::set_ui_canvas_stack(UiCanvasStack* stack) noexcept {
    if (impl_) impl_->host.ui_stack = stack;
}

void LuaRuntime::set_world_ui_billboards(WorldUiBillboardRuntime* billboards) noexcept {
    if (impl_) impl_->host.world_ui = billboards;
}

void LuaRuntime::set_combat_text(CombatTextRuntime* combat_text) noexcept {
    if (impl_) impl_->host.combat_text = combat_text;
}

void LuaRuntime::set_status_effects(StatusEffectRuntime* status_effects) noexcept {
    if (impl_) impl_->host.status_effects = status_effects;
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

void LuaRuntime::set_flag_runtime(FlagRuntime* flags) noexcept {
    if (impl_) impl_->host.flags = flags;
}

void LuaRuntime::set_inventory_runtime(InventoryRuntime* inventory) noexcept {
    if (impl_) impl_->host.inventory = inventory;
}

void LuaRuntime::set_animator_runtime(AnimatorRuntime* animator) noexcept {
    if (impl_) impl_->host.animator = animator;
}

void LuaRuntime::set_event_timeline_runtime(EventTimelineRuntime* timeline) noexcept {
    if (impl_) impl_->host.event_timeline = timeline;
}

void LuaRuntime::set_dialogue_runtime(DialogueRuntime* dialogue) noexcept {
    if (impl_) impl_->host.dialogue = dialogue;
}

DialogueUiSession& LuaRuntime::dialogue_ui_session() noexcept {
    static DialogueUiSession fallback{};
    if (!impl_) return fallback;
    return impl_->host.dialogue_ui;
}

const DialogueUiSession& LuaRuntime::dialogue_ui_session() const noexcept {
    static const DialogueUiSession fallback{};
    if (!impl_) return fallback;
    return impl_->host.dialogue_ui;
}

void LuaRuntime::set_audio_engine(AudioEngine* audio) noexcept {
    if (impl_) impl_->host.audio = audio;
}

void LuaRuntime::set_game_session(GameSession* session) noexcept {
    if (impl_) impl_->host.game_session = session;
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
    if (event.contact_point) {
        payload["contactPoint"] = {{"x", event.contact_point->x}, {"y", event.contact_point->y},
            {"z", event.contact_point->z}};
    }
    if (const auto result = call_handler(binding->second.handler, payload.dump()); !result)
        recent_errors_.push_back(result.error());
}

void LuaRuntime::dispatch_interaction_use(const std::string& interaction_id) {
    if (interaction_id.empty()) return;
    const auto binding = interaction_bindings_.find(interaction_id);
    if (binding == interaction_bindings_.end()) return;
    nlohmann::json payload;
    payload["type"] = "use";
    payload["interactionId"] = interaction_id;
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
    payload["comboStep"] = event.attacker_combo_step;
    payload["chainHop"] = event.chain_hop;
    payload["blocked"] = event.blocked;
    if (event.contact_point) {
        payload["contactPoint"] = {{"x", event.contact_point->x}, {"y", event.contact_point->y},
            {"z", event.contact_point->z}};
    }
    if (event.hurt_entity_position) {
        payload["hurtEntityPosition"] = {{"x", event.hurt_entity_position->x},
            {"y", event.hurt_entity_position->y}, {"z", event.hurt_entity_position->z}};
    }
    if (const auto result = call_handler(binding->second.handler, payload.dump()); !result)
        recent_errors_.push_back(result.error());
}

void LuaRuntime::dispatch_status_tick(const StatusTickEvent& event) {
    if (!impl_ || !impl_->state) return;
    lua_getglobal(impl_->state, "on_status_tick");
    if (!lua_isfunction(impl_->state, -1)) {
        lua_pop(impl_->state, 1);
        return;
    }
    nlohmann::json payload;
    payload["targetId"] = event.target_id;
    payload["status"] = status_effect_kind_id(event.kind);
    payload["amount"] = event.amount;
    payload["stacks"] = event.stacks;
    payload["x"] = event.world_x;
    payload["y"] = event.world_y;
    payload["z"] = event.world_z;
    lua_pushstring(impl_->state, payload.dump().c_str());
    if (lua_pcall(impl_->state, 1, 0, 0) != LUA_OK) {
        const std::string message = lua_tostring(impl_->state, -1) ? lua_tostring(impl_->state, -1) : "on_status_tick failed";
        lua_pop(impl_->state, 1);
        recent_errors_.push_back(lua_error("SCRIPT-HANDLER-FAILED", message, "Fix on_status_tick."));
    }
}

void LuaRuntime::dispatch_ui_button(const std::string& bind_id, const std::string& canvas_id,
    const std::string& widget_id) {
    if (impl_ && impl_->host.ui_stack) {
        if (bind_id == "dialogue.continue") {
            if (HudRuntime* canvas = impl_->host.ui_stack->find_canvas("dialogue")) {
                if (canvas->skip_typewriter("dialogue.body")) return;
            }
            if (dialogue_advance_continue(impl_->host.ui_stack, impl_->host.dialogue, impl_->host.dialogue_ui)) {
                return;
            }
        }
        if (bind_id == "prologue.continue") {
            if (HudRuntime* canvas = impl_->host.ui_stack->find_canvas("prologue")) {
                if (canvas->skip_typewriter("prologue.body")) return;
            }
        }
        if (bind_id.rfind("dialogue.choice_", 0) == 0 && impl_->host.dialogue) {
            const std::string suffix = bind_id.substr(std::string("dialogue.choice_").size());
            int slot = 0;
            try {
                slot = std::stoi(suffix);
            } catch (...) {
                slot = 0;
            }
            if (slot > 0) {
                if (const auto result = dialogue_activate_choice_slot(impl_->host.ui_stack, impl_->host.dialogue,
                        impl_->host.dialogue_ui, impl_->host.standing, impl_->host.flags, slot);
                    !result) {
                    recent_errors_.push_back(result.error());
                }
                return;
            }
        }
    }
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

void LuaRuntime::dispatch_animation_event(const AnimatorFiredEvent& event) {
    if (!impl_ || !impl_->state) return;
    lua_getglobal(impl_->state, "on_animation_event");
    if (!lua_isfunction(impl_->state, -1)) {
        lua_pop(impl_->state, 1);
        return; // optional global — no spam when gameplay has not defined a handler
    }
    lua_pop(impl_->state, 1);

    nlohmann::json payload;
    payload["entityId"] = event.entity_id;
    payload["name"] = event.name;
    payload["state"] = event.state;
    payload["layer"] = event.layer;
    payload["time"] = event.time;
    try {
        payload["payload"] = nlohmann::json::parse(event.payload_json.empty() ? "{}" : event.payload_json);
    } catch (...) {
        payload["payload"] = nlohmann::json::object();
    }
    if (const auto result = call_handler("on_animation_event", payload.dump()); !result)
        recent_errors_.push_back(result.error());
}

void LuaRuntime::dispatch_event_timeline_emit(const EventTimelineEmitEvent& event) {
    if (!impl_ || !impl_->state) return;
    lua_getglobal(impl_->state, "on_event_timeline_emit");
    if (!lua_isfunction(impl_->state, -1)) {
        lua_pop(impl_->state, 1);
        return; // optional — missing handler silent (DEC-0045 / TICKET-0221)
    }
    lua_pop(impl_->state, 1);

    nlohmann::json payload;
    payload["sequenceId"] = event.sequence_id;
    payload["name"] = event.name;
    try {
        payload["payload"] = nlohmann::json::parse(event.payload_json.empty() ? "{}" : event.payload_json);
    } catch (...) {
        payload["payload"] = nlohmann::json::object();
    }
    if (const auto result = call_handler("on_event_timeline_emit", payload.dump()); !result)
        recent_errors_.push_back(result.error());
}

void LuaRuntime::dispatch_update(float dt_seconds) {
    if (!impl_ || !impl_->state) return;
    if (!(dt_seconds > 0.0f) || !std::isfinite(dt_seconds)) return;
    lua_getglobal(impl_->state, "on_update");
    if (!lua_isfunction(impl_->state, -1)) {
        lua_pop(impl_->state, 1);
        return;
    }
    lua_pop(impl_->state, 1);
    nlohmann::json payload;
    payload["dt"] = dt_seconds;
    if (const auto result = call_handler("on_update", payload.dump()); !result)
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
