#include "engine/automation/editor_session.h"

#include "engine/assets/asset_registry.h"
#include "engine/assets/material_asset.h"
#include "engine/assets/mesh_asset.h"
#include "engine/assets/prefab_asset.h"
#include "engine/assets/script_bindings_asset.h"
#include "engine/assets/hud_asset.h"
#include "engine/assets/ui_canvas_asset.h"
#include "engine/assets/ui_canvas_mutate.h"
#include "engine/ui/ui_canvas_stack.h"
#include "engine/automation/command.h"
#include "engine/automation/terrain_edit_commands.h"
#include "engine/automation/world_forge_commands.h"
#include "engine/automation/project_git_commands.h"
#include "engine/scripting/lua_runtime.h"
#include "engine/quest/quest_runtime.h"
#include "engine/standing/standing_runtime.h"
#include "engine/assets/world_forge_quests_asset.h"
#include "engine/ui/hud_runtime.h"
#include "engine/world/combat_volumes.h"
#include "engine/world/interaction_volumes.h"
#include "engine/world/authored_components.h"
#include "engine/world/foliage_density.h"
#include "engine/world/foliage_layers.h"
#include "engine/world/terrain.h"
#include "engine/world/terrain_edits.h"
#include "engine/world/terrain_paint.h"
#include "engine/world/water_store.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace engine {
namespace {

EngineError session_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "automation", std::move(message),
                       ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

EditorBridgeResponse make_response(ExitCode exit_code, std::string summary,
    std::vector<std::string> changed = {}, std::vector<EngineError> diagnostics = {},
    std::map<std::string, std::string> metadata = {}) {
    EditorBridgeResponse response;
    response.exit_code = exit_code;
    response.summary = std::move(summary);
    response.changed_object_ids = std::move(changed);
    response.diagnostics = std::move(diagnostics);
    response.metadata = std::move(metadata);
    return response;
}

nlohmann::json parse_params(const std::string& params_json) {
    if (params_json.empty()) return nlohmann::json::object();
    return nlohmann::json::parse(params_json);
}

TransformComponent transform_from_json(const nlohmann::json& json) {
    TransformComponent transform;
    if (json.contains("position") && json["position"].is_array() && json["position"].size() >= 3) {
        transform.position[0] = json["position"][0];
        transform.position[1] = json["position"][1];
        transform.position[2] = json["position"][2];
    }
    if (json.contains("rotation") && json["rotation"].is_array() && json["rotation"].size() >= 4) {
        transform.rotation[0] = json["rotation"][0];
        transform.rotation[1] = json["rotation"][1];
        transform.rotation[2] = json["rotation"][2];
        transform.rotation[3] = json["rotation"][3];
    }
    if (json.contains("scale") && json["scale"].is_array() && json["scale"].size() >= 3) {
        transform.scale[0] = json["scale"][0];
        transform.scale[1] = json["scale"][1];
        transform.scale[2] = json["scale"][2];
    }
    return transform;
}

void apply_terrain_snap(TransformComponent& transform, const nlohmann::json& params) {
    if (!params.value("snapToTerrain", false)) return;
    const float offset = params.value("groundOffset", 0.0f);
    transform.position[1] = sample_terrain_height(transform.position[0], transform.position[2]) + offset;
}

constexpr std::size_t k_max_scene_batch_ops = 100;

Result<std::unique_ptr<SceneCommand>> scene_command_from_op(const nlohmann::json& op,
    const std::map<std::string, PrefabAsset>* prefab_catalog = nullptr) {
    const auto action = op.value("action", std::string{});
    if (action == "place") {
        const auto prefab = op.value("prefab", std::string{});
        if (prefab.empty()) {
            return Result<std::unique_ptr<SceneCommand>>::failure(
                session_error("SCENE-PREFAB-REQUIRED", "place requires prefab path.", "Provide assets/... path."));
        }
        auto transform = transform_from_json(op.contains("transform") ? op["transform"] : nlohmann::json::object());
        apply_terrain_snap(transform, op);
        std::optional<EntityId> requested;
        if (op.contains("entityId")) {
            const auto parsed = EntityId::parse(op["entityId"].get<std::string>());
            if (!parsed) return Result<std::unique_ptr<SceneCommand>>::failure(parsed.error());
            requested = parsed.value();
        }
        std::optional<PrefabAsset> seed;
        if (prefab_catalog) {
            if (const auto* asset = find_prefab_in_catalog(*prefab_catalog, prefab)) seed = *asset;
        }
        return Result<std::unique_ptr<SceneCommand>>::success(std::make_unique<PlaceWorldObjectCommand>(
            op.value("name", std::string{"Placed Object"}), prefab, transform, requested,
            op.contains("characterAsset") ? std::optional<std::string>(op["characterAsset"].get<std::string>())
                                          : std::nullopt,
            std::move(seed)));
    }
    if (action == "move") {
        const auto parsed = EntityId::parse(op.value("entityId", std::string{}));
        if (!parsed) return Result<std::unique_ptr<SceneCommand>>::failure(parsed.error());
        auto transform = transform_from_json(op.contains("transform") ? op["transform"] : nlohmann::json::object());
        apply_terrain_snap(transform, op);
        return Result<std::unique_ptr<SceneCommand>>::success(
            std::make_unique<MoveWorldObjectCommand>(parsed.value(), transform));
    }
    if (action == "remove") {
        const auto parsed = EntityId::parse(op.value("entityId", std::string{}));
        if (!parsed) return Result<std::unique_ptr<SceneCommand>>::failure(parsed.error());
        return Result<std::unique_ptr<SceneCommand>>::success(
            std::make_unique<RemoveWorldObjectCommand>(parsed.value()));
    }
    if (action == "rename") {
        const auto parsed = EntityId::parse(op.value("entityId", std::string{}));
        if (!parsed) return Result<std::unique_ptr<SceneCommand>>::failure(parsed.error());
        return Result<std::unique_ptr<SceneCommand>>::success(
            std::make_unique<RenameEntityCommand>(parsed.value(), op.value("name", std::string{"Renamed"})));
    }
    return Result<std::unique_ptr<SceneCommand>>::failure(session_error("SCENE-BATCH-OP-UNKNOWN",
        "Unsupported batch operation: " + action, "Use place, move, remove, or rename in batch ops."));
}

EditorBridgeResponse apply_scene_batch(EditorSessionContext& context, const nlohmann::json& params) {
    if (!params.contains("ops") || !params["ops"].is_array()) {
        return make_response(ExitCode::InvalidArguments, "batch requires ops array", {},
            {session_error("SCENE-BATCH-OPS-REQUIRED", "batch action requires a non-empty ops array.",
                "Provide ops: [{action, ...}, ...].")});
    }
    const auto& ops = params["ops"];
    if (ops.empty()) {
        return make_response(ExitCode::InvalidArguments, "batch ops array is empty", {},
            {session_error("SCENE-BATCH-OPS-EMPTY", "batch action requires at least one operation.",
                "Provide one or more scene operations.")});
    }
    if (ops.size() > k_max_scene_batch_ops) {
        return make_response(ExitCode::InvalidArguments, "batch exceeds operation limit", {},
            {session_error("SCENE-BATCH-OPS-LIMIT",
                "batch supports at most " + std::to_string(k_max_scene_batch_ops) + " operations.",
                "Split the request into smaller batches.")});
    }
    std::vector<std::unique_ptr<SceneCommand>> commands;
    commands.reserve(ops.size());
    for (std::size_t index = 0; index < ops.size(); ++index) {
        if (!ops[index].is_object()) {
            return make_response(ExitCode::InvalidArguments, "batch op must be an object", {},
                {session_error("SCENE-BATCH-OP-INVALID", "Each batch op must be a JSON object.",
                    "Use {action, ...} entries in ops.")},
                {{"failedOpIndex", std::to_string(index)}});
        }
        auto built = scene_command_from_op(ops[index], context.prefab_catalog);
        if (!built) {
            return make_response(ExitCode::ValidationFailed, built.error().message, {}, {built.error()},
                {{"failedOpIndex", std::to_string(index)}});
        }
        commands.push_back(std::move(built.value()));
    }
    const auto label = params.value("label", std::string{});
    const auto result = context.history->execute(*context.scene,
        std::make_unique<CompositeSceneCommand>(label, std::move(commands)));
    if (!result) {
        return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()},
            {{"appliedCount", "0"}});
    }
    if (context.scene_dirty) *context.scene_dirty = true;
    const auto changed = context.history->last_changed_object_ids();
    if (params.value("save", false)) {
        const auto saved = context.scene->save_atomic(context.world_path);
        if (!saved) return make_response(ExitCode::ValidationFailed, saved.error().message, changed, {saved.error()},
            {{"appliedCount", std::to_string(ops.size())}});
        if (context.scene_dirty) *context.scene_dirty = false;
        return make_response(ExitCode::Success, "Batch applied and world saved", changed,
            {}, {{"appliedCount", std::to_string(ops.size())},
                {"savedPath", context.world_path.generic_string()},
                {"summary", context.history->last_summary()}});
    }
    return make_response(ExitCode::Success, context.history->last_summary(), changed, {},
        {{"appliedCount", std::to_string(ops.size())}});
}

std::string normalize_asset_relative_path(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    while (!path.empty() && path.front() == '/') path.erase(path.begin());
    std::transform(path.begin(), path.end(), path.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return path;
}

bool is_prefab_asset_path(const std::string& path) {
    return path.size() >= 12 && path.compare(path.size() - 12, 12, ".prefab.json") == 0;
}

bool is_material_asset_path(const std::string& path) {
    return path.size() >= 15 && path.compare(path.size() - 15, 15, ".material.json") == 0;
}

std::string infer_asset_kind(const std::string& path) {
    if (is_prefab_asset_path(path)) return "prefab";
    if (is_material_asset_path(path)) return "material";
    return "text";
}

Result<void> write_text_asset_atomic(const std::filesystem::path& path, const std::string& text) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return Result<void>::failure(session_error("ASSET-WRITE", "Could not write asset: " + path.generic_string(), "Check path permissions."));
        output << text;
        if (!output) return Result<void>::failure(session_error("ASSET-WRITE", "Could not flush asset: " + path.generic_string(), "Retry the write."));
    }
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::rename(temporary, path);
    return Result<void>::success();
}

Result<std::size_t> refresh_prefab_catalog(EditorSessionContext& context) {
    if (!context.assets || !context.prefab_catalog || context.project_root.empty()) {
        return Result<std::size_t>::failure(
            session_error("EDITOR-ASSETS-MISSING", "Asset registry or prefab catalog is unavailable.", "Launch the editor first."));
    }
    if (const auto scanned = context.assets->scan(context.project_root); !scanned) return Result<std::size_t>::failure(scanned.error());
    const auto errors = context.assets->validate();
    if (!errors.empty()) return Result<std::size_t>::failure(errors.front());
    context.prefab_catalog->clear();
    std::size_t prefab_count = 0;
    for (const auto& entry : context.assets->records()) {
        const auto& relative = entry.second.path;
        if (!is_prefab_asset_path(relative)) continue;
        const auto loaded = PrefabAsset::load(context.project_root / relative);
        if (!loaded) return Result<std::size_t>::failure(loaded.error());
        (*context.prefab_catalog)[relative] = loaded.value();
        ++prefab_count;
    }
    if (context.scene) (void)context.scene->repair_prefab_paths(*context.prefab_catalog);
    if (context.prefab_meshes_dirty) *context.prefab_meshes_dirty = true;
    return Result<std::size_t>::success(prefab_count);
}

Result<std::string> asset_payload_text(const nlohmann::json& params) {
    if (params.contains("json")) return Result<std::string>::success(params["json"].dump(2));
    if (params.contains("source")) return Result<std::string>::success(params["source"].get<std::string>());
    return Result<std::string>::failure(
        session_error("ASSET-PAYLOAD-REQUIRED", "Asset apply requires json or source.", "Provide asset JSON text."));
}

EditorBridgeResponse apply_asset_write(EditorSessionContext& context, const nlohmann::json& params) {
    const auto relative = normalize_asset_relative_path(params.value("path", std::string{}));
    if (relative.empty()) {
        return make_response(ExitCode::InvalidArguments, "path is required", {},
            {session_error("ASSET-PATH-REQUIRED", "Asset path is required.", "Provide assets/... path.")});
    }
    const auto payload = asset_payload_text(params);
    if (!payload) return make_response(ExitCode::InvalidArguments, payload.error().message, {}, {payload.error()});
    const auto absolute = context.project_root / relative;
    const auto kind = params.value("kind", infer_asset_kind(relative));
    if (kind == "prefab") {
        const auto written = write_text_asset_atomic(absolute, payload.value());
        if (!written) return make_response(ExitCode::ValidationFailed, written.error().message, {}, {written.error()});
        const auto loaded = PrefabAsset::load(absolute);
        if (!loaded) return make_response(ExitCode::ValidationFailed, loaded.error().message, {}, {loaded.error()});
        if (context.prefab_catalog) (*context.prefab_catalog)[relative] = loaded.value();
        std::size_t propagated = 0;
        if (context.scene) {
            propagated = context.scene->propagate_prefab_components(relative, loaded.value());
            if (propagated > 0 && context.scene_dirty) *context.scene_dirty = true;
        }
        std::size_t prefab_count = 0;
        if (params.value("refreshCatalog", true)) {
            const auto refreshed = refresh_prefab_catalog(context);
            if (!refreshed) return make_response(ExitCode::ValidationFailed, refreshed.error().message, {}, {refreshed.error()});
            prefab_count = refreshed.value();
            if (context.prefab_catalog) (*context.prefab_catalog)[relative] = loaded.value();
        } else {
            prefab_count = 1;
            if (context.prefab_meshes_dirty) *context.prefab_meshes_dirty = true;
            if (context.scene && context.prefab_catalog) (void)context.scene->repair_prefab_paths(*context.prefab_catalog);
        }
        return make_response(ExitCode::Success, "Asset written and catalog refreshed", {}, {},
            {{"assetPath", relative}, {"assetKind", kind}, {"prefabCount", std::to_string(prefab_count)},
                {"propagatedInstances", std::to_string(propagated)}});
    } else if (kind == "material") {
        const auto parsed = MaterialAsset::from_json(payload.value());
        if (!parsed) return make_response(ExitCode::ValidationFailed, parsed.error().message, {}, {parsed.error()});
        if (const auto valid = parsed.value().validate(); !valid)
            return make_response(ExitCode::ValidationFailed, valid.error().message, {}, {valid.error()});
        if (const auto saved = parsed.value().save_atomic(absolute); !saved)
            return make_response(ExitCode::ValidationFailed, saved.error().message, {}, {saved.error()});
    } else {
        const auto written = write_text_asset_atomic(absolute, payload.value());
        if (!written) return make_response(ExitCode::ValidationFailed, written.error().message, {}, {written.error()});
    }
    const auto extension = std::filesystem::path(relative).extension().string();
    if (extension == ".gltf" || extension == ".glb") {
        if (context.pending_mesh_reloads) context.pending_mesh_reloads->insert(relative);
        if (context.prefab_meshes_dirty) *context.prefab_meshes_dirty = true;
    }
    std::size_t prefab_count = 0;
    if (params.value("refreshCatalog", true)) {
        const auto refreshed = refresh_prefab_catalog(context);
        if (!refreshed) return make_response(ExitCode::ValidationFailed, refreshed.error().message, {}, {refreshed.error()});
        prefab_count = refreshed.value();
    } else if (kind == "prefab" && context.prefab_catalog) {
        const auto loaded = PrefabAsset::load(absolute);
        if (!loaded) return make_response(ExitCode::ValidationFailed, loaded.error().message, {}, {loaded.error()});
        (*context.prefab_catalog)[relative] = loaded.value();
        if (context.scene) (void)context.scene->repair_prefab_paths(*context.prefab_catalog);
        if (context.prefab_meshes_dirty) *context.prefab_meshes_dirty = true;
        prefab_count = 1;
    }
    return make_response(ExitCode::Success, "Asset written and catalog refreshed", {}, {},
        {{"assetPath", relative}, {"assetKind", kind}, {"prefabCount", std::to_string(prefab_count)}});
}

} // namespace

ScenePlanResult classify_scene_plan(const std::string& change_description, const std::string& target_path) {
    ScenePlanResult result;
    const auto lower_path = target_path;
    const auto lower_desc = change_description;
    auto contains = [](const std::string& haystack, const std::string& needle) {
        return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                   [](char a, char b) { return static_cast<char>(std::tolower(static_cast<unsigned char>(a))) ==
                                              static_cast<char>(std::tolower(static_cast<unsigned char>(b))); }) !=
               haystack.end();
    };
    if (contains(lower_path, ".lua") || contains(lower_desc, "lua") || contains(lower_desc, "script")) {
        result.target_kind = "lua_script";
        result.requires_compile = "false";
        result.requires_reload = "lua_hot_reload";
        result.recommendation = "Edit script assets and bindings; reload through the live editor bridge.";
    } else if (contains(lower_path, ".uicanvas.json") || contains(lower_path, ".canvas.json") ||
               contains(lower_desc, "ui canvas") || contains(lower_desc, "uicanvas")) {
        result.target_kind = "ui_canvas";
        result.requires_compile = "false";
        result.requires_reload = "hud_hot_reload";
        result.recommendation =
            "Edit UI canvas JSON through engine_hud_apply (*.uicanvas.json); live reload is allowed during play test.";
    } else if (contains(lower_path, ".hud.json") || contains(lower_path, ".hud.") || contains(lower_desc, "hud") ||
               (contains(lower_desc, "health bar") || contains(lower_desc, "healthbar"))) {
        result.target_kind = "hud_asset";
        result.requires_compile = "false";
        result.requires_reload = "hud_hot_reload";
        result.recommendation =
            "Prefer *.uicanvas.json (ui_canvas). Legacy *.hud.json still works via engine_hud_apply shim.";
    } else if (contains(lower_path, "prefab") || contains(lower_desc, "prefab") || contains(lower_path, ".prefab.") ||
               (contains(lower_desc, "component") && contains(lower_desc, "prefab"))) {
        result.target_kind = "prefab_asset";
        result.requires_compile = "false";
        result.requires_reload = "prefab_catalog_refresh";
        result.recommendation = "Edit prefab JSON/components and refresh the editor catalog; non-overridden instances inherit.";
    } else if (contains(lower_path, "terrain") || contains(lower_path, "terrain-edits") ||
               contains(lower_path, "terrain-paint") || contains(lower_path, "foliage") ||
               contains(lower_desc, "terrain") || contains(lower_desc, "sculpt") ||
               contains(lower_desc, "flatten") || contains(lower_desc, "foliage") ||
               (contains(lower_desc, "paint") &&
                   (contains(lower_desc, "material") || contains(lower_desc, "grass") ||
                       contains(lower_desc, "ground cover")))) {
        result.target_kind = "terrain_data";
        result.requires_compile = "false";
        result.requires_reload = "live_terrain_command";
        result.recommendation =
            "Apply height/paint/foliage strokes through engine_terrain_apply while the editor MCP connection is enabled.";
    } else if (contains(lower_path, ".worldforge.json") || contains(lower_path, "world-forge") ||
               contains(lower_desc, "world forge") || contains(lower_desc, "relationship graph") ||
               contains(lower_desc, "story geography") || contains(lower_desc, "factions.worldforge") ||
               contains(lower_desc, "map.worldforge")) {
        result.target_kind = "world_forge";
        result.requires_compile = "false";
        result.requires_reload = "none";
        result.recommendation =
            "Edit World Forge JSON through engine_world_forge_apply (factions/relationships/map). Do not use scene/terrain tools.";
    } else if (contains(lower_path, ".world.") || contains(lower_desc, "scene") || contains(lower_desc, "placement") ||
               contains(lower_desc, "entity") || contains(lower_desc, "component") || contains(lower_desc, "collider") ||
               contains(lower_desc, "add component")) {
        result.target_kind = "scene_data";
        result.requires_compile = "false";
        result.requires_reload = "live_scene_command";
        result.recommendation = "Apply scene/component changes through CommandHistory while the editor is running.";
    } else if (contains(lower_path, "src/") || contains(lower_path, "include/") || contains(lower_desc, "c++") ||
               contains(lower_desc, "engine")) {
        result.target_kind = "engine_code";
        result.requires_compile = "true";
        result.requires_reload = "rebuild_engine";
        result.recommendation = "Rebuild the engine target; restart the editor if native APIs changed.";
    } else {
        result.target_kind = "unknown";
        result.requires_compile = "maybe";
        result.requires_reload = "validate_first";
        result.recommendation = "Run project validation and choose scene, prefab, script, or engine workflow.";
    }
    result.summary = "Classified as " + result.target_kind;
    return result;
}

CommandResponse validate_project_at(const std::filesystem::path& project_root) {
    CommandRequest request;
    request.name = "validate";
    request.project = project_root;
    request.json = true;
    request.correlation_id = make_correlation_id();
    return execute_command(request);
}

EditorBridgeResponse apply_terrain_operation(EditorSessionContext& context, const nlohmann::json& params) {
    const auto action = params.value("action", std::string{});
    if (action == "sample" || action == "sample_terrain") {
        if (!params.contains("x") || !params.contains("z")) {
            return make_response(ExitCode::InvalidArguments, "sample requires x and z", {},
                {session_error("TERRAIN-SAMPLE-ARGS", "sample requires x and z world coordinates.",
                    "Provide numeric x and z.")});
        }
        const float x = params["x"].get<float>();
        const float z = params["z"].get<float>();
        if (!std::isfinite(x) || !std::isfinite(z)) {
            return make_response(ExitCode::InvalidArguments, "sample coordinates must be finite", {},
                {session_error("TERRAIN-SAMPLE-FINITE", "sample x/z must be finite.", "Use finite world coordinates.")});
        }
        const float height = sample_terrain_height(x, z);
        const float offset = params.value("groundOffset", 0.0f);
        return make_response(ExitCode::Success, "Terrain height sampled", {}, {},
            {{"x", std::to_string(x)}, {"z", std::to_string(z)}, {"height", std::to_string(height)},
                {"groundOffset", std::to_string(offset)}, {"placedY", std::to_string(height + offset)}});
    }

    if (context.test_session_active) {
        return make_response(ExitCode::Unavailable, "Terrain edits blocked during play test", {},
            {session_error("TERRAIN-PLAY-BLOCKED", "Terrain apply is blocked while a play-test session is active.",
                "End the test session, then retry.")});
    }

    auto require_edits = [&]() -> std::optional<EditorBridgeResponse> {
        if (!context.terrain_edits || !context.terrain_history) {
            return make_response(ExitCode::Unavailable, "Terrain edit stores unavailable", {},
                {session_error("TERRAIN-STORE-MISSING", "Terrain height stores are not bound to the editor session.",
                    "Enable MCP connection in a running editor session.")});
        }
        return std::nullopt;
    };
    auto require_paint = [&]() -> std::optional<EditorBridgeResponse> {
        if (!context.terrain_paint || !context.terrain_paint_history) {
            return make_response(ExitCode::Unavailable, "Terrain paint stores unavailable", {},
                {session_error("TERRAIN-PAINT-MISSING", "Terrain paint stores are not bound to the editor session.",
                    "Enable MCP connection in a running editor session.")});
        }
        return std::nullopt;
    };
    auto require_foliage = [&]() -> std::optional<EditorBridgeResponse> {
        if (!context.foliage_density || !context.foliage_density_history) {
            return make_response(ExitCode::Unavailable, "Foliage density stores unavailable", {},
                {session_error("FOLIAGE-STORE-MISSING", "Foliage density stores are not bound to the editor session.",
                    "Enable MCP connection in a running editor session.")});
        }
        if (!context.foliage_layers || context.foliage_layers->layers.empty()) {
            return make_response(ExitCode::Unavailable, "Foliage layer palette unavailable", {},
                {session_error("FOLIAGE-PALETTE-MISSING", "Foliage layer palette is not loaded.",
                    "Ensure assets/foliage/ground-cover.layers.json loads in the editor.")});
        }
        return std::nullopt;
    };

    auto reload_now = [&](bool height_changed) {
        if (height_changed) {
            if (context.terrain_edits_dirty) *context.terrain_edits_dirty = true;
        } else {
            if (context.terrain_paint_dirty) *context.terrain_paint_dirty = true;
        }
        if (context.reload_terrain) context.reload_terrain(height_changed);
        if (height_changed && context.reload_water) context.reload_water();
    };
    auto reload_foliage_now = [&]() {
        if (context.foliage_density_dirty) *context.foliage_density_dirty = true;
        if (context.reload_foliage) context.reload_foliage();
    };

    auto probe_cells = [](float x, float z, float radius, float cell_size) {
        std::set<CellCoord> probe;
        const int cell_extent = static_cast<int>(std::ceil(radius / cell_size)) + 1;
        const auto center = terrain_cell_for_position(x, z, cell_size);
        for (int dz = -cell_extent; dz <= cell_extent; ++dz)
            for (int dx = -cell_extent; dx <= cell_extent; ++dx)
                probe.insert({center.x + dx, center.z + dz});
        return probe;
    };

    auto snapshot_height = [&](const std::set<CellCoord>& cells) {
        std::map<CellCoord, std::vector<float>> before;
        for (const auto& cell : cells) before[cell] = context.terrain_edits->cell_deltas_or_empty(cell);
        return before;
    };
    auto snapshot_paint = [&](const std::set<CellCoord>& cells) {
        std::map<CellCoord, std::vector<std::uint16_t>> before;
        for (const auto& cell : cells) before[cell] = context.terrain_paint->cell_indices_or_empty(cell);
        return before;
    };
    auto snapshot_foliage = [&](const std::set<CellCoord>& cells) {
        std::map<CellCoord, FoliageCellSnapshot> before;
        for (const auto& cell : cells) before[cell] = context.foliage_density->cell_snapshot_or_empty(cell);
        return before;
    };
    auto restore_height = [&](const std::map<CellCoord, std::vector<float>>& before) {
        for (const auto& entry : before) context.terrain_edits->set_cell_deltas(entry.first, entry.second);
    };
    auto restore_paint = [&](const std::map<CellCoord, std::vector<std::uint16_t>>& before) {
        for (const auto& entry : before) {
            if (entry.second.empty()) context.terrain_paint->remove_cell(entry.first);
            else context.terrain_paint->set_cell_indices(entry.first, entry.second);
        }
    };
    auto restore_foliage = [&](const std::map<CellCoord, FoliageCellSnapshot>& before) {
        for (const auto& entry : before) {
            if (entry.second.density.empty()) context.foliage_density->remove_cell(entry.first);
            else context.foliage_density->set_cell(entry.first, entry.second);
        }
    };

    auto commit_height = [&](std::map<CellCoord, std::vector<float>> before, bool do_reload) -> EditorBridgeResponse {
        std::map<CellCoord, std::vector<float>> after;
        for (const auto& entry : before) after[entry.first] = context.terrain_edits->cell_deltas_or_empty(entry.first);
        bool changed = false;
        for (const auto& entry : after) {
            const auto found = before.find(entry.first);
            if (found == before.end() || found->second != entry.second) {
                changed = true;
                break;
            }
        }
        if (!changed)
            return make_response(ExitCode::Success, "Terrain brush touched no samples", {}, {}, {{"touchedCells", "0"}});
        const auto result = context.terrain_history->execute(*context.terrain_edits,
            std::make_unique<TerrainBrushStrokeCommand>(std::move(before), std::move(after)));
        if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
        if (context.terrain_edits_dirty) *context.terrain_edits_dirty = true;
        if (do_reload && context.reload_terrain) context.reload_terrain(true);
        if (do_reload && context.reload_water) context.reload_water();
        return make_response(ExitCode::Success, "Terrain height stroke applied", {}, {}, {});
    };

    auto commit_paint = [&](std::map<CellCoord, std::vector<std::uint16_t>> before, bool do_reload) -> EditorBridgeResponse {
        std::map<CellCoord, std::vector<std::uint16_t>> after;
        for (const auto& entry : before) after[entry.first] = context.terrain_paint->cell_indices_or_empty(entry.first);
        bool changed = false;
        for (const auto& entry : after) {
            const auto found = before.find(entry.first);
            if (found == before.end() || found->second != entry.second) {
                changed = true;
                break;
            }
        }
        if (!changed)
            return make_response(ExitCode::Success, "Terrain paint touched no samples", {}, {}, {{"touchedCells", "0"}});
        const auto result = context.terrain_paint_history->execute(*context.terrain_paint,
            std::make_unique<TerrainPaintBrushStrokeCommand>(std::move(before), std::move(after)));
        if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
        if (context.terrain_paint_dirty) *context.terrain_paint_dirty = true;
        if (do_reload && context.reload_terrain) context.reload_terrain(false);
        return make_response(ExitCode::Success, "Terrain paint stroke applied", {}, {}, {});
    };

    auto commit_foliage = [&](std::map<CellCoord, FoliageCellSnapshot> before, bool do_reload) -> EditorBridgeResponse {
        std::map<CellCoord, FoliageCellSnapshot> after;
        for (const auto& entry : before) after[entry.first] = context.foliage_density->cell_snapshot_or_empty(entry.first);
        bool changed = false;
        for (const auto& entry : after) {
            const auto found = before.find(entry.first);
            if (found == before.end() || found->second.density != entry.second.density ||
                found->second.layer != entry.second.layer) {
                changed = true;
                break;
            }
        }
        if (!changed)
            return make_response(ExitCode::Success, "Foliage brush touched no samples", {}, {}, {{"touchedCells", "0"}});
        const auto result = context.foliage_density_history->execute(*context.foliage_density,
            std::make_unique<FoliageDensityBrushStrokeCommand>(std::move(before), std::move(after)));
        if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
        if (context.foliage_density_dirty) *context.foliage_density_dirty = true;
        if (do_reload && context.reload_foliage) context.reload_foliage();
        return make_response(ExitCode::Success, "Foliage density stroke applied", {}, {}, {});
    };

    auto resolve_foliage_layer = [&](const nlohmann::json& op) -> Result<std::uint8_t> {
        if (!context.foliage_layers || context.foliage_layers->layers.empty()) {
            return Result<std::uint8_t>::failure(session_error("FOLIAGE-PALETTE-MISSING",
                "Foliage layer palette is not loaded.", "Load ground-cover.layers.json in the editor."));
        }
        if (!op.contains("layer")) return Result<std::uint8_t>::success(0);
        const auto& layer_value = op.at("layer");
        if (layer_value.is_number_integer() || layer_value.is_number_unsigned()) {
            const int index = layer_value.get<int>();
            if (index < 0 || static_cast<std::size_t>(index) >= context.foliage_layers->layers.size()) {
                return Result<std::uint8_t>::failure(session_error("FOLIAGE-LAYER-RANGE",
                    "Foliage layer index is out of range.", "Use a valid palette index or layer id."));
            }
            return Result<std::uint8_t>::success(static_cast<std::uint8_t>(index));
        }
        if (layer_value.is_string()) {
            const auto id = layer_value.get<std::string>();
            if (!id.empty() && std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) {
                const int index = std::stoi(id);
                if (index < 0 || static_cast<std::size_t>(index) >= context.foliage_layers->layers.size()) {
                    return Result<std::uint8_t>::failure(session_error("FOLIAGE-LAYER-RANGE",
                        "Foliage layer index is out of range.", "Use a valid palette index or layer id."));
                }
                return Result<std::uint8_t>::success(static_cast<std::uint8_t>(index));
            }
            for (std::size_t index = 0; index < context.foliage_layers->layers.size(); ++index) {
                if (context.foliage_layers->layers[index].id == id)
                    return Result<std::uint8_t>::success(static_cast<std::uint8_t>(index));
            }
            return Result<std::uint8_t>::failure(session_error("FOLIAGE-LAYER-UNKNOWN",
                "Unknown foliage layer id: " + id, "Use grass/flower/bush/bush_wide/bush_tall or a palette index."));
        }
        return Result<std::uint8_t>::failure(session_error("FOLIAGE-LAYER-TYPE",
            "layer must be a palette index or id string.", "Example: \"grass\" or 0."));
    };

    auto apply_height_op = [&](const nlohmann::json& op) -> Result<std::set<CellCoord>> {
        const auto op_action = op.value("action", std::string{});
        if (!op.contains("x") || !op.contains("z")) {
            return Result<std::set<CellCoord>>::failure(
                session_error("TERRAIN-BRUSH-ARGS", "Terrain brush requires x and z.", "Provide world x/z."));
        }
        const float x = op["x"].get<float>();
        const float z = op["z"].get<float>();
        const float radius = op.value("radius", 4.0f);
        const float strength = op.value("strength", 0.12f);
        if (!std::isfinite(x) || !std::isfinite(z)) {
            return Result<std::set<CellCoord>>::failure(
                session_error("TERRAIN-BRUSH-FINITE", "Brush x/z must be finite.", "Use finite world coordinates."));
        }
        if (op_action == "flatten") {
            const float target = op.contains("targetHeight") ? op["targetHeight"].get<float>() : sample_terrain_height(x, z);
            return context.terrain_edits->apply_flatten_brush(x, z, radius, strength, target);
        }
        if (op_action == "set_height") {
            if (!op.contains("targetHeight")) {
                return Result<std::set<CellCoord>>::failure(session_error("TERRAIN-SET-HEIGHT-ARGS",
                    "set_height requires targetHeight.", "Provide targetHeight world Y."));
            }
            const float target = op["targetHeight"].get<float>();
            const float set_strength = op.value("strength", 1.0f);
            return context.terrain_edits->apply_set_height_brush(x, z, radius, set_strength, target);
        }
        return context.terrain_edits->apply_brush(x, z, radius, strength, op_action == "lower");
    };

    auto densify_polyline = [](const std::vector<std::array<float, 2>>& controls, float step) {
        std::vector<std::array<float, 2>> pts;
        if (controls.empty()) return pts;
        if (controls.size() == 1) {
            pts.push_back(controls.front());
            return pts;
        }
        const float use_step = std::max(0.5f, step);
        for (std::size_t i = 0; i + 1 < controls.size(); ++i) {
            const float x0 = controls[i][0];
            const float z0 = controls[i][1];
            const float x1 = controls[i + 1][0];
            const float z1 = controls[i + 1][1];
            const float seg = std::hypot(x1 - x0, z1 - z0);
            const int n = std::max(1, static_cast<int>(std::ceil(seg / use_step)));
            for (int k = 0; k < n; ++k) {
                const float t = static_cast<float>(k) / static_cast<float>(n);
                pts.push_back({x0 + (x1 - x0) * t, z0 + (z1 - z0) * t});
            }
        }
        pts.push_back(controls.back());
        return pts;
    };

    auto parse_points = [&](const nlohmann::json& root) -> Result<std::vector<std::array<float, 2>>> {
        if (!root.contains("points") || !root["points"].is_array() || root["points"].empty()) {
            return Result<std::vector<std::array<float, 2>>>::failure(session_error("TERRAIN-POINTS-ARGS",
                "points array is required.", "Provide points:[{x,z},...]"));
        }
        std::vector<std::array<float, 2>> controls;
        for (const auto& point : root["points"]) {
            if (!point.is_object() || !point.contains("x") || !point.contains("z")) {
                return Result<std::vector<std::array<float, 2>>>::failure(session_error("TERRAIN-POINT-INVALID",
                    "Each point requires x and z.", "Use {x,z} objects."));
            }
            const float px = point["x"].get<float>();
            const float pz = point["z"].get<float>();
            if (!std::isfinite(px) || !std::isfinite(pz)) {
                return Result<std::vector<std::array<float, 2>>>::failure(session_error("TERRAIN-POINT-FINITE",
                    "Point coordinates must be finite.", "Use finite world x/z."));
            }
            controls.push_back({px, pz});
        }
        return Result<std::vector<std::array<float, 2>>>::success(std::move(controls));
    };

    auto resolve_sea_level = [&]() -> float {
        if (params.contains("seaLevel") && params["seaLevel"].is_number()) return params["seaLevel"].get<float>();
        if (context.water_store) return context.water_store->sea_level();
        if (const WaterStore* active = active_water_store()) return active->sea_level();
        return -0.35f;
    };

    auto apply_paint_op = [&](const nlohmann::json& op) -> Result<std::set<CellCoord>> {
        if (!op.contains("x") || !op.contains("z")) {
            return Result<std::set<CellCoord>>::failure(
                session_error("TERRAIN-PAINT-ARGS", "paint requires x and z.", "Provide world x/z."));
        }
        const auto material = op.value("material", std::string{});
        if (material.empty()) {
            return Result<std::set<CellCoord>>::failure(session_error("TERRAIN-PAINT-MATERIAL",
                "paint requires a material asset path.", "Provide assets/materials/....material.json"));
        }
        const float x = op["x"].get<float>();
        const float z = op["z"].get<float>();
        const float radius = op.value("radius", 4.0f);
        const auto palette_index = context.terrain_paint->ensure_material_index(material);
        return context.terrain_paint->apply_material_brush(x, z, radius, palette_index);
    };

    auto apply_foliage_op = [&](const nlohmann::json& op) -> Result<std::set<CellCoord>> {
        if (!op.contains("x") || !op.contains("z")) {
            return Result<std::set<CellCoord>>::failure(
                session_error("FOLIAGE-BRUSH-ARGS", "paint_foliage requires x and z.", "Provide world x/z."));
        }
        const float x = op["x"].get<float>();
        const float z = op["z"].get<float>();
        const float radius = op.value("radius", 4.0f);
        const float strength = op.value("strength", 0.28f);
        const bool erase = op.value("erase", false);
        if (!std::isfinite(x) || !std::isfinite(z) || !std::isfinite(radius) || !std::isfinite(strength)) {
            return Result<std::set<CellCoord>>::failure(
                session_error("FOLIAGE-BRUSH-FINITE", "Foliage brush values must be finite.", "Use finite numbers."));
        }
        const auto op_action = op.value("action", std::string{});
        if (op_action == "paint_foliage_mixed" || op.value("mode", std::string{}) == "mixed") {
            const auto mix = default_meadow_mix_weights(*context.foliage_layers);
            return context.foliage_density->apply_foliage_mixed_brush(x, z, radius, strength, mix, erase);
        }
        const auto layer = resolve_foliage_layer(op);
        if (!layer) return Result<std::set<CellCoord>>::failure(layer.error());
        return context.foliage_density->apply_foliage_brush(x, z, radius, strength, layer.value(), erase);
    };

    auto save_terrain = [&]() -> EditorBridgeResponse {
        std::vector<std::string> saved;
        if (context.terrain_edits) {
            const auto path = default_terrain_edits_path(context.project_root);
            const auto result = context.terrain_edits->save_atomic(path);
            if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
            if (context.terrain_edits_dirty) *context.terrain_edits_dirty = false;
            saved.push_back(path.generic_string());
        }
        if (context.terrain_paint) {
            const auto path = default_terrain_paint_path(context.project_root);
            const auto result = context.terrain_paint->save_atomic(path);
            if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
            if (context.terrain_paint_dirty) *context.terrain_paint_dirty = false;
            saved.push_back(path.generic_string());
        }
        if (context.foliage_density) {
            const auto path = default_foliage_density_path(context.project_root);
            const auto result = context.foliage_density->save_atomic(path);
            if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
            if (context.foliage_density_dirty) *context.foliage_density_dirty = false;
            saved.push_back(path.generic_string());
        }
        if (saved.empty()) {
            return make_response(ExitCode::Unavailable, "No terrain stores to save", {},
                {session_error("TERRAIN-SAVE-MISSING", "Terrain stores are not bound.",
                    "Enable MCP connection in a running editor session.")});
        }
        return make_response(ExitCode::Success, "Terrain assets saved", std::move(saved));
    };

    constexpr std::size_t k_max_terrain_batch_ops = 200;
    if (action == "batch") {
        if (!params.contains("ops") || !params["ops"].is_array()) {
            return make_response(ExitCode::InvalidArguments, "batch requires ops array", {},
                {session_error("TERRAIN-BATCH-OPS", "batch requires an ops array.",
                    "Provide ops:[{action,x,z,...},...]")});
        }
        const auto& ops = params["ops"];
        if (ops.empty()) {
            return make_response(ExitCode::InvalidArguments, "batch ops is empty", {},
                {session_error("TERRAIN-BATCH-EMPTY", "batch requires at least one operation.",
                    "Provide one or more terrain operations.")});
        }
        if (ops.size() > k_max_terrain_batch_ops) {
            return make_response(ExitCode::InvalidArguments, "batch exceeds operation limit", {},
                {session_error("TERRAIN-BATCH-LIMIT",
                    "batch supports at most " + std::to_string(k_max_terrain_batch_ops) + " operations.",
                    "Split the request into smaller batches.")});
        }

        bool needs_height = false;
        bool needs_paint = false;
        bool needs_foliage = false;
        std::set<CellCoord> height_probe;
        std::set<CellCoord> paint_probe;
        std::set<CellCoord> foliage_probe;
        for (const auto& op : ops) {
            if (!op.is_object()) {
                return make_response(ExitCode::InvalidArguments, "batch op must be an object", {},
                    {session_error("TERRAIN-BATCH-OP-INVALID", "Each batch op must be a JSON object.",
                        "Use {action,x,z,...} entries.")});
            }
            const auto op_action = op.value("action", std::string{});
            if (op_action == "raise" || op_action == "lower" || op_action == "flatten" || op_action == "set_height") {
                needs_height = true;
                if (!op.contains("x") || !op.contains("z")) {
                    return make_response(ExitCode::InvalidArguments, "batch height op requires x/z", {},
                        {session_error("TERRAIN-BRUSH-ARGS", "Terrain brush requires x and z.", "Provide world x/z.")});
                }
                const float radius = op.value("radius", 4.0f);
                auto probe = probe_cells(op["x"].get<float>(), op["z"].get<float>(), radius, TerrainEditStore::k_cell_size);
                height_probe.insert(probe.begin(), probe.end());
            } else if (op_action == "paint") {
                needs_paint = true;
                if (!op.contains("x") || !op.contains("z")) {
                    return make_response(ExitCode::InvalidArguments, "batch paint op requires x/z", {},
                        {session_error("TERRAIN-PAINT-ARGS", "paint requires x and z.", "Provide world x/z.")});
                }
                const float radius = op.value("radius", 4.0f);
                auto probe = probe_cells(op["x"].get<float>(), op["z"].get<float>(), radius, TerrainPaintStore::k_cell_size);
                paint_probe.insert(probe.begin(), probe.end());
            } else if (op_action == "paint_foliage" || op_action == "paint_foliage_mixed") {
                needs_foliage = true;
                if (!op.contains("x") || !op.contains("z")) {
                    return make_response(ExitCode::InvalidArguments, "batch foliage op requires x/z", {},
                        {session_error("FOLIAGE-BRUSH-ARGS", "paint_foliage requires x and z.", "Provide world x/z.")});
                }
                const float radius = op.value("radius", 4.0f);
                auto probe =
                    probe_cells(op["x"].get<float>(), op["z"].get<float>(), radius, FoliageDensityStore::k_cell_size);
                foliage_probe.insert(probe.begin(), probe.end());
            } else {
                return make_response(ExitCode::InvalidArguments, "Unsupported batch terrain action: " + op_action, {},
                    {session_error("TERRAIN-BATCH-ACTION", "Unsupported batch action: " + op_action,
                        "Batch ops may be raise/lower/flatten/set_height/paint/paint_foliage/paint_foliage_mixed.")});
            }
        }
        if (needs_height) {
            if (auto missing = require_edits()) return *missing;
        }
        if (needs_paint) {
            if (auto missing = require_paint()) return *missing;
        }
        if (needs_foliage) {
            if (auto missing = require_foliage()) return *missing;
        }

        auto height_before = needs_height ? snapshot_height(height_probe) : std::map<CellCoord, std::vector<float>>{};
        auto paint_before =
            needs_paint ? snapshot_paint(paint_probe) : std::map<CellCoord, std::vector<std::uint16_t>>{};
        auto foliage_before =
            needs_foliage ? snapshot_foliage(foliage_probe) : std::map<CellCoord, FoliageCellSnapshot>{};

        std::size_t applied = 0;
        for (const auto& op : ops) {
            const auto op_action = op.value("action", std::string{});
            Result<std::set<CellCoord>> touched = Result<std::set<CellCoord>>::failure(
                session_error("TERRAIN-BATCH-ACTION", "Unsupported batch action", "Check action name."));
            if (op_action == "raise" || op_action == "lower" || op_action == "flatten" || op_action == "set_height")
                touched = apply_height_op(op);
            else if (op_action == "paint")
                touched = apply_paint_op(op);
            else
                touched = apply_foliage_op(op);
            if (!touched) {
                restore_height(height_before);
                restore_paint(paint_before);
                restore_foliage(foliage_before);
                return make_response(ExitCode::ValidationFailed, touched.error().message, {}, {touched.error()},
                    {{"failedIndex", std::to_string(applied)}});
            }
            ++applied;
        }

        if (needs_height) {
            const auto committed = commit_height(std::move(height_before), false);
            if (committed.exit_code != ExitCode::Success) {
                restore_paint(paint_before);
                restore_foliage(foliage_before);
                return committed;
            }
        }
        if (needs_paint) {
            const auto committed = commit_paint(std::move(paint_before), false);
            if (committed.exit_code != ExitCode::Success) {
                restore_foliage(foliage_before);
                return committed;
            }
        }
        if (needs_foliage) {
            const auto committed = commit_foliage(std::move(foliage_before), false);
            if (committed.exit_code != ExitCode::Success) return committed;
        }

        if (context.reload_terrain && (needs_height || needs_paint)) context.reload_terrain(needs_height);
        if (needs_height && context.reload_water) context.reload_water();
        if (needs_foliage && context.reload_foliage) context.reload_foliage();

        if (params.value("save", false)) {
            const auto saved = save_terrain();
            if (saved.exit_code != ExitCode::Success) return saved;
        }
        return make_response(ExitCode::Success, "Terrain batch applied", {}, {},
            {{"appliedCount", std::to_string(applied)}, {"heightChanged", needs_height ? "true" : "false"},
                {"paintChanged", needs_paint ? "true" : "false"},
                {"foliageChanged", needs_foliage ? "true" : "false"}});
    }

    if (action == "raise" || action == "lower" || action == "flatten" || action == "set_height") {
        if (auto missing = require_edits()) return *missing;
        if (!params.contains("x") || !params.contains("z")) {
            return make_response(ExitCode::InvalidArguments, action + " requires x and z", {},
                {session_error("TERRAIN-BRUSH-ARGS", "Terrain brush requires x and z.", "Provide world x/z.")});
        }
        if (action == "set_height" && !params.contains("targetHeight")) {
            return make_response(ExitCode::InvalidArguments, "set_height requires targetHeight", {},
                {session_error("TERRAIN-SET-HEIGHT-ARGS", "set_height requires targetHeight.",
                    "Provide targetHeight world Y.")});
        }
        const float x = params["x"].get<float>();
        const float z = params["z"].get<float>();
        const float radius = params.value("radius", 4.0f);
        auto before = snapshot_height(probe_cells(x, z, radius, TerrainEditStore::k_cell_size));
        nlohmann::json op = params;
        op["action"] = action;
        const auto touched = apply_height_op(op);
        if (!touched) return make_response(ExitCode::ValidationFailed, touched.error().message, {}, {touched.error()});
        auto response = commit_height(std::move(before), true);
        if (response.exit_code == ExitCode::Success) {
            response.metadata["touchedCells"] = std::to_string(touched.value().size());
            std::vector<std::string> changed;
            for (const auto& cell : touched.value())
                changed.push_back(std::to_string(cell.x) + "," + std::to_string(cell.z));
            response.changed_object_ids = std::move(changed);
            if (touched.value().empty()) response.summary = "Terrain brush touched no samples";
            else if (action == "set_height") response.summary = "Terrain height set";
        }
        return response;
    }

    if (action == "carve_channel" || action == "raise_banks") {
        if (auto missing = require_edits()) return *missing;
        const auto controls = parse_points(params);
        if (!controls) return make_response(ExitCode::InvalidArguments, controls.error().message, {}, {controls.error()});
        const float step = params.value("step", 2.5f);
        const auto pts = densify_polyline(controls.value(), step);
        const float sea = resolve_sea_level();
        const float half_width = params.value("halfWidth", action == "carve_channel" ? 3.5f : 0.0f);
        const float bed_depth = params.value("bedDepth", 1.4f);
        const float bed_height =
            params.contains("bedHeight") ? params["bedHeight"].get<float>() : (sea - std::max(0.2f, bed_depth));
        const float bank_width = params.value("bankWidth", 3.5f);
        // Keep bank brushes outside the bed footprint so soft falloff cannot re-raise the channel.
        const float min_bank_offset = half_width + bank_width;
        const float bank_offset = std::max(params.value("bankOffset", min_bank_offset), min_bank_offset);
        const float bank_clearance = params.value("bankClearance", 1.5f);
        const float bank_height =
            params.contains("bankHeight") ? params["bankHeight"].get<float>() : (sea + bank_clearance);
        const float strength = params.value("strength", 1.0f);

        float probe_radius = std::max(half_width, bank_width) + bank_offset + 1.0f;
        std::set<CellCoord> probe;
        for (const auto& p : pts) {
            auto local = probe_cells(p[0], p[1], probe_radius, TerrainEditStore::k_cell_size);
            probe.insert(local.begin(), local.end());
        }
        auto before = snapshot_height(probe);
        std::set<CellCoord> touched_all;
        for (std::size_t i = 0; i < pts.size(); ++i) {
            const float x = pts[i][0];
            const float z = pts[i][1];
            float dx = 0.0f;
            float dz = 1.0f;
            if (i + 1 < pts.size()) {
                dx = pts[i + 1][0] - x;
                dz = pts[i + 1][1] - z;
            } else if (i > 0) {
                dx = x - pts[i - 1][0];
                dz = z - pts[i - 1][1];
            }
            const float len = std::hypot(dx, dz);
            if (len > 1.0e-4f) {
                dx /= len;
                dz /= len;
            }
            const float px = -dz;
            const float pz = dx;

            // Banks first, then bed: bed must win if footprints ever touch.
            if (action == "carve_channel" || action == "raise_banks") {
                for (float side : {-1.0f, 1.0f}) {
                    const float bx = x + px * side * bank_offset;
                    const float bz = z + pz * side * bank_offset;
                    const auto bank =
                        context.terrain_edits->apply_set_height_brush(bx, bz, bank_width, strength, bank_height);
                    if (!bank) {
                        restore_height(before);
                        return make_response(ExitCode::ValidationFailed, bank.error().message, {}, {bank.error()});
                    }
                    touched_all.insert(bank.value().begin(), bank.value().end());
                }
            }
            if (action == "carve_channel") {
                const auto bed = context.terrain_edits->apply_set_height_brush(x, z, half_width, strength, bed_height);
                if (!bed) {
                    restore_height(before);
                    return make_response(ExitCode::ValidationFailed, bed.error().message, {}, {bed.error()});
                }
                touched_all.insert(bed.value().begin(), bed.value().end());
            }
        }
        auto response = commit_height(std::move(before), true);
        if (response.exit_code == ExitCode::Success) {
            response.summary = action == "carve_channel" ? "River channel carved" : "River banks raised";
            response.metadata["touchedCells"] = std::to_string(touched_all.size());
            response.metadata["pointCount"] = std::to_string(pts.size());
            response.metadata["seaLevel"] = std::to_string(sea);
            response.metadata["bedHeight"] = std::to_string(bed_height);
            response.metadata["bankHeight"] = std::to_string(bank_height);
            if (params.value("save", false)) {
                const auto saved = save_terrain();
                if (saved.exit_code != ExitCode::Success) return saved;
            }
        }
        return response;
    }

    if (action == "paint") {
        if (auto missing = require_paint()) return *missing;
        if (!params.contains("x") || !params.contains("z")) {
            return make_response(ExitCode::InvalidArguments, "paint requires x and z", {},
                {session_error("TERRAIN-PAINT-ARGS", "paint requires x and z.", "Provide world x/z.")});
        }
        const auto material = params.value("material", std::string{});
        const float x = params["x"].get<float>();
        const float z = params["z"].get<float>();
        const float radius = params.value("radius", 4.0f);
        auto before = snapshot_paint(probe_cells(x, z, radius, TerrainPaintStore::k_cell_size));
        const auto touched = apply_paint_op(params);
        if (!touched) return make_response(ExitCode::ValidationFailed, touched.error().message, {}, {touched.error()});
        auto response = commit_paint(std::move(before), true);
        if (response.exit_code == ExitCode::Success) {
            response.metadata["touchedCells"] = std::to_string(touched.value().size());
            response.metadata["material"] = material;
            std::vector<std::string> changed;
            for (const auto& cell : touched.value())
                changed.push_back(std::to_string(cell.x) + "," + std::to_string(cell.z));
            response.changed_object_ids = std::move(changed);
        }
        return response;
    }

    if (action == "paint_foliage" || action == "paint_foliage_mixed") {
        if (auto missing = require_foliage()) return *missing;
        if (!params.contains("x") || !params.contains("z")) {
            return make_response(ExitCode::InvalidArguments, action + " requires x and z", {},
                {session_error("FOLIAGE-BRUSH-ARGS", "paint_foliage requires x and z.", "Provide world x/z.")});
        }
        const float x = params["x"].get<float>();
        const float z = params["z"].get<float>();
        const float radius = params.value("radius", 4.0f);
        auto before = snapshot_foliage(probe_cells(x, z, radius, FoliageDensityStore::k_cell_size));
        const auto touched = apply_foliage_op(params);
        if (!touched) return make_response(ExitCode::ValidationFailed, touched.error().message, {}, {touched.error()});
        auto response = commit_foliage(std::move(before), true);
        if (response.exit_code == ExitCode::Success) {
            response.metadata["touchedCells"] = std::to_string(touched.value().size());
            response.metadata["erase"] = params.value("erase", false) ? "true" : "false";
            if (params.contains("layer")) {
                if (params["layer"].is_string()) response.metadata["layer"] = params["layer"].get<std::string>();
                else response.metadata["layer"] = std::to_string(params["layer"].get<int>());
            }
            std::vector<std::string> changed;
            for (const auto& cell : touched.value())
                changed.push_back(std::to_string(cell.x) + "," + std::to_string(cell.z));
            response.changed_object_ids = std::move(changed);
        }
        return response;
    }

    if (action == "undo") {
        const auto kind = params.value("kind", std::string{"height"});
        if (kind == "paint") {
            if (auto missing = require_paint()) return *missing;
            if (context.terrain_paint_history->undo_size() == 0)
                return make_response(ExitCode::ValidationFailed, "No terrain paint undo", {},
                    {session_error("TERRAIN-PAINT-UNDO-EMPTY", "No paint strokes to undo.", "Apply a paint stroke first.")});
            const auto result = context.terrain_paint_history->undo(*context.terrain_paint);
            if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
            reload_now(false);
            return make_response(ExitCode::Success, "Terrain paint undone");
        }
        if (kind == "foliage") {
            if (auto missing = require_foliage()) return *missing;
            if (context.foliage_density_history->undo_size() == 0)
                return make_response(ExitCode::ValidationFailed, "No foliage density undo", {},
                    {session_error("FOLIAGE-UNDO-EMPTY", "No foliage strokes to undo.", "Apply a foliage stroke first.")});
            const auto result = context.foliage_density_history->undo(*context.foliage_density);
            if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
            reload_foliage_now();
            return make_response(ExitCode::Success, "Foliage density undone");
        }
        if (auto missing = require_edits()) return *missing;
        if (context.terrain_history->undo_size() == 0)
            return make_response(ExitCode::ValidationFailed, "No terrain height undo", {},
                {session_error("TERRAIN-UNDO-EMPTY", "No height strokes to undo.", "Apply a height stroke first.")});
        const auto result = context.terrain_history->undo(*context.terrain_edits);
        if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
        reload_now(true);
        return make_response(ExitCode::Success, "Terrain height undone");
    }

    if (action == "redo") {
        const auto kind = params.value("kind", std::string{"height"});
        if (kind == "paint") {
            if (auto missing = require_paint()) return *missing;
            if (context.terrain_paint_history->redo_size() == 0)
                return make_response(ExitCode::ValidationFailed, "No terrain paint redo", {},
                    {session_error("TERRAIN-PAINT-REDO-EMPTY", "No paint strokes to redo.", "Undo a paint stroke first.")});
            const auto result = context.terrain_paint_history->redo(*context.terrain_paint);
            if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
            reload_now(false);
            return make_response(ExitCode::Success, "Terrain paint redone");
        }
        if (kind == "foliage") {
            if (auto missing = require_foliage()) return *missing;
            if (context.foliage_density_history->redo_size() == 0)
                return make_response(ExitCode::ValidationFailed, "No foliage density redo", {},
                    {session_error("FOLIAGE-REDO-EMPTY", "No foliage strokes to redo.", "Undo a foliage stroke first.")});
            const auto result = context.foliage_density_history->redo(*context.foliage_density);
            if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
            reload_foliage_now();
            return make_response(ExitCode::Success, "Foliage density redone");
        }
        if (auto missing = require_edits()) return *missing;
        if (context.terrain_history->redo_size() == 0)
            return make_response(ExitCode::ValidationFailed, "No terrain height redo", {},
                {session_error("TERRAIN-REDO-EMPTY", "No height strokes to redo.", "Undo a height stroke first.")});
        const auto result = context.terrain_history->redo(*context.terrain_edits);
        if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
        reload_now(true);
        return make_response(ExitCode::Success, "Terrain height redone");
    }

    if (action == "save") return save_terrain();

    return make_response(ExitCode::InvalidArguments, "Unsupported terrain action: " + action, {},
        {session_error("TERRAIN-ACTION-UNKNOWN", "Unsupported terrain action: " + action,
            "Use raise/lower/flatten/set_height/carve_channel/raise_banks/paint/paint_foliage/"
            "paint_foliage_mixed/sample/undo/redo/save/batch.")});
}

EditorBridgeResponse execute_editor_operation(EditorSessionContext& context, const std::string& operation,
    const std::string& params_json) {
    try {
        const auto params = parse_params(params_json);
        if (operation == "editor_status") {
            std::map<std::string, std::string> metadata{
                {"editorRunning", context.editor_running ? "true" : "false"},
                {"liveAutomationEnabled", context.live_automation_enabled ? "true" : "false"},
                {"worldPath", context.world_path.generic_string()},
                {"projectRoot", context.project_root.generic_string()},
                {"testSession", context.test_session_state},
                {"sceneDirty", context.scene_dirty && *context.scene_dirty ? "true" : "false"},
            };
            if (context.selected_entity_id) metadata["selectedEntityId"] = *context.selected_entity_id;
            return make_response(ExitCode::Success, "Editor status", {}, {}, std::move(metadata));
        }
        if (operation == "scene_plan") {
            const auto plan = classify_scene_plan(params.value("description", std::string{}),
                params.value("targetPath", std::string{}));
            std::map<std::string, std::string> metadata{
                {"targetKind", plan.target_kind},
                {"requiresCompile", plan.requires_compile},
                {"requiresReload", plan.requires_reload},
                {"recommendation", plan.recommendation},
            };
            return make_response(ExitCode::Success, plan.summary, {}, {}, std::move(metadata));
        }
        if (operation == "prefab_apply") return apply_asset_write(context, params);
        if (operation == "prefab_component_apply") {
            nlohmann::json forwarded = params;
            if (!forwarded.contains("kind")) forwarded["kind"] = "prefab";
            return apply_asset_write(context, forwarded);
        }
        if (operation == "asset_apply") {
            if (params.value("action", std::string{}) == "refresh_catalog") {
                const auto refreshed = refresh_prefab_catalog(context);
                if (!refreshed) return make_response(ExitCode::ValidationFailed, refreshed.error().message, {}, {refreshed.error()});
                return make_response(ExitCode::Success, "Asset catalog refreshed", {}, {},
                    {{"prefabCount", std::to_string(refreshed.value())}});
            }
            return apply_asset_write(context, params);
        }
        if (operation == "terrain_apply") {
            return apply_terrain_operation(context, params);
        }
        if (operation == "water_apply") {
            return apply_water_operation(context, params);
        }
        if (operation == "scene_apply" && params.value("action", std::string{}) == "sample_terrain") {
            nlohmann::json sample_params = params;
            sample_params["action"] = "sample";
            return apply_terrain_operation(context, sample_params);
        }
        // Script/HUD ops stay available without a scene and during play test — agents iterate
        // gameplay without rebuilds or physical volume overlaps.
        if (operation == "lua_apply") {
            const auto relative = params.value("path", std::string{});
            const auto source = params.value("source", std::string{});
            if (relative.empty() || source.empty()) {
                return make_response(ExitCode::InvalidArguments, "path and source are required", {},
                    {session_error("LUA-PAYLOAD-REQUIRED", "Lua apply requires path and source.", "Provide script path and body.")});
            }
            const auto absolute = context.project_root / relative;
            const auto written = write_lua_script_atomic(absolute, source);
            if (!written) return make_response(ExitCode::ValidationFailed, written.error().message, {}, {written.error()});
            return make_response(ExitCode::Success, "Lua script written", {}, {},
                {{"scriptPath", relative}, {"requiresReload", "lua_hot_reload"}});
        }
        if (operation == "hud_apply") {
            const auto relative = params.value("path", std::string{});
            const auto source = params.value("source", std::string{});
            if (relative.empty() || source.empty()) {
                return make_response(ExitCode::InvalidArguments, "path and source are required", {},
                    {session_error("HUD-PAYLOAD-REQUIRED", "HUD/UI canvas apply requires path and JSON body.",
                        "Provide *.uicanvas.json or *.hud.json path and JSON body.")});
            }
            const auto absolute = context.project_root / relative;
            const auto lower = [&] {
                std::string value = relative;
                std::transform(value.begin(), value.end(), value.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return value;
            }();
            const bool is_canvas =
                lower.find(".uicanvas.json") != std::string::npos || lower.find(".canvas.json") != std::string::npos;
            const auto written = is_canvas ? write_ui_canvas_json_atomic(absolute, source)
                                          : write_hud_json_atomic(absolute, source);
            if (!written) return make_response(ExitCode::ValidationFailed, written.error().message, {}, {written.error()});
            return make_response(ExitCode::Success, is_canvas ? "UI canvas written" : "HUD asset written", {}, {},
                {{"hudPath", relative}, {"requiresReload", "hud_hot_reload"},
                    {"kind", is_canvas ? "ui_canvas" : "hud_asset"}});
        }
        if (operation == "ui_canvas_mutate") {
            const auto relative = params.value("path", std::string{});
            const auto action = params.value("action", std::string{});
            if (relative.empty() || action.empty()) {
                return make_response(ExitCode::InvalidArguments, "path and action are required", {},
                    {session_error("UICANVAS-MUTATE-ARGS", "ui_canvas_mutate requires path and action.",
                        "Example: {\"path\":\"assets/ui/player.uicanvas.json\",\"action\":\"move\",\"id\":\"player_health\",\"delta\":[8,0]}")});
            }
            nlohmann::json widget_params = params;
            widget_params.erase("path");
            widget_params.erase("action");
            const auto absolute = context.project_root / relative;
            const auto mutated = mutate_ui_canvas_file(absolute, action, widget_params.dump());
            if (!mutated) return make_response(ExitCode::ValidationFailed, mutated.error().message, {}, {mutated.error()});
            return make_response(ExitCode::Success, "UI canvas mutated", {}, {},
                {{"canvasPath", relative}, {"action", action}, {"widgetCount", std::to_string(mutated.value().widgets.size())},
                    {"requiresReload", "hud_hot_reload"}});
        }
        if (operation == "ui_stack") {
            if (!context.ui_canvas_stack) {
                return make_response(ExitCode::Unavailable, "UI canvas stack is not available", {},
                    {session_error("UICANVAS-STACK-MISSING", "No live UI canvas stack on the editor session.",
                        "Start the editor with MCP connection enabled.")});
            }
            auto action = params.value("action", std::string{});
            std::transform(action.begin(), action.end(), action.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            const auto id = params.value("id", std::string{});
            const auto path = params.value("path", std::string{});
            auto* stack = context.ui_canvas_stack;

            if (action == "register") {
                if (id.empty() || path.empty()) {
                    return make_response(ExitCode::InvalidArguments, "id and path are required for register", {},
                        {session_error("UICANVAS-STACK-REGISTER", "register requires id and path.",
                            "Example: {\"action\":\"register\",\"id\":\"pause\",\"path\":\"assets/ui/pause.uicanvas.json\"}")});
                }
                const auto registered = stack->register_canvas(id, context.project_root / path);
                if (!registered)
                    return make_response(ExitCode::ValidationFailed, registered.error().message, {}, {registered.error()});
                if (auto* canvas = stack->find_canvas(id)) {
                    if (id == "pause") canvas->set_text("pause.title", "PAUSED");
                }
            } else if (action == "push" || action == "show") {
                if (id.empty()) {
                    return make_response(ExitCode::InvalidArguments, "id is required", {},
                        {session_error("UICANVAS-STACK-ID", "push/show requires canvas id.",
                            "Example: {\"action\":\"push\",\"id\":\"pause\"}")});
                }
                if (!path.empty()) {
                    const auto registered = stack->register_canvas(id, context.project_root / path);
                    if (!registered)
                        return make_response(ExitCode::ValidationFailed, registered.error().message, {},
                            {registered.error()});
                    if (auto* canvas = stack->find_canvas(id)) {
                        if (id == "pause") canvas->set_text("pause.title", "PAUSED");
                    }
                }
                const auto shown = action == "show" ? stack->show(id) : stack->push(id);
                if (!shown)
                    return make_response(ExitCode::ValidationFailed, shown.error().message, {}, {shown.error()});
            } else if (action == "pop") {
                const auto popped = stack->pop();
                if (!popped)
                    return make_response(ExitCode::ValidationFailed, popped.error().message, {}, {popped.error()});
            } else if (action == "hide") {
                if (id.empty()) {
                    return make_response(ExitCode::InvalidArguments, "id is required for hide", {},
                        {session_error("UICANVAS-STACK-ID", "hide requires canvas id.",
                            "Example: {\"action\":\"hide\",\"id\":\"pause\"}")});
                }
                const auto hidden = stack->hide(id);
                if (!hidden)
                    return make_response(ExitCode::ValidationFailed, hidden.error().message, {}, {hidden.error()});
            } else if (action == "clear" || action == "clear_modals") {
                stack->clear_modals();
            } else if (action == "status" || action.empty()) {
                // fall through to status metadata
            } else {
                return make_response(ExitCode::InvalidArguments, "Unknown ui_stack action", {},
                    {session_error("UICANVAS-STACK-ACTION", "Unsupported action: " + action,
                        "Use register, push, pop, show, hide, clear, or status.")});
            }

            std::string top;
            if (const auto top_id = stack->top_modal()) top = *top_id;
            std::string stack_csv;
            for (const auto& entry : stack->modal_ids()) {
                if (!stack_csv.empty()) stack_csv += ',';
                stack_csv += entry;
            }
            return make_response(ExitCode::Success, "UI canvas stack updated", {}, {},
                {{"action", action.empty() ? "status" : action}, {"id", id}, {"top", top},
                    {"stack", stack_csv}, {"depth", std::to_string(stack->modal_ids().size())}});
        }
        if (operation == "lua_call") {
            if (!context.lua_runtime) {
                return make_response(ExitCode::Unavailable, "Lua runtime is not available", {},
                    {session_error("LUA-RUNTIME-MISSING", "No live Lua runtime on the editor session.",
                        "Start the editor with MCP connection enabled.")});
            }
            auto kind = params.value("kind", std::string{});
            std::transform(kind.begin(), kind.end(), kind.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            const auto binding_id = params.value("id", std::string{});
            const auto handler_override = params.value("handler", std::string{});
            nlohmann::json payload = params.contains("payload") && params["payload"].is_object()
                ? params["payload"]
                : nlohmann::json::object();

            context.lua_runtime->clear_recent_errors();
            if (kind == "interaction" || kind == "interact") {
                if (binding_id.empty()) {
                    return make_response(ExitCode::InvalidArguments, "id is required for interaction", {},
                        {session_error("LUA-CALL-ID", "interaction kind requires binding id.",
                            "Example: {\"kind\":\"interaction\",\"id\":\"use_campfire\",\"type\":\"enter\"}")});
                }
                InteractionEvent event;
                const auto type = params.value("type", std::string{"enter"});
                event.type = (type == "exit") ? InteractionEventType::Exit : InteractionEventType::Enter;
                event.interaction_id = binding_id;
                event.placement_entity_id = payload.value("placementEntityId", std::string{"mcp"});
                event.interactor_id = payload.value("interactorId", std::string{"player"});
                event.volume_index = static_cast<std::uint32_t>(payload.value("volumeIndex", 0));
                context.lua_runtime->dispatch_interaction(event);
            } else if (kind == "combathurt" || kind == "combat_hurt" || kind == "hurt") {
                if (binding_id.empty()) {
                    return make_response(ExitCode::InvalidArguments, "id is required for combatHurt", {},
                        {session_error("LUA-CALL-ID", "combatHurt kind requires binding id.",
                            "Example: {\"kind\":\"combatHurt\",\"id\":\"body\"}")});
                }
                CombatContactEvent event;
                event.attacker_id = payload.value("attackerId", std::string{"mcp"});
                event.hurt_placement_entity_id = payload.value("hurtPlacementEntityId", std::string{"mcp-target"});
                event.hurt_combat_id = binding_id;
                event.hurt_volume_index = static_cast<std::uint32_t>(payload.value("hurtVolumeIndex", 0));
                context.lua_runtime->dispatch_combat_hit(event);
            } else if (kind == "handler" || kind.empty()) {
                const auto handler = !handler_override.empty() ? handler_override : binding_id;
                if (handler.empty()) {
                    return make_response(ExitCode::InvalidArguments, "handler is required", {},
                        {session_error("LUA-CALL-HANDLER", "handler kind requires handler name.",
                            "Example: {\"kind\":\"handler\",\"handler\":\"on_body_hit\",\"payload\":{...}}")});
                }
                const std::string payload_json = payload.dump();
                const auto called = context.lua_runtime->call_handler(handler, payload_json);
                if (!called) {
                    return make_response(ExitCode::ValidationFailed, called.error().message, {}, {called.error()},
                        {{"handler", handler}, {"kind", kind.empty() ? "handler" : kind}});
                }
            } else {
                return make_response(ExitCode::InvalidArguments, "Unknown lua_call kind", {},
                    {session_error("LUA-CALL-KIND", "Unsupported kind: " + kind,
                        "Use interaction, combatHurt, or handler.")});
            }

            if (!context.lua_runtime->recent_errors().empty()) {
                auto errors = context.lua_runtime->recent_errors();
                const auto message = errors.back().message;
                return make_response(ExitCode::ValidationFailed, message, {}, std::move(errors),
                    {{"kind", kind.empty() ? "handler" : kind}, {"id", binding_id}});
            }
            return make_response(ExitCode::Success, "Lua handler dispatched", {}, {},
                {{"kind", kind.empty() ? "handler" : kind}, {"id", binding_id},
                    {"handler", handler_override}});
        }
        if (operation == "quest_call") {
            if (!context.quest_runtime) {
                return make_response(ExitCode::Unavailable, "Quest runtime is not available", {},
                    {session_error("QUEST-RUNTIME-MISSING", "No live QuestRuntime on the editor session.",
                        "Start the editor with MCP connection enabled.")});
            }
            auto kind = params.value("kind", std::string{});
            std::transform(kind.begin(), kind.end(), kind.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            for (char& c : kind) {
                if (c == '-' || c == ' ') c = '_';
            }
            const auto quest_id = params.value("questId", params.value("quest_id", std::string{}));
            const auto objective_id = params.value("objectiveId", params.value("objective_id", std::string{}));

            auto sync_hud = [&]() {
                if (!context.hud_runtime) return;
                context.hud_runtime->set_text("quest.objectiveText", context.quest_runtime->primary_objective_text());
            };
            auto status_metadata = [&](const QuestProgressStatus& status) {
                std::string completed;
                for (std::size_t i = 0; i < status.completed_objective_ids.size(); ++i) {
                    if (i) completed += ',';
                    completed += status.completed_objective_ids[i];
                }
                return std::map<std::string, std::string>{
                    {"kind", kind},
                    {"questId", status.quest_id},
                    {"status", to_string(status.status)},
                    {"currentObjectiveId", status.current_objective_id},
                    {"currentObjectiveSummary", status.current_objective_summary},
                    {"completedObjectiveIds", completed},
                };
            };

            if (kind == "list") {
                const auto active = context.quest_runtime->list_active();
                std::string ids;
                for (std::size_t i = 0; i < active.size(); ++i) {
                    if (i) ids += ',';
                    ids += active[i].quest_id;
                }
                return make_response(ExitCode::Success, "Active quests listed", {}, {},
                    {{"kind", "list"}, {"count", std::to_string(active.size())}, {"questIds", ids}});
            }
            if (quest_id.empty()) {
                return make_response(ExitCode::InvalidArguments, "questId is required", {},
                    {session_error("QUEST-CALL-ID", "quest_call requires questId (except kind=list).",
                        "Example: {\"kind\":\"start\",\"questId\":\"sq_01_cart_again\"}")});
            }
            if (kind == "start") {
                const auto result = context.quest_runtime->start(quest_id);
                if (!result) {
                    return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()},
                        {{"kind", kind}, {"questId", quest_id}});
                }
                sync_hud();
                const auto status = context.quest_runtime->status(quest_id);
                if (!status) {
                    return make_response(ExitCode::ValidationFailed, status.error().message, {}, {status.error()},
                        {{"kind", kind}, {"questId", quest_id}});
                }
                return make_response(ExitCode::Success, "Quest started", {}, {}, status_metadata(status.value()));
            }
            if (kind == "complete_objective" || kind == "completeobjective") {
                if (objective_id.empty()) {
                    return make_response(ExitCode::InvalidArguments, "objectiveId is required", {},
                        {session_error("QUEST-CALL-OBJECTIVE", "complete_objective requires objectiveId.",
                            "Example: {\"kind\":\"complete_objective\",\"questId\":\"sq_01_cart_again\",\"objectiveId\":\"find_pellin\"}")});
                }
                const auto result = context.quest_runtime->complete_objective(quest_id, objective_id);
                if (!result) {
                    return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()},
                        {{"kind", kind}, {"questId", quest_id}, {"objectiveId", objective_id}});
                }
                sync_hud();
                const auto status = context.quest_runtime->status(quest_id);
                if (!status) {
                    return make_response(ExitCode::ValidationFailed, status.error().message, {}, {status.error()},
                        {{"kind", kind}, {"questId", quest_id}});
                }
                return make_response(ExitCode::Success, "Objective completed", {}, {}, status_metadata(status.value()));
            }
            if (kind == "abandon") {
                const auto result = context.quest_runtime->abandon(quest_id);
                if (!result) {
                    return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()},
                        {{"kind", kind}, {"questId", quest_id}});
                }
                sync_hud();
                const auto status = context.quest_runtime->status(quest_id);
                if (!status) {
                    return make_response(ExitCode::ValidationFailed, status.error().message, {}, {status.error()},
                        {{"kind", kind}, {"questId", quest_id}});
                }
                return make_response(ExitCode::Success, "Quest abandoned", {}, {}, status_metadata(status.value()));
            }
            if (kind == "status") {
                const auto status = context.quest_runtime->status(quest_id);
                if (!status) {
                    return make_response(ExitCode::ValidationFailed, status.error().message, {}, {status.error()},
                        {{"kind", kind}, {"questId", quest_id}});
                }
                return make_response(ExitCode::Success, "Quest status", {}, {}, status_metadata(status.value()));
            }
            return make_response(ExitCode::InvalidArguments, "Unknown quest_call kind", {},
                {session_error("QUEST-CALL-KIND", "Unsupported kind: " + kind,
                    "Use start, complete_objective, abandon, status, or list.")});
        }
        if (operation == "standing_call") {
            if (!context.standing_runtime) {
                return make_response(ExitCode::Unavailable, "Standing runtime is not available", {},
                    {session_error("STANDING-RUNTIME-MISSING", "No live StandingRuntime on the editor session.",
                        "Start the editor with MCP connection enabled.")});
            }
            auto kind = params.value("kind", std::string{});
            std::transform(kind.begin(), kind.end(), kind.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            for (char& c : kind) {
                if (c == '-' || c == ' ') c = '_';
            }
            const auto faction_id = params.value("factionId", params.value("faction_id", std::string{}));
            auto score_meta = [&](const std::string& id) {
                std::map<std::string, std::string> meta{{"kind", kind}, {"factionId", id}};
                if (const auto score = context.standing_runtime->get(id); score) {
                    meta["score"] = std::to_string(score.value());
                }
                if (const auto rank = context.standing_runtime->rank(id); rank) {
                    meta["rankId"] = rank.value();
                }
                return meta;
            };
            if (kind == "list") {
                const auto tracked = context.standing_runtime->list_tracked();
                std::string ids;
                for (std::size_t i = 0; i < tracked.size(); ++i) {
                    if (i) ids += ',';
                    ids += tracked[i].faction_id + "=" + std::to_string(tracked[i].score);
                }
                return make_response(ExitCode::Success, "Tracked standing listed", {}, {},
                    {{"kind", "list"}, {"count", std::to_string(tracked.size())}, {"scores", ids}});
            }
            if (kind == "lock_in") {
                const auto locked = context.standing_runtime->lock_in_faction();
                if (!locked) {
                    return make_response(ExitCode::ValidationFailed, locked.error().message, {}, {locked.error()},
                        {{"kind", kind}});
                }
                return make_response(ExitCode::Success, "Lock-in queried", {}, {},
                    {{"kind", kind}, {"factionId", locked.value()}});
            }
            if (faction_id.empty()) {
                return make_response(ExitCode::InvalidArguments, "factionId is required", {},
                    {session_error("STANDING-CALL-ID", "standing_call requires factionId (except kind=list|lock_in).",
                        "Example: {\"kind\":\"adjust\",\"factionId\":\"cristallo\",\"delta\":10}")});
            }
            if (kind == "get" || kind == "status") {
                const auto score = context.standing_runtime->get(faction_id);
                if (!score) {
                    return make_response(ExitCode::ValidationFailed, score.error().message, {}, {score.error()},
                        {{"kind", kind}, {"factionId", faction_id}});
                }
                return make_response(ExitCode::Success, "Standing score", {}, {}, score_meta(faction_id));
            }
            if (kind == "set") {
                const double score = params.value("score", params.value("value", 0.0));
                const auto result = context.standing_runtime->set(faction_id, score);
                if (!result) {
                    return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()},
                        {{"kind", kind}, {"factionId", faction_id}});
                }
                return make_response(ExitCode::Success, "Standing set", {}, {}, score_meta(faction_id));
            }
            if (kind == "adjust") {
                const double delta = params.value("delta", 0.0);
                const auto result = context.standing_runtime->adjust(faction_id, delta);
                if (!result) {
                    return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()},
                        {{"kind", kind}, {"factionId", faction_id}});
                }
                return make_response(ExitCode::Success, "Standing adjusted", {}, {}, score_meta(faction_id));
            }
            if (kind == "rank") {
                const auto rank = context.standing_runtime->rank(faction_id);
                if (!rank) {
                    return make_response(ExitCode::ValidationFailed, rank.error().message, {}, {rank.error()},
                        {{"kind", kind}, {"factionId", faction_id}});
                }
                return make_response(ExitCode::Success, "Standing rank", {}, {}, score_meta(faction_id));
            }
            if (kind == "meets") {
                WorldForgeQuestStandingRequirement req;
                req.faction_id = faction_id;
                if (params.contains("minScore")) req.min_score = params.value("minScore", 0.0);
                req.min_rank_id = params.value("minRankId", params.value("min_rank_id", std::string{}));
                const auto meets = context.standing_runtime->meets_requirement(req);
                if (!meets) {
                    return make_response(ExitCode::ValidationFailed, meets.error().message, {}, {meets.error()},
                        {{"kind", kind}, {"factionId", faction_id}});
                }
                auto meta = score_meta(faction_id);
                meta["meets"] = meets.value() ? "true" : "false";
                return make_response(ExitCode::Success, "Standing requirement checked", {}, {}, std::move(meta));
            }
            return make_response(ExitCode::InvalidArguments, "Unknown standing_call kind", {},
                {session_error("STANDING-CALL-KIND", "Unsupported kind: " + kind,
                    "Use get, set, adjust, rank, meets, lock_in, or list.")});
        }
        if (!context.scene || !context.history) {
            return make_response(ExitCode::Unavailable, "Editor session is unavailable", {}, {session_error(
                "EDITOR-SESSION-MISSING", "Scene or command history is not available.", "Launch the editor first.")});
        }
        if (context.test_session_active) {
            return make_response(ExitCode::Unavailable, "Scene edits are blocked during an active play test", {},
                {session_error("EDITOR-PLAY-SESSION-ACTIVE", "End the play test before applying scene edits.",
                    "Send test session end or switch back to Scene tab.")});
        }
        if (operation == "scene_apply") {
            const auto action = params.value("action", std::string{});
            if (action == "undo") {
                const auto result = context.history->undo(*context.scene);
                if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
                if (context.scene_dirty) *context.scene_dirty = true;
                return make_response(ExitCode::Success, context.history->last_summary(),
                    context.history->last_changed_object_ids());
            }
            if (action == "redo") {
                const auto result = context.history->redo(*context.scene);
                if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
                if (context.scene_dirty) *context.scene_dirty = true;
                return make_response(ExitCode::Success, context.history->last_summary(),
                    context.history->last_changed_object_ids());
            }
            if (action == "save") {
                const auto saved = context.scene->save_atomic(context.world_path);
                if (!saved) return make_response(ExitCode::ValidationFailed, saved.error().message, {}, {saved.error()});
                if (context.scene_dirty) *context.scene_dirty = false;
                return make_response(ExitCode::Success, "World saved", {}, {},
                    {{"savedPath", context.world_path.generic_string()}});
            }
            if (action == "place") {
                const auto prefab = params.value("prefab", std::string{});
                if (prefab.empty()) {
                    return make_response(ExitCode::InvalidArguments, "prefab is required", {},
                        {session_error("SCENE-PREFAB-REQUIRED", "place requires prefab path.", "Provide assets/... path.")});
                }
                auto transform = transform_from_json(params.contains("transform") ? params["transform"] : nlohmann::json::object());
                apply_terrain_snap(transform, params);
                std::optional<EntityId> requested;
                if (params.contains("entityId")) {
                    const auto parsed = EntityId::parse(params["entityId"].get<std::string>());
                    if (!parsed) return make_response(ExitCode::ValidationFailed, parsed.error().message, {}, {parsed.error()});
                    requested = parsed.value();
                }
                std::optional<PrefabAsset> seed;
                if (context.prefab_catalog) {
                    if (const auto* asset = find_prefab_in_catalog(*context.prefab_catalog, prefab)) seed = *asset;
                }
                auto command = std::make_unique<PlaceWorldObjectCommand>(params.value("name", std::string{"Placed Object"}),
                    prefab, transform, requested,
                    params.contains("characterAsset") ? std::optional<std::string>(params["characterAsset"].get<std::string>())
                                                    : std::nullopt,
                    std::move(seed));
                const auto result = context.history->execute(*context.scene, std::move(command));
                if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
                if (context.scene_dirty) *context.scene_dirty = true;
                if (context.selected && !context.history->last_changed_object_ids().empty()) {
                    if (const auto parsed = EntityId::parse(context.history->last_changed_object_ids().front()); parsed)
                        *context.selected = parsed.value();
                }
                return make_response(ExitCode::Success, context.history->last_summary(),
                    context.history->last_changed_object_ids());
            }
            if (action == "move") {
                const auto entity_text = params.value("entityId", std::string{});
                const auto parsed = EntityId::parse(entity_text);
                if (!parsed) return make_response(ExitCode::ValidationFailed, parsed.error().message, {}, {parsed.error()});
                auto transform = transform_from_json(params.contains("transform") ? params["transform"] : nlohmann::json::object());
                apply_terrain_snap(transform, params);
                const auto result = context.history->execute(*context.scene,
                    std::make_unique<MoveWorldObjectCommand>(parsed.value(), transform));
                if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
                if (context.scene_dirty) *context.scene_dirty = true;
                return make_response(ExitCode::Success, context.history->last_summary(),
                    context.history->last_changed_object_ids());
            }
            if (action == "remove") {
                const auto parsed = EntityId::parse(params.value("entityId", std::string{}));
                if (!parsed) return make_response(ExitCode::ValidationFailed, parsed.error().message, {}, {parsed.error()});
                const auto result = context.history->execute(*context.scene,
                    std::make_unique<RemoveWorldObjectCommand>(parsed.value()));
                if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
                if (context.scene_dirty) *context.scene_dirty = true;
                if (context.selected && *context.selected == parsed.value()) context.selected->reset();
                return make_response(ExitCode::Success, context.history->last_summary(),
                    context.history->last_changed_object_ids());
            }
            if (action == "rename") {
                const auto parsed = EntityId::parse(params.value("entityId", std::string{}));
                if (!parsed) return make_response(ExitCode::ValidationFailed, parsed.error().message, {}, {parsed.error()});
                const auto result = context.history->execute(*context.scene,
                    std::make_unique<RenameEntityCommand>(parsed.value(), params.value("name", std::string{"Renamed"})));
                if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
                if (context.scene_dirty) *context.scene_dirty = true;
                return make_response(ExitCode::Success, context.history->last_summary(),
                    context.history->last_changed_object_ids());
            }
            if (action == "duplicate") {
                const auto parsed = EntityId::parse(params.value("entityId", std::string{}));
                if (!parsed) return make_response(ExitCode::ValidationFailed, parsed.error().message, {}, {parsed.error()});
                const auto placement = context.scene->placement(parsed.value());
                const auto transform = context.scene->transform(parsed.value());
                const auto name = context.scene->name(parsed.value());
                if (!placement || !transform || !name) {
                    return make_response(ExitCode::ValidationFailed, "Selected entity is not a placeable object", {},
                        {session_error("SCENE-DUPLICATE-INVALID", "Entity has no placement.", "Select a placed prefab.")});
                }
                TransformComponent offset = *transform;
                offset.position[0] += 1.0f;
                std::optional<PrefabAsset> seed;
                if (context.prefab_catalog) {
                    if (const auto* prefab = find_prefab_in_catalog(*context.prefab_catalog, placement->prefab_asset))
                        seed = *prefab;
                }
                auto command = std::make_unique<PlaceWorldObjectCommand>(*name + " Copy", placement->prefab_asset, offset,
                    std::nullopt, placement->character_asset, std::move(seed));
                const auto result = context.history->execute(*context.scene, std::move(command));
                if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
                if (context.scene_dirty) *context.scene_dirty = true;
                return make_response(ExitCode::Success, context.history->last_summary(),
                    context.history->last_changed_object_ids());
            }
            if (action == "add_component" || action == "remove_component" || action == "set_component") {
                const auto parsed = EntityId::parse(params.value("entityId", std::string{}));
                if (!parsed) return make_response(ExitCode::ValidationFailed, parsed.error().message, {}, {parsed.error()});
                if (action == "remove_component") {
                    const auto component_id = params.value("componentId", params.value("id", std::string{}));
                    if (component_id.empty()) {
                        return make_response(ExitCode::InvalidArguments, "componentId is required", {},
                            {session_error("SCENE-COMPONENT-ID-REQUIRED", "remove_component requires componentId.",
                                "Provide the component id.")});
                    }
                    const auto result = context.history->execute(*context.scene,
                        std::make_unique<RemoveEntityComponentCommand>(parsed.value(), component_id));
                    if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
                    if (context.scene_dirty) *context.scene_dirty = true;
                    return make_response(ExitCode::Success, context.history->last_summary(),
                        context.history->last_changed_object_ids());
                }
                nlohmann::json entry_json = params.contains("component") ? params.at("component") : params;
                if (!entry_json.contains("id") && params.contains("componentId"))
                    entry_json["id"] = params["componentId"];
                if (!entry_json.contains("type") && params.contains("type")) entry_json["type"] = params["type"];
                if (!entry_json.contains("data") && params.contains("data")) entry_json["data"] = params["data"];
                const auto entry = authored_component_entry_from_json(entry_json.dump());
                if (!entry) return make_response(ExitCode::ValidationFailed, entry.error().message, {}, {entry.error()});
                std::unique_ptr<SceneCommand> command;
                if (action == "add_component")
                    command = std::make_unique<AddEntityComponentCommand>(parsed.value(), entry.value());
                else
                    command = std::make_unique<SetEntityComponentCommand>(parsed.value(), entry.value());
                const auto result = context.history->execute(*context.scene, std::move(command));
                if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
                if (context.scene_dirty) *context.scene_dirty = true;
                return make_response(ExitCode::Success, context.history->last_summary(),
                    context.history->last_changed_object_ids());
            }
            if (action == "batch") return apply_scene_batch(context, params);
            return make_response(ExitCode::InvalidArguments, "Unknown scene action", {},
                {session_error("SCENE-ACTION-UNKNOWN", "Unsupported scene action.",
                    "Use place/move/remove/rename/duplicate/add_component/remove_component/set_component/batch/undo/redo/save/sample_terrain.")});
        }
        if (operation == "entity_component_apply") {
            nlohmann::json forwarded = params;
            if (!forwarded.contains("action")) forwarded["action"] = "add_component";
            // Reuse scene_apply component actions without re-entering this operation.
            const auto action = forwarded.value("action", std::string{"add_component"});
            forwarded["action"] = action;
            return execute_editor_operation(context, "scene_apply", forwarded.dump());
        }
        if (operation == "world_forge_apply") {
            return apply_world_forge_operation(context.project_root, params);
        }
        if (operation == "project_git") {
            return apply_project_git_operation(context.project_root, params);
        }
        return make_response(ExitCode::InvalidArguments, "Unknown editor operation", {},
            {session_error("EDITOR-OP-UNKNOWN", "Unsupported operation: " + operation,
                "Use editor_status/scene_plan/.../hud_apply/world_forge_apply/project_git/ui_canvas_mutate/ui_stack/lua_call/quest_call/standing_call.")});
    } catch (const std::exception& exception) {
        return make_response(ExitCode::InternalError, "Editor operation failed", {},
            {session_error("EDITOR-OP-EXCEPTION", exception.what(), "Check params JSON and retry.")});
    }
}

} // namespace engine
