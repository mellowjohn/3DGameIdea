#include "engine/automation/mcp_server.h"

#include "engine/automation/automation_trace.h"
#include "engine/automation/editor_bridge.h"
#include "engine/automation/editor_session.h"
#include "engine/automation/world_forge_commands.h"
#include "engine/core/result.h"
#include "engine/assets/prefab_asset.h"
#include "engine/assets/hud_asset.h"
#include "engine/assets/ui_canvas_asset.h"
#include "engine/assets/ui_canvas_mutate.h"
#include "engine/scripting/lua_runtime.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#endif

namespace engine {
namespace {

void configure_mcp_stdio() {
#ifdef _WIN32
    (void)_setmode(_fileno(stdin), _O_BINARY);
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);
}

bool read_exact(std::istream& stream, char* buffer, std::size_t size) {
    std::size_t total = 0;
    while (total < size) {
        stream.read(buffer + total, static_cast<std::streamsize>(size - total));
        const auto read = static_cast<std::size_t>(stream.gcount());
        if (read == 0) return false;
        total += read;
    }
    return true;
}

bool is_content_length_header(const std::string& line) {
    if (line.size() < 15) return false;
    for (std::size_t i = 0; i < 15; ++i) {
        if (std::tolower(static_cast<unsigned char>(line[i])) != "content-length:"[i]) return false;
    }
    return true;
}

std::size_t parse_content_length(const std::string& line) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) return 0;
    std::string value = line.substr(colon + 1);
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
    return static_cast<std::size_t>(std::stoul(value));
}

bool client_uses_ndjson = false;

std::string read_message() {
    while (true) {
        std::string line;
        if (!std::getline(std::cin, line)) return {};
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (is_content_length_header(line)) {
            const auto length = parse_content_length(line);
            std::string blank;
            if (!std::getline(std::cin, blank)) return {};
            if (!blank.empty() && blank.back() == '\r') blank.pop_back();

            std::string payload(length, '\0');
            if (length > 0 && !read_exact(std::cin, payload.data(), length)) return {};
            return payload;
        }

        if (line.front() == '{') {
            client_uses_ndjson = true;
            AutomationTrace::log(AutomationTraceChannel::Mcp, "frame_ndjson");
            return line;
        }

        AutomationTrace::log(AutomationTraceChannel::Mcp, "frame_skip",
            {{"line", line.substr(0, std::min<std::size_t>(line.size(), 80))}});
    }
}

void write_message(const std::string& payload) {
    if (client_uses_ndjson) {
        std::fwrite(payload.data(), 1, payload.size(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
        return;
    }
    const auto header = "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n";
    std::fwrite(header.data(), 1, header.size(), stdout);
    std::fwrite(payload.data(), 1, payload.size(), stdout);
    std::fflush(stdout);
}

nlohmann::json tool_text_content(const std::string& text) {
    return nlohmann::json{{"type", "text"}, {"text", text}};
}

constexpr const char* k_live_automation_hint =
    "Enable \"MCP connection\" in the editor Diagnostics panel, then retry.";

nlohmann::json bridge_to_tool_result(const EditorBridgeResponse& response) {
    nlohmann::json payload;
    payload["schemaVersion"] = response.schema_version;
    payload["exitCode"] = static_cast<int>(response.exit_code);
    payload["summary"] = response.summary;
    payload["changedObjectIds"] = response.changed_object_ids;
    payload["metadata"] = response.metadata;
    nlohmann::json diagnostics = nlohmann::json::array();
    for (const auto& diagnostic : response.diagnostics) diagnostics.push_back(nlohmann::json::parse(diagnostic.to_json()));
    payload["diagnostics"] = diagnostics;
    return {{"content", nlohmann::json::array({tool_text_content(payload.dump(2))})}};
}

nlohmann::json command_to_tool_result(const CommandResponse& response) {
    return {{"content", nlohmann::json::array({tool_text_content(response.to_json())})}};
}

EditorBridgeResponse forward_asset_operation(const std::filesystem::path& project_root, const std::string& operation,
    const nlohmann::json& arguments) {
    EditorBridgeClient client(project_root);
    if (client.is_editor_running()) {
        EditorBridgeRequest request;
        request.request_id = make_correlation_id();
        request.operation = operation;
        request.params_json = arguments.dump();
        return client.send(request);
    }
    AssetRegistry assets;
    std::map<std::string, PrefabAsset> catalog;
    EditorSessionContext context;
    context.project_root = project_root;
    context.assets = &assets;
    context.prefab_catalog = &catalog;
    return execute_editor_operation(context, operation, arguments.dump());
}

EditorBridgeResponse forward_to_editor(const std::filesystem::path& project_root, const std::string& operation,
    const nlohmann::json& params) {
    const auto started = std::chrono::steady_clock::now();
    EditorBridgeClient client(project_root);
    const bool editor_running = client.is_editor_running();
    AutomationTrace::log(AutomationTraceChannel::Mcp, "bridge_probe",
        {{"operation", operation}, {"editorRunning", editor_running ? "true" : "false"}});
    if (!editor_running) {
        EditorBridgeResponse response;
        response.schema_version = 1;
        response.request_id = make_correlation_id();
        response.exit_code = ExitCode::Unavailable;
        response.summary = "Editor live automation is not connected";
        return response;
    }
    EditorBridgeRequest request;
    request.request_id = make_correlation_id();
    request.operation = operation;
    if (!params.is_null()) request.params_json = params.dump();
    const auto response = client.send(request);
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    AutomationTrace::log(AutomationTraceChannel::Mcp, "bridge_response",
        {{"operation", operation},
            {"requestId", response.request_id},
            {"exitCode", std::to_string(static_cast<int>(response.exit_code))},
            {"summary", response.summary},
            {"elapsedMs", std::to_string(elapsed_ms)}});
    return response;
}

const char* k_tools_list_json = R"([
    {
        "name": "engine_editor_status",
        "description": "Report whether the editor has live automation enabled and basic session state.",
        "inputSchema": { "type": "object", "properties": {}, "required": [] }
    },
    {
        "name": "engine_scene_plan",
        "description": "Classify whether a change belongs in scene data, prefab assets, Lua scripts, or C++ engine code.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "description": { "type": "string" },
                "targetPath": { "type": "string" }
            },
            "required": ["description"]
        }
    },
    {
        "name": "engine_scene_apply",
        "description": "Apply live scene edits through the editor command history. Use action batch with ops[] for multi-edit in one undo step. Use snapToTerrain on place/move to align Y with terrain. Use action sample_terrain to query ground height. Component actions: add_component, remove_component, set_component.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "action": { "type": "string" },
                "prefab": { "type": "string" },
                "entityId": { "type": "string" },
                "name": { "type": "string" },
                "transform": { "type": "object" },
                "characterAsset": { "type": "string" },
                "snapToTerrain": { "type": "boolean" },
                "groundOffset": { "type": "number" },
                "x": { "type": "number" },
                "z": { "type": "number" },
                "label": { "type": "string" },
                "save": { "type": "boolean" },
                "componentId": { "type": "string" },
                "type": { "type": "string" },
                "component": { "type": "object" },
                "data": { "type": "object" },
                "ops": {
                    "type": "array",
                    "items": { "type": "object" }
                }
            },
            "required": ["action"]
        }
    },
    {
        "name": "engine_entity_component_apply",
        "description": "Add, remove, or set a component on a scene entity (dedicated MCP path; same commands as engine_scene_apply component actions).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "action": { "type": "string" },
                "entityId": { "type": "string" },
                "componentId": { "type": "string" },
                "type": { "type": "string" },
                "component": { "type": "object" },
                "data": { "type": "object" }
            },
            "required": ["action", "entityId"]
        }
    },
    {
        "name": "engine_prefab_component_apply",
        "description": "Write prefab JSON that includes collision/components, validate, refresh catalog, and propagate to non-overridden scene instances.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": { "type": "string" },
                "json": { "type": "object" },
                "source": { "type": "string" },
                "refreshCatalog": { "type": "boolean" }
            },
            "required": ["path"]
        }
    },
    {
        "name": "engine_prefab_apply",
        "description": "Create or update a prefab JSON asset, validate it, and refresh the editor asset browser catalog.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": { "type": "string" },
                "json": { "type": "object" },
                "source": { "type": "string" },
                "refreshCatalog": { "type": "boolean" }
            },
            "required": ["path"]
        }
    },
    {
        "name": "engine_asset_apply",
        "description": "Create or update a prefab or material asset, validate it, and refresh the editor asset browser. Use action refresh_catalog to rescan without writing.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "action": { "type": "string" },
                "path": { "type": "string" },
                "kind": { "type": "string" },
                "json": { "type": "object" },
                "source": { "type": "string" },
                "refreshCatalog": { "type": "boolean" }
            },
            "required": []
        }
    },
    {
        "name": "engine_lua_apply",
        "description": "Write a Lua gameplay script asset for hot reload.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": { "type": "string" },
                "source": { "type": "string" }
            },
            "required": ["path", "source"]
        }
    },
    {
        "name": "engine_hud_apply",
        "description": "Write a UI canvas (*.uicanvas.json) or legacy HUD (*.hud.json) and hot reload during play test. Widgets: bar, text, panel, button. Prefer *.uicanvas.json with designResolution.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": { "type": "string" },
                "source": { "type": "string" }
            },
            "required": ["path", "source"]
        }
    },
    {
        "name": "engine_ui_canvas_mutate",
        "description": "Structural edit of a *.uicanvas.json: action=add|remove|move|resize|style. Pass path plus id and fields (offset/delta/size/color/fontSize/opacity/visible/enabled/text/textAlign/textVAlign). Hot-reloads when editor is live; allowed during play test.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": { "type": "string" },
                "action": { "type": "string" },
                "id": { "type": "string" },
                "type": { "type": "string" },
                "offset": { "type": "array", "items": { "type": "number" } },
                "delta": { "type": "array", "items": { "type": "number" } },
                "size": { "type": "array", "items": { "type": "number" } },
                "color": { "type": "array", "items": { "type": "number" } },
                "fontSize": { "type": "number" },
                "opacity": { "type": "number" },
                "visible": { "type": "boolean" },
                "enabled": { "type": "boolean" },
                "text": { "type": "string" },
                "textAlign": { "type": "string" },
                "textVAlign": { "type": "string" },
                "label": { "type": "string" },
                "bind": { "type": "string" },
                "anchor": { "type": "string" }
            },
            "required": ["path", "action"]
        }
    },
    {
        "name": "engine_ui_stack",
        "description": "Engine-owned UI canvas stack. action=register|push|pop|show|hide|clear|status. register/push/show take id (and path to register). Equal to Lua engine.ui_*. Requires live editor MCP. Allowed during play test.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "action": { "type": "string" },
                "id": { "type": "string" },
                "path": { "type": "string" }
            },
            "required": ["action"]
        }
    },
    {
        "name": "engine_lua_call",
        "description": "Dispatch a live Lua gameplay handler without physical overlap. kind=interaction|combatHurt|handler. For interaction/combatHurt pass binding id (e.g. use_campfire, body). For handler pass handler name. Optional payload object merges/overrides defaults. Requires live editor MCP.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "kind": { "type": "string" },
                "id": { "type": "string" },
                "handler": { "type": "string" },
                "type": { "type": "string" },
                "payload": { "type": "object" }
            },
            "required": ["kind"]
        }
    },
    {
        "name": "engine_quest_call",
        "description": "Drive session QuestRuntime for agent testing (DEC-0028). kind=start|complete_objective|abandon|status|list. Pass questId; complete_objective also needs objectiveId. Returns status metadata (currentObjectiveId, completedObjectiveIds). Requires live editor MCP. Allowed during play test.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "kind": { "type": "string" },
                "questId": { "type": "string" },
                "objectiveId": { "type": "string" }
            },
            "required": ["kind"]
        }
    },
    {
        "name": "engine_standing_call",
        "description": "Drive session StandingRuntime for agent testing (DEC-0029). kind=get|set|adjust|rank|meets|lock_in|list. Pass factionId (except list/lock_in); set needs score; adjust needs delta; meets needs minScore and/or minRankId. Requires live editor MCP. Allowed during play test.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "kind": { "type": "string" },
                "factionId": { "type": "string" },
                "score": { "type": "number" },
                "delta": { "type": "number" },
                "minScore": { "type": "number" },
                "minRankId": { "type": "string" }
            },
            "required": ["kind"]
        }
    },
    {
        "name": "engine_terrain_apply",
        "description": "Apply live terrain height/paint/foliage through the editor sculpt stores. Prefer action batch with ops[] for many strokes (one bridge round-trip and one reload). Single actions: raise, lower, flatten, paint, paint_foliage, paint_foliage_mixed, sample, undo, redo, save. paint_foliage uses layer id/index (grass/flower/bush/...) and optional erase. Mutate/save require editor MCP; sample works offline. Flatten blends toward targetHeight (default: height at x/z).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "action": { "type": "string" },
                "x": { "type": "number" },
                "z": { "type": "number" },
                "radius": { "type": "number" },
                "strength": { "type": "number" },
                "targetHeight": { "type": "number" },
                "material": { "type": "string" },
                "layer": {
                    "type": "string",
                    "description": "Foliage palette id (grass, flower, bush, bush_wide, bush_tall) or numeric index as a string"
                },
                "erase": { "type": "boolean" },
                "mode": { "type": "string" },
                "kind": { "type": "string" },
                "groundOffset": { "type": "number" },
                "save": { "type": "boolean" },
                "ops": {
                    "type": "array",
                    "items": { "type": "object" }
                }
            },
            "required": ["action"]
        }
    },
    {
        "name": "engine_project_validate",
        "description": "Validate the project manifest, world, and assets.",
        "inputSchema": { "type": "object", "properties": {}, "required": [] }
    },
    {
        "name": "engine_world_forge_apply",
        "description": "Read, validate, write, or import World Forge narrative assets (factions / pantheon / archetypes / resources / relationships / map / quests / dialogues). Actions: get|validate|apply|import_twee. Pass kind=factions|pantheon|archetypes|resources|relationships|map|quests|dialogues and/or path to *.worldforge.json. apply requires json object or source string. import_twee (kind=dialogues) requires tweePath + treeId; optional displayName, parentQuestId, entryNodeId, storyRef. Works offline. Not Scene/Sculpt.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "action": { "type": "string" },
                "kind": { "type": "string" },
                "path": { "type": "string" },
                "json": {},
                "source": { "type": "string" },
                "tweePath": { "type": "string" },
                "sourcePath": { "type": "string" },
                "treeId": { "type": "string" },
                "displayName": { "type": "string" },
                "parentQuestId": { "type": "string" },
                "entryNodeId": { "type": "string" },
                "storyRef": { "type": "string" }
            },
            "required": ["action"]
        }
    }
])";

nlohmann::json handle_tools_call(const std::filesystem::path& project_root, const nlohmann::json& params) {
    const auto tool_name = params.value("name", std::string{});
    AutomationTrace::log(AutomationTraceChannel::Mcp, "tool_call", {{"tool", tool_name}});
    const auto arguments = params.value("arguments", nlohmann::json::object());
    if (tool_name == "engine_editor_status") {
        auto response = forward_to_editor(project_root, "editor_status", nlohmann::json::object());
        if (response.exit_code == ExitCode::Unavailable) {
            nlohmann::json payload;
            payload["schemaVersion"] = 1;
            payload["exitCode"] = 0;
            payload["summary"] = "Editor live automation is not connected";
            payload["metadata"] = {{"editorRunning", "false"}, {"liveAutomationEnabled", "false"},
                {"projectRoot", project_root.generic_string()}};
            payload["recommendation"] = k_live_automation_hint;
            return {{"content", nlohmann::json::array({tool_text_content(payload.dump(2))})}};
        }
        return bridge_to_tool_result(response);
    }
    if (tool_name == "engine_scene_plan") {
        if (arguments.contains("description")) {
            const auto plan = classify_scene_plan(arguments["description"].get<std::string>(),
                arguments.value("targetPath", std::string{}));
            nlohmann::json payload;
            payload["summary"] = plan.summary;
            payload["targetKind"] = plan.target_kind;
            payload["requiresCompile"] = plan.requires_compile;
            payload["requiresReload"] = plan.requires_reload;
            payload["recommendation"] = plan.recommendation;
            return {{"content", nlohmann::json::array({tool_text_content(payload.dump(2))})}};
        }
        return bridge_to_tool_result(forward_to_editor(project_root, "scene_plan", arguments));
    }
    if (tool_name == "engine_prefab_apply")
        return bridge_to_tool_result(forward_asset_operation(project_root, "prefab_apply", arguments));
    if (tool_name == "engine_asset_apply")
        return bridge_to_tool_result(forward_asset_operation(project_root, "asset_apply", arguments));
    if (tool_name == "engine_lua_apply") {
        EditorBridgeClient client(project_root);
        if (!client.is_editor_running()) {
            const auto relative = arguments.value("path", std::string{});
            const auto source = arguments.value("source", std::string{});
            if (relative.empty() || source.empty()) {
                return {{"isError", true},
                    {"content", nlohmann::json::array({tool_text_content("path and source are required")})}};
            }
            const auto written = write_lua_script_atomic(project_root / relative, source);
            if (!written) {
                return {{"isError", true},
                    {"content", nlohmann::json::array({tool_text_content(written.error().to_json())})}};
            }
            LuaRuntime runtime;
            const auto validated = runtime.validate_script(project_root / relative);
            if (!validated) {
                return {{"isError", true},
                    {"content", nlohmann::json::array({tool_text_content(validated.error().to_json())})}};
            }
            nlohmann::json payload;
            payload["summary"] = "Lua script written offline";
            payload["scriptPath"] = relative;
            payload["requiresReload"] = "editor_or_runtime";
            return {{"content", nlohmann::json::array({tool_text_content(payload.dump(2))})}};
        }
        return bridge_to_tool_result(forward_to_editor(project_root, "lua_apply", arguments));
    }
    if (tool_name == "engine_hud_apply") {
        EditorBridgeClient client(project_root);
        if (!client.is_editor_running()) {
            const auto relative = arguments.value("path", std::string{});
            const auto source = arguments.value("source", std::string{});
            if (relative.empty() || source.empty()) {
                return {{"isError", true},
                    {"content", nlohmann::json::array({tool_text_content("path and source are required")})}};
            }
            std::string lower = relative;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            const bool is_canvas =
                lower.find(".uicanvas.json") != std::string::npos || lower.find(".canvas.json") != std::string::npos;
            const auto written = is_canvas ? write_ui_canvas_json_atomic(project_root / relative, source)
                                          : write_hud_json_atomic(project_root / relative, source);
            if (!written) {
                return {{"isError", true},
                    {"content", nlohmann::json::array({tool_text_content(written.error().to_json())})}};
            }
            nlohmann::json payload;
            payload["summary"] = is_canvas ? "UI canvas written offline" : "HUD asset written offline";
            payload["hudPath"] = relative;
            payload["kind"] = is_canvas ? "ui_canvas" : "hud_asset";
            payload["requiresReload"] = "editor_or_runtime";
            return {{"content", nlohmann::json::array({tool_text_content(payload.dump(2))})}};
        }
        return bridge_to_tool_result(forward_to_editor(project_root, "hud_apply", arguments));
    }
    if (tool_name == "engine_ui_canvas_mutate") {
        EditorBridgeClient client(project_root);
        if (!client.is_editor_running()) {
            const auto relative = arguments.value("path", std::string{});
            const auto action = arguments.value("action", std::string{});
            if (relative.empty() || action.empty()) {
                return {{"isError", true},
                    {"content", nlohmann::json::array({tool_text_content("path and action are required")})}};
            }
            nlohmann::json widget_params = arguments;
            widget_params.erase("path");
            widget_params.erase("action");
            const auto mutated = mutate_ui_canvas_file(project_root / relative, action, widget_params.dump());
            if (!mutated) {
                return {{"isError", true},
                    {"content", nlohmann::json::array({tool_text_content(mutated.error().to_json())})}};
            }
            nlohmann::json payload;
            payload["summary"] = "UI canvas mutated offline";
            payload["canvasPath"] = relative;
            payload["action"] = action;
            payload["widgetCount"] = mutated.value().widgets.size();
            payload["requiresReload"] = "editor_or_runtime";
            return {{"content", nlohmann::json::array({tool_text_content(payload.dump(2))})}};
        }
        return bridge_to_tool_result(forward_to_editor(project_root, "ui_canvas_mutate", arguments));
    }
    if (tool_name == "engine_ui_stack") {
        EditorBridgeClient client(project_root);
        if (!client.is_editor_running()) {
            return {{"isError", true},
                {"content", nlohmann::json::array({tool_text_content(
                    "engine_ui_stack requires a running editor with MCP connection enabled")})}};
        }
        return bridge_to_tool_result(forward_to_editor(project_root, "ui_stack", arguments));
    }
    if (tool_name == "engine_lua_call") {
        EditorBridgeClient client(project_root);
        if (!client.is_editor_running()) {
            return {{"isError", true},
                {"content", nlohmann::json::array({tool_text_content(
                    "engine_lua_call requires a running editor with MCP connection enabled")})}};
        }
        return bridge_to_tool_result(forward_to_editor(project_root, "lua_call", arguments));
    }
    if (tool_name == "engine_quest_call") {
        EditorBridgeClient client(project_root);
        if (!client.is_editor_running()) {
            return {{"isError", true},
                {"content", nlohmann::json::array({tool_text_content(
                    "engine_quest_call requires a running editor with MCP connection enabled")})}};
        }
        return bridge_to_tool_result(forward_to_editor(project_root, "quest_call", arguments));
    }
    if (tool_name == "engine_standing_call") {
        EditorBridgeClient client(project_root);
        if (!client.is_editor_running()) {
            return {{"isError", true},
                {"content", nlohmann::json::array({tool_text_content(
                    "engine_standing_call requires a running editor with MCP connection enabled")})}};
        }
        return bridge_to_tool_result(forward_to_editor(project_root, "standing_call", arguments));
    }
    if (tool_name == "engine_scene_apply")
        return bridge_to_tool_result(forward_to_editor(project_root, "scene_apply", arguments));
    if (tool_name == "engine_terrain_apply") {
        const auto action = arguments.value("action", std::string{});
        EditorBridgeClient client(project_root);
        if (!client.is_editor_running() && (action == "sample" || action == "sample_terrain")) {
            EditorSessionContext context;
            context.project_root = project_root;
            return bridge_to_tool_result(execute_editor_operation(context, "terrain_apply", arguments.dump()));
        }
        return bridge_to_tool_result(forward_to_editor(project_root, "terrain_apply", arguments));
    }
    if (tool_name == "engine_entity_component_apply")
        return bridge_to_tool_result(forward_to_editor(project_root, "entity_component_apply", arguments));
    if (tool_name == "engine_prefab_component_apply")
        return bridge_to_tool_result(forward_asset_operation(project_root, "prefab_component_apply", arguments));
    if (tool_name == "engine_world_forge_apply") {
        EditorBridgeClient client(project_root);
        if (client.is_editor_running())
            return bridge_to_tool_result(forward_to_editor(project_root, "world_forge_apply", arguments));
        return bridge_to_tool_result(apply_world_forge_operation(project_root, arguments));
    }
    if (tool_name == "engine_project_validate") return command_to_tool_result(validate_project_at(project_root));
    return {{"isError", true}, {"content", nlohmann::json::array({tool_text_content("Unknown tool: " + tool_name)})}};
}

nlohmann::json handle_request(const std::filesystem::path& project_root, const nlohmann::json& request) {
    const auto method = request.value("method", std::string{});
    const auto id = request.contains("id") ? request["id"] : nlohmann::json{};
    const bool has_id = request.contains("id");
    if (!method.empty() && method != "notifications/initialized") {
        AutomationTrace::log(AutomationTraceChannel::Mcp, "request",
            {{"method", method}, {"hasId", has_id ? "true" : "false"}});
    }
    nlohmann::json response = {{"jsonrpc", "2.0"}};
    if (has_id) response["id"] = id;
    try {
        if (method == "initialize") {
            const auto protocol =
                request.contains("params") && request["params"].contains("protocolVersion")
                    ? request["params"]["protocolVersion"].get<std::string>()
                    : "2024-11-05";
            response["result"] = {{"protocolVersion", protocol},
                {"capabilities", {{"tools", {{"listChanged", false}}}, {"resources", nlohmann::json::object()},
                    {"prompts", nlohmann::json::object()}}},
                {"serverInfo", {{"name", "ai-rpg-engine"}, {"version", "0.2.0"}}}};
            return response;
        }
        if (method == "notifications/initialized") return {};
        if (method == "ping") {
            response["result"] = nlohmann::json::object();
            return response;
        }
        if (method == "tools/list") {
            response["result"] = {{"tools", nlohmann::json::parse(k_tools_list_json)}};
            return response;
        }
        if (method == "resources/list") {
            response["result"] = {{"resources", nlohmann::json::array()}};
            return response;
        }
        if (method == "prompts/list") {
            response["result"] = {{"prompts", nlohmann::json::array()}};
            return response;
        }
        if (method == "tools/call") {
            response["result"] = handle_tools_call(project_root, request["params"]);
            return response;
        }
        if (has_id) {
            response["error"] = {{"code", -32601}, {"message", "Method not found: " + method}};
            AutomationTrace::log(AutomationTraceChannel::Mcp, "error",
                {{"method", method}, {"code", "-32601"}, {"message", "Method not found"}});
            return response;
        }
    } catch (const std::exception& exception) {
        if (has_id) response["error"] = {{"code", -32603}, {"message", exception.what()}};
        AutomationTrace::log(AutomationTraceChannel::Mcp, "error",
            {{"method", method}, {"code", "-32603"}, {"message", exception.what()}});
        return response;
    }
    return {};
}

} // namespace

Result<int> run_mcp_server(const std::filesystem::path& project_root) {
    configure_mcp_stdio();
    client_uses_ndjson = false;
    AutomationTrace::log(AutomationTraceChannel::Mcp, "server_start",
        {{"project", project_root.lexically_normal().generic_string()},
            {"logPath", AutomationTrace::log_path(AutomationTraceChannel::Mcp).generic_string()}});
    while (true) {
        const auto payload = read_message();
        if (payload.empty()) break;
        const auto request = nlohmann::json::parse(payload, nullptr, false);
        if (request.is_discarded()) {
            AutomationTrace::log(AutomationTraceChannel::Mcp, "parse_error", "invalid JSON payload");
            continue;
        }
        const auto response = handle_request(project_root, request);
        if (!response.is_null() && !response.empty()) {
            const auto method = request.value("method", std::string{});
            AutomationTrace::log(AutomationTraceChannel::Mcp, "response",
                {{"method", method}, {"hasError", response.contains("error") ? "true" : "false"}});
            write_message(response.dump());
        }
    }
    AutomationTrace::log(AutomationTraceChannel::Mcp, "server_stop", "stdin closed");
    return Result<int>::success(0);
}

} // namespace engine
