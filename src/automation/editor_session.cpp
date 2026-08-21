#include "engine/automation/editor_session.h"

#include "engine/assets/asset_registry.h"
#include "engine/assets/hud_asset.h"
#include "engine/assets/material_asset.h"
#include "engine/assets/mesh_asset.h"
#include "engine/assets/particle_emitter_asset.h"
#include "engine/assets/prefab_asset.h"
#include "engine/assets/script_bindings_asset.h"
#include "engine/assets/ui_canvas_asset.h"
#include "engine/assets/ui_canvas_mutate.h"
#include "engine/assets/ui_theme_asset.h"
#include "engine/assets/world_forge_archetypes_asset.h"
#include "engine/assets/world_forge_quests_asset.h"
#include "engine/automation/asset_bake_commands.h"
#include "engine/automation/command.h"
#include "engine/automation/mcp_job_queue.h"
#include "engine/automation/project_git_commands.h"
#include "engine/automation/terrain_edit_commands.h"
#include "engine/automation/world_forge_commands.h"
#include "engine/core/id_slug.h"
#include "engine/dialogue/dialogue_runtime.h"
#include "engine/dialogue/dialogue_ui.h"
#include "engine/flag/flag_runtime.h"
#include "engine/inventory/inventory_runtime.h"
#include "engine/inventory/starter_loadout.h"
#include "engine/quest/quest_runtime.h"
#include "engine/scripting/lua_runtime.h"
#include "engine/standing/standing_runtime.h"
#include "engine/ui/hud_runtime.h"
#include "engine/ui/ui_canvas_stack.h"
#include "engine/world/authored_components.h"
#include "engine/world/combat_volumes.h"
#include "engine/world/foliage_density.h"
#include "engine/world/foliage_layers.h"
#include "engine/world/interaction_volumes.h"
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
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace engine {
namespace {

EngineError session_error(std::string code, std::string message,
                          std::string remedy) {
  return EngineError{std::move(code),
                     Severity::Error,
                     ErrorCategory::Validation,
                     "automation",
                     std::move(message),
                     ENGINE_SOURCE_CONTEXT,
                     {},
                     std::move(remedy),
                     make_correlation_id()};
}

EditorBridgeResponse
make_response(ExitCode exit_code, std::string summary,
              std::vector<std::string> changed = {},
              std::vector<EngineError> diagnostics = {},
              std::map<std::string, std::string> metadata = {}) {
  EditorBridgeResponse response;
  response.exit_code = exit_code;
  response.summary = std::move(summary);
  response.changed_object_ids = std::move(changed);
  response.diagnostics = std::move(diagnostics);
  response.metadata = std::move(metadata);
  return response;
}

nlohmann::json parse_params(const std::string &params_json) {
  if (params_json.empty())
    return nlohmann::json::object();
  return nlohmann::json::parse(params_json);
}

TransformComponent transform_from_json(const nlohmann::json &json) {
  TransformComponent transform;
  if (json.contains("position") && json["position"].is_array() &&
      json["position"].size() >= 3) {
    transform.position[0] = json["position"][0];
    transform.position[1] = json["position"][1];
    transform.position[2] = json["position"][2];
  }
  if (json.contains("rotation") && json["rotation"].is_array() &&
      json["rotation"].size() >= 4) {
    transform.rotation[0] = json["rotation"][0];
    transform.rotation[1] = json["rotation"][1];
    transform.rotation[2] = json["rotation"][2];
    transform.rotation[3] = json["rotation"][3];
  }
  if (json.contains("scale") && json["scale"].is_array() &&
      json["scale"].size() >= 3) {
    transform.scale[0] = json["scale"][0];
    transform.scale[1] = json["scale"][1];
    transform.scale[2] = json["scale"][2];
  }
  return transform;
}

void apply_terrain_snap(TransformComponent &transform,
                        const nlohmann::json &params) {
  if (!params.value("snapToTerrain", false))
    return;
  const float offset = params.value("groundOffset", 0.0f);
  transform.position[1] =
      sample_terrain_height(transform.position[0], transform.position[2]) +
      offset;
}

std::string ascii_lower_copy(std::string value) {
  for (char &c : value)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}

float ground_offset_for_prefab(const std::string &prefab_path,
                               float default_offset,
                               const nlohmann::json &params) {
  if (params.contains("groundOffsetsByPrefab") &&
      params["groundOffsetsByPrefab"].is_object()) {
    const auto prefab_l = ascii_lower_copy(prefab_path);
    for (auto it = params["groundOffsetsByPrefab"].begin();
         it != params["groundOffsetsByPrefab"].end(); ++it) {
      if (!it.value().is_number())
        continue;
      const auto key_l = ascii_lower_copy(it.key());
      if (!key_l.empty() && prefab_l.find(key_l) != std::string::npos)
        return it.value().get<float>();
    }
  }
  if (params.value("usePrefabGroundDefaults", false)) {
    const auto prefab_l = ascii_lower_copy(prefab_path);
    if (prefab_l.find("oak") != std::string::npos ||
        prefab_l.find("/tree.") != std::string::npos ||
        prefab_l.find("\\tree.") != std::string::npos)
      return -0.9f;
    if (prefab_l.find("bush") != std::string::npos)
      return -0.3f;
  }
  return default_offset;
}

Result<EntityId> resolve_scene_entity_id(const Scene &scene,
                                         const nlohmann::json &params) {
  if (params.contains("entityId") && params["entityId"].is_string() &&
      !params["entityId"].get<std::string>().empty()) {
    return EntityId::parse(params["entityId"].get<std::string>());
  }
  const auto name = params.value("name", std::string{});
  if (name.empty()) {
    return Result<EntityId>::failure(session_error(
        "SCENE-ENTITY-REF", "entityId or unique name is required.",
        "Provide entityId UUID or name."));
  }
  std::vector<EntityId> matches;
  for (const auto &id : scene.entity_ids()) {
    const auto entity_name = scene.name(id);
    if (entity_name && *entity_name == name)
      matches.push_back(id);
  }
  if (matches.empty()) {
    return Result<EntityId>::failure(session_error(
        "SCENE-ENTITY-NAME", "No entity named '" + name + "'.",
        "Use list/query or provide entityId."));
  }
  if (matches.size() > 1) {
    return Result<EntityId>::failure(session_error(
        "SCENE-ENTITY-AMBIGUOUS",
        "Multiple entities named '" + name + "' (" +
            std::to_string(matches.size()) + ").",
        "Disambiguate with entityId."));
  }
  return Result<EntityId>::success(matches.front());
}

// Forest densify / bulk snap routinely exceeds 100 placements in one pass.
constexpr std::size_t k_max_scene_batch_ops = 512;

Result<std::unique_ptr<SceneCommand>> scene_command_from_op(
    const nlohmann::json &op, Scene *scene,
    const std::map<std::string, PrefabAsset> *prefab_catalog = nullptr) {
  nlohmann::json normalized_op = op;
  auto action = normalized_op.value("action", std::string{});
  if (action == "place_marker") {
    normalized_op["action"] = "place";
    if (!normalized_op.contains("prefab")) {
      normalized_op["prefab"] =
          "assets/prefabs/Scene Assets/camera_marker.prefab.json";
    }
    if (!normalized_op.contains("name"))
      normalized_op["name"] = "Marker";
    action = "place";
  }
  if ((action == "place" || action == "move") &&
      !normalized_op.contains("transform") &&
      (normalized_op.contains("x") || normalized_op.contains("z"))) {
    normalized_op["transform"] = {
        {"position", {normalized_op.value("x", 0.0f),
                      normalized_op.value("y", 0.0f),
                      normalized_op.value("z", 0.0f)}}};
  }
  if (action == "place") {
    const auto prefab = normalized_op.value("prefab", std::string{});
    if (prefab.empty()) {
      return Result<std::unique_ptr<SceneCommand>>::failure(
          session_error("SCENE-PREFAB-REQUIRED", "place requires prefab path.",
                        "Provide assets/... path."));
    }
    auto transform = transform_from_json(normalized_op.contains("transform")
                                             ? normalized_op["transform"]
                                             : nlohmann::json::object());
    apply_terrain_snap(transform, normalized_op);
    std::optional<EntityId> requested;
    if (normalized_op.contains("entityId")) {
      const auto parsed =
          EntityId::parse(normalized_op["entityId"].get<std::string>());
      if (!parsed)
        return Result<std::unique_ptr<SceneCommand>>::failure(parsed.error());
      requested = parsed.value();
    }
    std::optional<PrefabAsset> seed;
    if (prefab_catalog) {
      if (const auto *asset = find_prefab_in_catalog(*prefab_catalog, prefab))
        seed = *asset;
    }
    return Result<std::unique_ptr<SceneCommand>>::success(
        std::make_unique<PlaceWorldObjectCommand>(
            normalized_op.value("name", std::string{"Placed Object"}), prefab,
            transform,
            requested,
            normalized_op.contains("characterAsset")
                ? std::optional<std::string>(
                      normalized_op["characterAsset"].get<std::string>())
                : std::nullopt,
            std::move(seed)));
  }
  if (action == "move") {
    if (!scene) {
      return Result<std::unique_ptr<SceneCommand>>::failure(session_error(
          "SCENE-MISSING", "Scene is not bound.", "Open a world in the editor."));
    }
    const auto parsed = resolve_scene_entity_id(*scene, normalized_op);
    if (!parsed)
      return Result<std::unique_ptr<SceneCommand>>::failure(parsed.error());
    auto transform = transform_from_json(normalized_op.contains("transform")
                                             ? normalized_op["transform"]
                                             : nlohmann::json::object());
    apply_terrain_snap(transform, normalized_op);
    return Result<std::unique_ptr<SceneCommand>>::success(
        std::make_unique<MoveWorldObjectCommand>(parsed.value(), transform));
  }
  if (action == "remove") {
    if (!scene) {
      return Result<std::unique_ptr<SceneCommand>>::failure(session_error(
          "SCENE-MISSING", "Scene is not bound.", "Open a world in the editor."));
    }
    const auto parsed = resolve_scene_entity_id(*scene, op);
    if (!parsed)
      return Result<std::unique_ptr<SceneCommand>>::failure(parsed.error());
    return Result<std::unique_ptr<SceneCommand>>::success(
        std::make_unique<RemoveWorldObjectCommand>(parsed.value()));
  }
  if (action == "rename") {
    if (!scene) {
      return Result<std::unique_ptr<SceneCommand>>::failure(session_error(
          "SCENE-MISSING", "Scene is not bound.", "Open a world in the editor."));
    }
    const auto parsed = resolve_scene_entity_id(*scene, op);
    if (!parsed)
      return Result<std::unique_ptr<SceneCommand>>::failure(parsed.error());
    std::string new_name;
    if (op.contains("newName") && op["newName"].is_string())
      new_name = op["newName"].get<std::string>();
    else if (op.contains("entityId"))
      new_name = op.value("name", std::string{"Renamed"});
    else {
      return Result<std::unique_ptr<SceneCommand>>::failure(session_error(
          "SCENE-RENAME-NAME",
          "rename by name requires newName (name is the lookup).",
          "Provide {name,newName} or {entityId,name}."));
    }
    if (new_name.empty())
      new_name = "Renamed";
    return Result<std::unique_ptr<SceneCommand>>::success(
        std::make_unique<RenameEntityCommand>(parsed.value(),
                                              std::move(new_name)));
  }
  return Result<std::unique_ptr<SceneCommand>>::failure(session_error(
      "SCENE-BATCH-OP-UNKNOWN", "Unsupported batch operation: " + action,
      "Use place, move, remove, or rename in batch ops."));
}

EditorBridgeResponse apply_scene_batch(EditorSessionContext &context,
                                       const nlohmann::json &params) {
  if (!params.contains("ops") || !params["ops"].is_array()) {
    return make_response(
        ExitCode::InvalidArguments, "batch requires ops array", {},
        {session_error("SCENE-BATCH-OPS-REQUIRED",
                       "batch action requires a non-empty ops array.",
                       "Provide ops: [{action, ...}, ...].")});
  }
  const auto &ops = params["ops"];
  if (ops.empty()) {
    return make_response(
        ExitCode::InvalidArguments, "batch ops array is empty", {},
        {session_error("SCENE-BATCH-OPS-EMPTY",
                       "batch action requires at least one operation.",
                       "Provide one or more scene operations.")});
  }
  if (ops.size() > k_max_scene_batch_ops) {
    return make_response(
        ExitCode::InvalidArguments, "batch exceeds operation limit", {},
        {session_error("SCENE-BATCH-OPS-LIMIT",
                       "batch supports at most " +
                           std::to_string(k_max_scene_batch_ops) +
                           " operations.",
                       "Split the request into smaller batches.")});
  }
  std::vector<std::unique_ptr<SceneCommand>> commands;
  commands.reserve(ops.size());
  for (std::size_t index = 0; index < ops.size(); ++index) {
    if (!ops[index].is_object()) {
      return make_response(
          ExitCode::InvalidArguments, "batch op must be an object", {},
          {session_error("SCENE-BATCH-OP-INVALID",
                         "Each batch op must be a JSON object.",
                         "Use {action, ...} entries in ops.")},
          {{"failedOpIndex", std::to_string(index)}});
    }
    auto built = scene_command_from_op(ops[index], context.scene,
                                       context.prefab_catalog);
    if (!built) {
      return make_response(ExitCode::ValidationFailed, built.error().message,
                           {}, {built.error()},
                           {{"failedOpIndex", std::to_string(index)}});
    }
    commands.push_back(std::move(built.value()));
  }
  const auto label = params.value("label", std::string{});
  const auto result = context.history->execute(
      *context.scene,
      std::make_unique<CompositeSceneCommand>(label, std::move(commands)));
  if (!result) {
    return make_response(ExitCode::ValidationFailed, result.error().message, {},
                         {result.error()}, {{"appliedCount", "0"}});
  }
  if (context.scene_dirty)
    *context.scene_dirty = true;
  if (context.static_render_cache_dirty)
    *context.static_render_cache_dirty = true;
  const auto changed = context.history->last_changed_object_ids();
  if (params.value("save", false)) {
    const auto saved = context.scene->save_atomic(context.world_path);
    if (!saved)
      return make_response(ExitCode::ValidationFailed, saved.error().message,
                           changed, {saved.error()},
                           {{"appliedCount", std::to_string(ops.size())}});
    if (context.scene_dirty)
      *context.scene_dirty = false;
    return make_response(ExitCode::Success, "Batch applied and world saved",
                         changed, {},
                         {{"appliedCount", std::to_string(ops.size())},
                          {"savedPath", context.world_path.generic_string()},
                          {"summary", context.history->last_summary()}});
  }
  return make_response(ExitCode::Success, context.history->last_summary(),
                       changed, {},
                       {{"appliedCount", std::to_string(ops.size())}});
}

std::string normalize_asset_relative_path(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  while (!path.empty() && path.front() == '/')
    path.erase(path.begin());
  std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return path;
}

bool is_prefab_asset_path(const std::string &path) {
  return path.size() >= 12 &&
         path.compare(path.size() - 12, 12, ".prefab.json") == 0;
}

bool is_material_asset_path(const std::string &path) {
  return path.size() >= 15 &&
         path.compare(path.size() - 15, 15, ".material.json") == 0;
}

bool is_particle_asset_path(const std::string &path) {
  return path.size() >= 14 &&
         path.compare(path.size() - 14, 14, ".particle.json") == 0;
}

bool is_ui_theme_asset_path(const std::string &path) {
  return path.size() >= 13 &&
         path.compare(path.size() - 13, 13, "ui-theme.json") == 0;
}

std::string infer_asset_kind(const std::string &path) {
  if (is_prefab_asset_path(path))
    return "prefab";
  if (is_material_asset_path(path))
    return "material";
  if (is_particle_asset_path(path))
    return "particle";
  if (is_ui_theme_asset_path(path))
    return "ui_theme";
  return "text";
}

Result<void> write_text_asset_atomic(const std::filesystem::path &path,
                                     const std::string &text) {
  if (path.has_parent_path())
    std::filesystem::create_directories(path.parent_path());
  const auto temporary = path.string() + ".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output)
      return Result<void>::failure(session_error(
          "ASSET-WRITE", "Could not write asset: " + path.generic_string(),
          "Check path permissions."));
    output << text;
    if (!output)
      return Result<void>::failure(session_error(
          "ASSET-WRITE", "Could not flush asset: " + path.generic_string(),
          "Retry the write."));
  }
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  std::filesystem::rename(temporary, path);
  return Result<void>::success();
}

bool is_supported_graybox_primitive(const std::string &primitive) {
  return primitive == "cube" || primitive == "pyramid" ||
         primitive == "cylinder" || primitive == "sphere" ||
         primitive == "capsule";
}

std::string normalize_graybox_primitive(std::string primitive) {
  std::transform(primitive.begin(), primitive.end(), primitive.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return primitive;
}

Result<nlohmann::json>
build_graybox_prefab_json(const std::string &stamp_name,
                          const nlohmann::json &parts) {
  if (!parts.is_array() || parts.empty()) {
    return Result<nlohmann::json>::failure(session_error(
        "SCENE-COMPOSITION-PARTS",
        "stamp_compositions requires a non-empty parts array per stamp.",
        "Provide parts: [{primitive, transform?, color?, name?}, ...]."));
  }
  nlohmann::json entities = nlohmann::json::array();
  for (std::size_t index = 0; index < parts.size(); ++index) {
    const auto &part = parts[index];
    if (!part.is_object()) {
      return Result<nlohmann::json>::failure(session_error(
          "SCENE-COMPOSITION-PART",
          "Each composition part must be a JSON object.",
          "Use {primitive, transform?, color?, name?} entries."));
    }
    const auto primitive =
        normalize_graybox_primitive(part.value("primitive", std::string{}));
    if (!is_supported_graybox_primitive(primitive)) {
      return Result<nlohmann::json>::failure(session_error(
          "SCENE-COMPOSITION-PRIMITIVE",
          "Unsupported graybox primitive: " +
              part.value("primitive", std::string{}),
          "Use cube, pyramid, cylinder, sphere, or capsule."));
    }
    nlohmann::json entity;
    entity["name"] =
        part.value("name", stamp_name + "_part_" + std::to_string(index + 1));
    entity["parent"] = nullptr;
    // Prefer nested transform; also accept stamp shorthand x/y/z + scale /
    // scale:[...] so MCP agents can author parts without a full transform.
    nlohmann::json transform =
        part.contains("transform") && part["transform"].is_object()
            ? part["transform"]
            : nlohmann::json::object();
    if (!transform.contains("position")) {
      const float px = part.value("x", 0.0f);
      const float py = part.value("y", 0.0f);
      const float pz = part.value("z", 0.0f);
      transform["position"] = {px, py, pz};
    }
    if (!transform.contains("rotation"))
      transform["rotation"] = {0.0, 0.0, 0.0, 1.0};
    if (!transform.contains("scale")) {
      if (part.contains("scale") && part["scale"].is_array() &&
          part["scale"].size() >= 3) {
        transform["scale"] = {part["scale"][0].get<float>(),
                              part["scale"][1].get<float>(),
                              part["scale"][2].get<float>()};
      } else if (part.contains("scale") && part["scale"].is_number()) {
        const float s = part["scale"].get<float>();
        transform["scale"] = {s, s, s};
      } else {
        transform["scale"] = {1.0, 1.0, 1.0};
      }
    }
    entity["transform"] = std::move(transform);
    nlohmann::json mesh;
    mesh["primitive"] = primitive;
    if (part.contains("color") && part["color"].is_array() &&
        part["color"].size() >= 3) {
      mesh["color"] = {part["color"][0].get<float>(),
                       part["color"][1].get<float>(),
                       part["color"][2].get<float>()};
    } else {
      mesh["color"] = {0.45f, 0.43f, 0.40f};
    }
    entity["mesh"] = std::move(mesh);
    entities.push_back(std::move(entity));
  }
  nlohmann::json prefab;
  prefab["schemaVersion"] = 2;
  prefab["entities"] = std::move(entities);
  prefab["collision"] = nlohmann::json::array();
  return Result<nlohmann::json>::success(std::move(prefab));
}

Result<std::string>
ensure_graybox_prefab_on_disk(EditorSessionContext &context,
                              const std::string &stamp_name,
                              const nlohmann::json &parts) {
  auto built = build_graybox_prefab_json(stamp_name, parts);
  if (!built)
    return Result<std::string>::failure(built.error());
  const auto slug = slugify_id(stamp_name.empty() ? "graybox" : stamp_name);
  const auto relative =
      "assets/prefabs/Graybox/" +
      (slug.empty() ? std::string{"graybox"} : slug) + ".prefab.json";
  const auto absolute = context.project_root / relative;
  const auto written =
      write_text_asset_atomic(absolute, built.value().dump(2));
  if (!written)
    return Result<std::string>::failure(written.error());
  const auto loaded = PrefabAsset::load(absolute);
  if (!loaded)
    return Result<std::string>::failure(loaded.error());
  if (context.prefab_catalog)
    (*context.prefab_catalog)[relative] = loaded.value();
  if (context.prefab_meshes_dirty)
    *context.prefab_meshes_dirty = true;
  if (context.static_render_cache_dirty)
    *context.static_render_cache_dirty = true;
  if (context.scene && context.prefab_catalog)
    (void)context.scene->repair_prefab_paths(*context.prefab_catalog);
  return Result<std::string>::success(relative);
}

Result<std::size_t> refresh_prefab_catalog(EditorSessionContext &context) {
  if (!context.assets || !context.prefab_catalog ||
      context.project_root.empty()) {
    return Result<std::size_t>::failure(
        session_error("EDITOR-ASSETS-MISSING",
                      "Asset registry or prefab catalog is unavailable.",
                      "Launch the editor first."));
  }
  if (const auto scanned = context.assets->scan(context.project_root); !scanned)
    return Result<std::size_t>::failure(scanned.error());
  const auto errors = context.assets->validate();
  if (!errors.empty())
    return Result<std::size_t>::failure(errors.front());
  context.prefab_catalog->clear();
  std::size_t prefab_count = 0;
  for (const auto &entry : context.assets->records()) {
    const auto &relative = entry.second.path;
    if (!is_prefab_asset_path(relative))
      continue;
    const auto loaded = PrefabAsset::load(context.project_root / relative);
    if (!loaded)
      return Result<std::size_t>::failure(loaded.error());
    (*context.prefab_catalog)[relative] = loaded.value();
    ++prefab_count;
  }
  if (context.scene)
    (void)context.scene->repair_prefab_paths(*context.prefab_catalog);
  if (context.prefab_meshes_dirty)
    *context.prefab_meshes_dirty = true;
  return Result<std::size_t>::success(prefab_count);
}

Result<std::string> asset_payload_text(const nlohmann::json &params) {
  if (params.contains("json"))
    return Result<std::string>::success(params["json"].dump(2));
  if (params.contains("source"))
    return Result<std::string>::success(params["source"].get<std::string>());
  return Result<std::string>::failure(session_error(
      "ASSET-PAYLOAD-REQUIRED", "Asset apply requires json or source.",
      "Provide asset JSON text."));
}

EditorBridgeResponse apply_asset_write(EditorSessionContext &context,
                                       const nlohmann::json &params) {
  const auto relative =
      normalize_asset_relative_path(params.value("path", std::string{}));
  if (relative.empty()) {
    return make_response(
        ExitCode::InvalidArguments, "path is required", {},
        {session_error("ASSET-PATH-REQUIRED", "Asset path is required.",
                       "Provide assets/... path.")});
  }
  const auto payload = asset_payload_text(params);
  if (!payload)
    return make_response(ExitCode::InvalidArguments, payload.error().message,
                         {}, {payload.error()});
  const auto absolute = context.project_root / relative;
  const auto kind = params.value("kind", infer_asset_kind(relative));
  if (kind == "prefab") {
    const auto written = write_text_asset_atomic(absolute, payload.value());
    if (!written)
      return make_response(ExitCode::ValidationFailed, written.error().message,
                           {}, {written.error()});
    const auto loaded = PrefabAsset::load(absolute);
    if (!loaded)
      return make_response(ExitCode::ValidationFailed, loaded.error().message,
                           {}, {loaded.error()});
    if (context.prefab_catalog)
      (*context.prefab_catalog)[relative] = loaded.value();
    std::size_t propagated = 0;
    if (context.scene) {
      propagated =
          context.scene->propagate_prefab_components(relative, loaded.value());
      if (propagated > 0 && context.scene_dirty)
        *context.scene_dirty = true;
      if (propagated > 0 && context.static_render_cache_dirty)
        *context.static_render_cache_dirty = true;
    }
    std::size_t prefab_count = 0;
    if (params.value("refreshCatalog", true)) {
      const auto refreshed = refresh_prefab_catalog(context);
      if (!refreshed)
        return make_response(ExitCode::ValidationFailed,
                             refreshed.error().message, {},
                             {refreshed.error()});
      prefab_count = refreshed.value();
      if (context.prefab_catalog)
        (*context.prefab_catalog)[relative] = loaded.value();
    } else {
      prefab_count = 1;
      if (context.prefab_meshes_dirty)
        *context.prefab_meshes_dirty = true;
      if (context.scene && context.prefab_catalog)
        (void)context.scene->repair_prefab_paths(*context.prefab_catalog);
    }
    return make_response(ExitCode::Success,
                         "Asset written and catalog refreshed", {}, {},
                         {{"assetPath", relative},
                          {"assetKind", kind},
                          {"prefabCount", std::to_string(prefab_count)},
                          {"propagatedInstances", std::to_string(propagated)}});
  } else if (kind == "material") {
    const auto parsed = MaterialAsset::from_json(payload.value());
    if (!parsed)
      return make_response(ExitCode::ValidationFailed, parsed.error().message,
                           {}, {parsed.error()});
    if (const auto valid = parsed.value().validate(); !valid)
      return make_response(ExitCode::ValidationFailed, valid.error().message,
                           {}, {valid.error()});
    if (const auto saved = parsed.value().save_atomic(absolute); !saved)
      return make_response(ExitCode::ValidationFailed, saved.error().message,
                           {}, {saved.error()});
    if (!context.project_root.empty()) {
      if (const auto maps =
              parsed.value().validate_texture_maps(context.project_root);
          !maps)
        return make_response(ExitCode::ValidationFailed, maps.error().message,
                             {}, {maps.error()});
    }
    if (context.material_cache)
      (*context.material_cache)[relative] = parsed.value();
  } else if (kind == "particle") {
    const auto parsed = ParticleEmitterAsset::parse(payload.value(), relative);
    if (!parsed)
      return make_response(ExitCode::ValidationFailed, parsed.error().message,
                           {}, {parsed.error()});
    if (!context.project_root.empty()) {
      if (const auto texture =
              parsed.value().validate_texture(context.project_root);
          !texture)
        return make_response(ExitCode::ValidationFailed,
                             texture.error().message, {}, {texture.error()});
    }
    if (const auto saved = parsed.value().save_atomic(absolute); !saved)
      return make_response(ExitCode::ValidationFailed, saved.error().message,
                           {}, {saved.error()});
    if (context.particle_system)
      context.particle_system->register_asset(relative, parsed.value());
  } else if (kind == "ui_theme") {
    const auto parsed = UiThemeAsset::parse(payload.value(), relative);
    if (!parsed)
      return make_response(ExitCode::ValidationFailed, parsed.error().message,
                           {}, {parsed.error()});
    if (const auto saved = parsed.value().save_atomic(absolute); !saved)
      return make_response(ExitCode::ValidationFailed, saved.error().message,
                           {}, {saved.error()});
    if (context.ui_canvas_stack) {
      if (const auto loaded = context.ui_canvas_stack->load_theme(absolute);
          !loaded)
        return make_response(ExitCode::ValidationFailed,
                             loaded.error().message, {}, {loaded.error()});
    }
  } else {
    const auto written = write_text_asset_atomic(absolute, payload.value());
    if (!written)
      return make_response(ExitCode::ValidationFailed, written.error().message,
                           {}, {written.error()});
  }
  const auto extension = std::filesystem::path(relative).extension().string();
  if (extension == ".gltf" || extension == ".glb") {
    if (context.pending_mesh_reloads)
      context.pending_mesh_reloads->insert(relative);
    if (context.prefab_meshes_dirty)
      *context.prefab_meshes_dirty = true;
  }
  std::size_t prefab_count = 0;
  if (params.value("refreshCatalog", true)) {
    const auto refreshed = refresh_prefab_catalog(context);
    if (!refreshed)
      return make_response(ExitCode::ValidationFailed,
                           refreshed.error().message, {}, {refreshed.error()});
    prefab_count = refreshed.value();
  } else if (kind == "prefab" && context.prefab_catalog) {
    const auto loaded = PrefabAsset::load(absolute);
    if (!loaded)
      return make_response(ExitCode::ValidationFailed, loaded.error().message,
                           {}, {loaded.error()});
    (*context.prefab_catalog)[relative] = loaded.value();
    if (context.scene)
      (void)context.scene->repair_prefab_paths(*context.prefab_catalog);
    if (context.prefab_meshes_dirty)
      *context.prefab_meshes_dirty = true;
    prefab_count = 1;
  }
  return make_response(ExitCode::Success, "Asset written and catalog refreshed",
                       {}, {},
                       {{"assetPath", relative},
                        {"assetKind", kind},
                        {"prefabCount", std::to_string(prefab_count)}});
}

} // namespace

ScenePlanResult classify_scene_plan(const std::string &change_description,
                                    const std::string &target_path) {
  ScenePlanResult result;
  const auto lower_path = target_path;
  const auto lower_desc = change_description;
  auto contains = [](const std::string &haystack, const std::string &needle) {
    return std::search(haystack.begin(), haystack.end(), needle.begin(),
                       needle.end(), [](char a, char b) {
                         return static_cast<char>(std::tolower(
                                    static_cast<unsigned char>(a))) ==
                                static_cast<char>(std::tolower(
                                    static_cast<unsigned char>(b)));
                       }) != haystack.end();
  };
  if (contains(lower_path, ".lua") || contains(lower_desc, "lua") ||
      contains(lower_desc, "script")) {
    result.target_kind = "lua_script";
    result.requires_compile = "false";
    result.requires_reload = "lua_hot_reload";
    result.recommendation = "Edit script assets and bindings; reload through "
                            "the live editor bridge.";
  } else if (contains(lower_path, ".uicanvas.json") ||
             contains(lower_path, ".canvas.json") ||
             contains(lower_desc, "ui canvas") ||
             contains(lower_desc, "uicanvas")) {
    result.target_kind = "ui_canvas";
    result.requires_compile = "false";
    result.requires_reload = "hud_hot_reload";
    result.recommendation =
        "Edit UI canvas JSON through engine_hud_apply (*.uicanvas.json); live "
        "reload is allowed during play test.";
  } else if (contains(lower_path, ".hud.json") ||
             contains(lower_path, ".hud.") || contains(lower_desc, "hud") ||
             (contains(lower_desc, "health bar") ||
              contains(lower_desc, "healthbar"))) {
    result.target_kind = "hud_asset";
    result.requires_compile = "false";
    result.requires_reload = "hud_hot_reload";
    result.recommendation = "Prefer *.uicanvas.json (ui_canvas). Legacy "
                            "*.hud.json still works via engine_hud_apply shim.";
  } else if (contains(lower_path, "ui-theme.json") ||
             contains(lower_desc, "ui theme") ||
             contains(lower_desc, "uitheme")) {
    result.target_kind = "ui_theme";
    result.requires_compile = "false";
    result.requires_reload = "ui_theme_hot_reload";
    result.recommendation =
        "Edit assets/ui/ui-theme.json via engine_asset_apply (kind: ui_theme); "
        "live canvases resolve tokens on the next draw.";
  } else if (contains(lower_path, "prefab") || contains(lower_desc, "prefab") ||
             contains(lower_path, ".prefab.") ||
             (contains(lower_desc, "component") &&
              contains(lower_desc, "prefab"))) {
    result.target_kind = "prefab_asset";
    result.requires_compile = "false";
    result.requires_reload = "prefab_catalog_refresh";
    result.recommendation = "Edit prefab JSON/components and refresh the "
                            "editor catalog; non-overridden instances inherit.";
  } else if (contains(lower_path, ".particle.json") ||
             contains(lower_path, "assets/vfx") ||
             contains(lower_desc, "particle") ||
             contains(lower_desc, "vfx recipe") ||
             contains(lower_desc, "emitter")) {
    result.target_kind = "particle_asset";
    result.requires_compile = "false";
    result.requires_reload = "particle_hot_reload";
    result.recommendation =
        "Edit *.particle.json via engine_asset_apply (kind: particle); live "
        "editor registers the emitter immediately.";
  } else if (contains(lower_path, "terrain") ||
             contains(lower_path, "terrain-edits") ||
             contains(lower_path, "terrain-paint") ||
             contains(lower_path, "foliage") ||
             contains(lower_desc, "terrain") ||
             contains(lower_desc, "sculpt") ||
             contains(lower_desc, "flatten") ||
             contains(lower_desc, "foliage") ||
             (contains(lower_desc, "paint") &&
              (contains(lower_desc, "material") ||
               contains(lower_desc, "grass") ||
               contains(lower_desc, "ground cover")))) {
    result.target_kind = "terrain_data";
    result.requires_compile = "false";
    result.requires_reload = "live_terrain_command";
    result.recommendation =
        "Apply height/paint/foliage strokes through engine_terrain_apply while "
        "the editor MCP connection is enabled.";
  } else if (contains(lower_path, ".worldforge.json") ||
             contains(lower_path, "world-forge") ||
             contains(lower_desc, "world forge") ||
             contains(lower_desc, "relationship graph") ||
             contains(lower_desc, "story geography") ||
             contains(lower_desc, "factions.worldforge") ||
             contains(lower_desc, "map.worldforge")) {
    result.target_kind = "world_forge";
    result.requires_compile = "false";
    result.requires_reload = "none";
    result.recommendation =
        "Edit World Forge JSON through engine_world_forge_apply "
        "(factions/relationships/map). Do not use scene/terrain tools.";
  } else if (contains(lower_path, ".world.") || contains(lower_desc, "scene") ||
             contains(lower_desc, "placement") ||
             contains(lower_desc, "entity") ||
             contains(lower_desc, "component") ||
             contains(lower_desc, "collider") ||
             contains(lower_desc, "add component")) {
    result.target_kind = "scene_data";
    result.requires_compile = "false";
    result.requires_reload = "live_scene_command";
    result.recommendation = "Apply scene/component changes through "
                            "CommandHistory while the editor is running.";
  } else if (contains(lower_path, "src/") || contains(lower_path, "include/") ||
             contains(lower_desc, "c++") || contains(lower_desc, "engine")) {
    result.target_kind = "engine_code";
    result.requires_compile = "true";
    result.requires_reload = "rebuild_engine";
    result.recommendation =
        "Rebuild the engine target; restart the editor if native APIs changed.";
  } else {
    result.target_kind = "unknown";
    result.requires_compile = "maybe";
    result.requires_reload = "validate_first";
    result.recommendation = "Run project validation and choose scene, prefab, "
                            "script, or engine workflow.";
  }
  result.summary = "Classified as " + result.target_kind;
  return result;
}

CommandResponse validate_project_at(const std::filesystem::path &project_root) {
  CommandRequest request;
  request.name = "validate";
  request.project = project_root;
  request.json = true;
  request.correlation_id = make_correlation_id();
  return execute_command(request);
}

EditorBridgeResponse apply_terrain_operation(EditorSessionContext &context,
                                             const nlohmann::json &params) {
  const auto action = params.value("action", std::string{});
  if (action == "sample" || action == "sample_terrain") {
    if (!params.contains("x") || !params.contains("z")) {
      return make_response(
          ExitCode::InvalidArguments, "sample requires x and z", {},
          {session_error("TERRAIN-SAMPLE-ARGS",
                         "sample requires x and z world coordinates.",
                         "Provide numeric x and z.")});
    }
    const float x = params["x"].get<float>();
    const float z = params["z"].get<float>();
    if (!std::isfinite(x) || !std::isfinite(z)) {
      return make_response(
          ExitCode::InvalidArguments, "sample coordinates must be finite", {},
          {session_error("TERRAIN-SAMPLE-FINITE", "sample x/z must be finite.",
                         "Use finite world coordinates.")});
    }
    const float height = sample_terrain_height(x, z);
    const float offset = params.value("groundOffset", 0.0f);
    return make_response(ExitCode::Success, "Terrain height sampled", {}, {},
                         {{"x", std::to_string(x)},
                          {"z", std::to_string(z)},
                          {"height", std::to_string(height)},
                          {"groundOffset", std::to_string(offset)},
                          {"placedY", std::to_string(height + offset)}});
  }

  if (context.test_session_active) {
    return make_response(
        ExitCode::Unavailable, "Terrain edits blocked during play test", {},
        {session_error(
            "TERRAIN-PLAY-BLOCKED",
            "Terrain apply is blocked while a play-test session is active.",
            "End the test session, then retry.")});
  }

  auto require_edits = [&]() -> std::optional<EditorBridgeResponse> {
    if (!context.terrain_edits || !context.terrain_history) {
      return make_response(
          ExitCode::Unavailable, "Terrain edit stores unavailable", {},
          {session_error(
              "TERRAIN-STORE-MISSING",
              "Terrain height stores are not bound to the editor session.",
              "Enable MCP connection in a running editor session.")});
    }
    return std::nullopt;
  };
  auto require_paint = [&]() -> std::optional<EditorBridgeResponse> {
    if (!context.terrain_paint || !context.terrain_paint_history) {
      return make_response(
          ExitCode::Unavailable, "Terrain paint stores unavailable", {},
          {session_error(
              "TERRAIN-PAINT-MISSING",
              "Terrain paint stores are not bound to the editor session.",
              "Enable MCP connection in a running editor session.")});
    }
    return std::nullopt;
  };
  auto require_foliage = [&]() -> std::optional<EditorBridgeResponse> {
    if (!context.foliage_density || !context.foliage_density_history) {
      return make_response(
          ExitCode::Unavailable, "Foliage density stores unavailable", {},
          {session_error(
              "FOLIAGE-STORE-MISSING",
              "Foliage density stores are not bound to the editor session.",
              "Enable MCP connection in a running editor session.")});
    }
    if (!context.foliage_layers || context.foliage_layers->layers.empty()) {
      return make_response(
          ExitCode::Unavailable, "Foliage layer palette unavailable", {},
          {session_error("FOLIAGE-PALETTE-MISSING",
                         "Foliage layer palette is not loaded.",
                         "Ensure assets/foliage/ground-cover.layers.json loads "
                         "in the editor.")});
    }
    return std::nullopt;
  };

  auto reload_now = [&](bool height_changed) {
    if (height_changed) {
      if (context.terrain_edits_dirty)
        *context.terrain_edits_dirty = true;
    } else {
      if (context.terrain_paint_dirty)
        *context.terrain_paint_dirty = true;
    }
    if (context.reload_terrain)
      context.reload_terrain(height_changed);
    if (height_changed && context.reload_water)
      context.reload_water();
  };
  auto reload_foliage_now = [&]() {
    if (context.foliage_density_dirty)
      *context.foliage_density_dirty = true;
    if (context.reload_foliage)
      context.reload_foliage();
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

  auto snapshot_height = [&](const std::set<CellCoord> &cells) {
    std::map<CellCoord, std::vector<float>> before;
    for (const auto &cell : cells)
      before[cell] = context.terrain_edits->cell_deltas_or_empty(cell);
    return before;
  };
  auto snapshot_paint = [&](const std::set<CellCoord> &cells) {
    std::map<CellCoord, std::vector<std::uint16_t>> before;
    for (const auto &cell : cells)
      before[cell] = context.terrain_paint->cell_indices_or_empty(cell);
    return before;
  };
  auto snapshot_foliage = [&](const std::set<CellCoord> &cells) {
    std::map<CellCoord, FoliageCellSnapshot> before;
    for (const auto &cell : cells)
      before[cell] = context.foliage_density->cell_snapshot_or_empty(cell);
    return before;
  };
  auto restore_height =
      [&](const std::map<CellCoord, std::vector<float>> &before) {
        for (const auto &entry : before)
          context.terrain_edits->set_cell_deltas(entry.first, entry.second);
      };
  auto restore_paint =
      [&](const std::map<CellCoord, std::vector<std::uint16_t>> &before) {
        for (const auto &entry : before) {
          if (entry.second.empty())
            context.terrain_paint->remove_cell(entry.first);
          else
            context.terrain_paint->set_cell_indices(entry.first, entry.second);
        }
      };
  auto restore_foliage =
      [&](const std::map<CellCoord, FoliageCellSnapshot> &before) {
        for (const auto &entry : before) {
          if (entry.second.density.empty())
            context.foliage_density->remove_cell(entry.first);
          else
            context.foliage_density->set_cell(entry.first, entry.second);
        }
      };

  auto commit_height = [&](std::map<CellCoord, std::vector<float>> before,
                           bool do_reload) -> EditorBridgeResponse {
    std::map<CellCoord, std::vector<float>> after;
    for (const auto &entry : before)
      after[entry.first] =
          context.terrain_edits->cell_deltas_or_empty(entry.first);
    bool changed = false;
    for (const auto &entry : after) {
      const auto found = before.find(entry.first);
      if (found == before.end() || found->second != entry.second) {
        changed = true;
        break;
      }
    }
    if (!changed)
      return make_response(ExitCode::Success,
                           "Terrain brush touched no samples", {}, {},
                           {{"touchedCells", "0"}});
    const auto result = context.terrain_history->execute(
        *context.terrain_edits, std::make_unique<TerrainBrushStrokeCommand>(
                                    std::move(before), std::move(after)));
    if (!result)
      return make_response(ExitCode::ValidationFailed, result.error().message,
                           {}, {result.error()});
    if (context.terrain_edits_dirty)
      *context.terrain_edits_dirty = true;
    if (do_reload && context.reload_terrain)
      context.reload_terrain(true);
    if (do_reload && context.reload_water)
      context.reload_water();
    return make_response(ExitCode::Success, "Terrain height stroke applied", {},
                         {}, {});
  };

  auto commit_paint =
      [&](std::map<CellCoord, std::vector<std::uint16_t>> before,
          bool do_reload) -> EditorBridgeResponse {
    std::map<CellCoord, std::vector<std::uint16_t>> after;
    for (const auto &entry : before)
      after[entry.first] =
          context.terrain_paint->cell_indices_or_empty(entry.first);
    bool changed = false;
    for (const auto &entry : after) {
      const auto found = before.find(entry.first);
      if (found == before.end() || found->second != entry.second) {
        changed = true;
        break;
      }
    }
    if (!changed)
      return make_response(ExitCode::Success,
                           "Terrain paint touched no samples", {}, {},
                           {{"touchedCells", "0"}});
    const auto result = context.terrain_paint_history->execute(
        *context.terrain_paint,
        std::make_unique<TerrainPaintBrushStrokeCommand>(std::move(before),
                                                         std::move(after)));
    if (!result)
      return make_response(ExitCode::ValidationFailed, result.error().message,
                           {}, {result.error()});
    if (context.terrain_paint_dirty)
      *context.terrain_paint_dirty = true;
    if (do_reload && context.reload_terrain)
      context.reload_terrain(false);
    return make_response(ExitCode::Success, "Terrain paint stroke applied", {},
                         {}, {});
  };

  auto commit_foliage = [&](std::map<CellCoord, FoliageCellSnapshot> before,
                            bool do_reload) -> EditorBridgeResponse {
    std::map<CellCoord, FoliageCellSnapshot> after;
    for (const auto &entry : before)
      after[entry.first] =
          context.foliage_density->cell_snapshot_or_empty(entry.first);
    bool changed = false;
    for (const auto &entry : after) {
      const auto found = before.find(entry.first);
      if (found == before.end() ||
          found->second.density != entry.second.density ||
          found->second.layer != entry.second.layer) {
        changed = true;
        break;
      }
    }
    if (!changed)
      return make_response(ExitCode::Success,
                           "Foliage brush touched no samples", {}, {},
                           {{"touchedCells", "0"}});
    const auto result = context.foliage_density_history->execute(
        *context.foliage_density,
        std::make_unique<FoliageDensityBrushStrokeCommand>(std::move(before),
                                                           std::move(after)));
    if (!result)
      return make_response(ExitCode::ValidationFailed, result.error().message,
                           {}, {result.error()});
    if (context.foliage_density_dirty)
      *context.foliage_density_dirty = true;
    if (do_reload && context.reload_foliage)
      context.reload_foliage();
    return make_response(ExitCode::Success, "Foliage density stroke applied",
                         {}, {}, {});
  };

  auto resolve_foliage_layer =
      [&](const nlohmann::json &op) -> Result<std::uint8_t> {
    if (!context.foliage_layers || context.foliage_layers->layers.empty()) {
      return Result<std::uint8_t>::failure(session_error(
          "FOLIAGE-PALETTE-MISSING", "Foliage layer palette is not loaded.",
          "Load ground-cover.layers.json in the editor."));
    }
    if (!op.contains("layer"))
      return Result<std::uint8_t>::success(0);
    const auto &layer_value = op.at("layer");
    if (layer_value.is_number_integer() || layer_value.is_number_unsigned()) {
      const int index = layer_value.get<int>();
      if (index < 0 || static_cast<std::size_t>(index) >=
                           context.foliage_layers->layers.size()) {
        return Result<std::uint8_t>::failure(session_error(
            "FOLIAGE-LAYER-RANGE", "Foliage layer index is out of range.",
            "Use a valid palette index or layer id."));
      }
      return Result<std::uint8_t>::success(static_cast<std::uint8_t>(index));
    }
    if (layer_value.is_string()) {
      const auto id = layer_value.get<std::string>();
      if (!id.empty() && std::all_of(id.begin(), id.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
          })) {
        const int index = std::stoi(id);
        if (index < 0 || static_cast<std::size_t>(index) >=
                             context.foliage_layers->layers.size()) {
          return Result<std::uint8_t>::failure(session_error(
              "FOLIAGE-LAYER-RANGE", "Foliage layer index is out of range.",
              "Use a valid palette index or layer id."));
        }
        return Result<std::uint8_t>::success(static_cast<std::uint8_t>(index));
      }
      for (std::size_t index = 0; index < context.foliage_layers->layers.size();
           ++index) {
        if (context.foliage_layers->layers[index].id == id)
          return Result<std::uint8_t>::success(
              static_cast<std::uint8_t>(index));
      }
      return Result<std::uint8_t>::failure(session_error(
          "FOLIAGE-LAYER-UNKNOWN", "Unknown foliage layer id: " + id,
          "Use grass/flower/bush/bush_wide/bush_tall or a palette index."));
    }
    return Result<std::uint8_t>::failure(session_error(
        "FOLIAGE-LAYER-TYPE", "layer must be a palette index or id string.",
        "Example: \"grass\" or 0."));
  };

  auto apply_height_op =
      [&](const nlohmann::json &op) -> Result<std::set<CellCoord>> {
    const auto op_action = op.value("action", std::string{});
    if (!op.contains("x") || !op.contains("z")) {
      return Result<std::set<CellCoord>>::failure(
          session_error("TERRAIN-BRUSH-ARGS", "Terrain brush requires x and z.",
                        "Provide world x/z."));
    }
    const float x = op["x"].get<float>();
    const float z = op["z"].get<float>();
    const float radius = op.value("radius", 4.0f);
    const float strength = op.value("strength", 0.12f);
    if (!std::isfinite(x) || !std::isfinite(z)) {
      return Result<std::set<CellCoord>>::failure(
          session_error("TERRAIN-BRUSH-FINITE", "Brush x/z must be finite.",
                        "Use finite world coordinates."));
    }
    if (op_action == "flatten") {
      const float target = op.contains("targetHeight")
                               ? op["targetHeight"].get<float>()
                               : sample_terrain_height(x, z);
      return context.terrain_edits->apply_flatten_brush(x, z, radius, strength,
                                                        target);
    }
    if (op_action == "plateau") {
      if (!op.contains("targetHeight") && !op.contains("height")) {
        return Result<std::set<CellCoord>>::failure(session_error(
            "TERRAIN-PLATEAU-ARGS", "plateau requires targetHeight.",
            "Provide targetHeight (or height) world Y."));
      }
      const float target = op.contains("targetHeight")
                               ? op["targetHeight"].get<float>()
                               : op["height"].get<float>();
      const float skirt =
          op.value("skirtWidth", std::max(4.0f, radius * 0.45f));
      const float plateau_strength = op.value("strength", 1.0f);
      return context.terrain_edits->apply_plateau_brush(
          x, z, radius, skirt, plateau_strength, target);
    }
    if (op_action == "set_height") {
      if (!op.contains("targetHeight")) {
        return Result<std::set<CellCoord>>::failure(session_error(
            "TERRAIN-SET-HEIGHT-ARGS", "set_height requires targetHeight.",
            "Provide targetHeight world Y."));
      }
      const float target = op["targetHeight"].get<float>();
      const float set_strength = op.value("strength", 1.0f);
      return context.terrain_edits->apply_set_height_brush(
          x, z, radius, set_strength, target);
    }
    if (op_action == "smooth") {
      const float smooth_strength = op.value("strength", 0.55f);
      const float kernel =
          op.contains("kernelRadius")
              ? op["kernelRadius"].get<float>()
              : std::max(0.5f, radius * 0.35f);
      return context.terrain_edits->apply_smooth_brush(x, z, radius,
                                                       smooth_strength, kernel);
    }
    if (op_action == "terrace") {
      if (!op.contains("stepSize") && !op.contains("step")) {
        return Result<std::set<CellCoord>>::failure(session_error(
            "TERRAIN-TERRACE-ARGS", "terrace requires stepSize.",
            "Provide stepSize in meters (e.g. 2.0)."));
      }
      const float step_size = op.contains("stepSize")
                                  ? op["stepSize"].get<float>()
                                  : op["step"].get<float>();
      const float terrace_strength = op.value("strength", 0.85f);
      return context.terrain_edits->apply_terrace_brush(
          x, z, radius, terrace_strength, step_size);
    }
    return context.terrain_edits->apply_brush(x, z, radius, strength,
                                              op_action == "lower");
  };

  auto densify_polyline = [](const std::vector<std::array<float, 2>> &controls,
                             float step) {
    std::vector<std::array<float, 2>> pts;
    if (controls.empty())
      return pts;
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

  auto parse_points = [&](const nlohmann::json &root)
      -> Result<std::vector<std::array<float, 2>>> {
    if (!root.contains("points") || !root["points"].is_array() ||
        root["points"].empty()) {
      return Result<std::vector<std::array<float, 2>>>::failure(
          session_error("TERRAIN-POINTS-ARGS", "points array is required.",
                        "Provide points:[{x,z},...]"));
    }
    std::vector<std::array<float, 2>> controls;
    for (const auto &point : root["points"]) {
      if (!point.is_object() || !point.contains("x") || !point.contains("z")) {
        return Result<std::vector<std::array<float, 2>>>::failure(session_error(
            "TERRAIN-POINT-INVALID", "Each point requires x and z.",
            "Use {x,z} objects."));
      }
      const float px = point["x"].get<float>();
      const float pz = point["z"].get<float>();
      if (!std::isfinite(px) || !std::isfinite(pz)) {
        return Result<std::vector<std::array<float, 2>>>::failure(session_error(
            "TERRAIN-POINT-FINITE", "Point coordinates must be finite.",
            "Use finite world x/z."));
      }
      controls.push_back({px, pz});
    }
    return Result<std::vector<std::array<float, 2>>>::success(
        std::move(controls));
  };

  auto resolve_sea_level = [&]() -> float {
    if (params.contains("seaLevel") && params["seaLevel"].is_number())
      return params["seaLevel"].get<float>();
    if (context.water_store)
      return context.water_store->sea_level();
    if (const WaterStore *active = active_water_store())
      return active->sea_level();
    return -0.35f;
  };

  auto apply_paint_op =
      [&](const nlohmann::json &op) -> Result<std::set<CellCoord>> {
    if (!op.contains("x") || !op.contains("z")) {
      return Result<std::set<CellCoord>>::failure(
          session_error("TERRAIN-PAINT-ARGS", "paint requires x and z.",
                        "Provide world x/z."));
    }
    const auto material = op.value("material", std::string{});
    if (material.empty()) {
      return Result<std::set<CellCoord>>::failure(session_error(
          "TERRAIN-PAINT-MATERIAL", "paint requires a material asset path.",
          "Provide assets/materials/....material.json"));
    }
    const float x = op["x"].get<float>();
    const float z = op["z"].get<float>();
    const float radius = op.value("radius", 4.0f);
    const auto palette_index =
        context.terrain_paint->ensure_material_index(material);
    return context.terrain_paint->apply_material_brush(x, z, radius,
                                                       palette_index);
  };

  auto apply_foliage_op =
      [&](const nlohmann::json &op) -> Result<std::set<CellCoord>> {
    if (!op.contains("x") || !op.contains("z")) {
      return Result<std::set<CellCoord>>::failure(
          session_error("FOLIAGE-BRUSH-ARGS", "paint_foliage requires x and z.",
                        "Provide world x/z."));
    }
    const float x = op["x"].get<float>();
    const float z = op["z"].get<float>();
    const float radius = op.value("radius", 4.0f);
    const float strength = op.value("strength", 0.28f);
    const bool erase = op.value("erase", false);
    if (!std::isfinite(x) || !std::isfinite(z) || !std::isfinite(radius) ||
        !std::isfinite(strength)) {
      return Result<std::set<CellCoord>>::failure(session_error(
          "FOLIAGE-BRUSH-FINITE", "Foliage brush values must be finite.",
          "Use finite numbers."));
    }
    const auto op_action = op.value("action", std::string{});
    if (op_action == "paint_foliage_mixed" ||
        op.value("mode", std::string{}) == "mixed") {
      const auto mix = default_meadow_mix_weights(*context.foliage_layers);
      return context.foliage_density->apply_foliage_mixed_brush(
          x, z, radius, strength, mix, erase);
    }
    const auto layer = resolve_foliage_layer(op);
    if (!layer)
      return Result<std::set<CellCoord>>::failure(layer.error());
    return context.foliage_density->apply_foliage_brush(x, z, radius, strength,
                                                        layer.value(), erase);
  };

  auto apply_channel_op =
      [&](const nlohmann::json &op) -> Result<std::set<CellCoord>> {
    const auto op_action = op.value("action", std::string{});
    const auto controls = parse_points(op);
    if (!controls)
      return Result<std::set<CellCoord>>::failure(controls.error());
    const float step = op.value("step", 2.5f);
    const auto pts = densify_polyline(controls.value(), step);
    const float sea = [&]() {
      if (op.contains("seaLevel") && op["seaLevel"].is_number())
        return op["seaLevel"].get<float>();
      return resolve_sea_level();
    }();
    const float half_width =
        op.value("halfWidth", op_action == "carve_channel" ? 3.5f : 0.0f);
    const float bed_depth = op.value("bedDepth", 1.4f);
    const float bed_height = op.contains("bedHeight")
                                 ? op["bedHeight"].get<float>()
                                 : (sea - std::max(0.2f, bed_depth));
    const float bank_width = op.value("bankWidth", 3.5f);
    const float min_bank_offset = half_width + bank_width;
    const float bank_offset =
        std::max(op.value("bankOffset", min_bank_offset), min_bank_offset);
    const float bank_clearance = op.value("bankClearance", 1.5f);
    const float bank_height = op.contains("bankHeight")
                                  ? op["bankHeight"].get<float>()
                                  : (sea + bank_clearance);
    const float strength = op.value("strength", 1.0f);
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
      if (op_action == "carve_channel" || op_action == "raise_banks") {
        for (float side : {-1.0f, 1.0f}) {
          const float bx = x + px * side * bank_offset;
          const float bz = z + pz * side * bank_offset;
          const auto bank = context.terrain_edits->apply_set_height_brush(
              bx, bz, bank_width, strength, bank_height);
          if (!bank)
            return Result<std::set<CellCoord>>::failure(bank.error());
          touched_all.insert(bank.value().begin(), bank.value().end());
        }
      }
      if (op_action == "carve_channel") {
        const auto bed = context.terrain_edits->apply_set_height_brush(
            x, z, half_width, strength, bed_height);
        if (!bed)
          return Result<std::set<CellCoord>>::failure(bed.error());
        touched_all.insert(bed.value().begin(), bed.value().end());
      }
    }
    return Result<std::set<CellCoord>>::success(std::move(touched_all));
  };

  auto densify_polyline_heights =
      [](const std::vector<std::array<float, 3>> &controls, float step) {
        std::vector<std::array<float, 3>> pts;
        if (controls.empty())
          return pts;
        if (controls.size() == 1) {
          pts.push_back(controls.front());
          return pts;
        }
        const float use_step = std::max(0.5f, step);
        for (std::size_t i = 0; i + 1 < controls.size(); ++i) {
          const float x0 = controls[i][0];
          const float z0 = controls[i][1];
          const float h0 = controls[i][2];
          const float x1 = controls[i + 1][0];
          const float z1 = controls[i + 1][1];
          const float h1 = controls[i + 1][2];
          const float seg = std::hypot(x1 - x0, z1 - z0);
          const int n =
              std::max(1, static_cast<int>(std::ceil(seg / use_step)));
          for (int k = 0; k < n; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(n);
            pts.push_back({x0 + (x1 - x0) * t, z0 + (z1 - z0) * t,
                           h0 + (h1 - h0) * t});
          }
        }
        pts.push_back(controls.back());
        return pts;
      };

  auto resolve_height_along_controls =
      [&](const nlohmann::json &op)
      -> Result<std::vector<std::array<float, 3>>> {
    const auto xz = parse_points(op);
    if (!xz)
      return Result<std::vector<std::array<float, 3>>>::failure(xz.error());
    const auto &points_json = op.at("points");
    std::vector<float> point_heights(xz.value().size(),
                                     std::numeric_limits<float>::quiet_NaN());
    std::size_t explicit_heights = 0;
    for (std::size_t i = 0; i < points_json.size(); ++i) {
      const auto &point = points_json[i];
      if (!point.is_object())
        continue;
      if (point.contains("height") && point["height"].is_number()) {
        point_heights[i] = point["height"].get<float>();
        ++explicit_heights;
      } else if (point.contains("y") && point["y"].is_number()) {
        point_heights[i] = point["y"].get<float>();
        ++explicit_heights;
      } else if (point.contains("targetHeight") &&
                 point["targetHeight"].is_number()) {
        point_heights[i] = point["targetHeight"].get<float>();
        ++explicit_heights;
      }
    }

    const bool has_start = op.contains("startHeight") &&
                           op["startHeight"].is_number();
    const bool has_end =
        op.contains("endHeight") && op["endHeight"].is_number();
    const bool has_target =
        op.contains("targetHeight") && op["targetHeight"].is_number();
    const float start_h =
        has_start ? op["startHeight"].get<float>()
                  : (has_target ? op["targetHeight"].get<float>() : 0.0f);
    const float end_h = has_end ? op["endHeight"].get<float>()
                                : (has_target ? op["targetHeight"].get<float>()
                                              : start_h);

    if (explicit_heights == 0 && !has_start && !has_end && !has_target) {
      return Result<std::vector<std::array<float, 3>>>::failure(session_error(
          "TERRAIN-HEIGHT-ALONG-ARGS",
          "set_height_along requires heights on points or "
          "startHeight/endHeight/targetHeight.",
          "Example: points:[{x,z,height},...] or startHeight+endHeight."));
    }

    std::vector<float> arc(xz.value().size(), 0.0f);
    float total = 0.0f;
    for (std::size_t i = 1; i < xz.value().size(); ++i) {
      total += std::hypot(xz.value()[i][0] - xz.value()[i - 1][0],
                          xz.value()[i][1] - xz.value()[i - 1][1]);
      arc[i] = total;
    }

    auto lerp_path_height = [&](float s) -> float {
      if (total <= 1.0e-4f)
        return start_h;
      const float t = std::clamp(s / total, 0.0f, 1.0f);
      return start_h + (end_h - start_h) * t;
    };

    if (explicit_heights == 0) {
      std::vector<std::array<float, 3>> controls;
      controls.reserve(xz.value().size());
      for (std::size_t i = 0; i < xz.value().size(); ++i) {
        controls.push_back(
            {xz.value()[i][0], xz.value()[i][1], lerp_path_height(arc[i])});
      }
      return Result<std::vector<std::array<float, 3>>>::success(
          std::move(controls));
    }

    if (explicit_heights < xz.value().size()) {
      // Fill missing control heights from neighbors / start-end ramp.
      std::vector<std::size_t> anchors;
      for (std::size_t i = 0; i < point_heights.size(); ++i) {
        if (std::isfinite(point_heights[i]))
          anchors.push_back(i);
      }
      if (anchors.empty()) {
        return Result<std::vector<std::array<float, 3>>>::failure(session_error(
            "TERRAIN-HEIGHT-ALONG-ARGS",
            "No finite point heights found for set_height_along.",
            "Provide finite height/y/targetHeight on points."));
      }
      if (!std::isfinite(point_heights.front())) {
        point_heights.front() =
            has_start || has_target ? start_h : point_heights[anchors.front()];
        if (anchors.front() != 0)
          anchors.insert(anchors.begin(), 0);
      }
      if (!std::isfinite(point_heights.back())) {
        point_heights.back() =
            has_end || has_target ? end_h : point_heights[anchors.back()];
        if (anchors.back() != point_heights.size() - 1)
          anchors.push_back(point_heights.size() - 1);
      }
      for (std::size_t a = 0; a + 1 < anchors.size(); ++a) {
        const std::size_t i0 = anchors[a];
        const std::size_t i1 = anchors[a + 1];
        const float h0 = point_heights[i0];
        const float h1 = point_heights[i1];
        const float s0 = arc[i0];
        const float s1 = arc[i1];
        const float span = std::max(1.0e-4f, s1 - s0);
        for (std::size_t i = i0 + 1; i < i1; ++i) {
          if (std::isfinite(point_heights[i]))
            continue;
          const float t = (arc[i] - s0) / span;
          point_heights[i] = h0 + (h1 - h0) * t;
        }
      }
    }

    for (float h : point_heights) {
      if (!std::isfinite(h)) {
        return Result<std::vector<std::array<float, 3>>>::failure(session_error(
            "TERRAIN-HEIGHT-ALONG-FINITE",
            "Resolved set_height_along control heights must be finite.",
            "Provide complete heights or startHeight/endHeight."));
      }
    }

    std::vector<std::array<float, 3>> controls;
    controls.reserve(xz.value().size());
    for (std::size_t i = 0; i < xz.value().size(); ++i) {
      controls.push_back(
          {xz.value()[i][0], xz.value()[i][1], point_heights[i]});
    }
    return Result<std::vector<std::array<float, 3>>>::success(
        std::move(controls));
  };

  auto apply_height_along_op =
      [&](const nlohmann::json &op) -> Result<std::set<CellCoord>> {
    const auto op_action = op.value("action", std::string{});
    const bool is_smooth =
        op_action == "smooth_along" || op_action == "smooth_path";
    const bool is_grade = op_action == "set_height_along" ||
                          op_action == "grade_along" ||
                          op_action == "flatten_along";
    if (!is_smooth && !is_grade) {
      return Result<std::set<CellCoord>>::failure(session_error(
          "TERRAIN-HEIGHT-ALONG-ACTION",
          "Unsupported height-along action: " + op_action,
          "Use set_height_along/grade_along or smooth_along."));
    }

    const float step = op.value("step", 2.0f);
    const float radius = op.contains("halfWidth")
                             ? op["halfWidth"].get<float>()
                             : op.value("radius", 4.0f);
    const float strength =
        op.value("strength", is_smooth ? 0.55f : 1.0f);
    const int smooth_passes = std::max(0, op.value("smoothPasses", 0));
    const float kernel =
        op.contains("kernelRadius")
            ? op["kernelRadius"].get<float>()
            : std::max(0.5f, radius * 0.35f);
    if (!std::isfinite(radius) || radius <= 0.0f) {
      return Result<std::set<CellCoord>>::failure(session_error(
          "TERRAIN-HEIGHT-ALONG-RADIUS",
          "set_height_along/smooth_along require a positive radius/halfWidth.",
          "Provide radius or halfWidth in meters."));
    }

    std::vector<std::array<float, 3>> samples;
    if (is_grade) {
      const auto controls = resolve_height_along_controls(op);
      if (!controls)
        return Result<std::set<CellCoord>>::failure(controls.error());
      samples = densify_polyline_heights(controls.value(), step);
    } else {
      const auto controls = parse_points(op);
      if (!controls)
        return Result<std::set<CellCoord>>::failure(controls.error());
      const auto xz = densify_polyline(controls.value(), step);
      samples.reserve(xz.size());
      for (const auto &p : xz)
        samples.push_back({p[0], p[1], 0.0f});
    }

    std::set<CellCoord> touched_all;
    if (is_grade) {
      const float skirt = op.value("skirtWidth", std::max(2.0f, radius * 0.55f));
      const auto bed = context.terrain_edits->apply_grade_strip(
          samples, radius, skirt, strength);
      if (!bed)
        return Result<std::set<CellCoord>>::failure(bed.error());
      touched_all = bed.value();
    } else {
      for (const auto &sample : samples) {
        const auto smoothed = context.terrain_edits->apply_smooth_brush(
            sample[0], sample[1], radius, strength, kernel);
        if (!smoothed)
          return Result<std::set<CellCoord>>::failure(smoothed.error());
        touched_all.insert(smoothed.value().begin(), smoothed.value().end());
      }
    }

    if (is_grade && smooth_passes > 0) {
      const float smooth_strength =
          op.value("smoothStrength", std::min(0.55f, strength));
      const float smooth_radius =
          op.value("smoothRadius", radius + op.value("skirtWidth", radius * 0.55f));
      for (int pass = 0; pass < smooth_passes; ++pass) {
        for (const auto &sample : samples) {
          const auto smoothed = context.terrain_edits->apply_smooth_brush(
              sample[0], sample[1], smooth_radius, smooth_strength, kernel);
          if (!smoothed)
            return Result<std::set<CellCoord>>::failure(smoothed.error());
          touched_all.insert(smoothed.value().begin(), smoothed.value().end());
        }
      }
    }

    return Result<std::set<CellCoord>>::success(std::move(touched_all));
  };

  auto densify_paint_points =
      [&](const nlohmann::json &op)
      -> Result<std::vector<std::array<float, 2>>> {
    const auto controls = parse_points(op);
    if (!controls)
      return Result<std::vector<std::array<float, 2>>>::failure(
          controls.error());
    return Result<std::vector<std::array<float, 2>>>::success(
        densify_polyline(controls.value(), op.value("step", 2.5f)));
  };

  auto apply_paint_along_op =
      [&](const nlohmann::json &op) -> Result<std::set<CellCoord>> {
    const auto pts = densify_paint_points(op);
    if (!pts)
      return Result<std::set<CellCoord>>::failure(pts.error());
    const auto material = op.value("material", std::string{});
    if (material.empty()) {
      return Result<std::set<CellCoord>>::failure(session_error(
          "TERRAIN-PAINT-MATERIAL", "paint_along requires a material asset path.",
          "Provide assets/materials/....material.json"));
    }
    const float radius = op.value("radius", 4.0f);
    const auto palette_index =
        context.terrain_paint->ensure_material_index(material);
    std::set<CellCoord> touched_all;
    for (const auto &p : pts.value()) {
      const auto touched = context.terrain_paint->apply_material_brush(
          p[0], p[1], radius, palette_index);
      if (!touched)
        return Result<std::set<CellCoord>>::failure(touched.error());
      touched_all.insert(touched.value().begin(), touched.value().end());
    }
    return Result<std::set<CellCoord>>::success(std::move(touched_all));
  };

  auto is_smart_sculpt_action = [](const std::string &a) {
    return a == "gentle_hill" || a == "steep_cliff" || a == "flatten_pad" ||
           a == "smooth_natural" || a == "canyon";
  };

  auto expand_smart_sculpt =
      [&](const nlohmann::json &recipe)
      -> Result<std::vector<nlohmann::json>> {
    const auto recipe_action = recipe.value("action", std::string{});
    std::vector<nlohmann::json> expanded;
    if (recipe_action == "canyon") {
      nlohmann::json channel = recipe;
      channel["action"] = "carve_channel";
      if (!channel.contains("points")) {
        if (!recipe.contains("x") || !recipe.contains("z")) {
          return Result<std::vector<nlohmann::json>>::failure(session_error(
              "TERRAIN-CANYON-ARGS",
              "canyon requires points:[{x,z}] or center x/z.",
              "Provide a polyline or a center sample."));
        }
        const float x = recipe["x"].get<float>();
        const float z = recipe["z"].get<float>();
        const float length = recipe.value("length", 24.0f);
        const float yaw_deg = recipe.value("yawDegrees", 0.0f);
        const float yaw = yaw_deg * 0.01745329251f;
        const float dx = std::cos(yaw) * length * 0.5f;
        const float dz = std::sin(yaw) * length * 0.5f;
        channel["points"] = nlohmann::json::array(
            {{{"x", x - dx}, {"z", z - dz}},
             {{"x", x}, {"z", z}},
             {{"x", x + dx}, {"z", z + dz}}});
      }
      if (!channel.contains("halfWidth"))
        channel["halfWidth"] = recipe.value("halfWidth", 5.0f);
      if (!channel.contains("bedDepth"))
        channel["bedDepth"] = recipe.value("bedDepth", 3.5f);
      if (!channel.contains("bankWidth"))
        channel["bankWidth"] = recipe.value("bankWidth", 4.0f);
      if (!channel.contains("bankClearance"))
        channel["bankClearance"] = recipe.value("bankClearance", 2.0f);
      if (!channel.contains("strength"))
        channel["strength"] = recipe.value("strength", 1.0f);
      const float half_width = channel.value("halfWidth", 5.0f);
      const float bank_width = channel.value("bankWidth", 4.0f);
      expanded.push_back(std::move(channel));
      if (recipe.contains("x") && recipe.contains("z")) {
        expanded.push_back(
            {{"action", "smooth"},
             {"x", recipe["x"]},
             {"z", recipe["z"]},
             {"radius", recipe.value("radius", half_width + bank_width)},
             {"strength", 0.35}});
      }
    } else {
      if (!recipe.contains("x") || !recipe.contains("z")) {
        return Result<std::vector<nlohmann::json>>::failure(session_error(
            "TERRAIN-BRUSH-ARGS", "Terrain brush requires x and z.",
            "Provide world x/z."));
      }
      const float x = recipe["x"].get<float>();
      const float z = recipe["z"].get<float>();
      if (recipe_action == "gentle_hill") {
        const float radius = recipe.value("radius", 14.0f);
        const float rise = recipe.value("height", 4.0f);
        const float base = sample_terrain_height(x, z);
        expanded.push_back({{"action", "set_height"},
                            {"x", x},
                            {"z", z},
                            {"radius", radius},
                            {"strength", recipe.value("strength", 0.7)},
                            {"targetHeight", base + rise}});
        expanded.push_back({{"action", "smooth"},
                            {"x", x},
                            {"z", z},
                            {"radius", radius * 1.15f},
                            {"strength", 0.45}});
      } else if (recipe_action == "steep_cliff") {
        const float radius = recipe.value("radius", 10.0f);
        const float rise = recipe.value("height", 10.0f);
        const float base = sample_terrain_height(x, z);
        const float yaw_deg = recipe.value("yawDegrees", 90.0f);
        const float yaw = yaw_deg * 0.01745329251f;
        const float ox = x + std::cos(yaw) * radius * 0.65f;
        const float oz = z + std::sin(yaw) * radius * 0.65f;
        expanded.push_back({{"action", "set_height"},
                            {"x", x},
                            {"z", z},
                            {"radius", radius * 0.85f},
                            {"strength", 0.95},
                            {"targetHeight", base + rise}});
        expanded.push_back(
            {{"action", "set_height"},
             {"x", ox},
             {"z", oz},
             {"radius", radius * 0.9f},
             {"strength", 0.95},
             {"targetHeight",
              recipe.value("lowHeight", base - rise * 0.35f)}});
        expanded.push_back({{"action", "terrace"},
                            {"x", x},
                            {"z", z},
                            {"radius", radius},
                            {"strength", 0.7},
                            {"stepSize", recipe.value("stepSize", 1.5)}});
      } else if (recipe_action == "flatten_pad") {
        const float radius = recipe.value("radius", 12.0f);
        const float skirt =
            recipe.value("skirtWidth", std::max(4.0f, radius * 0.45f));
        const float target = recipe.contains("targetHeight")
                                 ? recipe["targetHeight"].get<float>()
                                 : (recipe.contains("height")
                                        ? recipe["height"].get<float>()
                                        : sample_terrain_height(x, z));
        expanded.push_back({{"action", "plateau"},
                            {"x", x},
                            {"z", z},
                            {"radius", radius},
                            {"skirtWidth", skirt},
                            {"strength", recipe.value("strength", 1.0)},
                            {"targetHeight", target}});
        expanded.push_back({{"action", "smooth"},
                            {"x", x},
                            {"z", z},
                            {"radius", radius + skirt * 0.75f},
                            {"strength", recipe.value("edgeSmooth", 0.4)},
                            {"kernelRadius",
                             recipe.value("kernelRadius",
                                          std::max(2.0f, radius * 0.25f))}});
      } else { // smooth_natural
        const float radius = recipe.value("radius", 12.0f);
        expanded.push_back(
            {{"action", "smooth"},
             {"x", x},
             {"z", z},
             {"radius", radius},
             {"strength", recipe.value("strength", 0.65)},
             {"kernelRadius",
              recipe.value("kernelRadius", radius * 0.4f)}});
      }
    }
    return Result<std::vector<nlohmann::json>>::success(std::move(expanded));
  };

  auto probe_height_op_cells =
      [&](const nlohmann::json &op,
          std::set<CellCoord> &out) -> std::optional<EditorBridgeResponse> {
    const auto op_action = op.value("action", std::string{});
    if (op_action == "raise" || op_action == "lower" ||
        op_action == "flatten" || op_action == "set_height" ||
        op_action == "plateau" || op_action == "smooth" ||
        op_action == "terrace") {
      if (!op.contains("x") || !op.contains("z")) {
        return make_response(ExitCode::InvalidArguments,
                             "batch height op requires x/z", {},
                             {session_error("TERRAIN-BRUSH-ARGS",
                                            "Terrain brush requires x and z.",
                                            "Provide world x/z.")});
      }
      float radius = op.value("radius", 4.0f);
      if (op_action == "plateau")
        radius += op.value("skirtWidth", std::max(4.0f, radius * 0.45f));
      auto probe = probe_cells(op["x"].get<float>(), op["z"].get<float>(),
                               radius, TerrainEditStore::k_cell_size);
      out.insert(probe.begin(), probe.end());
      return std::nullopt;
    }
    if (op_action == "carve_channel" || op_action == "raise_banks") {
      const auto controls = parse_points(op);
      if (!controls)
        return make_response(ExitCode::InvalidArguments,
                             controls.error().message, {}, {controls.error()});
      const float step = op.value("step", 2.5f);
      const auto pts = densify_polyline(controls.value(), step);
      const float half_width =
          op.value("halfWidth", op_action == "carve_channel" ? 3.5f : 0.0f);
      const float bank_width = op.value("bankWidth", 3.5f);
      const float bank_offset =
          std::max(op.value("bankOffset", half_width + bank_width),
                   half_width + bank_width);
      const float probe_radius =
          std::max(half_width, bank_width) + bank_offset + 1.0f;
      for (const auto &p : pts) {
        auto local =
            probe_cells(p[0], p[1], probe_radius, TerrainEditStore::k_cell_size);
        out.insert(local.begin(), local.end());
      }
      return std::nullopt;
    }
    if (op_action == "set_height_along" || op_action == "grade_along" ||
        op_action == "flatten_along" || op_action == "smooth_along" ||
        op_action == "smooth_path") {
      const auto controls = parse_points(op);
      if (!controls)
        return make_response(ExitCode::InvalidArguments,
                             controls.error().message, {}, {controls.error()});
      const float step = op.value("step", 2.0f);
      const auto pts = densify_polyline(controls.value(), step);
      float probe_radius = op.contains("halfWidth")
                               ? op["halfWidth"].get<float>()
                               : op.value("radius", 4.0f);
      probe_radius +=
          op.value("skirtWidth", std::max(2.0f, probe_radius * 0.55f));
      for (const auto &p : pts) {
        auto local =
            probe_cells(p[0], p[1], probe_radius, TerrainEditStore::k_cell_size);
        out.insert(local.begin(), local.end());
      }
      return std::nullopt;
    }
    return make_response(
        ExitCode::InvalidArguments,
        "Unsupported batch height action: " + op_action, {},
        {session_error("TERRAIN-BATCH-ACTION",
                       "Unsupported batch height action: " + op_action,
                       "Use raise/lower/flatten/set_height/smooth/terrace/"
                       "carve_channel/raise_banks/set_height_along/"
                       "smooth_along or smart sculpt recipes.")});
  };

  auto save_terrain = [&]() -> EditorBridgeResponse {
    std::vector<std::string> saved;
    if (context.terrain_edits) {
      const auto path = context.terrain_edits_path.empty()
                            ? default_terrain_edits_path(context.project_root)
                            : context.terrain_edits_path;
      const auto result = context.terrain_edits->save_atomic(path);
      if (!result)
        return make_response(ExitCode::ValidationFailed, result.error().message,
                             {}, {result.error()});
      if (context.terrain_edits_dirty)
        *context.terrain_edits_dirty = false;
      saved.push_back(path.generic_string());
    }
    if (context.terrain_paint) {
      const auto path = context.terrain_paint_path.empty()
                            ? default_terrain_paint_path(context.project_root)
                            : context.terrain_paint_path;
      const auto result = context.terrain_paint->save_atomic(path);
      if (!result)
        return make_response(ExitCode::ValidationFailed, result.error().message,
                             {}, {result.error()});
      if (context.terrain_paint_dirty)
        *context.terrain_paint_dirty = false;
      saved.push_back(path.generic_string());
    }
    if (context.foliage_density) {
      const auto path = context.foliage_density_path.empty()
                            ? default_foliage_density_path(context.project_root)
                            : context.foliage_density_path;
      const auto result = context.foliage_density->save_atomic(path);
      if (!result)
        return make_response(ExitCode::ValidationFailed, result.error().message,
                             {}, {result.error()});
      if (context.foliage_density_dirty)
        *context.foliage_density_dirty = false;
      saved.push_back(path.generic_string());
    }
    if (saved.empty()) {
      return make_response(
          ExitCode::Unavailable, "No terrain stores to save", {},
          {session_error(
              "TERRAIN-SAVE-MISSING", "Terrain stores are not bound.",
              "Enable MCP connection in a running editor session.")});
    }
    return make_response(ExitCode::Success, "Terrain assets saved",
                         std::move(saved));
  };

  constexpr std::size_t k_max_terrain_batch_ops = 200;
  if (action == "batch") {
    if (!params.contains("ops") || !params["ops"].is_array()) {
      return make_response(
          ExitCode::InvalidArguments, "batch requires ops array", {},
          {session_error("TERRAIN-BATCH-OPS", "batch requires an ops array.",
                         "Provide ops:[{action,x,z,...},...]")});
    }
    const auto &ops = params["ops"];
    if (ops.empty()) {
      return make_response(
          ExitCode::InvalidArguments, "batch ops is empty", {},
          {session_error("TERRAIN-BATCH-EMPTY",
                         "batch requires at least one operation.",
                         "Provide one or more terrain operations.")});
    }
    if (ops.size() > k_max_terrain_batch_ops) {
      return make_response(
          ExitCode::InvalidArguments, "batch exceeds operation limit", {},
          {session_error("TERRAIN-BATCH-LIMIT",
                         "batch supports at most " +
                             std::to_string(k_max_terrain_batch_ops) +
                             " operations.",
                         "Split the request into smaller batches.")});
    }

    bool needs_height = false;
    bool needs_paint = false;
    bool needs_foliage = false;
    bool has_sample = false;
    std::set<CellCoord> height_probe;
    std::set<CellCoord> paint_probe;
    std::set<CellCoord> foliage_probe;
    for (const auto &op : ops) {
      if (!op.is_object()) {
        return make_response(
            ExitCode::InvalidArguments, "batch op must be an object", {},
            {session_error("TERRAIN-BATCH-OP-INVALID",
                           "Each batch op must be a JSON object.",
                           "Use {action,x,z,...} entries.")});
      }
      const auto op_action = op.value("action", std::string{});
      if (op_action == "sample" || op_action == "sample_terrain") {
        has_sample = true;
        if (!op.contains("x") || !op.contains("z")) {
          return make_response(
              ExitCode::InvalidArguments, "batch sample requires x/z", {},
              {session_error("TERRAIN-SAMPLE-ARGS",
                             "sample requires x and z world coordinates.",
                             "Provide numeric x and z.")});
        }
        continue;
      }
      if (is_smart_sculpt_action(op_action)) {
        needs_height = true;
        nlohmann::json recipe = op;
        recipe["action"] = op_action;
        const auto expanded = expand_smart_sculpt(recipe);
        if (!expanded)
          return make_response(ExitCode::InvalidArguments,
                               expanded.error().message, {},
                               {expanded.error()});
        for (const auto &sub : expanded.value()) {
          if (auto err = probe_height_op_cells(sub, height_probe))
            return *err;
        }
        continue;
      }
      if (op_action == "raise" || op_action == "lower" ||
          op_action == "flatten" || op_action == "set_height" ||
          op_action == "plateau" || op_action == "smooth" ||
          op_action == "terrace" || op_action == "carve_channel" ||
          op_action == "raise_banks" || op_action == "set_height_along" ||
          op_action == "grade_along" || op_action == "flatten_along" ||
          op_action == "smooth_along" || op_action == "smooth_path") {
        needs_height = true;
        if (auto err = probe_height_op_cells(op, height_probe))
          return *err;
      } else if (op_action == "paint") {
        needs_paint = true;
        if (!op.contains("x") || !op.contains("z")) {
          return make_response(
              ExitCode::InvalidArguments, "batch paint op requires x/z", {},
              {session_error("TERRAIN-PAINT-ARGS", "paint requires x and z.",
                             "Provide world x/z.")});
        }
        const float radius = op.value("radius", 4.0f);
        auto probe = probe_cells(op["x"].get<float>(), op["z"].get<float>(),
                                 radius, TerrainPaintStore::k_cell_size);
        paint_probe.insert(probe.begin(), probe.end());
      } else if (op_action == "paint_along") {
        needs_paint = true;
        const auto pts = densify_paint_points(op);
        if (!pts)
          return make_response(ExitCode::InvalidArguments, pts.error().message,
                               {}, {pts.error()});
        const float radius = op.value("radius", 4.0f);
        for (const auto &p : pts.value()) {
          auto local =
              probe_cells(p[0], p[1], radius, TerrainPaintStore::k_cell_size);
          paint_probe.insert(local.begin(), local.end());
        }
      } else if (op_action == "paint_foliage" ||
                 op_action == "paint_foliage_mixed") {
        needs_foliage = true;
        if (!op.contains("x") || !op.contains("z")) {
          return make_response(ExitCode::InvalidArguments,
                               "batch foliage op requires x/z", {},
                               {session_error("FOLIAGE-BRUSH-ARGS",
                                              "paint_foliage requires x and z.",
                                              "Provide world x/z.")});
        }
        const float radius = op.value("radius", 4.0f);
        auto probe = probe_cells(op["x"].get<float>(), op["z"].get<float>(),
                                 radius, FoliageDensityStore::k_cell_size);
        foliage_probe.insert(probe.begin(), probe.end());
      } else {
        return make_response(
            ExitCode::InvalidArguments,
            "Unsupported batch terrain action: " + op_action, {},
            {session_error(
                "TERRAIN-BATCH-ACTION",
                "Unsupported batch action: " + op_action,
                "Batch ops may be "
                "raise/lower/flatten/set_height/smooth/terrace/"
                "gentle_hill/steep_cliff/flatten_pad/smooth_natural/canyon/"
                "carve_channel/raise_banks/set_height_along/grade_along/"
                "smooth_along/paint/paint_along/"
                "paint_foliage/paint_foliage_mixed/sample.")});
      }
    }
    if (needs_height) {
      if (auto missing = require_edits())
        return *missing;
    }
    if (needs_paint) {
      if (auto missing = require_paint())
        return *missing;
    }
    if (needs_foliage) {
      if (auto missing = require_foliage())
        return *missing;
    }

    auto height_before = needs_height
                             ? snapshot_height(height_probe)
                             : std::map<CellCoord, std::vector<float>>{};
    auto paint_before = needs_paint
                            ? snapshot_paint(paint_probe)
                            : std::map<CellCoord, std::vector<std::uint16_t>>{};
    auto foliage_before = needs_foliage
                              ? snapshot_foliage(foliage_probe)
                              : std::map<CellCoord, FoliageCellSnapshot>{};

    std::size_t applied = 0;
    nlohmann::json samples = nlohmann::json::array();
    for (const auto &op : ops) {
      const auto op_action = op.value("action", std::string{});
      if (op_action == "sample" || op_action == "sample_terrain") {
        const float x = op["x"].get<float>();
        const float z = op["z"].get<float>();
        if (!std::isfinite(x) || !std::isfinite(z)) {
          restore_height(height_before);
          restore_paint(paint_before);
          restore_foliage(foliage_before);
          return make_response(
              ExitCode::InvalidArguments, "sample coordinates must be finite",
              {},
              {session_error("TERRAIN-SAMPLE-FINITE",
                             "sample x/z must be finite.",
                             "Use finite world coordinates.")},
              {{"failedIndex", std::to_string(applied)}});
        }
        const float height = sample_terrain_height(x, z);
        const float offset = op.value("groundOffset", 0.0f);
        samples.push_back({{"x", x},
                           {"z", z},
                           {"height", height},
                           {"groundOffset", offset},
                           {"placedY", height + offset}});
        ++applied;
        continue;
      }

      auto apply_one_height = [&](const nlohmann::json &sub)
          -> Result<std::set<CellCoord>> {
        const auto sub_action = sub.value("action", std::string{});
        if (sub_action == "carve_channel" || sub_action == "raise_banks")
          return apply_channel_op(sub);
        if (sub_action == "set_height_along" || sub_action == "grade_along" ||
            sub_action == "flatten_along" || sub_action == "smooth_along" ||
            sub_action == "smooth_path")
          return apply_height_along_op(sub);
        return apply_height_op(sub);
      };

      Result<std::set<CellCoord>> touched =
          Result<std::set<CellCoord>>::failure(
              session_error("TERRAIN-BATCH-ACTION", "Unsupported batch action",
                            "Check action name."));
      if (is_smart_sculpt_action(op_action)) {
        nlohmann::json recipe = op;
        recipe["action"] = op_action;
        const auto expanded = expand_smart_sculpt(recipe);
        if (!expanded) {
          restore_height(height_before);
          restore_paint(paint_before);
          restore_foliage(foliage_before);
          return make_response(ExitCode::ValidationFailed,
                               expanded.error().message, {},
                               {expanded.error()},
                               {{"failedIndex", std::to_string(applied)}});
        }
        std::set<CellCoord> touched_all;
        for (const auto &sub : expanded.value()) {
          const auto sub_touched = apply_one_height(sub);
          if (!sub_touched) {
            restore_height(height_before);
            restore_paint(paint_before);
            restore_foliage(foliage_before);
            return make_response(
                ExitCode::ValidationFailed, sub_touched.error().message, {},
                {sub_touched.error()},
                {{"failedIndex", std::to_string(applied)}});
          }
          touched_all.insert(sub_touched.value().begin(),
                             sub_touched.value().end());
        }
        touched = Result<std::set<CellCoord>>::success(std::move(touched_all));
      } else if (op_action == "raise" || op_action == "lower" ||
                 op_action == "flatten" || op_action == "set_height" ||
                 op_action == "plateau" || op_action == "smooth" ||
                 op_action == "terrace")
        touched = apply_height_op(op);
      else if (op_action == "carve_channel" || op_action == "raise_banks")
        touched = apply_channel_op(op);
      else if (op_action == "set_height_along" || op_action == "grade_along" ||
               op_action == "flatten_along" || op_action == "smooth_along" ||
               op_action == "smooth_path")
        touched = apply_height_along_op(op);
      else if (op_action == "paint")
        touched = apply_paint_op(op);
      else if (op_action == "paint_along")
        touched = apply_paint_along_op(op);
      else
        touched = apply_foliage_op(op);
      if (!touched) {
        restore_height(height_before);
        restore_paint(paint_before);
        restore_foliage(foliage_before);
        return make_response(ExitCode::ValidationFailed,
                             touched.error().message, {}, {touched.error()},
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
      if (committed.exit_code != ExitCode::Success)
        return committed;
    }

    if (context.reload_terrain && (needs_height || needs_paint))
      context.reload_terrain(needs_height);
    if (needs_height && context.reload_water)
      context.reload_water();
    if (needs_foliage && context.reload_foliage)
      context.reload_foliage();

    if (params.value("save", false)) {
      const auto saved = save_terrain();
      if (saved.exit_code != ExitCode::Success)
        return saved;
    }
    std::map<std::string, std::string> metadata{
        {"appliedCount", std::to_string(applied)},
        {"heightChanged", needs_height ? "true" : "false"},
        {"paintChanged", needs_paint ? "true" : "false"},
        {"foliageChanged", needs_foliage ? "true" : "false"}};
    if (has_sample)
      metadata["samplesJson"] = samples.dump();
    return make_response(ExitCode::Success, "Terrain batch applied", {}, {},
                         std::move(metadata));
  }

  if (action == "raise" || action == "lower" || action == "flatten" ||
      action == "set_height" || action == "plateau" || action == "smooth" ||
      action == "terrace") {
    if (auto missing = require_edits())
      return *missing;
    if (!params.contains("x") || !params.contains("z")) {
      return make_response(ExitCode::InvalidArguments,
                           action + " requires x and z", {},
                           {session_error("TERRAIN-BRUSH-ARGS",
                                          "Terrain brush requires x and z.",
                                          "Provide world x/z.")});
    }
    if ((action == "set_height" || action == "plateau") &&
        !params.contains("targetHeight") && !params.contains("height")) {
      return make_response(
          ExitCode::InvalidArguments, action + " requires targetHeight", {},
          {session_error("TERRAIN-SET-HEIGHT-ARGS",
                         action + " requires targetHeight.",
                         "Provide targetHeight world Y.")});
    }
    if (action == "terrace" && !params.contains("stepSize") &&
        !params.contains("step")) {
      return make_response(
          ExitCode::InvalidArguments, "terrace requires stepSize", {},
          {session_error("TERRAIN-TERRACE-ARGS", "terrace requires stepSize.",
                         "Provide stepSize in meters (e.g. 2.0).")});
    }
    const float x = params["x"].get<float>();
    const float z = params["z"].get<float>();
    float radius = params.value("radius", 4.0f);
    if (action == "plateau")
      radius +=
          params.value("skirtWidth", std::max(4.0f, radius * 0.45f));
    auto before = snapshot_height(
        probe_cells(x, z, radius, TerrainEditStore::k_cell_size));
    nlohmann::json op = params;
    op["action"] = action;
    if (action == "plateau" && !op.contains("targetHeight") &&
        op.contains("height"))
      op["targetHeight"] = op["height"];
    const auto touched = apply_height_op(op);
    if (!touched)
      return make_response(ExitCode::ValidationFailed, touched.error().message,
                           {}, {touched.error()});
    auto response = commit_height(std::move(before), true);
    if (response.exit_code == ExitCode::Success) {
      response.metadata["touchedCells"] =
          std::to_string(touched.value().size());
      std::vector<std::string> changed;
      for (const auto &cell : touched.value())
        changed.push_back(std::to_string(cell.x) + "," +
                          std::to_string(cell.z));
      response.changed_object_ids = std::move(changed);
      if (touched.value().empty())
        response.summary = "Terrain brush touched no samples";
      else if (action == "set_height")
        response.summary = "Terrain height set";
      else if (action == "plateau")
        response.summary = "Terrain plateau flattened";
      else if (action == "smooth")
        response.summary = "Terrain smoothed";
      else if (action == "terrace")
        response.summary = "Terrain terraced";
    }
    return response;
  }

  if (action == "carve_channel" || action == "raise_banks") {
    if (auto missing = require_edits())
      return *missing;
    const auto controls = parse_points(params);
    if (!controls)
      return make_response(ExitCode::InvalidArguments, controls.error().message,
                           {}, {controls.error()});
    const float step = params.value("step", 2.5f);
    const auto pts = densify_polyline(controls.value(), step);
    const float sea = resolve_sea_level();
    const float half_width =
        params.value("halfWidth", action == "carve_channel" ? 3.5f : 0.0f);
    const float bank_width = params.value("bankWidth", 3.5f);
    const float min_bank_offset = half_width + bank_width;
    const float bank_offset =
        std::max(params.value("bankOffset", min_bank_offset), min_bank_offset);
    float probe_radius = std::max(half_width, bank_width) + bank_offset + 1.0f;
    std::set<CellCoord> probe;
    for (const auto &p : pts) {
      auto local =
          probe_cells(p[0], p[1], probe_radius, TerrainEditStore::k_cell_size);
      probe.insert(local.begin(), local.end());
    }
    auto before = snapshot_height(probe);
    nlohmann::json op = params;
    op["action"] = action;
    const auto touched = apply_channel_op(op);
    if (!touched) {
      restore_height(before);
      return make_response(ExitCode::ValidationFailed, touched.error().message,
                           {}, {touched.error()});
    }
    auto response = commit_height(std::move(before), true);
    if (response.exit_code == ExitCode::Success) {
      response.summary = action == "carve_channel" ? "River channel carved"
                                                   : "River banks raised";
      response.metadata["touchedCells"] = std::to_string(touched.value().size());
      response.metadata["pointCount"] = std::to_string(pts.size());
      response.metadata["seaLevel"] = std::to_string(sea);
      if (params.value("save", false)) {
        const auto saved = save_terrain();
        if (saved.exit_code != ExitCode::Success)
          return saved;
      }
    }
    return response;
  }

  if (action == "set_height_along" || action == "grade_along" ||
      action == "flatten_along" || action == "smooth_along" ||
      action == "smooth_path") {
    if (auto missing = require_edits())
      return *missing;
    const auto controls = parse_points(params);
    if (!controls)
      return make_response(ExitCode::InvalidArguments, controls.error().message,
                           {}, {controls.error()});
    const float step = params.value("step", 2.0f);
    const auto pts = densify_polyline(controls.value(), step);
    const float probe_radius = params.contains("halfWidth")
                                   ? params["halfWidth"].get<float>()
                                   : params.value("radius", 4.0f);
    std::set<CellCoord> probe;
    for (const auto &p : pts) {
      auto local =
          probe_cells(p[0], p[1], probe_radius, TerrainEditStore::k_cell_size);
      probe.insert(local.begin(), local.end());
    }
    auto before = snapshot_height(probe);
    nlohmann::json op = params;
    op["action"] = action;
    const auto touched = apply_height_along_op(op);
    if (!touched) {
      restore_height(before);
      return make_response(ExitCode::ValidationFailed, touched.error().message,
                           {}, {touched.error()});
    }
    auto response = commit_height(std::move(before), true);
    if (response.exit_code == ExitCode::Success) {
      const bool is_smooth =
          action == "smooth_along" || action == "smooth_path";
      response.summary =
          is_smooth ? "Terrain smoothed along polyline"
                    : "Terrain graded along polyline";
      response.metadata["touchedCells"] =
          std::to_string(touched.value().size());
      response.metadata["pointCount"] = std::to_string(pts.size());
      if (params.value("save", false)) {
        const auto saved = save_terrain();
        if (saved.exit_code != ExitCode::Success)
          return saved;
      }
    }
    return response;
  }

  if (action == "paint_along") {
    if (auto missing = require_paint())
      return *missing;
    const auto pts = densify_paint_points(params);
    if (!pts)
      return make_response(ExitCode::InvalidArguments, pts.error().message, {},
                           {pts.error()});
    const float radius = params.value("radius", 4.0f);
    std::set<CellCoord> probe;
    for (const auto &p : pts.value()) {
      auto local =
          probe_cells(p[0], p[1], radius, TerrainPaintStore::k_cell_size);
      probe.insert(local.begin(), local.end());
    }
    auto before = snapshot_paint(probe);
    nlohmann::json op = params;
    op["action"] = "paint_along";
    const auto touched = apply_paint_along_op(op);
    if (!touched) {
      restore_paint(before);
      return make_response(ExitCode::ValidationFailed, touched.error().message,
                           {}, {touched.error()});
    }
    auto response = commit_paint(std::move(before), true);
    if (response.exit_code == ExitCode::Success) {
      response.summary = "Terrain painted along polyline";
      response.metadata["touchedCells"] = std::to_string(touched.value().size());
      response.metadata["pointCount"] = std::to_string(pts.value().size());
      if (params.value("save", false)) {
        const auto saved = save_terrain();
        if (saved.exit_code != ExitCode::Success)
          return saved;
      }
    }
    return response;
  }

  if (action == "gentle_hill" || action == "steep_cliff" ||
      action == "flatten_pad" || action == "smooth_natural" ||
      action == "canyon") {
    if (auto missing = require_edits())
      return *missing;
    nlohmann::json recipe = params;
    recipe["action"] = action;
    const auto expanded = expand_smart_sculpt(recipe);
    if (!expanded)
      return make_response(ExitCode::InvalidArguments,
                           expanded.error().message, {}, {expanded.error()});
    std::set<CellCoord> probe;
    for (const auto &op : expanded.value()) {
      if (auto err = probe_height_op_cells(op, probe))
        return *err;
    }
    auto before = snapshot_height(probe);
    std::set<CellCoord> touched_all;
    for (const auto &op : expanded.value()) {
      const auto op_action = op.value("action", std::string{});
      Result<std::set<CellCoord>> touched =
          Result<std::set<CellCoord>>::failure(
              session_error("TERRAIN-SMART", "bad smart op", "Check action."));
      if (op_action == "carve_channel" || op_action == "raise_banks")
        touched = apply_channel_op(op);
      else
        touched = apply_height_op(op);
      if (!touched) {
        restore_height(before);
        return make_response(ExitCode::ValidationFailed,
                             touched.error().message, {}, {touched.error()});
      }
      touched_all.insert(touched.value().begin(), touched.value().end());
    }
    auto response = commit_height(std::move(before), true);
    if (response.exit_code == ExitCode::Success) {
      response.summary = "Smart sculpt applied: " + action;
      response.metadata["touchedCells"] = std::to_string(touched_all.size());
      response.metadata["expandedOps"] =
          std::to_string(expanded.value().size());
      response.metadata["recipe"] = action;
      if (params.value("save", false)) {
        const auto saved = save_terrain();
        if (saved.exit_code != ExitCode::Success)
          return saved;
      }
    }
    return response;
  }

  if (action == "paint") {
    if (auto missing = require_paint())
      return *missing;
    if (!params.contains("x") || !params.contains("z")) {
      return make_response(
          ExitCode::InvalidArguments, "paint requires x and z", {},
          {session_error("TERRAIN-PAINT-ARGS", "paint requires x and z.",
                         "Provide world x/z.")});
    }
    const auto material = params.value("material", std::string{});
    const float x = params["x"].get<float>();
    const float z = params["z"].get<float>();
    const float radius = params.value("radius", 4.0f);
    auto before = snapshot_paint(
        probe_cells(x, z, radius, TerrainPaintStore::k_cell_size));
    const auto touched = apply_paint_op(params);
    if (!touched)
      return make_response(ExitCode::ValidationFailed, touched.error().message,
                           {}, {touched.error()});
    auto response = commit_paint(std::move(before), true);
    if (response.exit_code == ExitCode::Success) {
      response.metadata["touchedCells"] =
          std::to_string(touched.value().size());
      response.metadata["material"] = material;
      std::vector<std::string> changed;
      for (const auto &cell : touched.value())
        changed.push_back(std::to_string(cell.x) + "," +
                          std::to_string(cell.z));
      response.changed_object_ids = std::move(changed);
    }
    return response;
  }

  if (action == "paint_foliage" || action == "paint_foliage_mixed") {
    if (auto missing = require_foliage())
      return *missing;
    if (!params.contains("x") || !params.contains("z")) {
      return make_response(ExitCode::InvalidArguments,
                           action + " requires x and z", {},
                           {session_error("FOLIAGE-BRUSH-ARGS",
                                          "paint_foliage requires x and z.",
                                          "Provide world x/z.")});
    }
    const float x = params["x"].get<float>();
    const float z = params["z"].get<float>();
    const float radius = params.value("radius", 4.0f);
    auto before = snapshot_foliage(
        probe_cells(x, z, radius, FoliageDensityStore::k_cell_size));
    const auto touched = apply_foliage_op(params);
    if (!touched)
      return make_response(ExitCode::ValidationFailed, touched.error().message,
                           {}, {touched.error()});
    auto response = commit_foliage(std::move(before), true);
    if (response.exit_code == ExitCode::Success) {
      response.metadata["touchedCells"] =
          std::to_string(touched.value().size());
      response.metadata["erase"] =
          params.value("erase", false) ? "true" : "false";
      if (params.contains("layer")) {
        if (params["layer"].is_string())
          response.metadata["layer"] = params["layer"].get<std::string>();
        else
          response.metadata["layer"] =
              std::to_string(params["layer"].get<int>());
      }
      std::vector<std::string> changed;
      for (const auto &cell : touched.value())
        changed.push_back(std::to_string(cell.x) + "," +
                          std::to_string(cell.z));
      response.changed_object_ids = std::move(changed);
    }
    return response;
  }

  if (action == "undo") {
    const auto kind = params.value("kind", std::string{"height"});
    if (kind == "paint") {
      if (auto missing = require_paint())
        return *missing;
      if (context.terrain_paint_history->undo_size() == 0)
        return make_response(ExitCode::ValidationFailed,
                             "No terrain paint undo", {},
                             {session_error("TERRAIN-PAINT-UNDO-EMPTY",
                                            "No paint strokes to undo.",
                                            "Apply a paint stroke first.")});
      const auto result =
          context.terrain_paint_history->undo(*context.terrain_paint);
      if (!result)
        return make_response(ExitCode::ValidationFailed, result.error().message,
                             {}, {result.error()});
      reload_now(false);
      return make_response(ExitCode::Success, "Terrain paint undone");
    }
    if (kind == "foliage") {
      if (auto missing = require_foliage())
        return *missing;
      if (context.foliage_density_history->undo_size() == 0)
        return make_response(
            ExitCode::ValidationFailed, "No foliage density undo", {},
            {session_error("FOLIAGE-UNDO-EMPTY", "No foliage strokes to undo.",
                           "Apply a foliage stroke first.")});
      const auto result =
          context.foliage_density_history->undo(*context.foliage_density);
      if (!result)
        return make_response(ExitCode::ValidationFailed, result.error().message,
                             {}, {result.error()});
      reload_foliage_now();
      return make_response(ExitCode::Success, "Foliage density undone");
    }
    if (auto missing = require_edits())
      return *missing;
    if (context.terrain_history->undo_size() == 0)
      return make_response(
          ExitCode::ValidationFailed, "No terrain height undo", {},
          {session_error("TERRAIN-UNDO-EMPTY", "No height strokes to undo.",
                         "Apply a height stroke first.")});
    const auto result = context.terrain_history->undo(*context.terrain_edits);
    if (!result)
      return make_response(ExitCode::ValidationFailed, result.error().message,
                           {}, {result.error()});
    reload_now(true);
    return make_response(ExitCode::Success, "Terrain height undone");
  }

  if (action == "redo") {
    const auto kind = params.value("kind", std::string{"height"});
    if (kind == "paint") {
      if (auto missing = require_paint())
        return *missing;
      if (context.terrain_paint_history->redo_size() == 0)
        return make_response(ExitCode::ValidationFailed,
                             "No terrain paint redo", {},
                             {session_error("TERRAIN-PAINT-REDO-EMPTY",
                                            "No paint strokes to redo.",
                                            "Undo a paint stroke first.")});
      const auto result =
          context.terrain_paint_history->redo(*context.terrain_paint);
      if (!result)
        return make_response(ExitCode::ValidationFailed, result.error().message,
                             {}, {result.error()});
      reload_now(false);
      return make_response(ExitCode::Success, "Terrain paint redone");
    }
    if (kind == "foliage") {
      if (auto missing = require_foliage())
        return *missing;
      if (context.foliage_density_history->redo_size() == 0)
        return make_response(
            ExitCode::ValidationFailed, "No foliage density redo", {},
            {session_error("FOLIAGE-REDO-EMPTY", "No foliage strokes to redo.",
                           "Undo a foliage stroke first.")});
      const auto result =
          context.foliage_density_history->redo(*context.foliage_density);
      if (!result)
        return make_response(ExitCode::ValidationFailed, result.error().message,
                             {}, {result.error()});
      reload_foliage_now();
      return make_response(ExitCode::Success, "Foliage density redone");
    }
    if (auto missing = require_edits())
      return *missing;
    if (context.terrain_history->redo_size() == 0)
      return make_response(
          ExitCode::ValidationFailed, "No terrain height redo", {},
          {session_error("TERRAIN-REDO-EMPTY", "No height strokes to redo.",
                         "Undo a height stroke first.")});
    const auto result = context.terrain_history->redo(*context.terrain_edits);
    if (!result)
      return make_response(ExitCode::ValidationFailed, result.error().message,
                           {}, {result.error()});
    reload_now(true);
    return make_response(ExitCode::Success, "Terrain height redone");
  }

  if (action == "save")
    return save_terrain();

  return make_response(
      ExitCode::InvalidArguments, "Unsupported terrain action: " + action, {},
      {session_error("TERRAIN-ACTION-UNKNOWN",
                     "Unsupported terrain action: " + action,
                     "Use "
                     "raise/lower/flatten/set_height/plateau/smooth/terrace/"
                     "gentle_hill/steep_cliff/flatten_pad/canyon/"
                     "carve_channel/raise_banks/set_height_along/grade_along/"
                     "smooth_along/paint/paint_along/"
                     "paint_foliage/paint_foliage_mixed/sample/undo/redo/save/"
                     "batch.")});
}

EditorBridgeResponse execute_editor_operation(EditorSessionContext &context,
                                              const std::string &operation,
                                              const std::string &params_json) {
  try {
    const auto params = parse_params(params_json);
    if (operation == "editor_status") {
      std::map<std::string, std::string> metadata{
          {"editorRunning", context.editor_running ? "true" : "false"},
          {"liveAutomationEnabled",
           context.live_automation_enabled ? "true" : "false"},
          {"worldPath", context.world_path.generic_string()},
          {"projectRoot", context.project_root.generic_string()},
          {"testSession", context.test_session_state},
          {"sceneDirty",
           context.scene_dirty && *context.scene_dirty ? "true" : "false"},
      };
      if (context.selected_entity_id)
        metadata["selectedEntityId"] = *context.selected_entity_id;
      if (context.mcp_jobs) {
        metadata["mcpJobsPending"] =
            std::to_string(context.mcp_jobs->queued_or_running_count());
        metadata["mcpJobsHasWork"] =
            context.mcp_jobs->has_work() ? "true" : "false";
      }
      return make_response(ExitCode::Success, "Editor status", {}, {},
                           std::move(metadata));
    }
    if (operation == "job_call") {
      if (!context.mcp_jobs) {
        return make_response(
            ExitCode::Unavailable, "MCP job queue unavailable", {},
            {session_error(
                "JOB-QUEUE-MISSING",
                "job_call requires a live editor session with an MCP job queue.",
                "Enable live automation in a running editor.")});
      }
      return context.mcp_jobs->handle_call(params);
    }
    if (operation == "scene_plan") {
      const auto plan =
          classify_scene_plan(params.value("description", std::string{}),
                              params.value("targetPath", std::string{}));
      std::map<std::string, std::string> metadata{
          {"targetKind", plan.target_kind},
          {"requiresCompile", plan.requires_compile},
          {"requiresReload", plan.requires_reload},
          {"recommendation", plan.recommendation},
      };
      return make_response(ExitCode::Success, plan.summary, {}, {},
                           std::move(metadata));
    }
    if (operation == "prefab_apply")
      return apply_asset_write(context, params);
    if (operation == "prefab_component_apply") {
      nlohmann::json forwarded = params;
      if (!forwarded.contains("kind"))
        forwarded["kind"] = "prefab";
      return apply_asset_write(context, forwarded);
    }
    if (operation == "asset_apply") {
      if (params.value("action", std::string{}) == "refresh_catalog") {
        const auto refreshed = refresh_prefab_catalog(context);
        if (!refreshed)
          return make_response(ExitCode::ValidationFailed,
                               refreshed.error().message, {},
                               {refreshed.error()});
        return make_response(
            ExitCode::Success, "Asset catalog refreshed", {}, {},
            {{"prefabCount", std::to_string(refreshed.value())}});
      }
      return apply_asset_write(context, params);
    }
    if (operation == "asset_bake") {
      if (params.value("action", std::string{}) == "reload") {
        if (context.pending_mesh_reloads) {
          if (params.contains("meshes") && params["meshes"].is_array()) {
            for (const auto &mesh : params["meshes"]) {
              if (mesh.is_string())
                context.pending_mesh_reloads->insert(mesh.get<std::string>());
            }
          }
          if (context.prefab_meshes_dirty)
            *context.prefab_meshes_dirty = true;
        }
        return make_response(
            ExitCode::Success, "Mesh reload queued", {}, {},
            {{"meshCount",
              std::to_string(
                  params.value("meshes", nlohmann::json::array()).size())}});
      }
      auto response = apply_asset_bake_operation(context.project_root, params);
      if (response.exit_code == ExitCode::Success &&
          context.pending_mesh_reloads) {
        for (const auto &mesh : response.changed_object_ids) {
          context.pending_mesh_reloads->insert(mesh);
        }
        if (context.prefab_meshes_dirty && !response.changed_object_ids.empty())
          *context.prefab_meshes_dirty = true;
      }
      return response;
    }
    if (operation == "terrain_apply") {
      return apply_terrain_operation(context, params);
    }
    if (operation == "water_apply") {
      return apply_water_operation(context, params);
    }
    if (operation == "scene_apply" &&
        params.value("action", std::string{}) == "sample_terrain") {
      nlohmann::json sample_params = params;
      sample_params["action"] = "sample";
      return apply_terrain_operation(context, sample_params);
    }
    // Script/HUD ops stay available without a scene and during play test â€”
    // agents iterate gameplay without rebuilds or physical volume overlaps.
    if (operation == "lua_apply") {
      const auto relative = params.value("path", std::string{});
      const auto source = params.value("source", std::string{});
      if (relative.empty() || source.empty()) {
        return make_response(
            ExitCode::InvalidArguments, "path and source are required", {},
            {session_error("LUA-PAYLOAD-REQUIRED",
                           "Lua apply requires path and source.",
                           "Provide script path and body.")});
      }
      const auto absolute = context.project_root / relative;
      const auto written = write_lua_script_atomic(absolute, source);
      if (!written)
        return make_response(ExitCode::ValidationFailed,
                             written.error().message, {}, {written.error()});
      return make_response(
          ExitCode::Success, "Lua script written", {}, {},
          {{"scriptPath", relative}, {"requiresReload", "lua_hot_reload"}});
    }
    if (operation == "hud_apply") {
      const auto relative = params.value("path", std::string{});
      const auto source = params.value("source", std::string{});
      if (relative.empty() || source.empty()) {
        return make_response(
            ExitCode::InvalidArguments, "path and source are required", {},
            {session_error(
                "HUD-PAYLOAD-REQUIRED",
                "HUD/UI canvas apply requires path and JSON body.",
                "Provide *.uicanvas.json or *.hud.json path and JSON body.")});
      }
      const auto absolute = context.project_root / relative;
      const auto lower = [&] {
        std::string value = relative;
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
      }();
      const bool is_canvas =
          lower.find(".uicanvas.json") != std::string::npos ||
          lower.find(".canvas.json") != std::string::npos;
      const auto written = is_canvas
                               ? write_ui_canvas_json_atomic(absolute, source)
                               : write_hud_json_atomic(absolute, source);
      if (!written)
        return make_response(ExitCode::ValidationFailed,
                             written.error().message, {}, {written.error()});
      return make_response(
          ExitCode::Success,
          is_canvas ? "UI canvas written" : "HUD asset written", {}, {},
          {{"hudPath", relative},
           {"requiresReload", "hud_hot_reload"},
           {"kind", is_canvas ? "ui_canvas" : "hud_asset"}});
    }
    if (operation == "ui_canvas_mutate") {
      const auto relative = params.value("path", std::string{});
      const auto action = params.value("action", std::string{});
      if (relative.empty() || action.empty()) {
        return make_response(
            ExitCode::InvalidArguments, "path and action are required", {},
            {session_error("UICANVAS-MUTATE-ARGS",
                           "ui_canvas_mutate requires path and action.",
                           "Example: "
                           "{\"path\":\"assets/ui/"
                           "player.uicanvas.json\",\"action\":\"move\",\"id\":"
                           "\"player_health\",\"delta\":[8,0]}")});
      }
      nlohmann::json widget_params = params;
      widget_params.erase("path");
      widget_params.erase("action");
      const auto absolute = context.project_root / relative;
      const auto mutated =
          mutate_ui_canvas_file(absolute, action, widget_params.dump());
      if (!mutated)
        return make_response(ExitCode::ValidationFailed,
                             mutated.error().message, {}, {mutated.error()});
      return make_response(
          ExitCode::Success, "UI canvas mutated", {}, {},
          {{"canvasPath", relative},
           {"action", action},
           {"widgetCount", std::to_string(mutated.value().widgets.size())},
           {"requiresReload", "hud_hot_reload"}});
    }
    if (operation == "ui_stack") {
      if (!context.ui_canvas_stack) {
        return make_response(
            ExitCode::Unavailable, "UI canvas stack is not available", {},
            {session_error("UICANVAS-STACK-MISSING",
                           "No live UI canvas stack on the editor session.",
                           "Start the editor with MCP connection enabled.")});
      }
      auto action = params.value("action", std::string{});
      std::transform(
          action.begin(), action.end(), action.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      const auto id = params.value("id", std::string{});
      const auto path = params.value("path", std::string{});
      auto *stack = context.ui_canvas_stack;

      if (action == "register") {
        if (id.empty() || path.empty()) {
          return make_response(
              ExitCode::InvalidArguments,
              "id and path are required for register", {},
              {session_error("UICANVAS-STACK-REGISTER",
                             "register requires id and path.",
                             "Example: "
                             "{\"action\":\"register\",\"id\":\"pause\","
                             "\"path\":\"assets/ui/pause.uicanvas.json\"}")});
        }
        const auto registered =
            stack->register_canvas(id, context.project_root / path);
        if (!registered)
          return make_response(ExitCode::ValidationFailed,
                               registered.error().message, {},
                               {registered.error()});
        if (auto *canvas = stack->find_canvas(id)) {
          if (id == "pause")
            canvas->set_text("pause.title", "PAUSED");
        }
      } else if (action == "push" || action == "show") {
        if (id.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "id is required", {},
              {session_error(
                  "UICANVAS-STACK-ID", "push/show requires canvas id.",
                  "Example: {\"action\":\"push\",\"id\":\"pause\"}")});
        }
        if (!path.empty()) {
          const auto registered =
              stack->register_canvas(id, context.project_root / path);
          if (!registered)
            return make_response(ExitCode::ValidationFailed,
                                 registered.error().message, {},
                                 {registered.error()});
          if (auto *canvas = stack->find_canvas(id)) {
            if (id == "pause")
              canvas->set_text("pause.title", "PAUSED");
          }
        }
        const auto shown = action == "show" ? stack->show(id) : stack->push(id);
        if (!shown)
          return make_response(ExitCode::ValidationFailed,
                               shown.error().message, {}, {shown.error()});
        if (id == "inventory" && context.refresh_inventory_ui)
          context.refresh_inventory_ui();
      } else if (action == "pop") {
        const auto popped = stack->pop();
        if (!popped)
          return make_response(ExitCode::ValidationFailed,
                               popped.error().message, {}, {popped.error()});
      } else if (action == "hide") {
        if (id.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "id is required for hide", {},
              {session_error(
                  "UICANVAS-STACK-ID", "hide requires canvas id.",
                  "Example: {\"action\":\"hide\",\"id\":\"pause\"}")});
        }
        const auto hidden = stack->hide(id);
        if (!hidden)
          return make_response(ExitCode::ValidationFailed,
                               hidden.error().message, {}, {hidden.error()});
      } else if (action == "clear" || action == "clear_modals") {
        stack->clear_modals();
      } else if (action == "status" || action.empty()) {
        // fall through to status metadata
      } else {
        return make_response(
            ExitCode::InvalidArguments, "Unknown ui_stack action", {},
            {session_error(
                "UICANVAS-STACK-ACTION", "Unsupported action: " + action,
                "Use register, push, pop, show, hide, clear, or status.")});
      }

      std::string top;
      if (const auto top_id = stack->top_modal())
        top = *top_id;
      std::string stack_csv;
      for (const auto &entry : stack->modal_ids()) {
        if (!stack_csv.empty())
          stack_csv += ',';
        stack_csv += entry;
      }
      return make_response(
          ExitCode::Success, "UI canvas stack updated", {}, {},
          {{"action", action.empty() ? "status" : action},
           {"id", id},
           {"top", top},
           {"stack", stack_csv},
           {"depth", std::to_string(stack->modal_ids().size())}});
    }
    if (operation == "lua_call") {
      if (!context.lua_runtime) {
        return make_response(
            ExitCode::Unavailable, "Lua runtime is not available", {},
            {session_error("LUA-RUNTIME-MISSING",
                           "No live Lua runtime on the editor session.",
                           "Start the editor with MCP connection enabled.")});
      }
      auto kind = params.value("kind", std::string{});
      std::transform(
          kind.begin(), kind.end(), kind.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      const auto binding_id = params.value("id", std::string{});
      const auto handler_override = params.value("handler", std::string{});
      nlohmann::json payload =
          params.contains("payload") && params["payload"].is_object()
              ? params["payload"]
              : nlohmann::json::object();

      context.lua_runtime->clear_recent_errors();
      if (kind == "interaction" || kind == "interact") {
        if (binding_id.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "id is required for interaction", {},
              {session_error("LUA-CALL-ID",
                             "interaction kind requires binding id.",
                             "Example: "
                             "{\"kind\":\"interaction\",\"id\":\"use_"
                             "campfire\",\"type\":\"enter\"}")});
        }
        InteractionEvent event;
        const auto type = params.value("type", std::string{"enter"});
        if (type == "use" || type == "activate") {
          context.lua_runtime->dispatch_interaction_use(binding_id);
        } else {
          event.type = (type == "exit") ? InteractionEventType::Exit
                                        : InteractionEventType::Enter;
          event.interaction_id = binding_id;
          event.placement_entity_id =
              payload.value("placementEntityId", std::string{"mcp"});
          event.interactor_id =
              payload.value("interactorId", std::string{"player"});
          event.volume_index =
              static_cast<std::uint32_t>(payload.value("volumeIndex", 0));
          context.lua_runtime->dispatch_interaction(event);
        }
      } else if (kind == "combathurt" || kind == "combat_hurt" ||
                 kind == "hurt") {
        if (binding_id.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "id is required for combatHurt", {},
              {session_error(
                  "LUA-CALL-ID", "combatHurt kind requires binding id.",
                  "Example: {\"kind\":\"combatHurt\",\"id\":\"body\"}")});
        }
        CombatContactEvent event;
        event.attacker_id = payload.value("attackerId", std::string{"mcp"});
        event.hurt_placement_entity_id =
            payload.value("hurtPlacementEntityId", std::string{"mcp-target"});
        event.hurt_combat_id = binding_id;
        event.hurt_volume_index =
            static_cast<std::uint32_t>(payload.value("hurtVolumeIndex", 0));
        context.lua_runtime->dispatch_combat_hit(event);
      } else if (kind == "handler" || kind.empty()) {
        const auto handler =
            !handler_override.empty() ? handler_override : binding_id;
        if (handler.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "handler is required", {},
              {session_error("LUA-CALL-HANDLER",
                             "handler kind requires handler name.",
                             "Example: "
                             "{\"kind\":\"handler\",\"handler\":\"on_body_"
                             "hit\",\"payload\":{...}}")});
        }
        const std::string payload_json = payload.dump();
        const auto called =
            context.lua_runtime->call_handler(handler, payload_json);
        if (!called) {
          return make_response(ExitCode::ValidationFailed,
                               called.error().message, {}, {called.error()},
                               {{"handler", handler},
                                {"kind", kind.empty() ? "handler" : kind}});
        }
      } else {
        return make_response(
            ExitCode::InvalidArguments, "Unknown lua_call kind", {},
            {session_error("LUA-CALL-KIND", "Unsupported kind: " + kind,
                           "Use interaction, combatHurt, or handler.")});
      }

      if (!context.lua_runtime->recent_errors().empty()) {
        auto errors = context.lua_runtime->recent_errors();
        const auto message = errors.back().message;
        return make_response(
            ExitCode::ValidationFailed, message, {}, std::move(errors),
            {{"kind", kind.empty() ? "handler" : kind}, {"id", binding_id}});
      }
      return make_response(ExitCode::Success, "Lua handler dispatched", {}, {},
                           {{"kind", kind.empty() ? "handler" : kind},
                            {"id", binding_id},
                            {"handler", handler_override}});
    }
    if (operation == "quest_call") {
      if (!context.quest_runtime) {
        return make_response(
            ExitCode::Unavailable, "Quest runtime is not available", {},
            {session_error("QUEST-RUNTIME-MISSING",
                           "No live QuestRuntime on the editor session.",
                           "Start the editor with MCP connection enabled.")});
      }
      auto kind = params.value("kind", std::string{});
      std::transform(
          kind.begin(), kind.end(), kind.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      for (char &c : kind) {
        if (c == '-' || c == ' ')
          c = '_';
      }
      const auto quest_id =
          params.value("questId", params.value("quest_id", std::string{}));
      const auto objective_id = params.value(
          "objectiveId", params.value("objective_id", std::string{}));

      auto sync_hud = [&]() {
        if (!context.hud_runtime)
          return;
        context.hud_runtime->set_text(
            "quest.objectiveText",
            context.quest_runtime->primary_objective_text());
      };
      auto status_metadata = [&](const QuestProgressStatus &status) {
        std::string completed;
        for (std::size_t i = 0; i < status.completed_objective_ids.size();
             ++i) {
          if (i)
            completed += ',';
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
          if (i)
            ids += ',';
          ids += active[i].quest_id;
        }
        return make_response(ExitCode::Success, "Active quests listed", {}, {},
                             {{"kind", "list"},
                              {"count", std::to_string(active.size())},
                              {"questIds", ids}});
      }
      if (quest_id.empty()) {
        return make_response(
            ExitCode::InvalidArguments, "questId is required", {},
            {session_error(
                "QUEST-CALL-ID",
                "quest_call requires questId (except kind=list).",
                "Example: "
                "{\"kind\":\"start\",\"questId\":\"sq_01_cart_again\"}")});
      }
      if (kind == "start") {
        const auto result = context.quest_runtime->start(quest_id);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"questId", quest_id}});
        }
        sync_hud();
        const auto status = context.quest_runtime->status(quest_id);
        if (!status) {
          return make_response(ExitCode::ValidationFailed,
                               status.error().message, {}, {status.error()},
                               {{"kind", kind}, {"questId", quest_id}});
        }
        return make_response(ExitCode::Success, "Quest started", {}, {},
                             status_metadata(status.value()));
      }
      if (kind == "complete_objective" || kind == "completeobjective") {
        if (objective_id.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "objectiveId is required", {},
              {session_error(
                  "QUEST-CALL-OBJECTIVE",
                  "complete_objective requires objectiveId.",
                  "Example: "
                  "{\"kind\":\"complete_objective\",\"questId\":\"sq_01_cart_"
                  "again\",\"objectiveId\":\"find_pellin\"}")});
        }
        const auto result =
            context.quest_runtime->complete_objective(quest_id, objective_id);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind},
                                {"questId", quest_id},
                                {"objectiveId", objective_id}});
        }
        sync_hud();
        const auto status = context.quest_runtime->status(quest_id);
        if (!status) {
          return make_response(ExitCode::ValidationFailed,
                               status.error().message, {}, {status.error()},
                               {{"kind", kind}, {"questId", quest_id}});
        }
        return make_response(ExitCode::Success, "Objective completed", {}, {},
                             status_metadata(status.value()));
      }
      if (kind == "abandon") {
        const auto result = context.quest_runtime->abandon(quest_id);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"questId", quest_id}});
        }
        sync_hud();
        const auto status = context.quest_runtime->status(quest_id);
        if (!status) {
          return make_response(ExitCode::ValidationFailed,
                               status.error().message, {}, {status.error()},
                               {{"kind", kind}, {"questId", quest_id}});
        }
        return make_response(ExitCode::Success, "Quest abandoned", {}, {},
                             status_metadata(status.value()));
      }
      if (kind == "resolve_fork" || kind == "resolvefork") {
        if (!context.flag_runtime) {
          return make_response(
              ExitCode::Unavailable, "Flag runtime is not available", {},
              {session_error("FLAG-RUNTIME-MISSING",
                             "No live FlagRuntime on the editor session.",
                             "Start the editor with MCP connection enabled.")});
        }
        const auto fork_id =
            params.value("forkId", params.value("fork_id", std::string{}));
        const auto outcome_flag = params.value(
            "outcomeFlag", params.value("outcome_flag",
                                        params.value("flagId", std::string{})));
        if (fork_id.empty() || outcome_flag.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "forkId and outcomeFlag are required",
              {},
              {session_error(
                  "QUEST-CALL-FORK",
                  "resolve_fork requires questId, forkId, and outcomeFlag.",
                  "Example: "
                  "{\"kind\":\"resolve_fork\",\"questId\":\"mq_act0_"
                  "calrenoth\","
                  "\"forkId\":\"larrell_save_vs_flee\",\"outcomeFlag\":\"act0."
                  "helped_larrell\"}")});
        }
        const auto result = context.quest_runtime->resolve_fork(
            quest_id, fork_id, outcome_flag, *context.flag_runtime);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind},
                                {"questId", quest_id},
                                {"forkId", fork_id},
                                {"outcomeFlag", outcome_flag}});
        }
        return make_response(
            ExitCode::Success, "Quest fork resolved", {}, {},
            {{"kind", kind},
             {"questId", quest_id},
             {"forkId", fork_id},
             {"outcomeFlag", outcome_flag},
             {"hasFlag",
              context.flag_runtime->has(outcome_flag) ? "true" : "false"}});
      }
      if (kind == "status") {
        const auto status = context.quest_runtime->status(quest_id);
        if (!status) {
          return make_response(ExitCode::ValidationFailed,
                               status.error().message, {}, {status.error()},
                               {{"kind", kind}, {"questId", quest_id}});
        }
        return make_response(ExitCode::Success, "Quest status", {}, {},
                             status_metadata(status.value()));
      }
      return make_response(
          ExitCode::InvalidArguments, "Unknown quest_call kind", {},
          {session_error("QUEST-CALL-KIND", "Unsupported kind: " + kind,
                         "Use start, complete_objective, abandon, "
                         "resolve_fork, status, or list.")});
    }
    if (operation == "inventory_call") {
      if (!context.inventory_runtime) {
        return make_response(
            ExitCode::Unavailable, "Inventory runtime is not available", {},
            {session_error("INV-RUNTIME-MISSING",
                           "No live InventoryRuntime on the editor session.",
                           "Start the editor with MCP connection enabled.")});
      }
      auto kind = params.value("kind", std::string{});
      std::transform(
          kind.begin(), kind.end(), kind.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      for (char &c : kind) {
        if (c == '-' || c == ' ')
          c = '_';
      }
      const auto item_id =
          params.value("itemId", params.value("item_id", std::string{}));
      const int count = params.value("count", 1);
      const int slot = params.value("slot", params.value("hotbarSlot", -1));
      const auto region = params.value("region", std::string{});
      const auto equip_slot =
          params.value("equipSlot", params.value("equip_slot", std::string{}));
      const int index = params.value("index", -1);

      auto stack_meta = [](const InventoryStack &stack,
                           const std::string &prefix) {
        return std::map<std::string, std::string>{
            {prefix + "ItemId", stack.item_id},
            {prefix + "Count", std::to_string(stack.count)},
        };
      };
      auto refresh_inv = [&]() {
        if (context.refresh_inventory_ui)
          context.refresh_inventory_ui();
      };

      auto load_archetypes = [&]() -> std::optional<WorldForgeArchetypesAsset> {
        const auto path =
            default_world_forge_archetypes_path(context.project_root);
        if (!std::filesystem::exists(path))
          return std::nullopt;
        auto loaded = WorldForgeArchetypesAsset::load(path);
        if (!loaded)
          return std::nullopt;
        return loaded.value();
      };
      auto resolve_starter_weapon = [&](const std::string &archetype_id) {
        const auto archetypes = load_archetypes();
        return resolve_starter_weapon_item_id(
            archetype_id, archetypes ? &*archetypes : nullptr);
      };
      auto apply_starter_loadout =
          [&](const std::string &archetype_id,
              bool grant_bandages) -> EditorBridgeResponse {
        const auto id = normalize_starter_archetype_id(archetype_id);
        const auto weapon = resolve_starter_weapon(id);
        if (const ItemDef *def = context.inventory_runtime->find_def(weapon);
            !def) {
          return make_response(
              ExitCode::ValidationFailed, "Starter weapon missing from catalog",
              {},
              {session_error("INV-STARTER-WEAPON",
                             "Resolved starter weapon '" + weapon +
                                 "' is not in the item catalog.",
                             "Add the item def or fix starterWeaponItemId on "
                             "the archetype.")},
              {{"kind", "apply_starter"},
               {"archetypeId", id},
               {"weaponItemId", weapon}});
        }
        const auto hotbar = context.inventory_runtime->set_hotbar(0, weapon, 1);
        if (!hotbar) {
          return make_response(ExitCode::ValidationFailed,
                               hotbar.error().message, {}, {hotbar.error()},
                               {{"kind", "apply_starter"},
                                {"archetypeId", id},
                                {"weaponItemId", weapon}});
        }
        if (grant_bandages) {
          (void)context.inventory_runtime->grant(kAct0StarterBandageItemId,
                                                 kAct0StarterBandageCount);
        }
        if (id == "outrider") {
          (void)context.inventory_runtime->grant(
              kAct0StarterArrowItemId, kAct0OutriderStarterArrowCount);
        }
        (void)context.inventory_runtime->select_hotbar(0);
        if (context.hud_runtime)
          context.hud_runtime->apply_archetype_hud(id);
        else if (context.ui_canvas_stack)
          context.ui_canvas_stack->hud().apply_archetype_hud(id);
        if (context.refresh_inventory_ui)
          context.refresh_inventory_ui();
        return make_response(
            ExitCode::Success, "Starter loadout applied", {}, {},
            {{"kind", "apply_starter"},
             {"archetypeId", id},
             {"weaponItemId", weapon},
             {"bandagesGranted",
              grant_bandages ? std::to_string(kAct0StarterBandageCount) : "0"},
             {"arrowsGranted",
              id == "outrider" ? std::to_string(kAct0OutriderStarterArrowCount)
                               : "0"}});
      };

      if (kind == "status") {
        const auto snap = context.inventory_runtime->status();
        std::string bag_ids;
        for (std::size_t i = 0; i < snap.bag.size(); ++i) {
          if (snap.bag[i].empty())
            continue;
          if (!bag_ids.empty())
            bag_ids += ',';
          bag_ids +=
              snap.bag[i].item_id + "x" + std::to_string(snap.bag[i].count);
        }
        std::string hotbar_ids;
        for (std::size_t i = 0; i < snap.hotbar.size(); ++i) {
          if (snap.hotbar[i].empty())
            continue;
          if (!hotbar_ids.empty())
            hotbar_ids += ',';
          hotbar_ids += std::to_string(i) + ":" + snap.hotbar[i].item_id;
        }
        std::string ammo_ids;
        for (std::size_t i = 0; i < snap.ammo.size(); ++i) {
          if (snap.ammo[i].empty())
            continue;
          if (!ammo_ids.empty())
            ammo_ids += ',';
          ammo_ids +=
              snap.ammo[i].item_id + "x" + std::to_string(snap.ammo[i].count);
        }
        std::string container_ids;
        for (std::size_t i = 0; i < snap.container.size(); ++i) {
          if (snap.container[i].empty())
            continue;
          if (!container_ids.empty())
            container_ids += ',';
          container_ids += std::to_string(i) + ":" + snap.container[i].item_id +
                           "x" + std::to_string(snap.container[i].count);
        }
        std::string starter_id = kDefaultPlayTestStarterArchetypeId;
        if (context.play_test_starter_archetype_id &&
            !context.play_test_starter_archetype_id->empty())
          starter_id = *context.play_test_starter_archetype_id;
        starter_id = normalize_starter_archetype_id(starter_id);
        const auto starter_weapon = resolve_starter_weapon(starter_id);
        return make_response(
            ExitCode::Success, "Inventory status", {}, {},
            {{"kind", "status"},
             {"bag", bag_ids},
             {"hotbar", hotbar_ids},
             {"ammo", ammo_ids},
             {"containerId", snap.container_id},
             {"container", container_ids},
             {"selectedHotbar", std::to_string(snap.selected_hotbar)},
             {"gold", std::to_string(snap.gold)},
             {"selectionRegion", snap.ui_selection.region},
             {"selectionIndex", std::to_string(snap.ui_selection.index)},
             {"starterArchetypeId", starter_id},
             {"starterWeaponItemId", starter_weapon},
             {"maxHealth", std::to_string(snap.player_stats.max_health)},
             {"maxStamina", std::to_string(snap.player_stats.max_stamina)},
             {"armor", std::to_string(snap.player_stats.armor)},
             {"strength", std::to_string(snap.player_stats.strength)}});
      }
      if (kind == "set_starter_archetype" || kind == "setstarterarchetype" ||
          kind == "set_archetype" || kind == "setarchetype") {
        auto archetype_id = params.value(
            "archetypeId",
            params.value("archetype_id",
                         params.value("archetype",
                                      params.value("itemId", std::string{}))));
        if (archetype_id.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "archetypeId is required", {},
              {session_error("INV-STARTER-ID",
                             "set_starter_archetype requires archetypeId.",
                             "Example: "
                             "{\"kind\":\"set_starter_archetype\","
                             "\"archetypeId\":\"outrider\"}")});
        }
        const auto id = normalize_starter_archetype_id(archetype_id);
        if (context.play_test_starter_archetype_id)
          *context.play_test_starter_archetype_id = id;
        const bool apply = params.value("apply", context.test_session_active);
        if (apply) {
          auto applied = apply_starter_loadout(id, false);
          if (applied.exit_code != ExitCode::Success)
            return applied;
          applied.metadata["kind"] = "set_starter_archetype";
          applied.metadata["applied"] = "true";
          applied.summary =
              "Play-test starter archetype set and loadout applied";
          return applied;
        }
        return make_response(ExitCode::Success,
                             "Play-test starter archetype set", {}, {},
                             {{"kind", "set_starter_archetype"},
                              {"archetypeId", id},
                              {"weaponItemId", resolve_starter_weapon(id)},
                              {"applied", "false"}});
      }
      if (kind == "apply_starter" || kind == "applystarter" ||
          kind == "grant_starter" || kind == "grantstarter") {
        std::string archetype_id = params.value(
            "archetypeId",
            params.value("archetype_id",
                         params.value("archetype",
                                      params.value("itemId", std::string{}))));
        if (archetype_id.empty() && context.play_test_starter_archetype_id)
          archetype_id = *context.play_test_starter_archetype_id;
        if (archetype_id.empty())
          archetype_id = kDefaultPlayTestStarterArchetypeId;
        const auto id = normalize_starter_archetype_id(archetype_id);
        if (context.play_test_starter_archetype_id)
          *context.play_test_starter_archetype_id = id;
        const bool grant_bandages = params.value(
            "grantBandages", params.value("grant_bandages", false));
        return apply_starter_loadout(id, grant_bandages);
      }
      if (kind == "grant") {
        if (item_id.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "itemId is required", {},
              {session_error("INV-CALL-ID",
                             "inventory_call grant requires itemId.",
                             "Example: "
                             "{\"kind\":\"grant\",\"itemId\":\"field_bandage\","
                             "\"count\":1}")});
        }
        const auto result = context.inventory_runtime->grant(item_id, count);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"itemId", item_id}});
        }
        refresh_inv();
        return make_response(ExitCode::Success, "Item granted", {}, {},
                             {{"kind", kind},
                              {"itemId", item_id},
                              {"count", std::to_string(count)}});
      }
      if (kind == "set_hotbar" || kind == "sethotbar") {
        if (item_id.empty() || slot < 0) {
          return make_response(
              ExitCode::InvalidArguments, "slot and itemId required", {},
              {session_error("INV-CALL-HOTBAR",
                             "set_hotbar requires slot + itemId.",
                             "Example: "
                             "{\"kind\":\"set_hotbar\",\"slot\":0,\"itemId\":"
                             "\"ashfell_arming_sword\"}")});
        }
        const auto result = context.inventory_runtime->set_hotbar(
            slot, item_id, count > 0 ? count : 1);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"itemId", item_id}});
        }
        refresh_inv();
        return make_response(ExitCode::Success, "Hotbar set", {}, {},
                             {{"kind", kind},
                              {"slot", std::to_string(slot)},
                              {"itemId", item_id}});
      }
      if (kind == "set_equip" || kind == "setequip") {
        if (item_id.empty() || equip_slot.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "equipSlot and itemId required", {},
              {session_error(
                  "INV-CALL-EQUIP", "set_equip requires equipSlot + itemId.",
                  "Example: "
                  "{\"kind\":\"set_equip\",\"equipSlot\":\"trinket0\","
                  "\"itemId\":\"vein_iron_pendant\"}")});
        }
        const auto result =
            context.inventory_runtime->set_equip(equip_slot, item_id, 1);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"itemId", item_id}});
        }
        refresh_inv();
        return make_response(
            ExitCode::Success, "Equip set", {}, {},
            {{"kind", kind}, {"equipSlot", equip_slot}, {"itemId", item_id}});
      }
      if (kind == "select_hotbar" || kind == "selecthotbar") {
        if (slot < 0) {
          return make_response(
              ExitCode::InvalidArguments, "slot required", {},
              {session_error(
                  "INV-CALL-SELECT", "select_hotbar requires slot 0..7.",
                  "Example: {\"kind\":\"select_hotbar\",\"slot\":0}")});
        }
        const auto result = context.inventory_runtime->select_hotbar(slot);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}});
        }
        refresh_inv();
        auto meta = stack_meta(context.inventory_runtime->active_hotbar_item(),
                               "active");
        meta["kind"] = kind;
        meta["slot"] = std::to_string(slot);
        return make_response(ExitCode::Success, "Hotbar selected", {}, {},
                             std::move(meta));
      }
      if (kind == "select") {
        const auto result = context.inventory_runtime->select_ui(
            region.empty() ? "none" : region, index, equip_slot);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}});
        }
        return make_response(ExitCode::Success, "Selection updated", {}, {},
                             {{"kind", kind},
                              {"region", region},
                              {"index", std::to_string(index)}});
      }
      if (kind == "equip_selected" || kind == "equipselected") {
        const auto result = context.inventory_runtime->equip_selected();
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}});
        }
        refresh_inv();
        return make_response(ExitCode::Success, "Equipped selection", {}, {},
                             {{"kind", kind}});
      }
      if (kind == "unequip_selected" || kind == "unequipselected") {
        const auto result = context.inventory_runtime->unequip_selected();
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}});
        }
        refresh_inv();
        return make_response(ExitCode::Success, "Unequipped selection", {}, {},
                             {{"kind", kind}});
      }
      if (kind == "move") {
        const auto from_region = params.value(
            "fromRegion", params.value("from_region", std::string{}));
        const int from_index =
            params.value("fromIndex", params.value("from_index", -1));
        const auto from_equip = params.value(
            "fromEquipSlot", params.value("from_equip_slot", std::string{}));
        const auto to_region =
            params.value("toRegion", params.value("to_region", std::string{}));
        const int to_index =
            params.value("toIndex", params.value("to_index", -1));
        const auto to_equip = params.value(
            "toEquipSlot", params.value("to_equip_slot", std::string{}));
        if (from_region.empty() || to_region.empty()) {
          return make_response(
              ExitCode::InvalidArguments,
              "inventory_call move requires fromRegion and toRegion", {},
              {session_error("INV-CALL-MOVE", "Missing from/to region.",
                             "Pass fromRegion/toRegion (bag|hotbar|equip).")},
              {{"kind", kind}});
        }
        const auto result = context.inventory_runtime->move_to(
            from_region, from_index, from_equip, to_region, to_index, to_equip);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}});
        }
        refresh_inv();
        return make_response(ExitCode::Success, "Inventory move applied", {},
                             {},
                             {{"kind", kind},
                              {"fromRegion", from_region},
                              {"toRegion", to_region}});
      }
      if (kind == "open_container" || kind == "opencontainer") {
        const auto container_id = params.value(
            "containerId", params.value("container_id", item_id));
        if (container_id.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "containerId is required", {},
              {session_error("INV-CALL-CONTAINER",
                             "open_container requires containerId.",
                             "Example: {\"kind\":\"open_container\","
                             "\"containerId\":\"combat_weapon_crate\"}")});
        }
        const auto result = context.inventory_runtime->open_container(container_id);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}});
        }
        return make_response(ExitCode::Success, "Container opened", {}, {},
                             {{"kind", kind}, {"containerId", container_id}});
      }
      if (kind == "close_container" || kind == "closecontainer") {
        context.inventory_runtime->close_container();
        return make_response(ExitCode::Success, "Container closed", {}, {},
                             {{"kind", kind}});
      }
      if (kind == "grant_container" || kind == "grantcontainer") {
        if (item_id.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "itemId is required", {},
              {session_error("INV-CALL-ID",
                             "grant_container requires itemId.",
                             "Open a container first, then grant into it.")});
        }
        const auto result =
            context.inventory_runtime->grant_container(item_id, count);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"itemId", item_id}});
        }
        return make_response(ExitCode::Success, "Item granted to container", {},
                             {},
                             {{"kind", kind},
                              {"itemId", item_id},
                              {"count", std::to_string(count)}});
      }
      return make_response(
          ExitCode::InvalidArguments, "Unknown inventory_call kind", {},
          {session_error("INV-CALL-KIND", "Unsupported kind: " + kind,
                         "Use status, grant, set_hotbar, set_equip, "
                         "select_hotbar, select, equip_selected, "
                         "unequip_selected, move, open_container, "
                         "close_container, grant_container, "
                         "set_starter_archetype, apply_starter.")});
    }
    if (operation == "dialogue_call") {
      if (!context.dialogue_runtime) {
        return make_response(
            ExitCode::Unavailable, "Dialogue runtime is not available", {},
            {session_error("DIALOGUE-RUNTIME-MISSING",
                           "No live DialogueRuntime on the editor session.",
                           "Start the editor with MCP connection enabled.")});
      }
      auto kind = params.value("kind", std::string{});
      std::transform(
          kind.begin(), kind.end(), kind.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      for (char &c : kind) {
        if (c == '-' || c == ' ')
          c = '_';
      }
      const auto tree_id =
          params.value("treeId", params.value("tree_id", std::string{}));
      const auto choice_id =
          params.value("choiceId", params.value("choice_id", std::string{}));

      auto sync_dialogue_ui = [&]() {
        DialogueUiSession fallback{};
        DialogueUiSession &session =
            context.lua_runtime ? context.lua_runtime->dialogue_ui_session()
                                : fallback;
        sync_dialogue_canvas(context.ui_canvas_stack, context.dialogue_runtime,
                             session);
      };
      auto present_metadata = [&](const DialoguePresent &present) {
        std::string choice_ids;
        for (std::size_t i = 0; i < present.choices.size(); ++i) {
          if (i)
            choice_ids += ',';
          choice_ids += present.choices[i].id;
        }
        return std::map<std::string, std::string>{
            {"kind", kind},
            {"treeId", present.tree_id},
            {"nodeId", present.node_id},
            {"speakerId", present.speaker_id},
            {"line", present.line},
            {"complete", present.complete ? "true" : "false"},
            {"choiceIds", choice_ids},
            {"choiceCount", std::to_string(present.choices.size())},
        };
      };

      if (kind == "status" || kind == "present") {
        if (context.dialogue_runtime->tree_id().empty()) {
          return make_response(
              ExitCode::Success, "Dialogue inactive", {}, {},
              {{"kind", kind}, {"active", "false"}, {"complete", "false"}});
        }
        const auto present = context.dialogue_runtime->present();
        if (!present) {
          return make_response(ExitCode::ValidationFailed,
                               present.error().message, {}, {present.error()},
                               {{"kind", kind}});
        }
        sync_dialogue_ui();
        auto meta = present_metadata(present.value());
        meta["active"] = present.value().complete ? "false" : "true";
        return make_response(ExitCode::Success, "Dialogue present", {}, {},
                             std::move(meta));
      }
      if (kind == "reset") {
        context.dialogue_runtime->reset();
        sync_dialogue_ui();
        return make_response(ExitCode::Success, "Dialogue reset", {}, {},
                             {{"kind", "reset"}, {"active", "false"}});
      }
      if (kind == "start") {
        if (tree_id.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "treeId is required", {},
              {session_error("DIALOGUE-CALL-TREE",
                             "dialogue_call start requires treeId.",
                             "Example: "
                             "{\"kind\":\"start\",\"treeId\":\"dlg_act0_"
                             "wrathful_conquest\"}")});
        }
        const auto result = context.dialogue_runtime->start(tree_id);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"treeId", tree_id}});
        }
        sync_dialogue_ui();
        const auto present = context.dialogue_runtime->present();
        if (!present) {
          return make_response(ExitCode::ValidationFailed,
                               present.error().message, {}, {present.error()},
                               {{"kind", kind}, {"treeId", tree_id}});
        }
        auto meta = present_metadata(present.value());
        meta["active"] = present.value().complete ? "false" : "true";
        return make_response(ExitCode::Success, "Dialogue started", {}, {},
                             std::move(meta));
      }
      if (kind == "continue") {
        DialogueUiSession fallback{};
        DialogueUiSession &session =
            context.lua_runtime ? context.lua_runtime->dialogue_ui_session()
                                : fallback;
        if (context.ui_canvas_stack) {
          if (HudRuntime *canvas =
                  context.ui_canvas_stack->find_canvas("dialogue")) {
            (void)canvas->skip_typewriter("dialogue.body");
          }
        }
        (void)dialogue_advance_continue(context.ui_canvas_stack,
                                        context.dialogue_runtime, session);
        if (context.dialogue_runtime->tree_id().empty() ||
            context.dialogue_runtime->is_complete()) {
          return make_response(
              ExitCode::Success, "Dialogue continue", {}, {},
              {{"kind", "continue"},
               {"complete", "true"},
               {"active", "false"},
               {"choicesPage", session.choices_page ? "true" : "false"}});
        }
        return make_response(
            ExitCode::Success, "Dialogue continue", {}, {},
            {{"kind", "continue"},
             {"choicesPage", session.choices_page ? "true" : "false"},
             {"active", "true"}});
      }
      if (kind == "choose") {
        if (choice_id.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "choiceId is required", {},
              {session_error(
                  "DIALOGUE-CALL-CHOICE",
                  "dialogue_call choose requires choiceId.",
                  "Example: {\"kind\":\"choose\",\"choiceId\":\"choice_0\"}")});
        }
        DialogueUiSession fallback{};
        DialogueUiSession &session =
            context.lua_runtime ? context.lua_runtime->dialogue_ui_session()
                                : fallback;
        const auto result = dialogue_choose_with_ui(
            context.ui_canvas_stack, context.dialogue_runtime, session,
            context.standing_runtime, context.flag_runtime, choice_id);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"choiceId", choice_id}});
        }
        if (context.dialogue_runtime->tree_id().empty() ||
            context.dialogue_runtime->is_complete()) {
          return make_response(ExitCode::Success, "Dialogue choice applied", {},
                               {},
                               {{"kind", kind},
                                {"choiceId", choice_id},
                                {"complete", "true"},
                                {"active", "false"}});
        }
        const auto present = context.dialogue_runtime->present();
        if (!present) {
          return make_response(ExitCode::ValidationFailed,
                               present.error().message, {}, {present.error()},
                               {{"kind", kind}, {"choiceId", choice_id}});
        }
        auto meta = present_metadata(present.value());
        meta["choiceId"] = choice_id;
        meta["active"] = present.value().complete ? "false" : "true";
        return make_response(ExitCode::Success, "Dialogue choice applied", {},
                             {}, std::move(meta));
      }
      return make_response(
          ExitCode::InvalidArguments, "Unknown dialogue_call kind", {},
          {session_error(
              "DIALOGUE-CALL-KIND", "Unsupported kind: " + kind,
              "Use start, present, status, continue, choose, or reset.")});
    }
    if (operation == "pathfinding_call") {
      auto kind = params.value("kind", std::string{});
      std::transform(
          kind.begin(), kind.end(), kind.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      for (char &c : kind) {
        if (c == '-' || c == ' ')
          c = '_';
      }
      StreamedNavigationField local_field;
      StreamedNavigationField *field =
          context.navigation_field ? context.navigation_field : &local_field;
      auto read_pos = [&](const char *key, const char *alt) -> std::optional<WorldPosition> {
        if (!params.contains(key) && !params.contains(alt))
          return std::nullopt;
        const auto &node = params.contains(key) ? params.at(key) : params.at(alt);
        if (!node.is_object())
          return std::nullopt;
        return WorldPosition{node.value("x", 0.0), node.value("y", 0.0),
                             node.value("z", 0.0)};
      };
      if (kind == "status") {
        return make_response(
            ExitCode::Success, "Navigation field status", {}, {},
            {{"kind", "status"},
             {"loadedCells", std::to_string(field->loaded_cell_count())},
             {"focusX", std::to_string(field->focus_cell().x)},
             {"focusZ", std::to_string(field->focus_cell().z)},
             {"sampleStep", std::to_string(StreamedNavigationField::k_sample_step)}});
      }
      if (kind == "nearest_walkable") {
        const auto query = read_pos("query", "position");
        if (!query) {
          return make_response(
              ExitCode::InvalidArguments, "nearest_walkable needs query {x,y,z}",
              {},
              {session_error("PATH-QUERY", "Missing query position.",
                             "Pass query:{x,y,z}.")});
        }
        const float radius = params.value("radius", params.value("maxSearch", 12.0f));
        (void)field->ensure_loaded_for_query(*query, *query, radius + 8.0f);
        const auto nearest = field->nearest_walkable_point(*query, radius);
        if (!nearest) {
          return make_response(ExitCode::Success, "No walkable sample nearby", {},
                               {},
                               {{"kind", kind}, {"found", "false"}});
        }
        return make_response(
            ExitCode::Success, "Nearest walkable found", {}, {},
            {{"kind", kind},
             {"found", "true"},
             {"x", std::to_string(nearest->x)},
             {"y", std::to_string(nearest->y)},
             {"z", std::to_string(nearest->z)}});
      }
      if (kind == "line_of_walk") {
        const auto from = read_pos("from", "start");
        const auto to = read_pos("to", "goal");
        if (!from || !to) {
          return make_response(
              ExitCode::InvalidArguments, "line_of_walk needs from/to {x,y,z}",
              {},
              {session_error("PATH-QUERY", "Missing from/to.",
                             "Pass from:{x,y,z} and to:{x,y,z}.")});
        }
        (void)field->ensure_loaded_for_query(*from, *to, 16.0f);
        const auto los = field->line_of_walk(*from, *to);
        if (!los) {
          return make_response(ExitCode::ValidationFailed, los.error().message, {},
                               {los.error()}, {{"kind", kind}});
        }
        return make_response(
            ExitCode::Success, "Line of walk evaluated", {}, {},
            {{"kind", kind}, {"clear", los.value() ? "true" : "false"}});
      }
      if (kind == "find_path") {
        const auto from = read_pos("from", "start");
        const auto to = read_pos("to", "goal");
        if (!from || !to) {
          return make_response(
              ExitCode::InvalidArguments, "find_path needs from/to {x,y,z}", {},
              {session_error("PATH-QUERY", "Missing from/to.",
                             "Pass from:{x,y,z} and to:{x,y,z}.")});
        }
        const float snap = params.value("snapRadius", params.value("snap", 12.0f));
        const bool simplify = params.value("simplify", true);
        (void)field->ensure_loaded_for_query(*from, *to, 24.0f);
        const auto planned = field->find_path(*from, *to, snap, simplify);
        if (!planned) {
          return make_response(ExitCode::ValidationFailed, planned.error().message,
                               {}, {planned.error()}, {{"kind", kind}});
        }
        std::string points;
        for (std::size_t i = 0; i < planned.value().points.size(); ++i) {
          if (i)
            points += ';';
          const auto &p = planned.value().points[i];
          points += std::to_string(p.x) + "," + std::to_string(p.y) + "," +
                    std::to_string(p.z);
        }
        return make_response(
            ExitCode::Success,
            planned.value().found ? "Path found" : "No path", {}, {},
            {{"kind", kind},
             {"found", planned.value().found ? "true" : "false"},
             {"pointCount", std::to_string(planned.value().points.size())},
             {"lengthXz", std::to_string(planned.value().length_xz)},
             {"points", points},
             {"loadedCells", std::to_string(field->loaded_cell_count())}});
      }
      return make_response(
          ExitCode::InvalidArguments, "Unknown pathfinding_call kind", {},
          {session_error("PATH-CALL-KIND", "Unsupported kind: " + kind,
                         "Use status, find_path, nearest_walkable, or line_of_walk.")});
    }
    if (operation == "standing_call") {
      if (!context.standing_runtime) {
        return make_response(
            ExitCode::Unavailable, "Standing runtime is not available", {},
            {session_error("STANDING-RUNTIME-MISSING",
                           "No live StandingRuntime on the editor session.",
                           "Start the editor with MCP connection enabled.")});
      }
      auto kind = params.value("kind", std::string{});
      std::transform(
          kind.begin(), kind.end(), kind.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      for (char &c : kind) {
        if (c == '-' || c == ' ')
          c = '_';
      }
      const auto faction_id =
          params.value("factionId", params.value("faction_id", std::string{}));
      auto score_meta = [&](const std::string &id) {
        std::map<std::string, std::string> meta{{"kind", kind},
                                                {"factionId", id}};
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
          if (i)
            ids += ',';
          ids += tracked[i].faction_id + "=" + std::to_string(tracked[i].score);
        }
        return make_response(ExitCode::Success, "Tracked standing listed", {},
                             {},
                             {{"kind", "list"},
                              {"count", std::to_string(tracked.size())},
                              {"scores", ids}});
      }
      if (kind == "lock_in") {
        const auto locked = context.standing_runtime->lock_in_faction();
        if (!locked) {
          return make_response(ExitCode::ValidationFailed,
                               locked.error().message, {}, {locked.error()},
                               {{"kind", kind}});
        }
        return make_response(ExitCode::Success, "Lock-in queried", {}, {},
                             {{"kind", kind}, {"factionId", locked.value()}});
      }
      if (faction_id.empty()) {
        return make_response(
            ExitCode::InvalidArguments, "factionId is required", {},
            {session_error(
                "STANDING-CALL-ID",
                "standing_call requires factionId (except kind=list|lock_in).",
                "Example: "
                "{\"kind\":\"adjust\",\"factionId\":\"cristallo\",\"delta\":"
                "10}")});
      }
      if (kind == "get" || kind == "status") {
        const auto score = context.standing_runtime->get(faction_id);
        if (!score) {
          return make_response(ExitCode::ValidationFailed,
                               score.error().message, {}, {score.error()},
                               {{"kind", kind}, {"factionId", faction_id}});
        }
        return make_response(ExitCode::Success, "Standing score", {}, {},
                             score_meta(faction_id));
      }
      if (kind == "set") {
        const double score = params.value("score", params.value("value", 0.0));
        const auto result = context.standing_runtime->set(faction_id, score);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"factionId", faction_id}});
        }
        return make_response(ExitCode::Success, "Standing set", {}, {},
                             score_meta(faction_id));
      }
      if (kind == "adjust") {
        const double delta = params.value("delta", 0.0);
        const auto result = context.standing_runtime->adjust(faction_id, delta);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"factionId", faction_id}});
        }
        return make_response(ExitCode::Success, "Standing adjusted", {}, {},
                             score_meta(faction_id));
      }
      if (kind == "rank") {
        const auto rank = context.standing_runtime->rank(faction_id);
        if (!rank) {
          return make_response(ExitCode::ValidationFailed, rank.error().message,
                               {}, {rank.error()},
                               {{"kind", kind}, {"factionId", faction_id}});
        }
        return make_response(ExitCode::Success, "Standing rank", {}, {},
                             score_meta(faction_id));
      }
      if (kind == "meets") {
        WorldForgeQuestStandingRequirement req;
        req.faction_id = faction_id;
        if (params.contains("minScore"))
          req.min_score = params.value("minScore", 0.0);
        req.min_rank_id = params.value(
            "minRankId", params.value("min_rank_id", std::string{}));
        const auto meets = context.standing_runtime->meets_requirement(req);
        if (!meets) {
          return make_response(ExitCode::ValidationFailed,
                               meets.error().message, {}, {meets.error()},
                               {{"kind", kind}, {"factionId", faction_id}});
        }
        auto meta = score_meta(faction_id);
        meta["meets"] = meets.value() ? "true" : "false";
        return make_response(ExitCode::Success, "Standing requirement checked",
                             {}, {}, std::move(meta));
      }
      return make_response(
          ExitCode::InvalidArguments, "Unknown standing_call kind", {},
          {session_error(
              "STANDING-CALL-KIND", "Unsupported kind: " + kind,
              "Use get, set, adjust, rank, meets, lock_in, or list.")});
    }
    if (operation == "flag_call") {
      if (!context.flag_runtime) {
        return make_response(
            ExitCode::Unavailable, "Flag runtime is not available", {},
            {session_error("FLAG-RUNTIME-MISSING",
                           "No live FlagRuntime on the editor session.",
                           "Start the editor with MCP connection enabled.")});
      }
      auto kind = params.value("kind", std::string{});
      std::transform(
          kind.begin(), kind.end(), kind.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      for (char &c : kind) {
        if (c == '-' || c == ' ')
          c = '_';
      }
      const auto flag_id =
          params.value("flagId", params.value("flag_id", std::string{}));
      if (kind == "list") {
        const auto listed = context.flag_runtime->list();
        std::string ids;
        for (std::size_t i = 0; i < listed.size(); ++i) {
          if (i)
            ids += ',';
          ids += listed[i];
        }
        return make_response(ExitCode::Success, "Flags listed", {}, {},
                             {{"kind", "list"},
                              {"count", std::to_string(listed.size())},
                              {"flagIds", ids}});
      }
      if (flag_id.empty()) {
        return make_response(
            ExitCode::InvalidArguments, "flagId is required", {},
            {session_error(
                "FLAG-CALL-ID", "flag_call requires flagId (except kind=list).",
                "Example: "
                "{\"kind\":\"set\",\"flagId\":\"act0.helped_larrell\"}")});
      }
      if (kind == "set") {
        const auto result = context.flag_runtime->set(flag_id);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"flagId", flag_id}});
        }
        return make_response(
            ExitCode::Success, "Flag set", {}, {},
            {{"kind", kind}, {"flagId", flag_id}, {"has", "true"}});
      }
      if (kind == "clear") {
        const auto result = context.flag_runtime->clear(flag_id);
        if (!result) {
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()},
                               {{"kind", kind}, {"flagId", flag_id}});
        }
        return make_response(
            ExitCode::Success, "Flag cleared", {}, {},
            {{"kind", kind},
             {"flagId", flag_id},
             {"has", context.flag_runtime->has(flag_id) ? "true" : "false"}});
      }
      if (kind == "has" || kind == "get" || kind == "status") {
        return make_response(
            ExitCode::Success, "Flag queried", {}, {},
            {{"kind", kind},
             {"flagId", flag_id},
             {"has", context.flag_runtime->has(flag_id) ? "true" : "false"}});
      }
      return make_response(
          ExitCode::InvalidArguments, "Unknown flag_call kind", {},
          {session_error("FLAG-CALL-KIND", "Unsupported kind: " + kind,
                         "Use set, clear, has, or list.")});
    }
    if (!context.scene || !context.history) {
      return make_response(
          ExitCode::Unavailable, "Editor session is unavailable", {},
          {session_error("EDITOR-SESSION-MISSING",
                         "Scene or command history is not available.",
                         "Launch the editor first.")});
    }
    // Scene place/move/component edits are allowed while a play test runs so
    // authors can inspect and adjust the world mid-session (Scene
    // free-cam/gizmos). Terrain and water still gate on play.
    if (operation == "scene_apply") {
      const auto action = params.value("action", std::string{});
      if (action == "place_marker") {
        nlohmann::json forwarded = params;
        forwarded["action"] = "place";
        if (!forwarded.contains("prefab")) {
          forwarded["prefab"] =
              "assets/prefabs/Scene Assets/camera_marker.prefab.json";
        }
        if (!forwarded.contains("name"))
          forwarded["name"] = "Marker";
        return execute_editor_operation(context, "scene_apply",
                                        forwarded.dump());
      }
      if (action == "stamp_prefabs") {
        if (!params.contains("stamps") || !params["stamps"].is_array() ||
            params["stamps"].empty()) {
          return make_response(
              ExitCode::InvalidArguments, "stamp_prefabs requires stamps", {},
              {session_error(
                  "SCENE-STAMPS-REQUIRED",
                  "stamp_prefabs requires a non-empty stamps array.",
                  "Provide stamps: [{prefab, name, x, z, ...}, ...].")});
        }
        nlohmann::json batch = params;
        batch["action"] = "batch";
        batch["ops"] = nlohmann::json::array();
        for (const auto &stamp : params["stamps"]) {
          if (!stamp.is_object()) {
            return make_response(
                ExitCode::InvalidArguments, "stamp must be an object", {},
                {session_error("SCENE-STAMP-INVALID",
                               "Each stamp must be a JSON object.",
                               "Use {prefab, name, x, z, ...} entries.")});
          }
          nlohmann::json op = stamp;
          op["action"] = "place";
          if (!op.contains("snapToTerrain"))
            op["snapToTerrain"] = params.value("snapToTerrain", true);
          if (!op.contains("transform")) {
            op["transform"] = nlohmann::json::object();
          }
          if (!op["transform"].contains("position") &&
              (op.contains("x") || op.contains("z"))) {
            op["transform"]["position"] = {
                op.value("x", 0.0f), op.value("y", 0.0f), op.value("z", 0.0f)};
          }
          batch["ops"].push_back(std::move(op));
        }
        batch.erase("stamps");
        return apply_scene_batch(context, batch);
      }
      if (action == "stamp_scatter") {
        std::vector<std::string> prefabs;
        if (params.contains("prefabs") && params["prefabs"].is_array()) {
          for (const auto &entry : params["prefabs"]) {
            if (entry.is_string() && !entry.get<std::string>().empty())
              prefabs.push_back(entry.get<std::string>());
          }
        } else if (params.contains("prefab") && params["prefab"].is_string() &&
                   !params["prefab"].get<std::string>().empty()) {
          prefabs.push_back(params["prefab"].get<std::string>());
        }
        if (prefabs.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "stamp_scatter requires prefabs", {},
              {session_error(
                  "SCENE-SCATTER-PREFABS",
                  "stamp_scatter requires prefab or non-empty prefabs[].",
                  "Example: prefabs:[\"assets/prefabs/Scene Assets/"
                  "oak_tall.prefab.json\"]")});
        }
        const int count = params.value("count", 0);
        if (count <= 0 || count > static_cast<int>(k_max_scene_batch_ops)) {
          return make_response(
              ExitCode::InvalidArguments, "stamp_scatter count out of range",
              {},
              {session_error(
                  "SCENE-SCATTER-COUNT",
                  "stamp_scatter count must be 1.." +
                      std::to_string(k_max_scene_batch_ops) + ".",
                  "Reduce count or split densify passes.")});
        }
        float min_x = 0.0f;
        float max_x = 0.0f;
        float min_z = 0.0f;
        float max_z = 0.0f;
        if (params.contains("minX") && params.contains("maxX") &&
            params.contains("minZ") && params.contains("maxZ")) {
          min_x = params["minX"].get<float>();
          max_x = params["maxX"].get<float>();
          min_z = params["minZ"].get<float>();
          max_z = params["maxZ"].get<float>();
        } else if (params.contains("x") && params.contains("z") &&
                   params.contains("radius")) {
          const float cx = params["x"].get<float>();
          const float cz = params["z"].get<float>();
          const float radius = params["radius"].get<float>();
          min_x = cx - radius;
          max_x = cx + radius;
          min_z = cz - radius;
          max_z = cz + radius;
        } else {
          return make_response(
              ExitCode::InvalidArguments, "stamp_scatter requires bounds", {},
              {session_error(
                  "SCENE-SCATTER-BOUNDS",
                  "Provide minX/maxX/minZ/maxZ or x/z/radius.",
                  "Example: minX:40,maxX:160,minZ:20,maxZ:110")});
        }
        if (!(std::isfinite(min_x) && std::isfinite(max_x) &&
              std::isfinite(min_z) && std::isfinite(max_z)) ||
            max_x <= min_x || max_z <= min_z) {
          return make_response(
              ExitCode::InvalidArguments, "stamp_scatter bounds invalid", {},
              {session_error("SCENE-SCATTER-BOUNDS-INVALID",
                             "Scatter AABB must be finite with max>min.",
                             "Check min/max XZ.")});
        }
        const float min_spacing = std::max(0.0f, params.value("minSpacing", 4.0f));
        const float ground_offset = params.value("groundOffset", 0.0f);
        const float scale_min = params.value("scaleMin", 1.0f);
        const float scale_max = params.value("scaleMax", 1.0f);
        const bool yaw_random = params.value("yawRandom", true);
        const bool snap = params.value("snapToTerrain", true);
        const float clear_radius =
            std::max(0.0f, params.value("clearRadius", 0.0f));
        float clear_x = 0.0f;
        float clear_z = 0.0f;
        const bool has_clear =
            clear_radius > 0.0f && params.contains("clearX") &&
            params.contains("clearZ");
        if (has_clear) {
          clear_x = params["clearX"].get<float>();
          clear_z = params["clearZ"].get<float>();
        }
        auto name_prefix = params.value("namePrefix", std::string{"scatter_"});
        if (name_prefix.empty())
          name_prefix = "scatter_";
        const std::uint32_t seed = params.value(
            "seed", static_cast<std::uint32_t>(
                        static_cast<std::uint32_t>(count * 2654435761u) ^
                        static_cast<std::uint32_t>(min_x * 100.0f) ^
                        static_cast<std::uint32_t>(min_z * 100.0f)));
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist_x(min_x, max_x);
        std::uniform_real_distribution<float> dist_z(min_z, max_z);
        std::uniform_real_distribution<float> dist_scale(
            std::min(scale_min, scale_max), std::max(scale_min, scale_max));
        std::uniform_real_distribution<float> dist_yaw(0.0f, 360.0f);
        std::uniform_int_distribution<std::size_t> dist_prefab(
            0, prefabs.size() - 1);

        std::vector<std::array<float, 2>> accepted;
        accepted.reserve(static_cast<std::size_t>(count));
        const int max_attempts = std::max(count * 40, count + 8);
        for (int attempt = 0;
             attempt < max_attempts &&
             static_cast<int>(accepted.size()) < count;
             ++attempt) {
          const float x = dist_x(rng);
          const float z = dist_z(rng);
          if (has_clear) {
            const float dx = x - clear_x;
            const float dz = z - clear_z;
            if (dx * dx + dz * dz < clear_radius * clear_radius)
              continue;
          }
          bool ok = true;
          for (const auto &prior : accepted) {
            const float dx = x - prior[0];
            const float dz = z - prior[1];
            if (dx * dx + dz * dz < min_spacing * min_spacing) {
              ok = false;
              break;
            }
          }
          if (ok)
            accepted.push_back({x, z});
        }
        if (accepted.empty()) {
          return make_response(
              ExitCode::ValidationFailed, "stamp_scatter placed nothing", {},
              {session_error(
                  "SCENE-SCATTER-EMPTY",
                  "Could not fit any stamps for the given bounds/spacing.",
                  "Lower minSpacing, widen bounds, or reduce clearRadius.")});
        }

        nlohmann::json batch = params;
        batch["action"] = "batch";
        batch["ops"] = nlohmann::json::array();
        if (!batch.contains("label"))
          batch["label"] = "stamp-scatter";
        int index = 1;
        for (const auto &p : accepted) {
          const auto &prefab = prefabs[dist_prefab(rng)];
          const float scale = dist_scale(rng);
          const float yaw_deg = yaw_random ? dist_yaw(rng) : 0.0f;
          const float yaw = yaw_deg * 0.01745329251f;
          const float half = yaw * 0.5f;
          const float qy = std::sin(half);
          const float qw = std::cos(half);
          nlohmann::json op{
              {"action", "place"},
              {"prefab", prefab},
              {"name", name_prefix + std::to_string(index++)},
              {"snapToTerrain", snap},
              {"groundOffset",
               ground_offset_for_prefab(prefab, ground_offset, params)},
              {"transform",
               {{"position", {p[0], 0.0f, p[1]}},
                {"rotation", {0.0f, qy, 0.0f, qw}},
                {"scale", {scale, scale, scale}}}}};
          batch["ops"].push_back(std::move(op));
        }
        auto response = apply_scene_batch(context, batch);
        if (response.exit_code == ExitCode::Success) {
          response.summary = "Scattered prefabs onto terrain";
          response.metadata["requestedCount"] = std::to_string(count);
          response.metadata["placedCount"] =
              std::to_string(accepted.size());
          response.metadata["seed"] = std::to_string(seed);
        }
        return response;
      }
      if (action == "stamp_compositions") {
        if (!params.contains("stamps") || !params["stamps"].is_array() ||
            params["stamps"].empty()) {
          return make_response(
              ExitCode::InvalidArguments,
              "stamp_compositions requires stamps", {},
              {session_error(
                  "SCENE-COMPOSITIONS-REQUIRED",
                  "stamp_compositions requires a non-empty stamps array.",
                  "Provide stamps: [{name, parts:[{primitive,...}], x, z, "
                  "...}, ...].")});
        }
        nlohmann::json batch = params;
        batch["action"] = "batch";
        batch["ops"] = nlohmann::json::array();
        nlohmann::json written_prefabs = nlohmann::json::array();
        for (const auto &stamp : params["stamps"]) {
          if (!stamp.is_object()) {
            return make_response(
                ExitCode::InvalidArguments, "stamp must be an object", {},
                {session_error(
                    "SCENE-COMPOSITION-INVALID",
                    "Each stamp must be a JSON object.",
                    "Use {name, parts, transform?/x/z, ...} entries.")});
          }
          if (stamp.contains("prefab")) {
            return make_response(
                ExitCode::InvalidArguments,
                "stamp_compositions cannot set prefab", {},
                {session_error(
                    "SCENE-COMPOSITION-PREFAB",
                    "stamp_compositions synthesizes Graybox prefabs from "
                    "parts; do not pass prefab.",
                    "Use stamp_prefabs for existing kit assets.")});
          }
          if (!stamp.contains("parts")) {
            return make_response(
                ExitCode::InvalidArguments, "stamp requires parts", {},
                {session_error(
                    "SCENE-COMPOSITION-PARTS",
                    "Each stamp_compositions entry requires parts[].",
                    "Provide at least one primitive part.")});
          }
          const auto stamp_name =
              stamp.value("name", std::string{"graybox_composition"});
          auto prefab_path =
              ensure_graybox_prefab_on_disk(context, stamp_name, stamp["parts"]);
          if (!prefab_path) {
            return make_response(ExitCode::ValidationFailed,
                                 prefab_path.error().message, {},
                                 {prefab_path.error()});
          }
          written_prefabs.push_back(prefab_path.value());
          nlohmann::json op = stamp;
          op.erase("parts");
          op["action"] = "place";
          op["prefab"] = prefab_path.value();
          if (!op.contains("name"))
            op["name"] = stamp_name;
          if (!op.contains("snapToTerrain"))
            op["snapToTerrain"] = params.value("snapToTerrain", true);
          if (!op.contains("transform"))
            op["transform"] = nlohmann::json::object();
          if (!op["transform"].contains("position") &&
              (op.contains("x") || op.contains("z"))) {
            op["transform"]["position"] = {
                op.value("x", 0.0f), op.value("y", 0.0f), op.value("z", 0.0f)};
          }
          batch["ops"].push_back(std::move(op));
        }
        batch.erase("stamps");
        auto response = apply_scene_batch(context, batch);
        if (!written_prefabs.empty())
          response.metadata["grayboxPrefabs"] = written_prefabs.dump();
        return response;
      }
      if (action == "undo") {
        const auto result = context.history->undo(*context.scene);
        if (!result)
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()});
        if (context.scene_dirty)
          *context.scene_dirty = true;
        if (context.static_render_cache_dirty)
          *context.static_render_cache_dirty = true;
        return make_response(ExitCode::Success, context.history->last_summary(),
                             context.history->last_changed_object_ids());
      }
      if (action == "redo") {
        const auto result = context.history->redo(*context.scene);
        if (!result)
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()});
        if (context.scene_dirty)
          *context.scene_dirty = true;
        if (context.static_render_cache_dirty)
          *context.static_render_cache_dirty = true;
        return make_response(ExitCode::Success, context.history->last_summary(),
                             context.history->last_changed_object_ids());
      }
      if (action == "save") {
        const auto saved = context.scene->save_atomic(context.world_path);
        if (!saved)
          return make_response(ExitCode::ValidationFailed,
                               saved.error().message, {}, {saved.error()});
        if (context.scene_dirty)
          *context.scene_dirty = false;
        return make_response(
            ExitCode::Success, "World saved", {}, {},
            {{"savedPath", context.world_path.generic_string()}});
      }
      if (action == "place") {
        const auto prefab = params.value("prefab", std::string{});
        if (prefab.empty()) {
          return make_response(ExitCode::InvalidArguments, "prefab is required",
                               {},
                               {session_error("SCENE-PREFAB-REQUIRED",
                                              "place requires prefab path.",
                                              "Provide assets/... path.")});
        }
        auto transform = transform_from_json(params.contains("transform")
                                                 ? params["transform"]
                                                 : nlohmann::json::object());
        apply_terrain_snap(transform, params);
        std::optional<EntityId> requested;
        if (params.contains("entityId")) {
          const auto parsed =
              EntityId::parse(params["entityId"].get<std::string>());
          if (!parsed)
            return make_response(ExitCode::ValidationFailed,
                                 parsed.error().message, {}, {parsed.error()});
          requested = parsed.value();
        }
        std::optional<PrefabAsset> seed;
        if (context.prefab_catalog) {
          if (const auto *asset =
                  find_prefab_in_catalog(*context.prefab_catalog, prefab))
            seed = *asset;
        }
        auto command = std::make_unique<PlaceWorldObjectCommand>(
            params.value("name", std::string{"Placed Object"}), prefab,
            transform, requested,
            params.contains("characterAsset")
                ? std::optional<std::string>(
                      params["characterAsset"].get<std::string>())
                : std::nullopt,
            std::move(seed));
        const auto result =
            context.history->execute(*context.scene, std::move(command));
        if (!result)
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()});
        if (context.scene_dirty)
          *context.scene_dirty = true;
        if (context.static_render_cache_dirty)
          *context.static_render_cache_dirty = true;
        if (context.selected &&
            !context.history->last_changed_object_ids().empty()) {
          if (const auto parsed = EntityId::parse(
                  context.history->last_changed_object_ids().front());
              parsed)
            *context.selected = parsed.value();
        }
        return make_response(ExitCode::Success, context.history->last_summary(),
                             context.history->last_changed_object_ids());
      }
      if (action == "move") {
        const auto parsed = resolve_scene_entity_id(*context.scene, params);
        if (!parsed)
          return make_response(ExitCode::ValidationFailed,
                               parsed.error().message, {}, {parsed.error()});
        auto transform = transform_from_json(params.contains("transform")
                                                 ? params["transform"]
                                                 : nlohmann::json::object());
        apply_terrain_snap(transform, params);
        const auto result = context.history->execute(
            *context.scene, std::make_unique<MoveWorldObjectCommand>(
                                parsed.value(), transform));
        if (!result)
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()});
        if (context.scene_dirty)
          *context.scene_dirty = true;
        if (context.static_render_cache_dirty)
          *context.static_render_cache_dirty = true;
        return make_response(ExitCode::Success, context.history->last_summary(),
                             context.history->last_changed_object_ids());
      }
      if (action == "remove") {
        const auto parsed = resolve_scene_entity_id(*context.scene, params);
        if (!parsed)
          return make_response(ExitCode::ValidationFailed,
                               parsed.error().message, {}, {parsed.error()});
        const auto result = context.history->execute(
            *context.scene,
            std::make_unique<RemoveWorldObjectCommand>(parsed.value()));
        if (!result)
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()});
        if (context.scene_dirty)
          *context.scene_dirty = true;
        if (context.static_render_cache_dirty)
          *context.static_render_cache_dirty = true;
        if (context.selected && *context.selected == parsed.value())
          context.selected->reset();
        return make_response(ExitCode::Success, context.history->last_summary(),
                             context.history->last_changed_object_ids());
      }
      if (action == "rename") {
        const auto parsed = resolve_scene_entity_id(*context.scene, params);
        if (!parsed)
          return make_response(ExitCode::ValidationFailed,
                               parsed.error().message, {}, {parsed.error()});
        std::string new_name;
        if (params.contains("newName") && params["newName"].is_string())
          new_name = params["newName"].get<std::string>();
        else if (params.contains("entityId"))
          new_name = params.value("name", std::string{"Renamed"});
        else {
          return make_response(
              ExitCode::InvalidArguments,
              "rename by name requires newName", {},
              {session_error(
                  "SCENE-RENAME-NAME",
                  "rename by name requires newName (name is the lookup).",
                  "Provide {name,newName} or {entityId,name}.")});
        }
        if (new_name.empty())
          new_name = "Renamed";
        const auto result = context.history->execute(
            *context.scene, std::make_unique<RenameEntityCommand>(
                                parsed.value(), std::move(new_name)));
        if (!result)
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()});
        if (context.scene_dirty)
          *context.scene_dirty = true;
        if (context.static_render_cache_dirty)
          *context.static_render_cache_dirty = true;
        return make_response(ExitCode::Success, context.history->last_summary(),
                             context.history->last_changed_object_ids());
      }
      if (action == "list" || action == "query") {
        const auto name_prefix = params.value("namePrefix", std::string{});
        const auto contains = params.value("contains", std::string{});
        const auto exact = params.value("name", std::string{});
        const std::size_t limit = static_cast<std::size_t>(
            std::max(1, params.value("limit", 200)));
        const bool has_float_min = params.contains("floatGapMin");
        const bool has_float_max = params.contains("floatGapMax");
        const float float_gap_min =
            has_float_min ? params["floatGapMin"].get<float>() : 0.0f;
        const float float_gap_max =
            has_float_max ? params["floatGapMax"].get<float>() : 0.0f;
        nlohmann::json entities = nlohmann::json::array();
        std::size_t matched = 0;
        std::size_t scanned = 0;
        for (const auto &id : context.scene->entity_ids()) {
          const auto entity_name = context.scene->name(id);
          const std::string name = entity_name ? *entity_name : std::string{};
          if (!exact.empty() && name != exact)
            continue;
          if (!name_prefix.empty() &&
              name.compare(0, name_prefix.size(), name_prefix) != 0)
            continue;
          if (!contains.empty() && name.find(contains) == std::string::npos)
            continue;
          ++scanned;
          nlohmann::json row{{"entityId", id.str()}, {"name", name}};
          float float_gap = 0.0f;
          if (const auto transform = context.scene->transform(id)) {
            row["x"] = transform->position[0];
            row["y"] = transform->position[1];
            row["z"] = transform->position[2];
            const float terrain_y = sample_terrain_height(
                transform->position[0], transform->position[2]);
            float_gap = transform->position[1] - terrain_y;
            row["terrainY"] = terrain_y;
            row["floatGap"] = float_gap;
          }
          if (const auto placement = context.scene->placement(id))
            row["prefab"] = placement->prefab_asset;
          if (has_float_min && float_gap < float_gap_min)
            continue;
          if (has_float_max && float_gap > float_gap_max)
            continue;
          entities.push_back(std::move(row));
          ++matched;
          if (matched >= limit)
            break;
        }
        return make_response(
            ExitCode::Success,
            has_float_min || has_float_max ? "Scene float audit listed"
                                           : "Scene entities listed",
            {}, {},
            {{"count", std::to_string(matched)},
             {"scanned", std::to_string(scanned)},
             {"entitiesJson", entities.dump()}});
      }
      if (action == "snap_to_terrain") {
        std::vector<EntityId> targets;
        const bool all = params.value("all", false);
        const auto name_prefix = params.value("namePrefix", std::string{});
        std::vector<std::string> names;
        if (params.contains("names") && params["names"].is_array()) {
          for (const auto &entry : params["names"]) {
            if (entry.is_string())
              names.push_back(entry.get<std::string>());
          }
        }
        if (params.contains("name") && params["name"].is_string() &&
            !params["name"].get<std::string>().empty())
          names.push_back(params["name"].get<std::string>());

        auto skipped = [&](const std::string &name) {
          if (params.contains("skipNames") && params["skipNames"].is_array()) {
            for (const auto &entry : params["skipNames"]) {
              if (entry.is_string() && entry.get<std::string>() == name)
                return true;
            }
          }
          if (params.contains("skipNameContains") &&
              params["skipNameContains"].is_array()) {
            for (const auto &entry : params["skipNameContains"]) {
              if (!entry.is_string())
                continue;
              const auto needle = entry.get<std::string>();
              if (!needle.empty() && name.find(needle) != std::string::npos)
                return true;
            }
          } else if (all || !name_prefix.empty()) {
            static const char *k_default_skip[] = {
                "flame", "torch", "smoke", "cam_", "camera"};
            for (const char *needle : k_default_skip) {
              if (name.find(needle) != std::string::npos)
                return true;
            }
          }
          return false;
        };

        if (all || !name_prefix.empty()) {
          for (const auto &id : context.scene->entity_ids()) {
            const auto entity_name = context.scene->name(id);
            const std::string name =
                entity_name ? *entity_name : std::string{};
            if (!name_prefix.empty() &&
                name.compare(0, name_prefix.size(), name_prefix) != 0)
              continue;
            if (skipped(name))
              continue;
            targets.push_back(id);
          }
        }
        for (const auto &name : names) {
          nlohmann::json ref{{"name", name}};
          const auto parsed = resolve_scene_entity_id(*context.scene, ref);
          if (!parsed)
            return make_response(ExitCode::ValidationFailed,
                                 parsed.error().message, {}, {parsed.error()});
          if (!skipped(name))
            targets.push_back(parsed.value());
        }
        if (targets.empty()) {
          return make_response(
              ExitCode::InvalidArguments, "snap_to_terrain matched no entities",
              {},
              {session_error(
                  "SCENE-SNAP-EMPTY",
                  "No entities matched snap_to_terrain filters.",
                  "Provide names[], namePrefix, or all:true.")});
        }

        const float default_offset = params.value("groundOffset", 0.0f);
        std::map<std::string, float> named_offsets;
        if (params.contains("groundOffsets") &&
            params["groundOffsets"].is_object()) {
          for (auto it = params["groundOffsets"].begin();
               it != params["groundOffsets"].end(); ++it) {
            if (it.value().is_number())
              named_offsets[it.key()] = it.value().get<float>();
          }
        }

        nlohmann::json batch_ops = nlohmann::json::array();
        for (const auto &id : targets) {
          const auto transform = context.scene->transform(id);
          if (!transform)
            continue;
          TransformComponent next = *transform;
          float offset = default_offset;
          std::string entity_name_str;
          if (const auto entity_name = context.scene->name(id)) {
            entity_name_str = *entity_name;
            const auto found = named_offsets.find(*entity_name);
            if (found != named_offsets.end())
              offset = found->second;
          }
          if (const auto placement = context.scene->placement(id)) {
            offset = ground_offset_for_prefab(placement->prefab_asset, offset,
                                              params);
            // Named per-entity offsets still win over prefab defaults.
            if (!entity_name_str.empty()) {
              const auto found = named_offsets.find(entity_name_str);
              if (found != named_offsets.end())
                offset = found->second;
            }
          }
          next.position[1] =
              sample_terrain_height(next.position[0], next.position[2]) +
              offset;
          batch_ops.push_back(
              {{"action", "move"},
               {"entityId", id.str()},
               {"transform",
                {{"position",
                  {next.position[0], next.position[1], next.position[2]}},
                 {"rotation",
                  {next.rotation[0], next.rotation[1], next.rotation[2],
                   next.rotation[3]}},
                 {"scale",
                  {next.scale[0], next.scale[1], next.scale[2]}}}}});
        }
        const auto snapped_count = batch_ops.size();
        nlohmann::json batch{{"ops", std::move(batch_ops)},
                             {"label", "snap-to-terrain"},
                             {"save", params.value("save", false)}};
        auto response = apply_scene_batch(context, batch);
        if (response.exit_code == ExitCode::Success) {
          response.summary = "Snapped entities to terrain";
          response.metadata["snappedCount"] = std::to_string(snapped_count);
        }
        return response;
      }
      if (action == "duplicate") {
        const auto parsed =
            EntityId::parse(params.value("entityId", std::string{}));
        if (!parsed)
          return make_response(ExitCode::ValidationFailed,
                               parsed.error().message, {}, {parsed.error()});
        const auto placement = context.scene->placement(parsed.value());
        const auto transform = context.scene->transform(parsed.value());
        const auto name = context.scene->name(parsed.value());
        if (!placement || !transform || !name) {
          return make_response(ExitCode::ValidationFailed,
                               "Selected entity is not a placeable object", {},
                               {session_error("SCENE-DUPLICATE-INVALID",
                                              "Entity has no placement.",
                                              "Select a placed prefab.")});
        }
        TransformComponent offset = *transform;
        offset.position[0] += 1.0f;
        std::optional<PrefabAsset> seed;
        if (context.prefab_catalog) {
          if (const auto *prefab = find_prefab_in_catalog(
                  *context.prefab_catalog, placement->prefab_asset))
            seed = *prefab;
        }
        auto command = std::make_unique<PlaceWorldObjectCommand>(
            *name + " Copy", placement->prefab_asset, offset, std::nullopt,
            placement->character_asset, std::move(seed));
        const auto result =
            context.history->execute(*context.scene, std::move(command));
        if (!result)
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()});
        if (context.scene_dirty)
          *context.scene_dirty = true;
        if (context.static_render_cache_dirty)
          *context.static_render_cache_dirty = true;
        return make_response(ExitCode::Success, context.history->last_summary(),
                             context.history->last_changed_object_ids());
      }
      if (action == "add_component" || action == "remove_component" ||
          action == "set_component") {
        const auto parsed =
            EntityId::parse(params.value("entityId", std::string{}));
        if (!parsed)
          return make_response(ExitCode::ValidationFailed,
                               parsed.error().message, {}, {parsed.error()});
        if (action == "remove_component") {
          const auto component_id =
              params.value("componentId", params.value("id", std::string{}));
          if (component_id.empty()) {
            return make_response(
                ExitCode::InvalidArguments, "componentId is required", {},
                {session_error("SCENE-COMPONENT-ID-REQUIRED",
                               "remove_component requires componentId.",
                               "Provide the component id.")});
          }
          const auto result = context.history->execute(
              *context.scene, std::make_unique<RemoveEntityComponentCommand>(
                                  parsed.value(), component_id));
          if (!result)
            return make_response(ExitCode::ValidationFailed,
                                 result.error().message, {}, {result.error()});
          if (context.scene_dirty)
            *context.scene_dirty = true;
          if (context.static_render_cache_dirty)
            *context.static_render_cache_dirty = true;
          return make_response(ExitCode::Success,
                               context.history->last_summary(),
                               context.history->last_changed_object_ids());
        }
        nlohmann::json entry_json =
            params.contains("component") ? params.at("component") : params;
        if (!entry_json.contains("id") && params.contains("componentId"))
          entry_json["id"] = params["componentId"];
        if (!entry_json.contains("type") && params.contains("type"))
          entry_json["type"] = params["type"];
        if (!entry_json.contains("data") && params.contains("data"))
          entry_json["data"] = params["data"];
        const auto entry =
            authored_component_entry_from_json(entry_json.dump());
        if (!entry)
          return make_response(ExitCode::ValidationFailed,
                               entry.error().message, {}, {entry.error()});
        std::unique_ptr<SceneCommand> command;
        if (action == "add_component")
          command = std::make_unique<AddEntityComponentCommand>(parsed.value(),
                                                                entry.value());
        else
          command = std::make_unique<SetEntityComponentCommand>(parsed.value(),
                                                                entry.value());
        const auto result =
            context.history->execute(*context.scene, std::move(command));
        if (!result)
          return make_response(ExitCode::ValidationFailed,
                               result.error().message, {}, {result.error()});
        if (context.scene_dirty)
          *context.scene_dirty = true;
        if (context.static_render_cache_dirty)
          *context.static_render_cache_dirty = true;
        return make_response(ExitCode::Success, context.history->last_summary(),
                             context.history->last_changed_object_ids());
      }
      if (action == "batch")
        return apply_scene_batch(context, params);
      return make_response(
          ExitCode::InvalidArguments, "Unknown scene action", {},
          {session_error("SCENE-ACTION-UNKNOWN", "Unsupported scene action.",
                         "Use "
                         "place/place_marker/stamp_prefabs/stamp_scatter/"
                         "stamp_compositions/"
                         "move/remove/rename/list/query/snap_to_terrain/"
                         "duplicate/add_component/"
                         "remove_component/set_component/batch/undo/redo/save/"
                         "sample_terrain.")});
    }
    if (operation == "entity_component_apply") {
      nlohmann::json forwarded = params;
      if (!forwarded.contains("action"))
        forwarded["action"] = "add_component";
      // Reuse scene_apply component actions without re-entering this operation.
      const auto action =
          forwarded.value("action", std::string{"add_component"});
      forwarded["action"] = action;
      return execute_editor_operation(context, "scene_apply", forwarded.dump());
    }
    if (operation == "world_forge_apply") {
      return apply_world_forge_operation(context.project_root, params);
    }
    if (operation == "project_git") {
      return apply_project_git_operation(context.project_root, params);
    }
    return make_response(
        ExitCode::InvalidArguments, "Unknown editor operation", {},
        {session_error(
            "EDITOR-OP-UNKNOWN", "Unsupported operation: " + operation,
            "Use "
            "editor_status/editor_session/editor_camera/scene_plan/.../"
            "hud_apply/world_forge_apply/project_git/ui_canvas_mutate/ui_stack/"
            "lua_call/quest_call/inventory_call/dialogue_call/standing_call/"
            "pathfinding_call/flag_call/coop_call/animation_call.")});
  } catch (const std::exception &exception) {
    return make_response(ExitCode::InternalError, "Editor operation failed", {},
                         {session_error("EDITOR-OP-EXCEPTION", exception.what(),
                                        "Check params JSON and retry.")});
  }
}

} // namespace engine
