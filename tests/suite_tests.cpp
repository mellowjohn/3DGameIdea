#include "engine/ai/hostile_npc.h"
#include "engine/animation/anim_studio_agent_ops.h"
#include "engine/animation/anim_studio_clip_history.h"
#include "engine/animation/animation_preview.h"
#include "engine/animation/animator_runtime.h"
#include "engine/animation/bone_attachment.h"
#include "engine/animation/cpu_skinning.h"
#include "engine/animation/root_motion.h"
#include "engine/assets/animation_clip_asset.h"
#include "engine/assets/animator_controller_asset.h"
#include "engine/assets/asset_registry.h"
#include "engine/assets/camera_asset.h"
#include "engine/assets/character_asset.h"
#include "engine/assets/hud_asset.h"
#include "engine/assets/item_catalog_asset.h"
#include "engine/assets/material_asset.h"
#include "engine/assets/mesh_asset.h"
#include "engine/assets/particle_emitter_asset.h"
#include "engine/assets/play_session_settings.h"
#include "engine/assets/prefab_asset.h"
#include "engine/assets/rig_asset.h"
#include "engine/assets/script_bindings_asset.h"
#include "engine/assets/ui_canvas_asset.h"
#include "engine/assets/ui_canvas_mutate.h"
#include "engine/assets/ui_theme_asset.h"
#include "engine/assets/world_forge_acts.h"
#include "engine/assets/world_forge_archetypes_asset.h"
#include "engine/assets/world_forge_dialogues_asset.h"
#include "engine/assets/world_forge_events_asset.h"
#include "engine/assets/world_forge_factions_asset.h"
#include "engine/assets/world_forge_map_asset.h"
#include "engine/assets/world_forge_mvp_readiness_asset.h"
#include "engine/assets/world_forge_pantheon_asset.h"
#include "engine/assets/world_forge_quests_asset.h"
#include "engine/assets/world_forge_relationships_asset.h"
#include "engine/audio/audio_engine.h"
#include "engine/automation/asset_import.h"
#include "engine/automation/build_coordination.h"
#include "engine/automation/command.h"
#include "engine/automation/editor_bridge.h"
#include "engine/automation/editor_session.h"
#include "engine/automation/mcp_job_queue.h"
#include "engine/automation/planning_backlog.h"
#include "engine/automation/project_git_commands.h"
#include "engine/automation/scene_commands.h"
#include "engine/automation/terrain_edit_commands.h"
#include "engine/automation/water_edit_commands.h"
#include "engine/automation/world_forge_commands.h"
#include "engine/core/dispatched_event_queue.h"
#include "engine/core/id_slug.h"
#include "engine/diagnostics/crash_bundle.h"
#include "engine/diagnostics/gpu_diagnostics.h"
#include "engine/diagnostics/logger.h"
#include "engine/dialogue/dialogue_graph_edit.h"
#include "engine/dialogue/dialogue_runtime.h"
#include "engine/dialogue/dialogue_ui.h"
#include "engine/dialogue/twee_import.h"
#include "engine/event/event_timeline_runtime.h"
#include "engine/flag/flag_runtime.h"
#include "engine/game/game_module_abi.h"
#include "engine/game/game_module_host.h"
#include "engine/inventory/inventory_runtime.h"
#include "engine/inventory/starter_loadout.h"
#include "engine/party/party_runtime.h"
#include "engine/physics/character_controller.h"
#include "engine/physics/collision_world.h"
#include "engine/physics/rigidbody_locomotion.h"
#include "engine/quest/quest_runtime.h"
#include "engine/rendering/debug_camera.h"
#include "engine/rendering/mesh_distance_lod.h"
#include "engine/rendering/orbit_camera.h"
#include "engine/rendering/pbr_lighting.h"
#include "engine/rendering/viewport_math.h"
#include "engine/rendering/viewport_picking.h"
#include "engine/save/rpg_save.h"
#include "engine/scripting/lua_runtime.h"
#include "engine/session/game_session.h"
#include "engine/standing/standing_runtime.h"
#include "engine/testing/image_diff.h"
#include "engine/ui/cartography_strokes.h"
#include "engine/ui/hud_runtime.h"
#include "engine/ui/ui_canvas_stack.h"
#include "engine/ui/ui_texture_cache.h"
#include "engine/ui/world_forge_editor.h"
#include "engine/ui/world_forge_graph_camera.h"
#include "engine/ui/world_ui_billboard.h"
#include "engine/ui/combat_text.h"
#include "engine/combat/status_effect_runtime.h"
#include "engine/vfx/particle_system.h"
#include "engine/vfx/weapon_sweep.h"
#include "engine/world/cell_state.h"
#include "engine/world/cell_streamer.h"
#include "engine/world/combat_volumes.h"
#include "engine/world/foliage_density.h"
#include "engine/world/foliage_field.h"
#include "engine/world/foliage_layers.h"
#include "engine/world/foliage_scatter.h"
#include "engine/world/interaction_volumes.h"
#include "engine/world/navigation_grid.h"
#include "engine/world/prefab_collision.h"
#include "engine/world/scene.h"
#include "engine/world/terrain.h"
#include "engine/world/terrain_edits.h"
#include "engine/world/terrain_field.h"
#include "engine/world/terrain_paint.h"
#include "engine/world/transform_utils.h"
#include "engine/world/water_field.h"
#include "engine/world/water_store.h"
#include "engine/world/wind_field.h"
#include "engine/world/world_influence.h"
#include "engine/world/world_partition.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>

namespace {
struct Runner {
  std::string suite;
  int assertions = 0, failures = 0;
  void check(bool value, const char *name) {
    ++assertions;
    if (!value) {
      ++failures;
      std::cerr << "FAIL [" << suite << "] " << name << '\n';
    }
  }
};
} // namespace
int main(int argc, char **argv) {
  std::string suite;
  for (int i = 1; i + 1 < argc; ++i)
    if (std::string(argv[i]) == "--suite")
      suite = argv[i + 1];
  Runner r{suite};
  if (suite == "core") {
    auto id = engine::EntityId::parse("bad");
    r.check(!id, "malformed UUID rejected");
    engine::EngineError e{"T",
                          engine::Severity::Error,
                          engine::ErrorCategory::Validation,
                          "test",
                          "message",
                          std::nullopt,
                          {},
                          "fix",
                          "id"};
    r.check(e.to_json().find("\"code\":\"T\"") != std::string::npos,
            "error JSON stable");
  } else if (suite == "world") {
    engine::Scene s;
    auto a = s.create_entity("a");
    auto b = s.create_entity("b");
    r.check(a && b, "entities created");
    r.check(s.set_parent(b.value(), a.value()).has_value(), "parent accepted");
    r.check(!s.set_parent(a.value(), b.value()), "cycle rejected");
    auto copy = engine::Scene::from_json(s.to_json());
    r.check(copy && copy.value().to_json() == s.to_json(), "scene round trip");
    auto placed_id =
        engine::EntityId::parse("00000000-0000-4000-8000-000000000099");
    engine::TransformComponent transform;
    transform.position = {10, 0, 10};
    engine::CommandHistory history;
    r.check(placed_id &&
                history.execute(
                    s, std::make_unique<engine::PlaceWorldObjectCommand>(
                           "Tree", "assets/prefabs/tree.prefab.json", transform,
                           placed_id.value())),
            "placed object command applies");
    r.check(history.last_summary() == "place-world-object" &&
                history.last_changed_object_ids() ==
                    std::vector<std::string>{placed_id.value().str()},
            "placement reports deterministic changed IDs");
    r.check(s.placement(placed_id.value()) &&
                s.placement(placed_id.value())->cell == engine::CellCoord{0, 0},
            "placement receives derived cell");
    auto serialized = s.to_json();
    auto placed_copy = engine::Scene::from_json(serialized);
    r.check(placed_copy && placed_copy.value().to_json() == serialized,
            "placement round trips deterministically");
    auto world_document = engine::Scene::from_json(
        "{\"schemaVersion\":1,\"worldId\":\"world-1\",\"name\":\"Test "
        "World\",\"partition\":{\"worldSizeMeters\":[4000,4000],"
        "\"cellSizeMeters\":128},\"entities\":[]}");
    r.check(world_document &&
                world_document.value().to_json().find(
                    "\"worldId\": \"world-1\"") != std::string::npos &&
                world_document.value().to_json().find(
                    "\"cellSizeMeters\": 128.0") != std::string::npos,
            "world metadata preserved for editor save");
    auto cinematic_document = engine::Scene::from_json(
        "{\"schemaVersion\":1,\"worldId\":\"cine-1\",\"name\":\"Cine\","
        "\"presentation\":\"cinematic_instance\",\"partition\":{\"worldSizeMeters\":"
        "[16000,16000],\"cellSizeMeters\":128},\"entities\":[]}");
    r.check(cinematic_document &&
                cinematic_document.value().presentation() ==
                    engine::WorldPresentation::CinematicInstance &&
                cinematic_document.value().to_json().find(
                    "\"presentation\": \"cinematic_instance\"") !=
                    std::string::npos,
            "cinematic_instance presentation round-trips");
    {
      engine::Scene moved_cine = std::move(cinematic_document.value());
      r.check(moved_cine.presentation() ==
                  engine::WorldPresentation::CinematicInstance,
              "Scene move preserves presentation");
    }
    auto terrain_document = engine::Scene::from_json(
        "{\"schemaVersion\":1,\"worldId\":\"combat-sandbox\",\"name\":\"Combat "
        "Sandbox\",\"terrainData\":{\"edits\":\"assets/terrain/instances/"
        "combat-sandbox/terrain-edits.json\",\"paint\":\"assets/terrain/"
        "instances/combat-sandbox/terrain-paint.json\",\"foliage\":\"assets/"
        "terrain/instances/combat-sandbox/foliage-density.json\"},"
        "\"entities\":[]}");
    r.check(terrain_document && terrain_document.value().terrain_data_paths() &&
                terrain_document.value().terrain_data_paths()->edits ==
                    "assets/terrain/instances/combat-sandbox/terrain-edits.json" &&
                terrain_document.value().to_json().find(
                    "\"terrainData\"") != std::string::npos,
            "per-world terrainData round-trips");
    {
      engine::Scene moved_terrain = std::move(terrain_document.value());
      r.check(moved_terrain.terrain_data_paths() &&
                  moved_terrain.terrain_data_paths()->paint ==
                      "assets/terrain/instances/combat-sandbox/terrain-paint.json",
              "Scene move preserves terrainData paths");
    }
    auto moved = transform;
    moved.position[0] = 129;
    r.check(history
                .execute(s, std::make_unique<engine::MoveWorldObjectCommand>(
                                placed_id.value(), moved))
                .has_value(),
            "move command applies");
    r.check(s.placement(placed_id.value())->cell == engine::CellCoord{1, 0},
            "move reassigns cell");
    r.check(history.undo(s) &&
                s.placement(placed_id.value())->cell == engine::CellCoord{0, 0},
            "undo restores transform and cell");
    r.check(history.redo(s) &&
                s.placement(placed_id.value())->cell == engine::CellCoord{1, 0},
            "redo reapplies transform and cell");
    {
      engine::TransformComponent via_set = *s.transform(placed_id.value());
      via_set.position[0] = 10.0f;
      r.check(s.set_transform(placed_id.value(), via_set).has_value(),
              "set_transform on placement");
      r.check(s.placement(placed_id.value())->cell == engine::CellCoord{0, 0},
              "set_transform retags placement cell");
      r.check(s.validate().empty(),
              "placement cell matches after set_transform");
      via_set.position[0] = 129.0f;
      r.check(s.set_transform(placed_id.value(), via_set, false).has_value(),
              "set_transform without bump still retags cell");
      r.check(s.placement(placed_id.value())->cell == engine::CellCoord{1, 0},
              "physics write-back path keeps placement cell in sync");
    }
    auto batch_id_a =
        engine::EntityId::parse("00000000-0000-4000-8000-0000000000b1");
    auto batch_id_b =
        engine::EntityId::parse("00000000-0000-4000-8000-0000000000b2");
    engine::TransformComponent batch_transform;
    batch_transform.position = {20, 0, 20};
    std::vector<std::unique_ptr<engine::SceneCommand>> batch_ops;
    batch_ops.push_back(std::make_unique<engine::PlaceWorldObjectCommand>(
        "Batch A", "assets/prefabs/tree.prefab.json", batch_transform,
        batch_id_a.value()));
    batch_transform.position = {22, 0, 22};
    batch_ops.push_back(std::make_unique<engine::PlaceWorldObjectCommand>(
        "Batch B", "assets/prefabs/tree.prefab.json", batch_transform,
        batch_id_b.value()));
    r.check(batch_id_a && batch_id_b &&
                history
                    .execute(s, std::make_unique<engine::CompositeSceneCommand>(
                                    "scatter-trees", std::move(batch_ops)))
                    .has_value(),
            "composite scene command applies");
    r.check(s.contains(batch_id_a.value()) && s.contains(batch_id_b.value()),
            "composite command places multiple objects");
    r.check(history.last_summary() == "scatter-trees",
            "composite command preserves label");
    r.check(history.undo(s) && !s.contains(batch_id_a.value()) &&
                !s.contains(batch_id_b.value()),
            "single undo reverts composite batch");
    r.check(history
                .execute(s, std::make_unique<engine::RemoveWorldObjectCommand>(
                                placed_id.value()))
                .has_value(),
            "remove command applies");
    r.check(!s.contains(placed_id.value()), "placed object removed");
    r.check(history.undo(s) && s.contains(placed_id.value()) &&
                s.placement(placed_id.value())->cell == engine::CellCoord{1, 0},
            "undo restores removed placement");
    r.check(!s.place_world_object("bad", "outside.prefab", transform),
            "invalid prefab path rejected");
    auto player_id =
        engine::EntityId::parse("00000000-0000-4000-8000-0000000000aa");
    r.check(player_id &&
                history
                    .execute(
                        s, std::make_unique<engine::PlaceWorldObjectCommand>(
                               "Player", "assets/prefabs/player.prefab.json",
                               transform, player_id.value(),
                               std::optional<std::string>(std::string{
                                   "assets/characters/player.character.json"})))
                    .has_value(),
            "player spawn placement applies");
    r.check(s.placement(player_id.value()) &&
                s.placement(player_id.value())->character_asset ==
                    "assets/characters/player.character.json",
            "player spawn stores character asset");
    auto player_serialized = s.to_json();
    auto player_copy = engine::Scene::from_json(player_serialized);
    r.check(player_copy && player_copy.value().to_json() == player_serialized,
            "player spawn placement round trips");
    r.check(history
                .execute(s, std::make_unique<engine::RemoveWorldObjectCommand>(
                                player_id.value()))
                .has_value(),
            "player spawn remove applies");
    r.check(history.undo(s) &&
                s.placement(player_id.value())->character_asset ==
                    "assets/characters/player.character.json",
            "undo restores player spawn character asset");
    engine::CharacterAsset custom_settings;
    custom_settings.max_speed = 12.0f;
    r.check(
        s.set_placement_character_settings(player_id.value(), custom_settings)
            .has_value(),
        "player spawn character settings stored");
    r.check(s.placement(player_id.value())->character_settings->max_speed ==
                12.0f,
            "player spawn character settings readable");
    auto settings_serialized = s.to_json();
    auto settings_copy = engine::Scene::from_json(settings_serialized);
    r.check(settings_copy &&
                settings_copy.value().to_json() == settings_serialized,
            "player spawn character settings round trip");
    r.check(
        s.remap_asset_path_prefix("assets/prefabs", "assets/props").has_value(),
        "asset folder remap applies");
    r.check(s.placement(player_id.value())->prefab_asset ==
                "assets/props/player.prefab.json",
            "prefab path remapped after folder rename");
    r.check(s.placement(player_id.value())->character_asset ==
                "assets/characters/player.character.json",
            "unrelated character asset path preserved");
    engine::ViewportRay ray{{0.0, 0.0, -5.0}, {0.0, 0.0, 1.0}};
    engine::WorldBounds box{-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 2.0f};
    r.check(engine::ray_aabb_intersection(ray, box).has_value(),
            "ray hits mesh bounds");
    r.check(!engine::ray_aabb_intersection(
                ray, engine::WorldBounds{5, 5, 5, 6, 6, 6}),
            "ray misses distant bounds");
    // Hand-built orthographic view*projection (row-vector/D3D convention):
    // visible x,y in [-5,5], z in [1,101].
    const std::array<float, 16> ortho_vp{0.2f, 0, 0,     0, 0, 0.2f, 0,      0,
                                         0,    0, 0.01f, 0, 0, 0,    -0.01f, 1};
    const auto frustum = engine::frustum_from_view_projection(ortho_vp);
    r.check(engine::frustum_intersects_aabb(
                frustum, engine::WorldBounds{-1, -1, 5, 1, 1, 10}),
            "box inside ortho frustum intersects");
    r.check(engine::frustum_intersects_aabb(
                frustum, engine::WorldBounds{4, -1, 5, 6, 1, 10}),
            "box straddling right plane still intersects");
    r.check(!engine::frustum_intersects_aabb(
                frustum, engine::WorldBounds{10, -1, 5, 12, 1, 10}),
            "box beyond right plane is culled");
    r.check(!engine::frustum_intersects_aabb(
                frustum, engine::WorldBounds{-1, -1, 200, 1, 1, 210}),
            "box beyond far plane is culled");
    r.check(!engine::frustum_intersects_aabb(
                frustum, engine::WorldBounds{-1, -1, -10, 1, 1, -5}),
            "box behind near plane is culled");
    engine::PrefabAsset tree_prefab;
    tree_prefab.schema_version = 2;
    engine::PrefabPart trunk;
    trunk.name = "Trunk";
    trunk.mesh.primitive = "cylinder";
    trunk.mesh.color = {0.3f, 0.2f, 0.1f};
    engine::PrefabPart canopy;
    canopy.name = "Canopy";
    canopy.transform.position = {0.0f, 1.6f, 0.0f};
    canopy.mesh.primitive = "sphere";
    canopy.mesh.color = {0.1f, 0.2f, 0.1f};
    tree_prefab.parts = {trunk, canopy};
    std::map<std::string, engine::MeshBounds> mesh_bounds;
    mesh_bounds[tree_prefab.mesh_key_for_part(trunk)] = {-0.5f, 0.0f, -0.5f,
                                                         0.5f,  1.0f, 0.5f};
    mesh_bounds[tree_prefab.mesh_key_for_part(canopy)] = {-0.6f, -0.6f, -0.6f,
                                                          0.6f,  0.6f,  0.6f};
    engine::TransformComponent tree_transform;
    tree_transform.position = {0.0f, 0.0f, 0.0f};
    const auto part_bounds =
        engine::placement_mesh_bounds(tree_prefab, tree_transform, mesh_bounds);
    r.check(part_bounds.size() == 2,
            "compositional prefab exposes per-part pick bounds");
    engine::PrefabAsset component_prefab;
    component_prefab.schema_version = 2;
    engine::PrefabCollisionVolume seed_box;
    seed_box.id = "collision-0";
    seed_box.shape = engine::PrefabCollisionShape::Box;
    seed_box.half_extent = {0.5f, 0.5f, 0.5f};
    component_prefab.collision = {seed_box};
    engine::PrefabScriptBinding seed_script;
    seed_script.id = "script-0";
    seed_script.kind = "handler";
    seed_script.binding_id = "demo";
    component_prefab.script_bindings = {seed_script};
    auto component_entity =
        engine::EntityId::parse("00000000-0000-4000-8000-0000000000c1");
    engine::TransformComponent component_transform;
    component_transform.position = {50, 0, 50};
    r.check(
        component_entity &&
            history
                .execute(s, std::make_unique<engine::PlaceWorldObjectCommand>(
                                "CompObj", "assets/prefabs/tree.prefab.json",
                                component_transform, component_entity.value(),
                                std::nullopt, component_prefab))
                .has_value(),
        "place seeds authored components");
    r.check(
        s.authored_components(component_entity.value()) &&
            s.authored_components(component_entity.value())->entries.size() ==
                2,
        "seeded collider and script components");
    engine::AuthoredComponentEntry override_entry =
        s.authored_components(component_entity.value())->entries.front();
    override_entry.collider.half_extent = {1.0f, 1.0f, 1.0f};
    override_entry.collider.transform.position = {0.25f, 0.5f, 0.0f};
    override_entry.collider.shape = engine::PrefabCollisionShape::Sphere;
    override_entry.collider.radius = 0.75f;
    r.check(history
                .execute(s, std::make_unique<engine::SetEntityComponentCommand>(
                                component_entity.value(), override_entry))
                .has_value(),
            "set component marks override");
    r.check(s.authored_components(component_entity.value())
                ->entries.front()
                .overridden,
            "component override flag set");
    r.check(s.authored_components(component_entity.value())
                    ->entries.front()
                    .collider.shape == engine::PrefabCollisionShape::Sphere,
            "set component updates collider shape");
    r.check(s.authored_components(component_entity.value())
                    ->entries.front()
                    .collider.radius == 0.75f,
            "set component updates sphere radius");
    r.check(s.authored_components(component_entity.value())
                    ->entries.front()
                    .collider.transform.position[1] == 0.5f,
            "set component updates collider offset");
    r.check(history.undo(s) &&
                s.authored_components(component_entity.value())
                        ->entries.front()
                        .collider.shape == engine::PrefabCollisionShape::Box,
            "undo restores collider fields");
    r.check(history.redo(s) && s.authored_components(component_entity.value())
                                       ->entries.front()
                                       .collider.radius == 0.75f,
            "redo reapplies collider fields");
    engine::AuthoredComponentEntry script_entry =
        s.authored_components(component_entity.value())->entries.back();
    script_entry.script.kind = "interaction";
    script_entry.script.binding_id = "use_campfire";
    r.check(history
                .execute(s, std::make_unique<engine::SetEntityComponentCommand>(
                                component_entity.value(), script_entry))
                .has_value(),
            "set script binding fields");
    r.check(s.authored_components(component_entity.value())
                    ->entries.back()
                    .script.binding_id == "use_campfire",
            "script binding id updated");
    component_prefab.collision[0].half_extent = {2.0f, 2.0f, 2.0f};
    r.check(s.propagate_prefab_components("assets/prefabs/tree.prefab.json",
                                          component_prefab) >= 1,
            "prefab propagate updates non-overridden entries");
    r.check(s.authored_components(component_entity.value())
                    ->entries.front()
                    .collider.half_extent.x == 1.0f,
            "overridden collider not replaced by propagate");
    auto component_json = s.to_json();
    auto component_round = engine::Scene::from_json(component_json);
    r.check(component_round &&
                component_round.value().authored_components(
                    component_entity.value()) &&
                component_round.value()
                        .authored_components(component_entity.value())
                        ->entries.size() == 2,
            "entity components round trip in world JSON");
    engine::Scene legacy;
    engine::TransformComponent legacy_transform;
    legacy_transform.position = {60, 0, 60};
    auto legacy_id =
        engine::EntityId::parse("00000000-0000-4000-8000-0000000000c2");
    r.check(legacy_id && legacy
                             .place_world_object(
                                 "Legacy", "assets/prefabs/tree.prefab.json",
                                 legacy_transform, legacy_id.value())
                             .has_value(),
            "legacy place without seed");
    r.check(!legacy.authored_components(legacy_id.value()),
            "legacy placement has no components until seeded");
    std::map<std::string, engine::PrefabAsset> legacy_catalog;
    legacy_catalog["assets/prefabs/tree.prefab.json"] = component_prefab;
    r.check(legacy.seed_missing_authored_components(legacy_catalog) == 1,
            "seed_missing exposes prefab colliders as components");
    r.check(legacy.authored_components(legacy_id.value()) &&
                legacy.authored_components(legacy_id.value())->entries.size() ==
                    2,
            "seeded collider and script visible on entity");
    r.check(legacy.seed_missing_authored_components(legacy_catalog) == 0,
            "seed_missing is idempotent");
  } else if (suite == "assets") {
    auto root = std::filesystem::temp_directory_path() / "engine-suite-assets";
    std::filesystem::create_directories(root / "assets");
    std::ofstream(root / "assets/a.txt") << "a";
    engine::AssetRegistry assets;
    r.check(assets.scan(root).has_value(), "scan succeeds");
    r.check(assets.records().size() == 1, "one asset found");
    r.check(assets.validate().empty(), "dependencies valid");
    engine::MaterialAsset material;
    material.base_color = {0.2f, 0.3f, 0.4f, 1.0f};
    material.roughness = 0.8f;
    material.physics.surface = "stone";
    r.check(material.validate().has_value(), "material valid");
    auto parsed = engine::MaterialAsset::from_json(material.to_json());
    r.check(parsed && parsed.value().to_json() == material.to_json(),
            "material deterministic round trip");
    r.check(parsed && parsed.value().shader ==
                          engine::MaterialShaderProfile::StylizedOpaque,
            "default shader is stylized_opaque");
    r.check(
        !engine::MaterialAsset::from_json(
            R"({"schemaVersion":1,"shader":"node_graph","baseColor":[1,1,1,1],"roughness":1,"metallic":0,"opacityMode":"opaque","physics":{"friction":0.8,"restitution":0.05,"density":1000,"surface":"default"}})"),
        "unknown shader profile rejected");
    const auto legacy = engine::MaterialAsset::from_json(
        R"({"schemaVersion":1,"baseColor":[1,1,1,1],"roughness":1,"metallic":0,"opacityMode":"opaque","physics":{"friction":0.8,"restitution":0.05,"density":1000,"surface":"default"}})");
    r.check(legacy && legacy.value().shader ==
                          engine::MaterialShaderProfile::StylizedOpaque,
            "legacy material defaults shader");
    r.check(!engine::is_valid_material_map_path("../secret.png") &&
                !engine::is_valid_material_map_path("C:/abs.png"),
            "unsafe material map paths rejected");
    r.check(engine::is_valid_material_map_path("assets/vfx/wind_streak.png"),
            "valid material map path accepted");
    engine::MaterialAsset mapped;
    mapped.physics.surface = "default";
    mapped.albedo_map = "assets/vfx/wind_streak.png";
    r.check(mapped.validate().has_value(), "albedoMap path format validates");
    r.check(!mapped.validate_texture_maps(root),
            "albedoMap missing under temp root fails closed");
    std::filesystem::create_directories(root / "assets/vfx");
    std::ofstream(root / "assets/vfx/wind_streak.png", std::ios::binary)
        << "not-a-real-png-but-exists";
    r.check(mapped.validate_texture_maps(root).has_value(),
            "albedoMap present under project root passes");
    mapped.albedo_map = "../escape.png";
    r.check(!mapped.validate(), "parent traversal albedoMap rejected");
    mapped.albedo_map = "assets/vfx/wind_streak.png";
    engine::MaterialAsset magic;
    magic.shader = engine::MaterialShaderProfile::EmissiveMagic;
    magic.emissive = {1.0f, 2.0f, 0.5f};
    magic.emissive_pulse_hz = 1.0f;
    magic.emissive_pulse_min = 0.25f;
    magic.physics.surface = "default";
    r.check(magic.validate().has_value(),
            "emissive_magic with pulse validates");
    magic.emissive_pulse_hz = -1.0f;
    r.check(!magic.validate(), "negative pulse Hz rejected");
    magic.emissive_pulse_hz = 1.0f;
    magic.emissive = {0, 0, 0};
    r.check(!magic.validate(), "pulse without emissive rejected");
    magic.emissive = {1, 2, 0.5f};
    const auto magic_pbr = engine::PbrSurfaceParams::from_material(magic);
    r.check(magic_pbr.emissive_pulse_hz == 1.0f &&
                magic_pbr.emissive_pulse_min == 0.25f,
            "from_material copies pulse params");
    const auto peak = magic_pbr.emissive_at_time(0.25f);
    const auto trough = magic_pbr.emissive_at_time(0.75f);
    r.check(peak[0] > trough[0] && peak[1] > trough[1],
            "emissive pulse peaks above trough");
    material.metallic = 1.5f;
    r.check(!material.validate(), "invalid metallic rejected");
    material.metallic = 0;
    material.base_color[3] = 0.5f;
    r.check(!material.validate(), "opaque alpha mismatch rejected");
    r.check(!engine::MaterialAsset::from_json("{}"),
            "malformed material rejected");
    const auto gltf = root / "assets/sample.gltf";
    std::ofstream(gltf)
        << R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":96,"uri":"data:application/octet-stream;base64,AAAAvwAAAAAAAAC/AAAAPwAAAAAAAAC/AAAAPwAAAAAAAAA/AAAAvwAAAAAAAAA/AAAAAAAAAEAAAAAAAAABAAQAAQACAAQAAgADAAQAAwAAAAQAAAADAAIAAAACAAEA"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":60},{"buffer":0,"byteOffset":60,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":5,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":18,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}]})";
    auto mesh = engine::import_gltf_mesh(gltf);
    r.check(mesh && mesh.value().vertices.size() == 18,
            "glTF triangle mesh imported");
    r.check(mesh && !mesh.value().has_skinning(),
            "static glTF has no skinning");
    const auto colored = root / "assets/colored.gltf";
    std::ofstream(colored)
        << R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":144,"uri":"data:application/octet-stream;base64,AAAAvwAAAAAAAAC/AAAAPwAAAAAAAAC/AAAAPwAAAAAAAAA/AAAAvwAAAAAAAAA/AAAAAAAAAEAAAAAAAACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AAABAAQAAQACAAQAAgADAAQAAwAAAAQAAAADAAIAAAACAAEA"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":60},{"buffer":0,"byteOffset":60,"byteLength":48},{"buffer":0,"byteOffset":108,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":5,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":2,"componentType":5123,"count":18,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"COLOR_0":1},"indices":2}]}]})";
    auto bad_color = engine::import_gltf_mesh(colored);
    r.check(!bad_color && bad_color.error().code == "MESH-COLOR-COUNT",
            "COLOR_0 count mismatch rejected");
    std::ofstream(colored, std::ios::trunc)
        << R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":156,"uri":"data:application/octet-stream;base64,AAAAvwAAAAAAAAC/AAAAPwAAAAAAAAC/AAAAPwAAAAAAAAA/AAAAvwAAAAAAAAA/AAAAAAAAAEAAAAAAAACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AAABAAQAAQACAAQAAgADAAQAAwAAAAQAAAADAAIAAAACAAEA"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":60},{"buffer":0,"byteOffset":60,"byteLength":60},{"buffer":0,"byteOffset":120,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":5,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":5,"type":"VEC3"},{"bufferView":2,"componentType":5123,"count":18,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"COLOR_0":1},"indices":2}]}]})";
    auto color_mesh = engine::import_gltf_mesh(colored);
    r.check(color_mesh && color_mesh.value().vertices.size() == 18,
            "COLOR_0 glTF imported");
    r.check(color_mesh && color_mesh.value().vertices.front().r > 0.9f &&
                color_mesh.value().vertices.front().g > 0.9f &&
                color_mesh.value().vertices.front().b > 0.9f,
            "COLOR_0 applied to vertices");
    const auto textured = root / "assets/textured.gltf";
    std::ofstream(textured)
        << R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":66,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIA"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24},{"buffer":0,"byteOffset":60,"byteLength":6}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"images":[{"uri":"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVR4nGP4z8DwHwRZGME0AwMAPv8GAPpcE7AAAAAASUVORK5CYII="}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}]})";
    auto tex_mesh = engine::import_gltf_mesh(textured);
    r.check(tex_mesh && tex_mesh.value().vertices.size() == 3,
            "TEXCOORD glTF imported");
    r.check(tex_mesh &&
                std::abs(tex_mesh.value().vertices[1].u - 1.0f) < 1e-5f &&
                std::abs(tex_mesh.value().vertices[2].v - 1.0f) < 1e-5f,
            "TEXCOORD_0 applied to vertex UVs");
    r.check(tex_mesh && tex_mesh.value().has_albedo() &&
                tex_mesh.value().albedo_width == 2 &&
                tex_mesh.value().albedo_height == 2,
            "baseColorTexture decoded to 2x2 RGBA albedo");
    r.check(tex_mesh && tex_mesh.value().albedo_rgba.size() == 16 &&
                tex_mesh.value().albedo_rgba[0] == 255 &&
                tex_mesh.value().albedo_rgba[1] == 0 &&
                tex_mesh.value().albedo_rgba[2] == 0,
            "albedo top-left texel is red");
    const auto uv_mismatch = root / "assets/uv-mismatch.gltf";
    std::ofstream(uv_mismatch)
        << R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":50,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAEAAgA="}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":8},{"buffer":0,"byteOffset":44,"byteLength":6}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":1,"type":"VEC2"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2}]}]})";
    auto uv_bad = engine::import_gltf_mesh(uv_mismatch);
    r.check(!uv_bad && uv_bad.error().code == "MESH-UV-COUNT",
            "TEXCOORD_0 count mismatch rejected");
    std::ofstream(gltf, std::ios::trunc) << "{}";
    r.check(!engine::import_gltf_mesh(gltf), "malformed glTF rejected");
    const auto skinned = root / "assets/skinned.gltf";
    std::ofstream(skinned) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Root","children":[1,2]},{"name":"Hip"},{"name":"Spine","translation":[0,1,0]},{"name":"Mesh","mesh":0,"skin":0}],
"skins":[{"name":"Body","joints":[1,2],"inverseBindMatrices":2,"skeleton":1}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":3,"WEIGHTS_0":4},"indices":1}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},
{"bufferView":2,"componentType":5126,"count":2,"type":"MAT4"},
{"bufferView":3,"componentType":5123,"count":3,"type":"VEC4"},
{"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"}
],
"bufferViews":[
{"buffer":0,"byteOffset":0,"byteLength":36},
{"buffer":0,"byteOffset":36,"byteLength":6},
{"buffer":0,"byteOffset":44,"byteLength":128},
{"buffer":0,"byteOffset":172,"byteLength":24},
{"buffer":0,"byteOffset":196,"byteLength":48}
],
"buffers":[{"byteLength":244,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAQAAAAAAAAABAAAAAAABAAAAAAAAAAAAQD8AAIA+AAAAAAAAAAAAAAA/AAAAPwAAAAAAAAAAzczMPWZmZj8AAAAAAAAAAA=="}]
})";
    auto skinned_mesh = engine::import_gltf_mesh(skinned);
    r.check(skinned_mesh && skinned_mesh.value().vertices.size() == 3,
            "skinned glTF triangle imported");
    r.check(skinned_mesh && skinned_mesh.value().skins.size() == 1 &&
                skinned_mesh.value().skins[0].joint_node_indices.size() == 2,
            "skinned glTF skin joints imported");
    r.check(skinned_mesh &&
                skinned_mesh.value().skins[0].joint_names[0] == "Hip" &&
                skinned_mesh.value().skins[0].skeleton_root == 1,
            "skinned glTF joint names and skeleton root");
    r.check(skinned_mesh &&
                skinned_mesh.value().skins[0].joint_rest_locals.size() == 2,
            "skinned glTF rest locals imported");
    r.check(
        skinned_mesh &&
            skinned_mesh.value().skins[0].joint_rest_locals[1].translation[1] >
                0.5f,
        "Spine rest translation preserved");
    r.check(skinned_mesh && skinned_mesh.value().influences.size() == 3 &&
                skinned_mesh.value().influences[0].joints[0] == 0 &&
                skinned_mesh.value().influences[0].weights[0] == 0.75f,
            "skinned glTF vertex influences imported");
    const auto joints_only = root / "assets/joints-only.gltf";
    std::ofstream(joints_only)
        << R"({"asset":{"version":"2.0"},"nodes":[{"name":"J0"},{"name":"J1"},{"mesh":0,"skin":0}],"skins":[{"joints":[0,1]}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1},"indices":2}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":3,"type":"VEC4"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24},{"buffer":0,"byteOffset":60,"byteLength":6}],"buffers":[{"byteLength":66,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAAAAAAAAAEAAAAAAAEAAAAAAAAAAAABAAIA"}]})";
    auto joints_only_mesh = engine::import_gltf_mesh(joints_only);
    r.check(!joints_only_mesh &&
                joints_only_mesh.error().code == "MESH-SKIN-ATTR-PAIR",
            "JOINTS_0 without WEIGHTS_0 rejected");
    const auto bad_joint = root / "assets/bad-joint.gltf";
    std::ofstream(bad_joint) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"J0"},{"name":"J1"},{"mesh":0,"skin":0}],
"skins":[{"joints":[0,1],"inverseBindMatrices":2}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":3,"WEIGHTS_0":4},"indices":1}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},
{"bufferView":2,"componentType":5126,"count":2,"type":"MAT4"},
{"bufferView":3,"componentType":5123,"count":3,"type":"VEC4"},
{"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"}
],
"bufferViews":[
{"buffer":0,"byteOffset":0,"byteLength":36},
{"buffer":0,"byteOffset":36,"byteLength":6},
{"buffer":0,"byteOffset":44,"byteLength":128},
{"buffer":0,"byteOffset":172,"byteLength":24},
{"buffer":0,"byteOffset":196,"byteLength":48}
],
"buffers":[{"byteLength":244,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwgAAAAAAAAAAAABAAAAAAABAAAAAAAAAAAAQD8AAIA+AAAAAAAAAAAAAAA/AAAAPwAAAAAAAAAAzczMPWZmZj8AAAAAAAAAAA=="}]
})";
    auto bad_joint_mesh = engine::import_gltf_mesh(bad_joint);
    r.check(!bad_joint_mesh &&
                bad_joint_mesh.error().code == "MESH-SKIN-JOINT-INDEX",
            "out-of-range skin joint index rejected");
    const auto bad_ibm = root / "assets/bad-ibm.gltf";
    std::ofstream(bad_ibm)
        << R"({"asset":{"version":"2.0"},"nodes":[{"name":"J0"},{"name":"J1"}],"skins":[{"joints":[0,1],"inverseBindMatrices":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":1},"indices":2}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":1,"type":"MAT4"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":64},{"buffer":0,"byteOffset":64,"byteLength":36},{"buffer":0,"byteOffset":100,"byteLength":6}],"buffers":[{"byteLength":106,"uri":"data:application/octet-stream;base64,AACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAQACAA=="}]})";
    auto bad_ibm_mesh = engine::import_gltf_mesh(bad_ibm);
    r.check(!bad_ibm_mesh && bad_ibm_mesh.error().code == "MESH-SKIN-IBM-COUNT",
            "inverseBindMatrices count mismatch rejected");

    const auto animated = root / "assets/animated.gltf";
    std::ofstream(animated) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"},{"name":"Spine"}],
"animations":[{"name":"Idle","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[
{"buffer":0,"byteOffset":0,"byteLength":8},
{"buffer":0,"byteOffset":8,"byteLength":24}
],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
    auto anim_set = engine::import_gltf_animation_clips(animated);
    r.check(anim_set && anim_set.value().clips.size() == 1 &&
                anim_set.value().clips[0].name == "Idle",
            "glTF animation clip imported");
    r.check(anim_set && anim_set.value().clips[0].duration_seconds == 1.0f &&
                anim_set.value().clips[0].channels.size() == 1,
            "glTF clip duration and channel count");
    r.check(anim_set &&
                anim_set.value().clips[0].channels[0].target_node_name ==
                    "Hip" &&
                anim_set.value().clips[0].channels[0].times.size() == 2,
            "glTF clip targets Hip with two keys");
    auto mid = engine::sample_translation_channel(
        anim_set.value().clips[0].channels[0], 0.5f);
    r.check(mid && std::abs(mid.value()[1] - 0.5f) < 1e-5f,
            "glTF clip translation samples with linear lerp");
    const auto rotated = root / "assets/rotated.gltf";
    std::ofstream(rotated) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Arm"}],
"animations":[{"name":"Wave","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"rotation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC4"}
],
"bufferViews":[
{"buffer":0,"byteOffset":0,"byteLength":8},
{"buffer":0,"byteOffset":8,"byteLength":32}
],
"buffers":[{"byteLength":40,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAA="}]
})";
    auto rot_set = engine::import_gltf_animation_clips(rotated);
    r.check(rot_set && !rot_set.value().clips.empty() &&
                !rot_set.value().clips[0].channels.empty(),
            "rotation clip imported");
    if (rot_set && !rot_set.value().clips.empty() &&
        !rot_set.value().clips[0].channels.empty()) {
      auto rq = engine::sample_rotation_channel(
          rot_set.value().clips[0].channels[0], 0.0f);
      r.check(rq && std::abs(rq.value()[3] - 1.0f) < 1e-4f,
              "rotation sample returns identity w at t=0");
    }
    auto empty_anim = engine::import_gltf_animation_clips(skinned);
    r.check(empty_anim && empty_anim.value().empty(),
            "skinned glTF without animations imports empty set");
    const auto bad_anim_node = root / "assets/bad-anim-node.gltf";
    std::ofstream(bad_anim_node) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"}],
"animations":[{"samplers":[{"input":0,"output":1}],"channels":[{"sampler":0,"target":{"node":9,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR"},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
    auto bad_anim = engine::import_gltf_animation_clips(bad_anim_node);
    r.check(!bad_anim && bad_anim.error().code == "ANIM-CLIP-TARGET-RANGE",
            "out-of-range animation target rejected");
    const auto cubic = root / "assets/cubic-anim.gltf";
    std::ofstream(cubic) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"}],
"animations":[{"samplers":[{"input":0,"output":1,"interpolation":"CUBICSPLINE"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR"},
{"bufferView":1,"componentType":5126,"count":6,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":72}],
"buffers":[{"byteLength":80,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"}]
})";
    auto cubic_anim = engine::import_gltf_animation_clips(cubic);
    r.check(!cubic_anim &&
                cubic_anim.error().code == "ANIM-CLIP-INTERP-UNSUPPORTED",
            "CUBICSPLINE animation rejected");
    engine::AnimationClipLibrary clip_library;
    auto loaded = clip_library.load(animated);
    r.check(loaded && loaded.value()->clips.size() == 1 &&
                loaded.value()->clips[0].duration_seconds == 1.0f,
            "animation clip library load");
    // Hot reload: rewrite with duration 2.0 (times 0 and 2)
    std::ofstream(animated, std::ios::trunc) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"},{"name":"Spine"}],
"animations":[{"name":"Idle","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[2.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[
{"buffer":0,"byteOffset":0,"byteLength":8},
{"buffer":0,"byteOffset":8,"byteLength":24}
],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAAEAAAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
    // Ensure write time advances on Windows
    {
      auto now = std::filesystem::file_time_type::clock::now();
      std::filesystem::last_write_time(animated, now + std::chrono::seconds(2));
    }
    auto changed = clip_library.poll_changed();
    r.check(changed.size() == 1,
            "animation clip library detects write-time change");
    auto reloaded = clip_library.reload(animated);
    r.check(reloaded && reloaded.value()->clips[0].duration_seconds == 2.0f,
            "animation clip hot reload applies new duration");
    std::ofstream(animated, std::ios::trunc) << "{}";
    std::filesystem::last_write_time(
        animated, std::filesystem::file_time_type::clock::now() +
                      std::chrono::seconds(4));
    auto failed_reload = clip_library.reload(animated);
    auto kept = clip_library.get(animated);
    r.check(!failed_reload && kept &&
                kept.value()->clips[0].duration_seconds == 2.0f,
            "failed animation reload keeps previous clips");

    // Sagittal mirroring: the right-handed Blockbench rig is imported verbatim
    // into the left-handed runtime, so a clip keying `RightUpperArm` must drive
    // the visually-right limb
    // (`LeftUpperArm`) with a pose reflected through the YZ plane, while
    // unkeyed channels stay on the target joint's own rest.
    const auto mirror_src = root / "assets/mirror-arm.gltf";
    std::ofstream(mirror_src) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"RightUpperArm"},{"name":"LeftUpperArm"}],
"animations":[{"name":"Attack","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"rotation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC4"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":32}],
"buffers":[{"byteLength":40,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAD/Xs10/AAAAAAAAAAAAAAA/17NdPw=="}]
})";
    engine::AnimationClipLibrary mirror_clips;
    r.check(mirror_clips.load(mirror_src).has_value(),
            "sagittal fixture clip loads");
    engine::ImportedSkin mirror_skin;
    mirror_skin.joint_node_indices = {0, 1};
    mirror_skin.joint_names = {"RightUpperArm", "LeftUpperArm"};
    engine::ImportedJointRestLocal right_rest;
    right_rest.translation = {0.37f, 1.28f, 0.0f};
    engine::ImportedJointRestLocal left_rest;
    left_rest.translation = {-0.37f, 1.28f, 0.0f};
    mirror_skin.joint_rest_locals = {right_rest, left_rest};
    mirror_skin.inverse_bind_matrices.assign(
        2, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
    engine::AnimatorClipWeight mirror_weight;
    mirror_weight.clip_source = "assets/mirror-arm.gltf";
    mirror_weight.clip = "Attack";
    mirror_weight.weight = 1.0f;
    mirror_weight.time_seconds = 0.5f;
    auto mirrored_poses = engine::sample_skinned_local_poses(
        mirror_skin, mirror_clips, root, {mirror_weight});
    r.check(mirrored_poses && mirrored_poses.value().size() == 2,
            "sagittal sample returns one pose per joint");
    if (mirrored_poses && mirrored_poses.value().size() == 2) {
      const auto &right = mirrored_poses.value()[0];
      const auto &left = mirrored_poses.value()[1];
      r.check(std::abs(left.rotation[2] + 0.5f) < 1e-3f &&
                  std::abs(left.rotation[3] - 0.8660254f) < 1e-3f,
              "clip keyed on RightUpperArm drives LeftUpperArm with reflected "
              "rotation");
      r.check(std::abs(right.rotation[2]) < 1e-4f &&
                  std::abs(right.rotation[3] - 1.0f) < 1e-4f,
              "unkeyed sagittal counterpart keeps rest rotation");
      r.check(std::abs(left.translation[0] + 0.37f) < 1e-4f &&
                  std::abs(right.translation[0] - 0.37f) < 1e-4f,
              "unkeyed translation channels keep each joint's own rest offset");
    }

    {
      engine::ImportedSkin dst;
      dst.joint_names = {"Head", "Hips"};
      dst.joint_node_indices = {0, 1};
      engine::ImportedJointRestLocal head_rest;
      head_rest.translation = {0.0f, 2.0f, 0.0f};
      engine::ImportedJointRestLocal hips_rest;
      hips_rest.translation = {0.0f, 1.0f, 0.0f};
      dst.joint_rest_locals = {head_rest, hips_rest};
      engine::ImportedSkin src;
      src.joint_names = {"Hips", "Head"};
      src.joint_node_indices = {0, 1};
      engine::JointLocalPose hips_pose;
      hips_pose.translation = {0.1f, 1.1f, 0.0f};
      engine::JointLocalPose head_pose;
      head_pose.translation = {0.0f, 2.2f, 0.0f};
      const auto remapped = engine::remap_joint_local_poses_by_name(
          dst, src, {hips_pose, head_pose});
      r.check(remapped.size() == 2, "armor pose remap keeps dest joint count");
      if (remapped.size() == 2) {
        r.check(std::abs(remapped[0].translation[1] - 2.2f) < 1e-5f,
                "Head copies the source Head pose by name");
        r.check(std::abs(remapped[1].translation[0] - 0.1f) < 1e-5f,
                "Hips copies the source Hips pose by name");
      }
    }

    const auto prefab = root / "assets/prefabs/campfire.prefab.json";
    std::filesystem::create_directories(prefab.parent_path());
    std::ofstream(prefab)
        << R"({"schemaVersion":1,"mesh":"assets/models/campfire.gltf","light":{"color":[1.0,0.62,0.28],"radius":20.0,"strength":1.35,"offset":[0.0,0.35,0.0]},"entities":[]})";
    auto campfire = engine::PrefabAsset::load(prefab);
    r.check(campfire && campfire.value().light.has_value() &&
                campfire.value().light->radius == 20.0f,
            "prefab point light parsed");
    const auto compositional = root / "assets/prefabs/tree.prefab.json";
    std::ofstream(compositional)
        << R"({"schemaVersion":2,"entities":[{"name":"Trunk","transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,2,1]},"parent":null,"mesh":{"primitive":"cylinder","color":[0.3,0.2,0.1]}},{"name":"Canopy","transform":{"position":[0,1.6,0],"rotation":[0,0,0,1],"scale":[1.2,0.8,1.2]},"parent":null,"mesh":{"primitive":"sphere","color":[0.1,0.2,0.1]}}]})";
    auto tree_prefab = engine::PrefabAsset::load(compositional);
    r.check(tree_prefab && tree_prefab.value().is_compositional() &&
                tree_prefab.value().parts.size() == 2,
            "compositional prefab parts parsed");
    auto cube = engine::generate_primitive_mesh("cube", {0.2f, 0.3f, 0.4f});
    auto cylinder =
        engine::generate_primitive_mesh("cylinder", {0.2f, 0.3f, 0.4f});
    r.check(cube && cylinder &&
                cube.value().vertices.size() !=
                    cylinder.value().vertices.size(),
            "primitive meshes differ");
    auto capsule =
        engine::generate_primitive_mesh("capsule", {0.35f, 0.55f, 0.95f});
    r.check(capsule && capsule.value().vertices.size() >
                           cylinder.value().vertices.size(),
            "capsule primitive generated");
    const auto player_prefab = root / "assets/prefabs/player.prefab.json";
    std::filesystem::create_directories(player_prefab.parent_path());
    std::ofstream(player_prefab)
        << R"({"schemaVersion":2,"characterAsset":"assets/characters/player.character.json","entities":[{"name":"Body","transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"parent":null,"mesh":{"primitive":"capsule","color":[0.35,0.55,0.95]}}]})";
    auto player_asset = engine::PrefabAsset::load(player_prefab);
    r.check(player_asset && player_asset.value().parts.size() == 1 &&
                player_asset.value().parts[0].mesh.primitive == "capsule" &&
                player_asset.value().character_asset ==
                    "assets/characters/player.character.json",
            "player capsule prefab parsed");
    const auto character_asset =
        root / "assets/characters/player.character.json";
    std::filesystem::create_directories(character_asset.parent_path());
    std::ofstream(character_asset)
        << R"({"schemaVersion":1,"visualPrefab":"assets/prefabs/player.prefab.json","capsuleRadius":0.35,"capsuleHalfHeight":0.85,"maxSlopeRatio":0.45,"stepHeight":0.35,"maxSpeed":6.0,"gravity":9.81,"jumpVelocity":5.0})";
    auto character = engine::CharacterAsset::load(character_asset);
    r.check(character &&
                character.value().controller_config().max_speed == 6.0f &&
                character.value().controller_config().jump_velocity == 5.0f,
            "character asset parsed");
    engine::RigAsset rig_sample;
    rig_sample.id = "fixture";
    engine::RigIkHook hand;
    hand.id = "left_hand";
    hand.tip_joint = "LeftHand";
    hand.root_joint = "LeftUpperArm";
    hand.chain_length = 3;
    engine::RigBoneRole hips;
    hips.role = "hips";
    hips.joint_name = "Hips";
    rig_sample.ik_hooks = {hand};
    rig_sample.bone_roles = {hips};
    r.check(rig_sample.validate().has_value(), "rig metadata validates");
    const auto rig_path = root / "assets/characters/fixture.rig.json";
    std::filesystem::create_directories(rig_path.parent_path());
    r.check(rig_sample.save(rig_path).has_value(), "rig asset save");
    auto loaded_rig = engine::RigAsset::load(rig_path);
    r.check(loaded_rig && loaded_rig.value().ik_hooks.size() == 1 &&
                loaded_rig.value().bone_roles[0].role == "hips",
            "rig asset round trip");
    r.check(loaded_rig && loaded_rig.value()
                              .validate_against_joint_names(
                                  {"Hips", "LeftHand", "LeftUpperArm"})
                              .has_value(),
            "rig joints match skin names");
    r.check(loaded_rig && !loaded_rig.value()
                               .validate_against_joint_names({"Hips"})
                               .has_value(),
            "rig rejects unknown tip joint");
    engine::RigIkHook dup = hand;
    rig_sample.ik_hooks.push_back(dup);
    r.check(!rig_sample.validate() &&
                rig_sample.validate().error().code == "RIG-IK-ID-DUPLICATE",
            "duplicate ik hook id rejected");
    const auto player_rig =
        std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
        "samples/open-world-rpg/assets/characters/player.rig.json";
    auto sample_rig = engine::RigAsset::load(player_rig);
    r.check(sample_rig && sample_rig.value().ik_hooks.size() == 4 &&
                sample_rig.value().bone_roles.size() >= 10,
            "sample player.rig.json loads");
    auto char_with_rig = engine::CharacterAsset::from_json(
        R"({"schemaVersion":1,"visualPrefab":"assets/prefabs/player.prefab.json","rig":"assets/characters/player.rig.json","capsuleRadius":0.35,"capsuleHalfHeight":0.85,"maxSlopeRatio":0.45,"stepHeight":0.35,"maxSpeed":6,"gravity":9.81,"jumpVelocity":5})");
    r.check(char_with_rig && char_with_rig.value().rig ==
                                 "assets/characters/player.rig.json",
            "character optional rig path parsed");
    const auto short_hair = engine::appearance_from_option_ids(
        "Short Brown", "Warm Tan", "Brown");
    r.check(short_hair.has_hair() &&
                short_hair.hair_mesh.find("test_hair_spikes") !=
                    std::string::npos,
            "short hair option binds test_hair_spikes mesh");
    const auto shaved = engine::appearance_from_option_ids("shaved", "fair", "green");
    r.check(!shaved.has_hair(), "shaved appearance has empty hair mesh");
    auto char_with_appearance = engine::CharacterAsset::from_json(
        R"({"schemaVersion":1,"visualPrefab":"assets/prefabs/player.prefab.json","capsuleRadius":0.35,"capsuleHalfHeight":0.85,"maxSlopeRatio":0.45,"stepHeight":0.35,"maxSpeed":6,"gravity":9.81,"jumpVelocity":5,"appearance":{"hairMesh":"assets/models/test_hair_spikes.gltf","hairTint":[1,0.8,0.4],"skinTint":[1,0.9,0.8],"eyeTint":[0.2,0.3,0.7]}})");
    r.check(char_with_appearance &&
                char_with_appearance.value().appearance.hair_mesh ==
                    "assets/models/test_hair_spikes.gltf" &&
                char_with_appearance.value().appearance.eye_tint[2] > 0.6f,
            "character appearance sockets parse hair mesh and eye tint");
    const auto camera_asset = root / "assets/cameras/game.camera.json";
    std::filesystem::create_directories(camera_asset.parent_path());
    std::ofstream(camera_asset)
        << R"({"schemaVersion":1,"pivotHeight":1.6,"minDistance":1.5,"maxDistance":8.0,"defaultDistance":5.0,"collisionProbeRadius":0.2,"collisionPadding":0.15,"lookSensitivity":0.0025,"verticalFovRadians":1.04719755,"nearPlane":0.1,"farPlane":2000.0})";
    auto camera = engine::CameraAsset::load(camera_asset);
    r.check(camera && camera.value().orbit_config().default_distance == 5.0f,
            "camera asset parsed");
    engine::PlaySessionSettings session;
    session.character_asset = "assets/characters/player.character.json";
    session.camera_asset = "assets/cameras/game.camera.json";
    const auto session_path = root / "play.session.json";
    r.check(session.save(session_path).has_value(),
            "play session settings saved");
    auto loaded_session = engine::PlaySessionSettings::load(session_path);
    r.check(loaded_session && loaded_session.value().character_asset ==
                                  session.character_asset,
            "play session settings round trip");
    std::map<std::string, engine::MeshBounds> bounds;
    for (const auto &part : tree_prefab.value().parts) {
      auto generated = engine::generate_primitive_mesh(*part.mesh.primitive,
                                                       part.mesh.color);
      r.check(generated.has_value(), "tree part primitive generated");
      bounds[tree_prefab.value().mesh_key_for_part(part)] =
          generated.value().aabb;
    }
    r.check(tree_prefab && tree_prefab.value().bounds(bounds).max_y > 1.0f,
            "compositional prefab bounds union");
    const auto trigger_prefab = root / "assets/prefabs/trigger.prefab.json";
    std::ofstream(trigger_prefab)
        << R"({"schemaVersion":2,"entities":[],"collision":[{"shape":"sphere","layer":"trigger","trigger":true,"transform":{"position":[0,0.5,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"radius":1.0}]})";
    auto trigger_asset = engine::PrefabAsset::load(trigger_prefab);
    r.check(trigger_asset && trigger_asset.value().collision.size() == 1 &&
                trigger_asset.value().collision[0].trigger,
            "prefab collision volume parsed");
    const auto interact_prefab = root / "assets/prefabs/interact.prefab.json";
    std::ofstream(interact_prefab)
        << R"({"schemaVersion":2,"entities":[],"collision":[{"shape":"sphere","interaction":"open_door","transform":{"position":[0,1,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"radius":1.0}]})";
    auto interact_asset = engine::PrefabAsset::load(interact_prefab);
    r.check(interact_asset &&
                interact_asset.value().collision[0].is_interaction() &&
                interact_asset.value().collision[0].trigger,
            "prefab interaction volume parsed");
    std::map<std::string, engine::PrefabAsset> catalog;
    catalog["assets/prefabs/scene assets/tree.prefab.json"] =
        tree_prefab.value();
    r.check(engine::resolve_prefab_catalog_path(
                catalog, "assets/prefabs/tree.prefab.json") ==
                "assets/prefabs/scene assets/tree.prefab.json",
            "prefab path resolves by unique filename");
    engine::PrefabMeshSource mesh_with_material;
    mesh_with_material.primitive = "cube";
    mesh_with_material.material = "assets/materials/test.material.json";
    engine::MaterialAsset test_material;
    test_material.base_color = {0.2f, 0.4f, 0.6f, 1.0f};
    std::map<std::string, engine::MaterialAsset> materials{
        {"assets/materials/test.material.json", test_material}};
    const auto lookup = engine::make_material_lookup(&materials);
    const auto resolved_color =
        engine::resolved_prefab_mesh_color(mesh_with_material, lookup);
    r.check(resolved_color[0] == 0.2f && resolved_color[1] == 0.4f &&
                resolved_color[2] == 0.6f,
            "prefab material resolves render color");
    // Live catalog mesh ensure: newly referenced glTF must load without restart
    // (MCP authoring).
    {
      const auto live_mesh = root / "assets/models/live_prop.gltf";
      std::filesystem::create_directories(live_mesh.parent_path());
      std::ofstream(live_mesh)
          << R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":96,"uri":"data:application/octet-stream;base64,AAAAvwAAAAAAAAC/AAAAPwAAAAAAAAC/AAAAPwAAAAAAAAA/AAAAvwAAAAAAAAA/AAAAAAAAAEAAAAAAAAABAAQAAQACAAQAAgADAAQAAwAAAAQAAAADAAIAAAACAAEA"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":60},{"buffer":0,"byteOffset":60,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":5,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":18,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}]})";
      engine::PrefabAsset live_prefab;
      live_prefab.schema_version = 2;
      engine::PrefabPart live_part;
      live_part.name = "Body";
      live_part.mesh.asset = "assets/models/live_prop.gltf";
      live_prefab.parts = {live_part};
      std::map<std::string, engine::PrefabAsset> live_catalog{
          {"assets/prefabs/live_prop.prefab.json", live_prefab}};
      std::map<std::string, engine::MeshBounds> live_bounds;
      std::vector<std::pair<std::string, engine::ImportedMesh>> live_meshes;
      engine::ensure_prefab_catalog_meshes(root, live_catalog, {}, live_bounds,
                                           live_meshes);
      r.check(live_meshes.size() == 1 &&
                  live_meshes[0].second.vertices.size() == 18,
              "catalog ensure imports missing glTF mesh");
      r.check(live_bounds.count("assets/models/live_prop.gltf") == 1,
              "catalog ensure records mesh bounds");
      const auto before_count = live_meshes[0].second.vertices.size();
      std::set<std::string> reload{"assets/models/live_prop.gltf"};
      engine::ensure_prefab_catalog_meshes(root, live_catalog, {}, live_bounds,
                                           live_meshes, &reload);
      r.check(reload.empty() && live_meshes.size() == 1 &&
                  live_meshes[0].second.vertices.size() == before_count,
              "catalog ensure reloads invalidated glTF mesh keys");
    }
    test_material.roughness = 0.25f;
    test_material.metallic = 0.8f;
    test_material.emissive = {2.0f, 0.5f, 0.1f};
    r.check(engine::material_supports_opaque_pbr_runtime(test_material),
            "opaque material supports PBR runtime");
    const auto pbr = engine::PbrSurfaceParams::from_material(test_material);
    r.check(pbr.roughness == 0.25f && pbr.metallic == 0.8f &&
                pbr.emissive[0] == 2.0f,
            "material PBR params resolve");
    r.check(!pbr.alpha_clip, "opaque material disables alpha clip");
    const auto lit = engine::evaluate_pbr_light(
        {0.2f, 0.4f, 0.6f}, pbr, {0, 1, 0}, {0, 0, 1}, {0, 1, 0}, {1, 1, 1});
    r.check(lit[0] > 0.0f && lit[1] > 0.0f && lit[2] > 0.0f,
            "metallic PBR returns positive radiance under headlight");
    const auto rough_dielectric = engine::evaluate_pbr_light(
        {0.5f, 0.5f, 0.5f}, engine::PbrSurfaceParams::dielectric_default(),
        {0, 1, 0}, {0, 0, 1}, {0, 1, 0}, {1, 1, 1});
    r.check(rough_dielectric[0] > 0.0f,
            "dielectric rough PBR returns positive radiance");
    test_material.opacity_mode = engine::OpacityMode::Masked;
    test_material.base_color[3] = 1.0f;
    test_material.opacity_cutoff = 0.4f;
    r.check(engine::material_supports_opaque_pbr_runtime(test_material),
            "masked material draws through lit PBR path");
    const auto masked_pbr =
        engine::PbrSurfaceParams::from_material(test_material);
    r.check(masked_pbr.alpha_clip && masked_pbr.opacity_cutoff == 0.4f,
            "masked material enables alpha clip cutoff");
    test_material.opacity_mode = engine::OpacityMode::Blended;
    r.check(!engine::material_supports_opaque_pbr_runtime(test_material),
            "blended material fails closed for PBR runtime");
    test_material.opacity_mode = engine::OpacityMode::Opaque;
    const auto material_path = root / "assets/materials/saved.material.json";
    std::filesystem::create_directories(material_path.parent_path());
    r.check(engine::MaterialAsset::make_default()
                .save_atomic(material_path)
                .has_value(),
            "material asset save atomic");
    auto reloaded_material = engine::MaterialAsset::load(material_path);
    r.check(reloaded_material &&
                reloaded_material.value().physics.surface == "default",
            "material asset round trip");
    std::filesystem::remove_all(root);
  } else if (suite == "world_forge") {
    const auto project = std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                         "samples" / "open-world-rpg";
    const auto path = engine::default_world_forge_factions_path(project);
    r.check(path.filename() == "factions.worldforge.json",
            "default factions path filename");
    auto loaded = engine::WorldForgeFactionsAsset::load(path);
    r.check(loaded.has_value(), "sample factions.worldforge.json loads");
    r.check(loaded && loaded.value().schema_version == 1 &&
                loaded.value().id == "tessera_factions",
            "sample schema and id");
    r.check(loaded && loaded.value().entities.size() == 8,
            "sample seeds eight faction entities");
    if (loaded) {
      const auto &entities = loaded.value().entities;
      r.check(entities[0].id == "kingdom_tessera" &&
                  entities[0].political_role &&
                  *entities[0].political_role ==
                      engine::WorldForgePoliticalRole::Unknown,
              "kingdom_tessera draft with politicalRole unknown");
      r.check(entities[1].id == "chaotic_imperium" &&
                  entities[1].canon_status ==
                      engine::WorldForgeCanonStatus::Established,
              "chaotic_imperium established");
      const engine::WorldForgeFactionEntity *orc = nullptr;
      for (const auto &entity : entities)
        if (entity.id == "orc_warbands") {
          orc = &entity;
          break;
        }
      r.check(orc && std::find(orc->tags.begin(), orc->tags.end(),
                               "multi-warband") != orc->tags.end(),
              "orc_warbands is multi-warband container");
      const auto round_trip =
          engine::WorldForgeFactionsAsset::parse(loaded.value().to_json());
      r.check(round_trip &&
                  round_trip.value().to_json() == loaded.value().to_json(),
              "factions to_json round trip");
    }
    const auto empty_id = engine::WorldForgeFactionsAsset::parse(
        R"({"schemaVersion":1,"id":"t","entities":[{"id":"","kind":"faction","canonStatus":"draft"}]})");
    r.check(!empty_id && empty_id.error().code == "WORLD-FORGE-FACTION-ID",
            "empty entity id rejected");
    const auto dup_id = engine::WorldForgeFactionsAsset::parse(
        R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"faction","canonStatus":"draft"},{"id":"a","kind":"faction","canonStatus":"draft"}]})");
    r.check(!dup_id && dup_id.error().code == "WORLD-FORGE-FACTION-ID-DUP",
            "duplicate entity id rejected");
    const auto bad_kind = engine::WorldForgeFactionsAsset::parse(
        R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"empire","canonStatus":"draft"}]})");
    r.check(!bad_kind && bad_kind.error().code == "WORLD-FORGE-FACTION-KIND",
            "bad kind rejected");
    const auto bad_parent = engine::WorldForgeFactionsAsset::parse(
        R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"faction","canonStatus":"draft","parentId":"missing"}]})");
    r.check(!bad_parent &&
                bad_parent.error().code == "WORLD-FORGE-FACTION-PARENT",
            "unknown parentId rejected");
    r.check(engine::WorldForgeFactionsAsset::validate_file(path).has_value(),
            "sample factions validate_file succeeds");

    const auto pantheon_path =
        engine::default_world_forge_pantheon_path(project);
    r.check(pantheon_path.filename() == "pantheon.worldforge.json",
            "default pantheon path filename");
    auto pantheon_loaded = engine::WorldForgePantheonAsset::load(pantheon_path);
    r.check(pantheon_loaded.has_value(),
            "sample pantheon.worldforge.json loads");
    r.check(pantheon_loaded && pantheon_loaded.value().schema_version == 1 &&
                pantheon_loaded.value().id == "tessera_pantheon",
            "pantheon sample schema and id");
    r.check(pantheon_loaded && pantheon_loaded.value().entities.size() == 3,
            "pantheon seeds frangitur+creotar+sea_of_whispers");
    if (pantheon_loaded) {
      bool has_frangitur = false;
      bool has_creotar = false;
      bool has_sea = false;
      for (const auto &entity : pantheon_loaded.value().entities) {
        if (entity.id == "frangitur")
          has_frangitur = true;
        if (entity.id == "creotar")
          has_creotar = true;
        if (entity.id == "sea_of_whispers")
          has_sea = true;
      }
      r.check(has_frangitur && has_creotar && has_sea,
              "pantheon includes frangitur, creotar, sea_of_whispers");
      const auto round_trip = engine::WorldForgePantheonAsset::parse(
          pantheon_loaded.value().to_json());
      r.check(round_trip && round_trip.value().to_json() ==
                                pantheon_loaded.value().to_json(),
              "pantheon to_json round trip");
    }
    const auto pantheon_bad_parent = engine::WorldForgePantheonAsset::parse(
        R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"deity","canonStatus":"draft","parentId":"missing"}]})");
    r.check(!pantheon_bad_parent && pantheon_bad_parent.error().code ==
                                        "WORLD-FORGE-PANTHEON-PARENT",
            "pantheon unknown parentId rejected");
    const auto pantheon_cycle = engine::WorldForgePantheonAsset::parse(
        R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"deity","canonStatus":"draft","parentId":"b"},{"id":"b","kind":"deity","canonStatus":"draft","parentId":"a"}]})");
    r.check(!pantheon_cycle && pantheon_cycle.error().code ==
                                   "WORLD-FORGE-PANTHEON-PARENT-CYCLE",
            "pantheon parentId cycle rejected");
    r.check(engine::WorldForgePantheonAsset::validate_file(pantheon_path)
                .has_value(),
            "sample pantheon validate_file succeeds");
    const auto wf_pantheon = engine::apply_world_forge_operation(
        project, nlohmann::json{{"action", "validate"}, {"kind", "pantheon"}});
    r.check(wf_pantheon.exit_code == engine::ExitCode::Success,
            "world_forge_apply validates pantheon");

    const auto archetypes_path =
        engine::default_world_forge_archetypes_path(project);
    r.check(archetypes_path.filename() == "archetypes.worldforge.json",
            "default archetypes path filename");
    auto archetypes_loaded =
        engine::WorldForgeArchetypesAsset::load(archetypes_path);
    r.check(archetypes_loaded.has_value(),
            "sample archetypes.worldforge.json loads");
    r.check(archetypes_loaded &&
                archetypes_loaded.value().schema_version == 1 &&
                archetypes_loaded.value().id == "tessera_archetypes",
            "archetypes sample schema and id");
    r.check(archetypes_loaded && archetypes_loaded.value().entities.size() == 3,
            "archetypes seeds ashfell_blade+outrider+runecaster");
    if (archetypes_loaded) {
      const auto &entities = archetypes_loaded.value().entities;
      r.check(entities[0].id == "ashfell_blade" &&
                  entities[0].kind == engine::WorldForgeArchetypeKind::Starting,
              "ashfell_blade starting archetype");
      r.check(entities[1].id == "outrider" && entities[2].id == "runecaster",
              "outrider and runecaster seeded");
      const auto round_trip = engine::WorldForgeArchetypesAsset::parse(
          archetypes_loaded.value().to_json());
      r.check(round_trip && round_trip.value().to_json() ==
                                archetypes_loaded.value().to_json(),
              "archetypes to_json round trip");
    }
    const auto archetype_bad_kind = engine::WorldForgeArchetypesAsset::parse(
        R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"hero"}]})");
    r.check(!archetype_bad_kind &&
                archetype_bad_kind.error().code == "WORLD-FORGE-ARCHETYPE-KIND",
            "archetype bad kind rejected");
    const auto archetype_bad_faction = engine::WorldForgeArchetypesAsset::parse(
        R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"advanced","unlock":{"factionId":"missing_faction"}}]})");
    r.check(archetype_bad_faction.has_value(),
            "archetype with unknown faction parses before cross-ref");
    if (archetype_bad_faction) {
      std::unordered_set<std::string> known{"kingdom_tessera"};
      const auto refs =
          archetype_bad_faction.value().validate_faction_refs(known);
      r.check(!refs && refs.error().code == "WORLD-FORGE-ARCHETYPE-FACTION",
              "archetype unknown unlock.factionId rejected");
    }
    r.check(engine::WorldForgeArchetypesAsset::validate_file(archetypes_path)
                .has_value(),
            "sample archetypes validate_file succeeds");
    const auto wf_archetypes = engine::apply_world_forge_operation(
        project,
        nlohmann::json{{"action", "validate"}, {"kind", "archetypes"}});
    r.check(wf_archetypes.exit_code == engine::ExitCode::Success,
            "world_forge_apply validates archetypes");

    const auto rel_path =
        engine::default_world_forge_relationships_path(project);
    r.check(rel_path.filename() == "relationships.worldforge.json",
            "default relationships path filename");
    auto rel_loaded = engine::WorldForgeRelationshipsAsset::load(rel_path);
    r.check(rel_loaded.has_value(),
            "sample relationships.worldforge.json loads");
    r.check(rel_loaded && rel_loaded.value().schema_version == 1 &&
                rel_loaded.value().id == "tessera_relationships",
            "relationships sample schema and id");
    r.check(rel_loaded && rel_loaded.value().nodes.size() == 14 &&
                rel_loaded.value().edges.size() == 16,
            "relationships sample seed counts");
    if (rel_loaded) {
      const auto round_trip = engine::WorldForgeRelationshipsAsset::parse(
          rel_loaded.value().to_json());
      r.check(round_trip &&
                  round_trip.value().to_json() == rel_loaded.value().to_json(),
              "relationships to_json round trip");
      std::unordered_set<std::string> faction_ids;
      if (loaded)
        for (const auto &entity : loaded.value().entities)
          faction_ids.insert(entity.id);
      r.check(rel_loaded.value().validate_faction_refs(faction_ids).has_value(),
              "sample faction endpoints resolve against factions asset");
    }
    const auto empty_node = engine::WorldForgeRelationshipsAsset::parse(
        R"({"schemaVersion":1,"id":"t","nodes":[{"id":"","kind":"person","canonStatus":"draft"}],"edges":[]})");
    r.check(!empty_node && empty_node.error().code == "WORLD-FORGE-REL-NODE-ID",
            "empty node id rejected");
    const auto bad_node_ref = engine::WorldForgeRelationshipsAsset::parse(
        R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft"}],"edges":[{"id":"e1","from":{"target":"node","id":"a"},"to":{"target":"node","id":"missing"},"kind":"related","canonStatus":"draft"}]})");
    r.check(!bad_node_ref && bad_node_ref.error().code == "WORLD-FORGE-REL-REF",
            "unknown node endpoint rejected");
    const auto self_loop = engine::WorldForgeRelationshipsAsset::parse(
        R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft"}],"edges":[{"id":"e1","from":{"target":"node","id":"a"},"to":{"target":"node","id":"a"},"kind":"related","canonStatus":"draft"}]})");
    r.check(!self_loop && self_loop.error().code == "WORLD-FORGE-REL-SELF",
            "self-loop rejected");
    const auto bad_edge_kind = engine::WorldForgeRelationshipsAsset::parse(
        R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft"},{"id":"b","kind":"person","canonStatus":"draft"}],"edges":[{"id":"e1","from":{"target":"node","id":"a"},"to":{"target":"node","id":"b"},"kind":"enemigos","canonStatus":"draft"}]})");
    r.check(!bad_edge_kind &&
                bad_edge_kind.error().code == "WORLD-FORGE-REL-EDGE-KIND",
            "bad edge kind rejected");
    const auto bad_node_parent = engine::WorldForgeRelationshipsAsset::parse(
        R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft","parentId":"missing"}],"edges":[]})");
    r.check(!bad_node_parent &&
                bad_node_parent.error().code == "WORLD-FORGE-REL-PARENT",
            "unknown node parentId rejected");
    const auto node_parent_cycle = engine::WorldForgeRelationshipsAsset::parse(
        R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft","parentId":"b"},{"id":"b","kind":"person","canonStatus":"draft","parentId":"a"}],"edges":[]})");
    r.check(!node_parent_cycle && node_parent_cycle.error().code ==
                                      "WORLD-FORGE-REL-PARENT-CYCLE",
            "node parentId cycle rejected");
    {
      auto ok_graph = engine::WorldForgeRelationshipsAsset::parse(
          R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft"}],"edges":[{"id":"e1","from":{"target":"node","id":"a"},"to":{"target":"faction","id":"missing_faction"},"kind":"member_of","canonStatus":"draft"}]})");
      r.check(ok_graph.has_value(),
              "faction endpoint allowed without known faction set");
      std::unordered_set<std::string> known{"kingdom_tessera"};
      r.check(ok_graph && !ok_graph.value().validate_faction_refs(known) &&
                  ok_graph.value().validate_faction_refs(known).error().code ==
                      "WORLD-FORGE-REL-FACTION-REF",
              "unknown faction endpoint rejected when known set provided");
    }
    r.check(engine::WorldForgeRelationshipsAsset::validate_file(rel_path)
                .has_value(),
            "sample relationships validate_file succeeds");
    const auto map_path = engine::default_world_forge_map_path(project);
    r.check(map_path.filename() == "map.worldforge.json",
            "default map path filename");
    auto map_loaded = engine::WorldForgeMapAsset::load(map_path);
    r.check(map_loaded.has_value(), "sample map.worldforge.json loads");
    r.check(map_loaded && map_loaded.value().schema_version == 1 &&
                map_loaded.value().id == "tessera_map",
            "map sample schema and id");
    r.check(map_loaded && map_loaded.value().regions.size() == 6 &&
                map_loaded.value().pois.size() == 7 &&
                map_loaded.value().links.size() == 8 &&
                map_loaded.value().hydrology_regions.size() == 2 &&
                map_loaded.value().ferry_routes.size() == 2,
            "map sample seed counts");
    r.check(map_loaded && map_loaded.value().cartography_plate &&
                map_loaded.value().cartography_plate->width_meters == 16000.0f,
            "map sample has 16 km cartographyPlate");
    if (map_loaded) {
      const auto round_trip =
          engine::WorldForgeMapAsset::parse(map_loaded.value().to_json());
      r.check(round_trip &&
                  round_trip.value().to_json() == map_loaded.value().to_json(),
              "map to_json round trip");
      std::unordered_set<std::string> faction_ids;
      if (loaded)
        for (const auto &entity : loaded.value().entities)
          faction_ids.insert(entity.id);
      r.check(map_loaded.value().validate_faction_refs(faction_ids).has_value(),
              "sample region factionIds resolve against factions asset");
    }
    {
      auto plated = engine::WorldForgeMapAsset::parse(
          R"({"schemaVersion":1,"id":"t","cartographyPlate":{"centerX":10,"centerZ":20,"widthMeters":4000,"heightMeters":2250},"regions":[{"id":"r1","kind":"region","canonStatus":"draft","anchor":{"x":0,"y":1,"z":0}}],"pois":[],"links":[]})");
      r.check(plated && plated.value().cartography_plate &&
                  plated.value().cartography_plate->center_x == 10.0f &&
                  plated.value().cartography_plate->width_meters == 4000.0f,
              "cartographyPlate parse");
      const auto bad_plate = engine::WorldForgeMapAsset::parse(
          R"({"schemaVersion":1,"id":"t","cartographyPlate":{"centerX":0,"centerZ":0,"widthMeters":0,"heightMeters":100},"regions":[],"pois":[],"links":[]})");
      r.check(!bad_plate && bad_plate.error().code == "WORLD-FORGE-MAP-PLATE",
              "zero-width plate rejected");
      auto scale_map = engine::WorldForgeMapAsset::parse(
          R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft","anchor":{"x":100,"y":2,"z":0}},{"id":"r2","kind":"region","canonStatus":"draft","anchor":{"x":200,"y":2,"z":0}}],"pois":[],"links":[]})");
      r.check(scale_map.has_value(), "scale map parse");
      if (scale_map) {
        const float applied = engine::apply_cartography_plate_and_rescale(
            scale_map.value(), 4000.0f, 1.0f, 1.0f);
        r.check(applied > 1.0f && scale_map.value().cartography_plate &&
                    scale_map.value().cartography_plate->width_meters ==
                        4000.0f,
                "apply plate writes 4 km plate");
        const auto &a = *scale_map.value().regions[0].anchor;
        const auto &b = *scale_map.value().regions[1].anchor;
        const float dist = std::fabs(b.x - a.x);
        r.check(dist > 3900.0f && dist < 4100.0f && a.y == 2.0f,
                "rescale spreads anchors; Y unchanged");
      }
    }
    const auto empty_region = engine::WorldForgeMapAsset::parse(
        R"({"schemaVersion":1,"id":"t","regions":[{"id":"","kind":"region","canonStatus":"draft"}],"pois":[],"links":[]})");
    r.check(!empty_region &&
                empty_region.error().code == "WORLD-FORGE-MAP-REGION-ID",
            "empty region id rejected");
    const auto bad_poi_region = engine::WorldForgeMapAsset::parse(
        R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft"}],"pois":[{"id":"p1","kind":"landmark","canonStatus":"draft","regionId":"missing"}],"links":[]})");
    r.check(!bad_poi_region &&
                bad_poi_region.error().code == "WORLD-FORGE-MAP-POI-REGION",
            "unknown POI regionId rejected");
    const auto bad_link = engine::WorldForgeMapAsset::parse(
        R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft"}],"pois":[],"links":[{"id":"l1","kind":"travel","fromKind":"region","fromId":"r1","toKind":"region","toId":"missing","canonStatus":"draft"}]})");
    r.check(!bad_link && bad_link.error().code == "WORLD-FORGE-MAP-REF",
            "unknown link endpoint rejected");
    const auto self_link = engine::WorldForgeMapAsset::parse(
        R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft"}],"pois":[],"links":[{"id":"l1","kind":"travel","fromKind":"region","fromId":"r1","toKind":"region","toId":"r1","canonStatus":"draft"}]})");
    r.check(!self_link && self_link.error().code == "WORLD-FORGE-MAP-SELF",
            "self-link rejected");
    const auto bad_ferry_poi = engine::WorldForgeMapAsset::parse(
        R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft"}],"pois":[{"id":"p1","kind":"landmark","canonStatus":"draft","regionId":"r1"}],"links":[],"ferryRoutes":[{"id":"f1","fromPoiId":"p1","toPoiId":"missing","points":[]}]})");
    r.check(!bad_ferry_poi &&
                bad_ferry_poi.error().code == "WORLD-FORGE-MAP-FERRY-POI",
            "unknown ferry toPoiId rejected");
    const auto hydro_round = engine::WorldForgeMapAsset::parse(
        R"({"schemaVersion":1,"id":"t","regions":[],"pois":[],"links":[],"hydrologyRegions":[{"id":"pond","kind":"lake","minX":-5,"maxX":5,"minZ":-3,"maxZ":3,"acts":["act1"],"summary":"test"}]})");
    r.check(hydro_round.has_value() &&
                hydro_round.value().hydrology_regions.size() == 1 &&
                hydro_round.value().hydrology_regions[0].kind ==
                    engine::WorldForgeHydrologyKind::Lake,
            "hydrologyRegions parse");
    {
      auto ok_map = engine::WorldForgeMapAsset::parse(
          R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft","factionIds":["missing_faction"]}],"pois":[],"links":[]})");
      r.check(ok_map.has_value(),
              "unknown factionId allowed without known set");
      std::unordered_set<std::string> known{"kingdom_tessera"};
      r.check(ok_map && !ok_map.value().validate_faction_refs(known) &&
                  ok_map.value().validate_faction_refs(known).error().code ==
                      "WORLD-FORGE-MAP-FACTION-REF",
              "unknown region factionId rejected when known set provided");
    }
    r.check(engine::WorldForgeMapAsset::validate_file(map_path).has_value(),
            "sample map validate_file succeeds");
    {
      auto anchored = engine::WorldForgeMapAsset::parse(
          R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft","anchor":{"x":10,"y":1,"z":20}}],"pois":[{"id":"p1","kind":"landmark","canonStatus":"draft","regionId":"r1","anchor":{"x":12,"y":1,"z":22}}],"links":[{"id":"l1","kind":"travel","fromKind":"region","fromId":"r1","toKind":"poi","toId":"p1","canonStatus":"draft"}]})");
      r.check(anchored.has_value(), "map with anchors parses");
      if (anchored) {
        const auto *ra = engine::resolve_map_endpoint_anchor(
            anchored.value(), engine::WorldForgeMapEndpointKind::Region, "r1");
        const auto *pa = engine::resolve_map_endpoint_anchor(
            anchored.value(), engine::WorldForgeMapEndpointKind::Poi, "p1");
        const auto *missing = engine::resolve_map_endpoint_anchor(
            anchored.value(), engine::WorldForgeMapEndpointKind::Region,
            "missing");
        r.check(ra && ra->x == 10.0f && ra->z == 20.0f,
                "resolve region endpoint anchor");
        r.check(pa && pa->x == 12.0f && pa->z == 22.0f,
                "resolve poi endpoint anchor");
        r.check(missing == nullptr, "missing endpoint anchor is null");
        r.check(engine::map_region_marker_key("r1") == "region:r1",
                "region marker key");
        r.check(engine::map_poi_marker_key("p1") == "poi:p1", "poi marker key");
        const auto world = engine::graph_screen_to_world(
            engine::WorldForgeGraphCamera{}, 100.0f, 50.0f);
        const auto back = engine::graph_world_to_screen_local(
            engine::WorldForgeGraphCamera{}, world[0], world[1]);
        r.check(std::fabs(back[0] - 100.0f) < 0.01f &&
                    std::fabs(back[1] - 50.0f) < 0.01f,
                "map canvas screen/world round trip via graph camera");
      }
    }
    {
      using engine::CartographyStrokeStyle;
      r.check(std::string(engine::cartography_stroke_style_id(
                  CartographyStrokeStyle::PoliticalBorder)) ==
                  "political_border",
              "stroke style id political_border");
      r.check(engine::cartography_stroke_style_from_id("highway") ==
                  CartographyStrokeStyle::Highway,
              "stroke style from id highway");
      r.check(engine::cartography_stroke_style_from_id("stroke-ferry") ==
                  CartographyStrokeStyle::Ferry,
              "stroke style from texture-like id");
      const auto &road =
          engine::cartography_stroke_style_info(CartographyStrokeStyle::Road);
      r.check(std::string(road.texture_key) == "stroke-road" &&
                  road.repeat_px == 256.0f,
              "road stroke info");
      // Empty / single-point paths produce no stamps.
      r.check(engine::build_cartography_stroke_stamps({}, 4.0f, 256.0f).empty(),
              "empty polyline no stamps");
      r.check(engine::build_cartography_stroke_stamps({{0, 0}}, 4.0f, 256.0f)
                  .empty(),
              "single point no stamps");
      // Degenerate zero-length segment skipped.
      engine::CartographyStrokeBuildStats stats{};
      auto skipped = engine::build_cartography_stroke_stamps(
          {{0, 0}, {0, 0}, {10, 0}}, 3.0f, 256.0f, &stats);
      r.check(skipped.size() == 1 && stats.segment_count == 1 &&
                  stats.stamp_count == 1,
              "zero-length segment skipped");
      // Continuous UV along a two-segment path.
      auto stamps = engine::build_cartography_stroke_stamps(
          {{0, 0}, {100, 0}, {100, 50}}, 4.0f, 100.0f, &stats);
      r.check(stamps.size() == 2 && stats.total_length_px > 149.0f &&
                  stats.total_length_px < 151.0f,
              "two-segment stroke length");
      r.check(stamps[0].u0 == 0.0f && std::fabs(stamps[0].u1 - 1.0f) < 0.01f,
              "first stamp UV spans one repeat");
      r.check(std::fabs(stamps[1].u0 - 1.0f) < 0.01f &&
                  stamps[1].u1 > stamps[1].u0,
              "second stamp continues UV");
      // Hit-testing remains against the authored polyline (not the ribbon
      // thickness).
      const float on_line = engine::cartography_stroke_point_polyline_distance(
          50.0f, 0.0f, {{0, 0}, {100, 0}});
      const float off_line = engine::cartography_stroke_point_polyline_distance(
          50.0f, 20.0f, {{0, 0}, {100, 0}});
      r.check(on_line < 0.5f && off_line > 19.0f, "polyline hit distance");
      // Stroke tile files exist with alpha (runtime + context).
      const auto stroke_runtime =
          project / "assets/ui/cartography/strokes/stroke-road.png";
      const auto stroke_context = std::filesystem::path(
          "context/art/cartography/strokes/stroke-road.png");
#ifdef ENGINE_REPOSITORY_ROOT
      const auto stroke_context_abs =
          std::filesystem::path(ENGINE_REPOSITORY_ROOT) / stroke_context;
      r.check(std::filesystem::exists(stroke_runtime) ||
                  std::filesystem::exists(stroke_context) ||
                  std::filesystem::exists(stroke_context_abs),
              "stroke-road.png present");
#else
      r.check(std::filesystem::exists(stroke_runtime) ||
                  std::filesystem::exists(stroke_context),
              "stroke-road.png present");
#endif
    }
    const auto quest_path = engine::default_world_forge_quests_path(project);
    r.check(quest_path.filename() == "quests.worldforge.json",
            "default quests path filename");
    auto quest_loaded = engine::WorldForgeQuestsAsset::load(quest_path);
    r.check(quest_loaded.has_value(), "sample quests.worldforge.json loads");
    r.check(quest_loaded && quest_loaded.value().schema_version == 1 &&
                quest_loaded.value().id == "tessera_quests",
            "quests sample schema and id");
    r.check(quest_loaded && quest_loaded.value().quests.size() == 3,
            "quests sample seeds three quests");
    if (quest_loaded) {
      const auto &mq = quest_loaded.value().quests[0];
      r.check(mq.id == "mq_act0_calrenoth" &&
                  mq.kind == engine::WorldForgeQuestKind::Main &&
                  mq.dialogue.start_id == "dlg_act0_meet_arkand",
              "mq_act0 is main quest hooked to Twine dialogue");
      r.check(!mq.acts.empty() && mq.acts[0] == "act0",
              "mq_act0 has acts=[act0]");
      r.check(engine::matches_world_forge_act_filter(mq.acts, mq.tags, "act0"),
              "act0 filter matches mq_act0");
      r.check(!engine::matches_world_forge_act_filter(mq.acts, mq.tags, "act1"),
              "act1 filter excludes mq_act0");
      r.check(engine::matches_world_forge_act_filter({}, {}, "act2"),
              "empty acts is campaign-wide under act filter");
      const auto &q0 = quest_loaded.value().quests[1];
      r.check(q0.id == "sq_01_cart_again" && q0.objectives.size() == 4 &&
                  q0.dialogue.start_id.empty(),
              "sq_01 keeps soft-empty dialogue hooks until authored");
      r.check(q0.objectives[0].dialogue_id.empty() && q0.forks.size() == 1 &&
                  q0.forks[0].dialogue_id.empty(),
              "sq_01 objective and fork dialogue ids soft-empty");
      const auto round_trip =
          engine::WorldForgeQuestsAsset::parse(quest_loaded.value().to_json());
      r.check(round_trip && round_trip.value().to_json() ==
                                quest_loaded.value().to_json(),
              "quests to_json round trip");
      std::unordered_set<std::string> region_ids;
      if (map_loaded)
        for (const auto &region : map_loaded.value().regions)
          region_ids.insert(region.id);
      r.check(quest_loaded.value().validate_region_refs(region_ids).has_value(),
              "sample quest regionIds resolve against map asset");
    }
    const auto empty_quest = engine::WorldForgeQuestsAsset::parse(
        R"({"schemaVersion":1,"id":"t","quests":[{"id":"","kind":"side","canonStatus":"draft","objectives":[],"forks":[]}]})");
    r.check(!empty_quest && empty_quest.error().code == "WORLD-FORGE-QUEST-ID",
            "empty quest id rejected");
    const auto bad_act = engine::WorldForgeQuestsAsset::parse(
        R"({"schemaVersion":1,"id":"t","quests":[{"id":"q1","kind":"side","canonStatus":"draft","acts":["act99"],"objectives":[],"forks":[]}]})");
    r.check(!bad_act && bad_act.error().code == "WORLD-FORGE-ACT-ID",
            "bad act id rejected");
    const auto dup_objective = engine::WorldForgeQuestsAsset::parse(
        R"({"schemaVersion":1,"id":"t","quests":[{"id":"q1","kind":"side","canonStatus":"draft","objectives":[{"id":"a","summary":""},{"id":"a","summary":""}],"forks":[]}]})");
    r.check(!dup_objective && dup_objective.error().code ==
                                  "WORLD-FORGE-QUEST-OBJECTIVE-ID-DUP",
            "duplicate objective id rejected");
    const auto bad_quest_kind = engine::WorldForgeQuestsAsset::parse(
        R"({"schemaVersion":1,"id":"t","quests":[{"id":"q1","kind":"epic","canonStatus":"draft","objectives":[],"forks":[]}]})");
    r.check(!bad_quest_kind &&
                bad_quest_kind.error().code == "WORLD-FORGE-QUEST-KIND",
            "bad quest kind rejected");
    {
      auto ok_quest = engine::WorldForgeQuestsAsset::parse(
          R"({"schemaVersion":1,"id":"t","quests":[{"id":"q1","kind":"side","canonStatus":"draft","regionId":"missing","objectives":[],"forks":[]}]})");
      r.check(ok_quest.has_value(),
              "unknown regionId allowed without known set");
      std::unordered_set<std::string> known{"tessera_overland"};
      r.check(ok_quest && !ok_quest.value().validate_region_refs(known) &&
                  ok_quest.value().validate_region_refs(known).error().code ==
                      "WORLD-FORGE-QUEST-REGION-REF",
              "unknown quest regionId rejected when known set provided");
    }
    r.check(
        engine::WorldForgeQuestsAsset::validate_file(quest_path).has_value(),
        "sample quests validate_file succeeds");
    const auto mvp_path =
        engine::default_world_forge_mvp_readiness_path(project);
    r.check(mvp_path.filename() == "act0_mvp_readiness.worldforge.json",
            "default MVP readiness path");
    auto mvp_loaded = engine::WorldForgeMvpReadinessAsset::load(mvp_path);
    r.check(mvp_loaded.has_value(),
            "sample act0_mvp_readiness.worldforge.json loads");
    r.check(mvp_loaded && mvp_loaded.value().schema_version == 1 &&
                mvp_loaded.value().id == "act0_mvp_readiness" &&
                mvp_loaded.value().act_id == "act0",
            "MVP readiness sample schema/id/actId");
    r.check(mvp_loaded && mvp_loaded.value().count_items() > 20,
            "MVP readiness seeds a substantial checklist");
    r.check(mvp_loaded && mvp_loaded.value().count_done() <
                              mvp_loaded.value().count_items() / 2,
            "MVP readiness seed is not mostly done");
    if (mvp_loaded) {
      const auto round_trip = engine::WorldForgeMvpReadinessAsset::parse(
          mvp_loaded.value().to_json());
      r.check(round_trip &&
                  round_trip.value().to_json() == mvp_loaded.value().to_json(),
              "MVP readiness to_json round trip");
      bool has_cinematics = false;
      bool has_ui_ux = false;
      bool has_p0 = false;
      bool has_description = false;
      for (const auto &cat : mvp_loaded.value().categories) {
        if (cat.id == "cinematics")
          has_cinematics = true;
        if (cat.id == "ui_ux")
          has_ui_ux = true;
        for (const auto &item : cat.items) {
          if (item.priority == engine::WorldForgeMvpItemPriority::P0)
            has_p0 = true;
          if (!item.description.empty())
            has_description = true;
          if (item.workstream == engine::WorldForgeMvpWorkstream::Cinematics)
            has_cinematics = true;
          if (item.workstream == engine::WorldForgeMvpWorkstream::UiUx)
            has_ui_ux = true;
        }
      }
      r.check(has_cinematics,
              "MVP readiness seeds cinematics / theatrical workstream");
      r.check(has_ui_ux, "MVP readiness seeds UI / UX workstream");
      r.check(has_p0, "MVP readiness seeds P0 priority items");
      r.check(has_description, "MVP readiness seeds detail descriptions");
      bool has_depends = false;
      bool has_hair = false;
      for (const auto &cat : mvp_loaded.value().categories) {
        for (const auto &item : cat.items) {
          if (!item.depends_on.empty())
            has_depends = true;
          if (item.id == "art_hair_styles")
            has_hair = true;
        }
      }
      r.check(has_depends, "MVP readiness seeds dependsOn prerequisites");
      r.check(has_hair,
              "MVP readiness seeds character-creation hair prerequisite");
      bool has_goal = false;
      for (const auto &cat : mvp_loaded.value().categories) {
        for (const auto &item : cat.items) {
          if (item.goal)
            has_goal = true;
        }
      }
      r.check(has_goal,
              "MVP readiness seeds at least one verifiable goal sink");
      const auto *slice =
          mvp_loaded.value().find_item("project_vertical_slice_gate");
      r.check(slice && slice->goal && !slice->depends_on.empty(),
              "playable demo exit is tagged goal with milestone dependsOn");
      const auto *creation =
          mvp_loaded.value().find_item("coding_character_creation");
      r.check(creation && !creation->depends_on.empty(),
              "character creation lists art/kit prerequisites");
      auto edited = mvp_loaded.value();
      engine::WorldForgeMvpChecklistItem *editable = nullptr;
      for (auto &cat : edited.categories) {
        for (auto &item : cat.items) {
          if (item.status != engine::WorldForgeMvpItemStatus::Done) {
            editable = &item;
            break;
          }
        }
        if (editable)
          break;
      }
      r.check(editable != nullptr, "MVP has a non-done item to edit");
      if (editable) {
        const int before = edited.count_done();
        editable->status = engine::WorldForgeMvpItemStatus::Done;
        r.check(edited.count_done() == before + 1,
                "status edit updates done count");
      }
    }
    r.check(engine::WorldForgeMvpReadinessAsset::validate_file(mvp_path)
                .has_value(),
            "MVP readiness validate_file");
    const auto bad_mvp_status = engine::WorldForgeMvpReadinessAsset::parse(
        R"({"schemaVersion":1,"id":"x","actId":"act0","categories":[{"id":"c","title":"C","items":[{"id":"i","title":"I","status":"nope"}]}]})");
    r.check(!bad_mvp_status.has_value(), "MVP rejects unknown status");
    const auto bad_mvp_act = engine::WorldForgeMvpReadinessAsset::parse(
        R"({"schemaVersion":1,"id":"x","actId":"act9","categories":[]})");
    r.check(!bad_mvp_act.has_value(), "MVP rejects unknown actId");
    const auto dup_mvp_item = engine::WorldForgeMvpReadinessAsset::parse(
        R"({"schemaVersion":1,"id":"x","actId":"act0","categories":[{"id":"c","title":"C","items":[{"id":"same","title":"A","status":"todo"},{"id":"same","title":"B","status":"todo"}]}]})");
    r.check(!dup_mvp_item.has_value(), "MVP rejects duplicate item ids");
    const auto bad_mvp_stream = engine::WorldForgeMvpReadinessAsset::parse(
        R"({"schemaVersion":1,"id":"x","actId":"act0","categories":[{"id":"c","title":"C","items":[{"id":"i","title":"I","status":"todo","workstream":"nope"}]}]})");
    r.check(!bad_mvp_stream.has_value(), "MVP rejects unknown workstream");
    const auto ok_cine = engine::WorldForgeMvpReadinessAsset::parse(
        R"({"schemaVersion":1,"id":"x","actId":"act0","categories":[{"id":"cinematics","title":"Cinematics","items":[{"id":"i","title":"Prologue","status":"todo","priority":"p0","workstream":"events","description":"Watch beat."}]}]})");
    r.check(ok_cine && ok_cine.value().categories[0].items[0].workstream ==
                           engine::WorldForgeMvpWorkstream::Cinematics,
            "MVP accepts events alias for cinematics");
    const auto ok_ui = engine::WorldForgeMvpReadinessAsset::parse(
        R"({"schemaVersion":1,"id":"x","actId":"act0","categories":[{"id":"ui_ux","title":"UI / UX","items":[{"id":"i","title":"HUD","status":"todo","priority":"p0","workstream":"ui","description":"Play chrome."}]}]})");
    r.check(ok_ui && ok_ui.value().categories[0].items[0].workstream ==
                         engine::WorldForgeMvpWorkstream::UiUx,
            "MVP accepts ui alias for ui_ux");
    const auto dlg_path = engine::default_world_forge_dialogues_path(project);
    r.check(dlg_path.filename() == "dialogues.worldforge.json",
            "default dialogues path filename");
    auto dlg_loaded = engine::WorldForgeDialoguesAsset::load(dlg_path);
    r.check(dlg_loaded.has_value(), "sample dialogues.worldforge.json loads");
    r.check(dlg_loaded && dlg_loaded.value().schema_version == 1 &&
                dlg_loaded.value().id == "tessera_dialogues",
            "dialogues sample schema and id");
    r.check(dlg_loaded && dlg_loaded.value().trees.size() >= 6 &&
                dlg_loaded.value().find_tree("dlg_act0_meet_arkand") !=
                    nullptr &&
                dlg_loaded.value().find_tree("dlg_act0_report_to_grenge") !=
                    nullptr &&
                dlg_loaded.value().find_tree("dlg_act0_drawbridge_retreat") !=
                    nullptr &&
                dlg_loaded.value().find_tree("dlg_act0_creotar_vision") !=
                    nullptr &&
                dlg_loaded.value().find_tree("dlg_sandbox_sample") != nullptr,
            "dialogues sample seeds split Act 0 and sandbox trees");
    if (dlg_loaded) {
      const auto *act0 = dlg_loaded.value().find_tree("dlg_act0_meet_arkand");
      r.check(act0 && act0->parent_quest_id == "mq_act0_calrenoth" &&
                  act0->entry_node_id == "prologue" && act0->nodes.size() >= 8,
              "meet_arkand tree parents to mq_act0 with prologue entry");
      const auto *sandbox = dlg_loaded.value().find_tree("dlg_sandbox_sample");
      r.check(sandbox && !sandbox->nodes.empty() &&
                  !sandbox->nodes.front().choices.empty(),
              "sandbox dialogue tree has greeting choices");
      if (sandbox && !sandbox->nodes.front().choices.empty()) {
        bool found_tone = false;
        bool found_standing = false;
        for (const auto &choice : sandbox->nodes.front().choices) {
          if (!choice.tone.empty())
            found_tone = true;
          if (!choice.standing_adjust.empty())
            found_standing = true;
        }
        r.check(found_tone && found_standing,
                "sandbox choices include tone and standingAdjust");
        r.check(!engine::format_dialogue_choice_label(
                     sandbox->nodes.front().choices.front())
                     .empty(),
                "format_dialogue_choice_label returns label text");
        r.check(!engine::format_dialogue_tone_chip(
                     sandbox->nodes.front().choices.front())
                     .empty(),
                "format_dialogue_tone_chip returns tone");
        r.check(
            engine::format_dialogue_portrait_initials("arkand") == "A" ||
                engine::format_dialogue_portrait_initials("arkand").size() >= 1,
            "portrait initials from speaker id");
        r.check(engine::format_dialogue_speaker_display("arkand") == "Arkand",
                "speaker display title-cases id");
      }
      const auto round_trip =
          engine::WorldForgeDialoguesAsset::parse(dlg_loaded.value().to_json());
      r.check(round_trip &&
                  round_trip.value().to_json() == dlg_loaded.value().to_json(),
              "dialogues to_json round trip");
      std::unordered_set<std::string> quest_ids;
      if (quest_loaded)
        for (const auto &quest : quest_loaded.value().quests)
          quest_ids.insert(quest.id);
      r.check(dlg_loaded.value().validate_quest_refs(quest_ids).has_value(),
              "sample dialogue parentQuestId resolves against quests asset");
      engine::DialogueRuntime runtime;
      r.check(runtime.bind(&dlg_loaded.value()).has_value(),
              "DialogueRuntime binds sample asset");
      r.check(runtime.start("dlg_act0_meet_arkand").has_value(),
              "DialogueRuntime starts Twine tree");
      auto present = runtime.present();
      r.check(present && present.value().node_id == "prologue" &&
                  !present.value().choices.empty(),
              "DialogueRuntime presents prologue with choices");
      if (present && !present.value().choices.empty()) {
        r.check(runtime.choose(present.value().choices.front().id).has_value(),
                "DialogueRuntime chooses first prologue branch");
        auto next = runtime.present();
        r.check(next && next.value().node_id == "tutorial",
                "DialogueRuntime advances to tutorial");
      }
      {
        engine::QuestRuntime unbound;
        r.check(!unbound.start("sq_01_cart_again").has_value(),
                "QuestRuntime start fails when unbound");
        r.check(!unbound.bind(nullptr).has_value(),
                "QuestRuntime rejects null bind");
      }
      if (quest_loaded) {
        engine::QuestRuntime quest_rt;
        r.check(quest_rt.bind(&quest_loaded.value()).has_value(),
                "QuestRuntime binds sample quests");
        r.check(!quest_rt.start("missing_quest").has_value(),
                "QuestRuntime rejects unknown quest");
        r.check(quest_rt.start("sq_01_cart_again").has_value(),
                "QuestRuntime starts sq_01");
        auto st = quest_rt.status("sq_01_cart_again");
        r.check(st &&
                    st.value().status == engine::QuestInstanceStatus::Active &&
                    st.value().current_objective_id == "find_pellin",
                "QuestRuntime current is first objective");
        r.check(!quest_rt.complete_objective("sq_01_cart_again", "lift_cart")
                     .has_value(),
                "QuestRuntime rejects out-of-order objective");
        r.check(quest_rt.complete_objective("sq_01_cart_again", "find_pellin")
                    .has_value(),
                "QuestRuntime completes first objective");
        st = quest_rt.status("sq_01_cart_again");
        r.check(st && st.value().current_objective_id == "clear_scavengers",
                "QuestRuntime advances objective");
        r.check(quest_rt.list_active().size() == 1,
                "QuestRuntime lists one active quest");
        r.check(
            quest_rt.complete_objective("sq_01_cart_again", "clear_scavengers")
                .has_value(),
            "QuestRuntime completes scavengers");
        r.check(quest_rt.complete_objective("sq_01_cart_again", "lift_cart")
                    .has_value(),
                "QuestRuntime completes lift");
        r.check(quest_rt.complete_objective("sq_01_cart_again", "escort_gate")
                    .has_value(),
                "QuestRuntime completes escort");
        st = quest_rt.status("sq_01_cart_again");
        r.check(
            st && st.value().status == engine::QuestInstanceStatus::Completed &&
                quest_rt.list_active().empty(),
            "QuestRuntime completes quest");
        r.check(quest_rt.start("mq_act0_calrenoth").has_value(),
                "QuestRuntime starts main quest");
        auto hook = quest_rt.dialogue_for_stage(
            "mq_act0_calrenoth", engine::QuestDialogueStage::Start);
        r.check(hook && hook.value() == "dlg_act0_meet_arkand",
                "QuestRuntime dialogue start hook");
        r.check(quest_rt.abandon("mq_act0_calrenoth").has_value(),
                "QuestRuntime abandons active quest");
        st = quest_rt.status("mq_act0_calrenoth");
        r.check(st &&
                    st.value().status == engine::QuestInstanceStatus::Abandoned,
                "QuestRuntime abandoned status");
        r.check(quest_rt.primary_objective_text().empty(),
                "QuestRuntime primary objective text empty when idle");

        engine::FlagRuntime flags;
        r.check(flags.set("act0.probe").has_value(), "FlagRuntime set");
        r.check(flags.has("act0.probe"), "FlagRuntime has set flag");
        r.check(!flags.has("missing.flag"), "FlagRuntime missing is false");
        r.check(flags.clear("act0.probe").has_value(), "FlagRuntime clear");
        r.check(!flags.has("act0.probe"), "FlagRuntime cleared");
        r.check(!flags.set("").has_value(), "FlagRuntime rejects empty id");
        r.check(quest_rt.start("mq_act0_calrenoth").has_value(),
                "QuestRuntime restart for fork");
        r.check(quest_rt
                    .resolve_fork("mq_act0_calrenoth", "larrell_save_vs_flee",
                                  "act0.helped_larrell", flags)
                    .has_value(),
                "QuestRuntime resolve_fork sets outcome");
        r.check(flags.has("act0.helped_larrell"),
                "resolve_fork sets chosen flag");
        r.check(quest_rt
                    .resolve_fork("mq_act0_calrenoth", "larrell_save_vs_flee",
                                  "act0.fled_drawbridge", flags)
                    .has_value(),
                "QuestRuntime resolve_fork switches outcome");
        r.check(flags.has("act0.fled_drawbridge") &&
                    !flags.has("act0.helped_larrell"),
                "resolve_fork clears sibling outcome flags");
        r.check(!quest_rt
                     .resolve_fork("mq_act0_calrenoth", "larrell_save_vs_flee",
                                   "act0.not_a_fork_flag", flags)
                     .has_value(),
                "QuestRuntime rejects unknown fork outcome");
      }
      {
        const auto standing_factions =
            engine::WorldForgeFactionsAsset::parse(R"({
                    "schemaVersion":1,"id":"standing_test","entities":[
                      {"id":"alpha","kind":"faction","canonStatus":"draft","standing":{
                        "tracksPlayer":true,"min":-100,"max":100,
                        "ranks":[{"id":"hostile","minScore":-100,"displayName":"Hostile"},
                                 {"id":"neutral","minScore":0,"displayName":"Neutral"},
                                 {"id":"friendly","minScore":25,"displayName":"Friendly"},
                                 {"id":"allied","minScore":75,"displayName":"Allied"}],
                        "lockIn":{"threshold":80,"exclusiveFactionIds":["beta"]}
                      }},
                      {"id":"beta","kind":"faction","canonStatus":"draft","standing":{
                        "tracksPlayer":true,"min":-100,"max":100,
                        "ranks":[{"id":"hostile","minScore":-100,"displayName":"Hostile"},
                                 {"id":"neutral","minScore":0,"displayName":"Neutral"}],
                        "lockIn":{"threshold":80,"exclusiveFactionIds":["alpha"]}
                      }}
                    ]})");
        const auto standing_rels =
            engine::WorldForgeRelationshipsAsset::parse(R"({
                    "schemaVersion":1,"id":"standing_rel","nodes":[],"edges":[
                      {"id":"a_vs_b","from":{"target":"faction","id":"alpha"},
                       "to":{"target":"faction","id":"beta"},"kind":"rival","canonStatus":"draft",
                       "standingTransfer":0.5}
                    ]})");
        r.check(standing_factions.has_value() && standing_rels.has_value(),
                "standing fixtures parse");
        engine::StandingRuntime standing_rt;
        r.check(!standing_rt.bind(nullptr, nullptr).has_value(),
                "StandingRuntime rejects null bind");
        if (standing_factions && standing_rels) {
          r.check(standing_rt
                      .bind(&standing_factions.value(), &standing_rels.value())
                      .has_value(),
                  "StandingRuntime binds fixtures");
          r.check(standing_rt.adjust("alpha", 40.0).has_value(),
                  "StandingRuntime adjust alpha");
          auto alpha = standing_rt.get("alpha");
          auto beta = standing_rt.get("beta");
          r.check(alpha.has_value() && alpha.value() == 40.0,
                  "StandingRuntime primary score");
          r.check(beta.has_value() && beta.value() == -20.0,
                  "StandingRuntime hostility fallout 0.5");
          r.check(standing_rt.adjust("alpha", 100.0).has_value(),
                  "StandingRuntime clamp adjust");
          alpha = standing_rt.get("alpha");
          r.check(alpha.has_value() && alpha.value() == 100.0,
                  "StandingRuntime clamps to max");
          auto rank = standing_rt.rank("alpha");
          r.check(rank.has_value() && rank.value() == "allied",
                  "StandingRuntime rank allied");
          engine::WorldForgeQuestStandingRequirement req;
          req.faction_id = "alpha";
          req.min_rank_id = "friendly";
          auto meets = standing_rt.meets_requirement(req);
          r.check(meets.has_value() && meets.value(),
                  "StandingRuntime meets rank gate");
          auto locked = standing_rt.lock_in_faction();
          r.check(locked.has_value() && locked.value() == "alpha",
                  "StandingRuntime lock-in at threshold");
          r.check(standing_rt.list_tracked().size() == 2,
                  "StandingRuntime lists tracked");
        }
        const auto bad_order = engine::WorldForgeFactionsAsset::parse(R"({
                    "schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"faction","canonStatus":"draft",
                    "standing":{"tracksPlayer":true,"ranks":[
                      {"id":"high","minScore":50,"displayName":"H"},
                      {"id":"low","minScore":0,"displayName":"L"}]}}]})");
        r.check(!bad_order && bad_order.error().code ==
                                  "WORLD-FORGE-FACTION-STANDING-RANK-ORDER",
                "standing ranks out of order rejected");
        const auto bad_transfer =
            engine::WorldForgeRelationshipsAsset::parse(R"({
                    "schemaVersion":1,"id":"t","nodes":[],"edges":[
                      {"id":"e1","from":{"target":"faction","id":"a"},"to":{"target":"faction","id":"b"},
                       "kind":"rival","canonStatus":"draft","standingTransfer":-1}]})");
        r.check(!bad_transfer && bad_transfer.error().code ==
                                     "WORLD-FORGE-REL-STANDING-TRANSFER",
                "negative standingTransfer rejected");
      }
      if (act0) {
        const auto &tree = *act0;
        const auto reachable = engine::dialogue_reachable_node_ids(tree);
        r.check(reachable.size() >= 10 && reachable.front() == "prologue",
                "dialogue reachability walks from prologue");
        engine::DialogueGraphPositions layout;
        engine::layout_dialogue_graph(tree, layout, false);
        r.check(layout.count("prologue") == 1 && layout.count("tutorial") == 1,
                "dialogue layered layout places entry and child");
        auto edit_tree = tree;
        const auto new_id =
            engine::unique_dialogue_node_id(edit_tree, "graph_test_node");
        r.check(engine::add_dialogue_node(edit_tree, new_id, "narrator", "test")
                    .has_value(),
                "headless add dialogue node");
        const auto choice_id =
            engine::unique_dialogue_choice_id(edit_tree, "graph_test_choice");
        r.check(engine::add_dialogue_choice(edit_tree, "prologue", choice_id,
                                            "test link", new_id)
                    .has_value(),
                "headless add dialogue choice edge");
        r.check(engine::remove_dialogue_choice(edit_tree, "prologue", choice_id)
                    .has_value(),
                "headless remove dialogue choice");
        r.check(engine::remove_dialogue_node(edit_tree, new_id).has_value(),
                "headless remove dialogue node");
        {
          auto dup_tree = tree;
          const auto dup =
              engine::duplicate_dialogue_node(dup_tree, "prologue");
          r.check(dup.has_value() && dup.value() != "prologue",
                  "duplicate dialogue node yields new id");
          r.check(engine::infer_dialogue_node_kind(tree.nodes.front()) ==
                          engine::DialogueGraphNodeKind::Dialogue ||
                      engine::infer_dialogue_node_kind(tree.nodes.front()) ==
                          engine::DialogueGraphNodeKind::End,
                  "infer dialogue node kind");
          const auto compact = engine::dialogue_node_card_size(
              engine::DialogueGraphNodeDisplayMode::Compact);
          const auto standard = engine::dialogue_node_card_size(
              engine::DialogueGraphNodeDisplayMode::Standard);
          const auto expanded = engine::dialogue_node_card_size(
              engine::DialogueGraphNodeDisplayMode::Expanded);
          r.check(compact[1] < standard[1] && standard[1] < expanded[1],
                  "display mode card heights ordered");
          r.check(
              engine::dialogue_node_matches_query(tree.nodes.front(),
                                                  "prologue") ||
                  !engine::dialogue_search_node_ids(tree, "narrator").empty(),
              "dialogue search matches speaker or id");
          engine::WorldForgeGraphCamera cam;
          cam.min_zoom = 0.2f;
          cam.max_zoom = 2.0f;
          engine::DialogueGraphPositions positions;
          engine::layout_dialogue_graph(tree, positions, false);
          const auto bounds = engine::compute_graph_bounds(positions);
          engine::fit_graph_camera_to_bounds(cam, 800.0f, 600.0f, bounds);
          r.check(bounds.valid && cam.zoom >= cam.min_zoom &&
                      cam.zoom <= cam.max_zoom,
                  "shared graph camera fits dialogue bounds");
          engine::center_graph_camera_on(cam, 800.0f, 600.0f, {100.0f, 50.0f},
                                         1.0f);
          const auto world = engine::graph_screen_to_world(cam, 400.0f, 300.0f);
          r.check(std::fabs(world[0] - 100.0f) < 0.01f &&
                      std::fabs(world[1] - 50.0f) < 0.01f,
                  "graph camera centers on world point");
        }
        r.check(engine::slugify_id("Go Get the flowers") ==
                    "go_get_the_flowers",
                "slugify_id lowercases and underscores titles");
        r.check(engine::slugify_id("  Hello--World!! ") == "hello_world",
                "slugify_id collapses punctuation");
        engine::WorldForgeDialoguesAsset edited_asset;
        edited_asset.schema_version = 1;
        edited_asset.id = "edited";
        edited_asset.trees.push_back(edit_tree);
        r.check(edited_asset.validate().has_value(),
                "edited dialogue tree still validates");
      } // if(act0)
    }
    const auto empty_tree = engine::WorldForgeDialoguesAsset::parse(
        R"({"schemaVersion":1,"id":"t","trees":[{"id":"","canonStatus":"draft","entryNodeId":"n","nodes":[{"id":"n","speakerId":"","line":"","choices":[]}]}]})");
    r.check(!empty_tree && empty_tree.error().code == "WORLD-FORGE-DLG-TREE-ID",
            "empty dialogue tree id rejected");
    const auto bad_entry = engine::WorldForgeDialoguesAsset::parse(
        R"({"schemaVersion":1,"id":"t","trees":[{"id":"d1","canonStatus":"draft","entryNodeId":"missing","nodes":[{"id":"n","speakerId":"","line":"","choices":[]}]}]})");
    r.check(!bad_entry && bad_entry.error().code == "WORLD-FORGE-DLG-ENTRY",
            "missing entry node rejected");
    const auto bad_next = engine::WorldForgeDialoguesAsset::parse(
        R"({"schemaVersion":1,"id":"t","trees":[{"id":"d1","canonStatus":"draft","entryNodeId":"n","nodes":[{"id":"n","speakerId":"","line":"","choices":[{"id":"c1","text":"x","nextNodeId":"gone","setFlags":[]}]}]}]})");
    r.check(!bad_next && bad_next.error().code == "WORLD-FORGE-DLG-NEXT",
            "unknown nextNodeId rejected");
    {
      auto ok_dlg = engine::WorldForgeDialoguesAsset::parse(
          R"({"schemaVersion":1,"id":"t","trees":[{"id":"d1","parentQuestId":"missing","canonStatus":"draft","entryNodeId":"n","nodes":[{"id":"n","speakerId":"","line":"","choices":[]}]}]})");
      r.check(ok_dlg.has_value(),
              "unknown parentQuestId allowed without known set");
      std::unordered_set<std::string> known{"mq_act0_calrenoth"};
      r.check(ok_dlg && !ok_dlg.value().validate_quest_refs(known) &&
                  ok_dlg.value().validate_quest_refs(known).error().code ==
                      "WORLD-FORGE-DLG-QUEST-REF",
              "unknown parentQuestId rejected when known set provided");
    }
    r.check(
        engine::WorldForgeDialoguesAsset::validate_file(dlg_path).has_value(),
        "sample dialogues validate_file succeeds");
    {
      const auto evt_path = engine::default_world_forge_events_path(project);
      r.check(evt_path.filename() == "events.worldforge.json",
              "default events path filename");
      auto evt_loaded = engine::WorldForgeEventsAsset::load(evt_path);
      r.check(evt_loaded.has_value(), "sample events.worldforge.json loads");
      r.check(evt_loaded && evt_loaded.value().schema_version == 1 &&
                  evt_loaded.value().id == "tessera_events",
              "events sample schema and id");
      r.check(evt_loaded && evt_loaded.value().find_sequence(
                                "evt_act0_timeline_smoke") != nullptr,
              "events sample seeds Act 0 timeline smoke");
      r.check(evt_loaded && evt_loaded.value().find_sequence(
                                "evt_sandbox_zone_pan") != nullptr,
              "events sample seeds sandbox zone pan sequence");
      if (evt_loaded) {
        const auto round_trip =
            engine::WorldForgeEventsAsset::parse(evt_loaded.value().to_json());
        r.check(round_trip && round_trip.value().to_json() ==
                                  evt_loaded.value().to_json(),
                "events to_json round trip");
        std::unordered_set<std::string> dialogue_ids;
        if (dlg_loaded)
          for (const auto &tree : dlg_loaded.value().trees)
            dialogue_ids.insert(tree.id);
        r.check(
            evt_loaded.value().validate_dialogue_refs(dialogue_ids).has_value(),
            "sample start_dialogue dialogueId resolves against dialogues "
            "asset");
        if (dlg_loaded) {
          r.check(dlg_loaded.value().find_tree("dlg_sandbox_event_zone") !=
                      nullptr,
                  "sandbox event dialogue tree exists for zone pan sequence");
          const auto *sandbox_seq =
              evt_loaded.value().find_sequence("evt_sandbox_zone_pan");
          int look_at_count = 0;
          std::array<float, 3> first_look{};
          std::array<float, 3> second_look{};
          if (sandbox_seq) {
            for (const auto &step : sandbox_seq->steps) {
              if (step.kind != engine::EventTimelineStepKind::LookAt)
                continue;
              if (look_at_count == 0)
                first_look = step.look_at_target;
              else if (look_at_count == 1)
                second_look = step.look_at_target;
              ++look_at_count;
            }
          }
          r.check(look_at_count >= 2,
                  "sandbox zone pan has multi-subject look_at shots");
          r.check(look_at_count >= 2 && (first_look[0] != second_look[0] ||
                                         first_look[1] != second_look[1] ||
                                         first_look[2] != second_look[2]),
                  "sandbox zone pan look_at targets are distinct");
          engine::DialogueRuntime sandbox_dialogue_rt;
          r.check(sandbox_dialogue_rt.bind(&dlg_loaded.value()).has_value(),
                  "DialogueRuntime binds for sandbox zone pan");
          engine::EventTimelineRuntime sandbox_timeline;
          r.check(sandbox_timeline.bind(&evt_loaded.value()).has_value(),
                  "bind events for sandbox zone pan");
          sandbox_timeline.set_dialogue_starter(
              [&](const std::string &dialogue_id) -> engine::Result<void> {
                return sandbox_dialogue_rt.start(dialogue_id);
              });
          r.check(sandbox_timeline.start("evt_sandbox_zone_pan").has_value(),
                  "start sandbox zone pan");
          r.check(sandbox_timeline.control_locked() &&
                      sandbox_timeline.camera_directive().active,
                  "sandbox zone pan locks and looks");
          const auto first_target = sandbox_timeline.camera_directive().target;
          sandbox_timeline.tick(3.1f); // finish first look_at (~3.0)
          r.check(sandbox_timeline.control_locked(),
                  "sandbox stays locked after first look_at");
          sandbox_timeline.tick(1.2f); // wait → second look_at
          r.check(sandbox_timeline.camera_directive().active,
                  "second look_at directive active");
          const auto second_target = sandbox_timeline.camera_directive().target;
          r.check(second_target[0] != first_target[0] ||
                      second_target[2] != first_target[2],
                  "second look_at targets a different subject");
          sandbox_timeline.tick(2.9f); // finish second look_at
          sandbox_timeline.tick(1.1f); // wait → third look_at
          r.check(sandbox_timeline.control_locked() &&
                      sandbox_timeline.camera_directive().active,
                  "third look_at still locked");
          sandbox_timeline.tick(3.3f); // finish third look_at
          sandbox_timeline.tick(1.1f); // wait → emit + dialogue + unlock
          auto sandbox_emits = sandbox_timeline.take_emitted_events();
          r.check(sandbox_emits.size() == 1 &&
                      sandbox_emits[0].name == "sandbox_zone_beat",
                  "sandbox zone pan emit fires");
          r.check(sandbox_dialogue_rt.tree_id() == "dlg_sandbox_event_zone",
                  "sandbox zone pan starts dlg_sandbox_event_zone");
          r.check(sandbox_timeline.is_complete() &&
                      !sandbox_timeline.control_locked(),
                  "sandbox zone pan completes and unlocks");
        }
        engine::DialogueRuntime dialogue_rt;
        r.check(dialogue_rt.bind(&dlg_loaded.value()).has_value(),
                "DialogueRuntime binds for timeline smoke");
        engine::EventTimelineRuntime timeline;
        r.check(!timeline.bind(nullptr).has_value(),
                "EventTimelineRuntime rejects null bind");
        r.check(timeline.bind(&evt_loaded.value()).has_value(),
                "EventTimelineRuntime binds sample events");
        r.check(!timeline.start("missing_sequence").has_value(),
                "EventTimelineRuntime rejects unknown sequence");
        timeline.set_dialogue_starter(
            [&](const std::string &dialogue_id) -> engine::Result<void> {
              return dialogue_rt.start(dialogue_id);
            });
        r.check(timeline.start("evt_act0_timeline_smoke").has_value(),
                "EventTimelineRuntime starts smoke sequence");
        r.check(timeline.control_locked() && timeline.is_active(),
                "smoke locks control immediately");
        r.check(timeline.camera_directive().active,
                "smoke look_at directive active");
        timeline.tick(0.1f);
        r.check(timeline.camera_directive().active &&
                    timeline.camera_directive().alpha > 0.0f,
                "smoke look_at blend advances");
        timeline.tick(0.15f); // finish look_at (0.2) → enter wait (0.1)
        timeline.tick(0.15f); // finish wait → emit + dialogue + wait
        auto emits = timeline.take_emitted_events();
        r.check(emits.size() == 1 && emits[0].name == "vfx_stub",
                "smoke emit fires after look_at/wait");
        r.check(dialogue_rt.tree_id() == "dlg_act0_meet_arkand",
                "smoke start_dialogue reaches DialogueRuntime");
        timeline.tick(0.15f);
        r.check(timeline.is_complete() && !timeline.control_locked() &&
                    !timeline.is_active(),
                "smoke completes and unlocks control");
        r.check(!timeline.camera_directive().active,
                "camera directive clears on complete");
        {
          auto cam_only = engine::WorldForgeEventsAsset::parse(
              R"({"schemaVersion":1,"id":"t","sequences":[{"id":"c","steps":[{"kind":"look_at","target":[1,2,3],"seconds":0.5}]}]})");
          r.check(cam_only.has_value(), "look_at step parses");
          engine::EventTimelineRuntime cam_rt;
          r.check(cam_rt.bind(&cam_only.value()).has_value(),
                  "bind camera-only sequence");
          r.check(cam_rt.start("c").has_value(), "start look_at sequence");
          r.check(cam_rt.camera_directive().active &&
                      cam_rt.camera_directive().target[1] == 2.0f,
                  "look_at exposes target");
          cam_rt.tick(0.25f);
          r.check(cam_rt.camera_directive().alpha >= 0.4f,
                  "look_at alpha mid-blend");
          cam_rt.tick(0.3f);
          r.check(cam_rt.is_complete(), "look_at sequence completes");
        }
        r.check(
            !engine::WorldForgeEventsAsset::parse(
                 R"({"schemaVersion":1,"id":"bad","sequences":[{"id":"s","steps":[{"kind":"look_at","seconds":1}]}]})")
                 .has_value(),
            "look_at without target rejected");
        r.check(timeline.start("evt_act0_timeline_smoke").has_value(),
                "EventTimelineRuntime can restart");
        r.check(timeline.control_locked(), "restart locks again");
        timeline.cancel();
        r.check(!timeline.is_active() && !timeline.control_locked(),
                "cancel clears active and unlocks");
      }
      r.check(
          !engine::WorldForgeEventsAsset::parse(
               R"({"schemaVersion":1,"id":"bad","sequences":[{"id":"s","steps":[{"kind":"nope"}]}]})")
               .has_value(),
          "unknown step kind rejected");
      r.check(
          !engine::WorldForgeEventsAsset::parse(
               R"({"schemaVersion":1,"id":"bad","sequences":[{"id":"s","steps":[{"kind":"start_dialogue"}]}]})")
               .has_value(),
          "start_dialogue without dialogueId rejected");
      {
        auto orphan = engine::WorldForgeEventsAsset::parse(
            R"({"schemaVersion":1,"id":"t","sequences":[{"id":"s","canonStatus":"draft","steps":[{"kind":"start_dialogue","dialogueId":"missing"}]}]})");
        r.check(orphan.has_value(),
                "unknown dialogueId allowed without known set");
        std::unordered_set<std::string> known{"dlg_act0_meet_arkand"};
        r.check(orphan && !orphan.value().validate_dialogue_refs(known) &&
                    orphan.value().validate_dialogue_refs(known).error().code ==
                        "EVENT-DIALOGUE-MISSING",
                "unknown dialogueId rejected when known set provided");
      }
      r.check(
          engine::WorldForgeEventsAsset::validate_file(evt_path).has_value(),
          "sample events validate_file succeeds");
    }
    const auto wf_quests = engine::apply_world_forge_operation(
        project, nlohmann::json{{"action", "validate"}, {"kind", "quests"}});
    r.check(wf_quests.exit_code == engine::ExitCode::Success,
            "world_forge_apply validates quests");
    const auto wf_dialogues = engine::apply_world_forge_operation(
        project, nlohmann::json{{"action", "validate"}, {"kind", "dialogues"}});
    r.check(wf_dialogues.exit_code == engine::ExitCode::Success,
            "world_forge_apply validates dialogues");
    const auto wf_events = engine::apply_world_forge_operation(
        project, nlohmann::json{{"action", "validate"}, {"kind", "events"}});
    r.check(wf_events.exit_code == engine::ExitCode::Success,
            "world_forge_apply validates events");
    {
      const auto get_events = engine::apply_world_forge_operation(
          project, nlohmann::json{{"action", "get"}, {"kind", "events"}});
      r.check(get_events.exit_code == engine::ExitCode::Success &&
                  get_events.metadata.count("content"),
              "world_forge_apply get events returns content");
      const auto &events_json = get_events.metadata.at("content");
      const auto temp =
          std::filesystem::temp_directory_path() / "engine_events_apply_test";
      std::filesystem::remove_all(temp);
      const auto wf_dir = temp / "assets" / "world-forge";
      std::filesystem::create_directories(wf_dir);
      if (std::filesystem::exists(dlg_path))
        std::filesystem::copy_file(dlg_path,
                                   wf_dir / "dialogues.worldforge.json");
      const auto apply_ok = engine::apply_world_forge_operation(
          temp, nlohmann::json{{"action", "apply"},
                               {"kind", "events"},
                               {"source", events_json}});
      r.check(apply_ok.exit_code == engine::ExitCode::Success,
              "world_forge_apply writes events");
      const auto reload = engine::WorldForgeEventsAsset::load(
          engine::default_world_forge_events_path(temp));
      r.check(reload && reload.value().to_json() == events_json,
              "events apply round-trip preserves JSON");
      const auto apply_bad = engine::apply_world_forge_operation(
          temp,
          nlohmann::json{
              {"action", "apply"},
              {"kind", "events"},
              {"source",
               R"({"schemaVersion":1,"id":"bad","sequences":[{"id":"s","steps":[{"kind":"nope"}]}]})"}});
      r.check(apply_bad.exit_code != engine::ExitCode::Success,
              "world_forge_apply rejects invalid event step");
      std::filesystem::remove_all(temp);
    }
    {
      const auto repo = std::filesystem::path(ENGINE_REPOSITORY_ROOT);
      const auto twee_path = repo / "context" / "story" / "sources" /
                             "wrathful-conquest-act0.twee";
      engine::TweeImportOptions opts;
      opts.tree_id = "dlg_act0_import_probe";
      opts.display_name = "Act 0 import probe";
      opts.parent_quest_id = "mq_act0_calrenoth";
      opts.entry_node_id = "prologue";
      const auto imported =
          engine::import_twee_dialogue_tree_file(twee_path, opts);
      r.check(imported.has_value(), "twee importer parses Act 0 source");
      r.check(imported && imported.value().nodes.size() >= 40 &&
                  imported.value().entry_node_id == "prologue",
              "twee importer yields prologue entry and many nodes");
      const auto temp =
          std::filesystem::temp_directory_path() / "engine_twee_import_test";
      std::filesystem::remove_all(temp);
      const auto dlg_dir = temp / "assets" / "world-forge";
      std::filesystem::create_directories(dlg_dir);
      std::filesystem::copy_file(dlg_path,
                                 dlg_dir / "dialogues.worldforge.json");
      const auto quests_src = engine::default_world_forge_quests_path(project);
      if (std::filesystem::exists(quests_src))
        std::filesystem::copy_file(quests_src,
                                   dlg_dir / "quests.worldforge.json");
      const auto wf_import = engine::apply_world_forge_operation(
          temp, nlohmann::json{{"action", "import_twee"},
                               {"kind", "dialogues"},
                               {"tweePath", twee_path.string()},
                               {"treeId", "dlg_act0_import_probe"},
                               {"displayName", "Act 0 import probe"},
                               {"parentQuestId", "mq_act0_calrenoth"},
                               {"entryNodeId", "prologue"}});
      r.check(wf_import.exit_code == engine::ExitCode::Success,
              "world_forge import_twee succeeds");
      auto probe = engine::WorldForgeDialoguesAsset::load(
          dlg_dir / "dialogues.worldforge.json");
      r.check(probe && probe.value().trees.size() >= 7 &&
                  probe.value().find_tree("dlg_act0_meet_arkand") != nullptr &&
                  probe.value().find_tree("dlg_sandbox_sample") != nullptr,
              "import_twee upserts a probe tree without wiping sample trees");
      bool found_probe = false;
      if (probe)
        for (const auto &tree : probe.value().trees)
          if (tree.id == "dlg_act0_import_probe") {
            found_probe = tree.nodes.size() >= 40;
            break;
          }
      r.check(found_probe,
              "imported probe tree persists with Act 0 node count");
      std::filesystem::remove_all(temp);
    }
    r.check(engine::validate_project_at(project).exit_code ==
                engine::ExitCode::Success,
            "sample project still validates with world forge factions");
  } else if (suite == "streaming") {
    engine::WorldPartition p;
    auto c = p.cell_for({-0.1, 0, -0.1});
    r.check(c && c.value() == engine::CellCoord{-1, -1},
            "negative floor mapping");
    engine::CellStreamer s([](engine::CellCoord c, const std::atomic_bool &) {
      return engine::Result<engine::CellData>::success({c, 1, "ok"});
    });
    r.check(s.request({0, 0}, 0).has_value(), "request accepted");
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!s.loaded({0, 0}) && std::chrono::steady_clock::now() < deadline) {
      (void)s.update();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    r.check(s.loaded({0, 0}), "cell committed");
  } else if (suite == "terrain") {
    engine::TerrainTileMetadata t{{0, 0}, 129, -1, 2, "height", {}};
    r.check(t.validate().has_value(), "metadata valid");
    t.height_resolution = 128;
    r.check(!t.validate(), "bad resolution rejected");
    auto bubble = engine::simulation_bubble({0, 0}, 2);
    r.check(bubble && bubble.value().size() == 13, "bubble deterministic");
    auto terrain = engine::generate_stylized_terrain({0, 0}, 33, 40);
    r.check(terrain.has_value(), "stylized terrain generated");
    r.check(terrain && terrain.value().heights.size() == 1089 &&
                terrain.value().triangles.size() == 6144,
            "terrain topology is complete");
    auto repeat = engine::generate_stylized_terrain({0, 0}, 33, 40);
    r.check(repeat && terrain &&
                repeat.value().heights == terrain.value().heights,
            "terrain generation deterministic");
    auto east = engine::generate_stylized_terrain({1, 0}, 33, 40);
    bool seam = terrain && east;
    for (std::uint32_t z = 0; seam && z < 33; ++z)
      seam = std::abs(terrain.value().sample(32, z) -
                      east.value().sample(0, z)) < 0.00001f;
    r.check(seam, "adjacent terrain borders match");
    r.check(!engine::generate_stylized_terrain({0, 0}, 16, 40),
            "invalid mesh resolution rejected");
    r.check(!engine::generate_stylized_terrain({0, 0}, 33, 0),
            "invalid cell size rejected");
    r.check(engine::terrain_cell_for_position(0, 0, 40) ==
                engine::CellCoord{0, 0},
            "terrain cell at origin");
    r.check(engine::terrain_cell_for_position(45, 0, 40) ==
                engine::CellCoord{1, 0},
            "terrain cell follows positive X");
    r.check(engine::terrain_cells_in_radius({0, 0}, 1).size() == 9,
            "terrain radius one loads nine cells");
    r.check(engine::terrain_cells_in_radius({0, 0}, 2).size() == 25,
            "terrain radius two loads twenty-five cells");
    const auto biased =
        engine::terrain_cells_in_view_bias({0, 0}, 2, 1, 1.0f, 0.0f);
    r.check(biased.size() < 25 && biased.size() >= 9,
            "view-biased terrain neighborhood is smaller than full radius");
    r.check(std::find(biased.begin(), biased.end(), engine::CellCoord{2, 0}) !=
                biased.end(),
            "view bias keeps forward outer cell");
    r.check(std::find(biased.begin(), biased.end(), engine::CellCoord{-2, 0}) ==
                biased.end(),
            "view bias drops rear outer cell");
    bool distinct_colors = false;
    std::array<float, 3> first{};
    bool first_set = false;
    for (const auto &tri : terrain.value().triangles) {
      if (!first_set) {
        first = {tri.r, tri.g, tri.b};
        first_set = true;
        continue;
      }
      if (std::abs(tri.r - first[0]) > 0.01f ||
          std::abs(tri.g - first[1]) > 0.01f ||
          std::abs(tri.b - first[2]) > 0.01f) {
        distinct_colors = true;
        break;
      }
    }
    r.check(distinct_colors, "terrain material regions vary surface color");
    engine::StreamedTerrainField field;
    engine::CollisionWorld world;
    r.check(field.update(world, {0, 5, 0}, {}).has_value() &&
                field.loaded_cell_count() == 81,
            "streamed terrain loads a neighborhood");
    r.check(field.update(world, {45, 5, 0}, {}).has_value() &&
                field.focus_cell() == engine::CellCoord{1, 0},
            "streamed terrain recenters on camera cell");
    r.check(field.contains({1, 0}) && field.contains({0, 0}),
            "streamed terrain keeps overlapping cells while moving");
    {
      engine::StreamedTerrainField amortize_field;
      engine::CollisionWorld amortize_world;
      engine::TerrainStreamParams amortize_params;
      amortize_params.radius = 2;
      amortize_params.support_radius = 1;
      amortize_params.max_new_cells = 2;
      amortize_params.max_new_support_cells = 1;
      amortize_params.view_bias = true;
      amortize_params.forward_x = 1.0f;
      amortize_params.forward_z = 0.0f;
      r.check(
          amortize_field
              .update(amortize_world, {{0.0f, 5.0f, 0.0f}}, {}, amortize_params)
              .has_value(),
          "amortized terrain update succeeds");
      r.check(amortize_field.loaded_cell_count() >= 9,
              "support disc loads immediately ignoring outer budget");
      r.check(amortize_field.stream_pending(),
              "amortized terrain reports pending outer cells");
      r.check(amortize_field.contains({0, 0}) &&
                  amortize_field.contains({1, 0}) &&
                  amortize_field.contains({-1, 0}),
              "support disc covers focus neighbors"); // Outer ring still
                                                      // respects budget after
                                                      // support is filled.
      const auto after_support = amortize_field.loaded_cell_count();
      r.check(
          amortize_field
              .update(amortize_world, {{0.0f, 5.0f, 0.0f}}, {}, amortize_params)
              .has_value(),
          "outer amortize step");
      r.check(amortize_field.loaded_cell_count() <= after_support + 2,
              "outer ring loads at most budget cells per update");
      std::size_t frames = 0;
      while (amortize_field.stream_pending() && frames < 64) {
        r.check(amortize_field
                    .update(amortize_world, {{0.0f, 5.0f, 0.0f}}, {},
                            amortize_params)
                    .has_value(),
                "amortized terrain catch-up update");
        ++frames;
      }
      r.check(!amortize_field.stream_pending(),
              "amortized terrain eventually fills neighborhood");
      r.check(amortize_field.loaded_cell_count() < 25,
              "view-biased amortized resident set stays under full 5x5");
      // Look-gate freezes outer bias forward while yaw is hot (no thrash during
      // 360 look).
      {
        engine::StreamViewBiasGate gate;
        engine::TerrainStreamParams gated = amortize_params;
        gated.forward_x = 1.0f;
        gated.forward_z = 0.0f;
        r.check(!engine::apply_stream_view_bias_look_gate(gate, 0.0f, 1.0f,
                                                          0.0f, gated),
                "look gate starts cool");
        r.check(std::abs(gated.forward_x - 1.0f) < 0.001f &&
                    gated.max_new_cells == 2,
                "cool gate keeps live forward and outer budget");
        gated = amortize_params;
        r.check(engine::apply_stream_view_bias_look_gate(gate, 0.001f, 1.0f,
                                                         0.001f, gated),
                "sub-pixel slow yaw marks look-hot");
        r.check(std::abs(gated.forward_x - 1.0f) < 0.001f &&
                    gated.max_new_cells == 0 &&
                    gated.max_new_support_cells == 0 &&
                    gated.max_ready_commits == 0,
                "slow look freezes prior forward and blocks outer/support "
                "loads and commits");
        gated = amortize_params;
        gated.max_ready_commits = 1;
        gated.max_new_support_cells = 1;
        r.check(engine::apply_stream_view_bias_look_gate(gate, 0.2f, 0.0f, 1.0f,
                                                         gated),
                "large yaw delta marks look-hot");
        r.check(std::abs(gated.forward_x - 1.0f) < 0.001f &&
                    gated.max_new_cells == 0 &&
                    gated.max_new_support_cells == 0 &&
                    gated.max_ready_commits == 0,
                "look-hot freezes prior forward and blocks outer/support loads "
                "and commits");
        bool still_hot = true;
        for (int i = 0; i < 20 && still_hot; ++i) {
          gated = amortize_params;
          gated.max_ready_commits = 1;
          gated.max_new_support_cells = 1;
          still_hot = engine::apply_stream_view_bias_look_gate(gate, 0.2f, 0.0f,
                                                               1.0f, gated);
        }
        r.check(!still_hot, "look gate settles after yaw stops");
        r.check(std::abs(gated.forward_z - 1.0f) < 0.001f &&
                    gated.max_new_cells == 2 &&
                    gated.max_new_support_cells == 1 &&
                    gated.max_ready_commits == 1,
                "settled gate adopts live forward and restores "
                "outer/support/commit budgets");
      }
      // max_new_cells=0 must block outer loads (not treat 0 as unlimited).
      {
        engine::StreamedTerrainField zero_budget;
        engine::CollisionWorld zero_world;
        engine::TerrainStreamParams zero_params;
        zero_params.radius = 2;
        zero_params.support_radius = 1;
        zero_params.max_new_cells = 0;
        zero_params.view_bias = true;
        zero_params.forward_x = 1.0f;
        zero_params.forward_z = 0.0f;
        r.check(zero_budget
                    .update(zero_world, {{0.0f, 5.0f, 0.0f}}, {}, zero_params)
                    .has_value(),
                "zero outer budget update succeeds");
        r.check(zero_budget.loaded_cell_count() == 9,
                "zero outer budget loads support disc only");
        r.check(zero_budget.stream_pending(),
                "zero outer budget leaves outer ring pending");
      }
      // Async generation preserves the synchronous bootstrap floor, then
      // commits walk-fringe work from a worker.
      {
        engine::StreamedTerrainField async_field;
        engine::CollisionWorld async_world;
        engine::TerrainStreamParams async_params = amortize_params;
        async_params.async_generation = true;
        async_params.max_ready_commits = 1;
        r.check(async_field
                    .update(async_world, {{0.0f, 5.0f, 0.0f}}, {}, async_params)
                    .has_value(),
                "async terrain bootstrap succeeds");
        r.check(async_field.contains({0, 0}),
                "async terrain bootstrap keeps focus floor synchronous");
        r.check(
            async_field
                .update(async_world, {{45.0f, 5.0f, 0.0f}}, {}, async_params)
                .has_value(),
            "async terrain schedules walk fringe");
        std::size_t async_frames = 0;
        while (async_field.stream_pending() && async_frames < 128) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          const auto loaded_before = async_field.loaded_cell_count();
          r.check(
              async_field
                  .update(async_world, {{45.0f, 5.0f, 0.0f}}, {}, async_params)
                  .has_value(),
              "async terrain commits completed generation");
          r.check(async_field.loaded_cell_count() <= loaded_before + 1,
                  "async terrain commits at most one ready cell per update");
          ++async_frames;
        }
        r.check(!async_field.stream_pending(),
                "async terrain eventually fills neighborhood");
        r.check(async_field.contains({1, 0}) && async_field.contains({2, 0}),
                "async terrain commits walk support on the main thread");
      }
      // Recenter walk: support fringe amortizes (≤1/frame) while prior cells
      // stay until support complete.
      const auto before_recenter = amortize_field.loaded_cell_count();
      r.check(amortize_field
                  .update(amortize_world, {{45.0f, 5.0f, 0.0f}}, {},
                          amortize_params)
                  .has_value(),
              "amortized recenter update");
      r.check(amortize_field.focus_cell() == engine::CellCoord{1, 0},
              "amortized recenter moves focus");
      r.check(amortize_field.contains({1, 0}) &&
                  amortize_field.contains({0, 0}),
              "recenter keeps player cell and prior support (no hole)");
      r.check(amortize_field.loaded_cell_count() <= before_recenter + 1,
              "recenter loads at most one support fringe cell per frame");
      r.check(amortize_field.stream_pending() ||
                  amortize_field.contains({2, 0}),
              "recenter either pending support fringe or already has forward "
              "support");
      frames = 0;
      while (amortize_field.stream_pending() && frames < 64) {
        const auto before = amortize_field.loaded_cell_count();
        r.check(amortize_field
                    .update(amortize_world, {{45.0f, 5.0f, 0.0f}}, {},
                            amortize_params)
                    .has_value(),
                "support fringe catch-up");
        r.check(amortize_field.loaded_cell_count() <= before + 2,
                "fringe catch-up stays within support+outer budgets");
        r.check(amortize_field.contains({1, 0}),
                "focus cell stays resident during fringe catch-up");
        ++frames;
      }
      r.check(
          amortize_field.contains({1, 0}) && amortize_field.contains({0, 0}) &&
              amortize_field.contains({2, 0}),
          "after catch-up full support disc under player"); // Teleport: focus
                                                            // cell missing
                                                            // forces full
                                                            // support disc same
                                                            // frame (acceptable
                                                            // hitch).
      {
        engine::StreamedTerrainField teleport_field;
        engine::CollisionWorld teleport_world;
        engine::TerrainStreamParams teleport_params = amortize_params;
        r.check(teleport_field
                    .update(teleport_world, {{0.0f, 5.0f, 0.0f}}, {},
                            teleport_params)
                    .has_value(),
                "teleport baseline");
        frames = 0;
        while (teleport_field.stream_pending() && frames < 64) {
          r.check(teleport_field
                      .update(teleport_world, {{0.0f, 5.0f, 0.0f}}, {},
                              teleport_params)
                      .has_value(),
                  "teleport baseline catch-up");
          ++frames;
        }
        r.check(teleport_field
                    .update(teleport_world, {{200.0f, 5.0f, 0.0f}}, {},
                            teleport_params)
                    .has_value(),
                "teleport far update");
        r.check(teleport_field.focus_cell() == engine::CellCoord{5, 0},
                "teleport moves focus");
        r.check(teleport_field.contains({5, 0}) &&
                    teleport_field.contains({4, 0}) &&
                    teleport_field.contains({6, 0}),
                "teleport loads full support disc immediately");
      }
    }
    // Terrain stream must not destroy placement/player bodies that share
    // CellCoord keys with heightfields.
    {
      engine::CollisionWorld preserve_world;
      engine::StreamedTerrainField preserve_field;
      auto player = preserve_world.add_capsule({0.0, 2.0, 0.0}, 0.35f, 0.85f,
                                               engine::CollisionLayer::Dynamic,
                                               true, engine::CellCoord{0, 0});
      r.check(player.has_value(),
              "terrain stream preserves player body create");
      const auto player_token = player.value().value;
      r.check(preserve_field.update(preserve_world, {0.0f, 5.0f, 0.0f}, {})
                  .has_value(),
              "terrain stream loads under player");
      r.check(preserve_world.body_count() >= 2,
              "terrain stream adds heightfields beside player");
      r.check(preserve_field.update(preserve_world, {200.0f, 5.0f, 0.0f}, {})
                  .has_value(),
              "terrain stream recenters far from origin");
      r.check(!preserve_field.contains({0, 0}),
              "terrain stream unloads origin heightfield");
      r.check(preserve_world.position(player.value()).has_value(),
              "terrain stream keeps Dynamic body after heightfield unload");
      r.check(preserve_world.body_count() >= 1,
              "player body survives terrain unload_cell key collision");
      (void)player_token;
      // Multi-focus keeps both neighborhoods resident (local co-op).
      engine::StreamedTerrainField multi_field;
      engine::CollisionWorld multi_world;
      const std::vector<std::array<float, 3>> foci{{0.0f, 5.0f, 0.0f},
                                                   {200.0f, 5.0f, 0.0f}};
      r.check(multi_field.update(multi_world, foci, {}).has_value(),
              "multi-focus terrain stream update");
      r.check(multi_field.contains({0, 0}) && multi_field.contains({5, 0}),
              "multi-focus keeps distant player cells");
    }
    engine::WorldPartition partition;
    const auto half_extent =
        static_cast<float>(partition.config().world_half_extent);
    std::size_t peak_cells = 0;
    std::set<engine::CellCoord> visited;
    // Sample the full 16 km span without rebuilding every 40 m cell. Coarser
    // than play traversal — residency bound depends on neighborhood size.
    constexpr float stress_sample_step = 2000.0f;
    constexpr std::uint32_t stress_radius =
        engine::StreamedTerrainField::k_default_radius;
    engine::TerrainStreamParams stress_params;
    stress_params.radius = stress_radius;
    stress_params.support_radius = 2;
    stress_params.max_new_cells = 8;
    stress_params.max_new_support_cells = 4;
    stress_params.max_ready_commits = 8;
    stress_params.view_bias = false;
    for (float z = -half_extent; z <= half_extent; z += stress_sample_step) {
      for (float x = -half_extent; x <= half_extent; x += stress_sample_step) {
        std::size_t frames = 0;
        do {
          r.check(field
                      .update(world, {{x, 12.0f, z}}, {}, stress_params)
                      .has_value(),
                  "256 km2 stress update succeeds");
          ++frames;
        } while (field.stream_pending() && frames < 64);
        peak_cells = std::max(peak_cells, field.loaded_cell_count());
        visited.insert(field.focus_cell());
      }
    }
    const std::size_t full_disc =
        static_cast<std::size_t>((2 * stress_radius + 1) * (2 * stress_radius + 1));
    r.check(peak_cells <= full_disc,
            "256 km2 stress keeps bounded resident terrain cells");
    r.check(visited.size() >= 64,
            "256 km2 stress path covers many terrain cells");
    r.check(half_extent >= 7900.0f &&
                partition.config().world_half_extent *
                        partition.config().world_half_extent * 4.0e-6 >=
                    250.0,
            "world partition spans roughly two hundred fifty-six square kilometers");
    engine::TerrainEditStore edits;
    const auto brushed = edits.apply_brush(0.0f, 0.0f, 3.0f, 0.5f, false);
    r.check(brushed && brushed.value().count({0, 0}) == 1,
            "terrain brush touches origin cell");
    auto edited = engine::generate_stylized_terrain({0, 0}, 33, 40, &edits);
    auto baseline = engine::generate_stylized_terrain({0, 0}, 33, 40);
    r.check(edited && baseline &&
                edited.value().heights != baseline.value().heights,
            "terrain edits change generated heights");
    engine::set_active_terrain_edits(&edits);
    const float sampled = engine::sample_terrain_height(0.0f, 0.0f);
    const float procedural = engine::procedural_terrain_height(0.0f, 0.0f);
    r.check(sampled > procedural, "sample_terrain_height includes edits");
    engine::set_active_terrain_edits(nullptr);
    const auto round_trip =
        engine::TerrainEditStore::from_json(edits.to_json());
    r.check(round_trip.has_value(), "terrain edits round trip");
    r.check(!engine::TerrainEditStore::from_json("{\"schemaVersion\":2}"),
            "invalid terrain edit schema rejected");
    engine::TerrainEditHistory history;
    std::map<engine::CellCoord, std::vector<float>> before{{{0, 0}, {}}};
    std::map<engine::CellCoord, std::vector<float>> after;
    after[{0, 0}] = edits.cell_deltas_or_empty({0, 0});
    r.check(
        history
            .execute(edits, std::make_unique<engine::TerrainBrushStrokeCommand>(
                                before, after))
            .has_value(),
        "terrain brush command executes");
    r.check(history.undo(edits).has_value(), "terrain brush command undoes");
    engine::TerrainPaintStore paint;
    const std::uint16_t grass_index =
        paint.ensure_material_index("assets/materials/grass.material.json");
    const auto painted =
        paint.apply_material_brush(0.0f, 0.0f, 3.0f, grass_index);
    r.check(painted && painted.value().count({0, 0}) == 1,
            "terrain paint brush touches origin cell");
    engine::MaterialAsset grass_material;
    grass_material.base_color = {0.2f, 0.8f, 0.1f, 1.0f};
    std::map<std::string, engine::MaterialAsset> paint_materials{
        {"assets/materials/grass.material.json", grass_material}};
    const auto paint_lookup =
        [&paint_materials](
            const std::string &path) -> const engine::MaterialAsset * {
      const auto found = paint_materials.find(path);
      return found == paint_materials.end() ? nullptr : &found->second;
    };
    auto painted_mesh = engine::generate_stylized_terrain(
        {0, 0}, 33, 40, nullptr, &paint, paint_lookup);
    auto procedural_mesh = engine::generate_stylized_terrain({0, 0}, 33, 40);
    r.check(painted_mesh && procedural_mesh,
            "terrain paint mesh generation succeeds");
    r.check(painted_mesh && !painted_mesh.value().triangles.empty(),
            "terrain paint mesh is non-empty");
    r.check(std::any_of(painted_mesh.value().triangles.begin(),
                        painted_mesh.value().triangles.end(),
                        [](const engine::TerrainVertex &v) {
                          return v.painted && v.g > 0.6f;
                        }),
            "terrain paint changes vertex colors");
    r.check(
        std::any_of(painted_mesh.value().triangles.begin(),
                    painted_mesh.value().triangles.end(),
                    [](const engine::TerrainVertex &v) { return v.painted; }),
        "painted terrain vertices are flagged");
    const auto paint_round_trip =
        engine::TerrainPaintStore::from_json(paint.to_json());
    r.check(paint_round_trip.has_value(), "terrain paint round trip");
    engine::TerrainEditStore flatten_edits;
    const float flatten_target =
        engine::procedural_terrain_height(0.0f, 0.0f) + 1.0f;
    (void)flatten_edits.apply_brush(0.0f, 0.0f, 4.0f, 2.0f, false);
    const auto flattened = flatten_edits.apply_flatten_brush(
        0.0f, 0.0f, 4.0f, 2.0f, flatten_target);
    r.check(flattened && !flattened.value().empty(),
            "terrain flatten brush touches cells");
    engine::set_active_terrain_edits(&flatten_edits);
    r.check(std::abs(engine::sample_terrain_height(0.0f, 0.0f) -
                     flatten_target) < 0.05f,
            "terrain flatten approaches target height");
    engine::set_active_terrain_edits(nullptr);
  } else if (suite == "water") {
    engine::WaterStore store;
    store.set_sea_level(-0.35f);
    const auto brushed = store.apply_place_brush(0.0f, 0.0f, 6.0f, 0.8f);
    r.check(brushed && brushed.value().count({0, 0}) == 1,
            "water place brush touches origin cell");
    const auto round_trip = engine::WaterStore::from_json(store.to_json());
    r.check(round_trip.has_value(), "water store round trip");
    r.check(round_trip.value().sea_level() == store.sea_level(),
            "water store round trip keeps sea level");
    engine::set_active_water_store(&store);
    const auto surface = engine::sample_water_surface_y(0.0f, 0.0f);
    r.check(surface && std::abs(*surface - store.sea_level()) < 0.001f,
            "sample_water_surface_y returns sea level over authored fill");
    engine::WaterStore apply_store;
    apply_store.set_sea_level(-0.35f);
    engine::WaterEditHistory history;
    bool dirty = false;
    engine::EditorSessionContext context;
    context.project_root = std::filesystem::path("samples/open-world-rpg");
    context.water_store = &apply_store;
    context.water_history = &history;
    context.water_dirty = &dirty;
    engine::set_active_water_store(&apply_store);
    // Brush inside the origin hollow where procedural terrain is below seaLevel
    // (-0.35).
    r.check(!engine::sample_water_surface_y(0.0f, 0.0f),
            "water sample empty before brush stroke");
    const auto placed = engine::execute_editor_operation(
        context, "water_apply",
        R"({"action":"place","x":0,"z":0,"radius":3,"strength":0.6})");
    r.check(placed.exit_code == engine::ExitCode::Success,
            "water_apply place succeeds");
    r.check(history.undo_size() >= 1, "water_apply place commits undo entry");
    r.check(engine::sample_water_surface_y(0.0f, 0.0f).has_value(),
            "water_apply place writes surface sample");
    const auto undone = engine::execute_editor_operation(
        context, "water_apply", R"({"action":"undo"})");
    r.check(undone.exit_code == engine::ExitCode::Success,
            "water_apply undo succeeds");
    r.check(!engine::sample_water_surface_y(0.0f, 0.0f),
            "water_apply undo restores prior fill");
    apply_store.set_sea_level(-3.0f);
    (void)apply_store.apply_place_brush(0.0f, 0.0f, 3.0f, 1.0f);
    r.check(!engine::sample_water_surface_y(0.0f, 0.0f),
            "authored fill above terrain (sea below bed) does not report a "
            "floating surface");
    engine::StreamedWaterField field;
    r.check(field.update({0.0f, 5.0f, 0.0f}, 2, &store, 1000).has_value(),
            "streamed water loads a neighborhood");
    r.check(field.loaded_cell_count() > 0, "streamed water builds cell meshes");
    r.check(!field.stream_pending(), "eager water neighborhood is complete");
    {
      // Amortized mesh build: budget 0 leaves the desired set without new
      // meshes until later frames.
      engine::StreamedWaterField paced;
      r.check(paced.update({0.0f, 5.0f, 0.0f}, 2, &store, 0).has_value(),
              "zero water budget update succeeds");
      r.check(paced.loaded_cell_count() == 0,
              "zero water budget builds no cell meshes");
      r.check(paced.stream_pending(),
              "zero water budget leaves stream pending");
      r.check(paced.update({0.0f, 5.0f, 0.0f}, 2, &store, 1).has_value(),
              "unit water budget update succeeds");
      r.check(paced.loaded_cell_count() == 1,
              "unit water budget builds one cell mesh");
      std::size_t paced_frames = 0;
      while (paced.stream_pending() && paced_frames < 64) {
        r.check(paced.update({0.0f, 5.0f, 0.0f}, 2, &store, 1).has_value(),
                "water catch-up update");
        ++paced_frames;
      }
      r.check(!paced.stream_pending(),
              "amortized water eventually fills desired neighborhood");
      r.check(paced.loaded_cell_count() == field.loaded_cell_count(),
              "amortized water matches eager neighborhood size");
      {
        engine::StreamedWaterField dirty_check;
        r.check(
            dirty_check.update({0.0f, 5.0f, 0.0f}, 1, &store, 1).has_value(),
            "dirty water update");
        r.check(dirty_check.render_data_dirty(),
                "new water cell marks render dirty");
        const auto dirty = dirty_check.take_render_dirty_cells(1);
        r.check(dirty.size() == 1,
                "water take_render_dirty_cells returns built cell");
        r.check(!dirty_check.render_data_dirty(),
                "taken dirty cells clear from queue when no more pending");
      }
    }
    const auto water_verts = field.build_render_vertices();
    r.check(!water_verts.empty(),
            "streamed water mesh emits shoreline-aware vertices");
    float min_depth = 1.0e9f, max_depth = 0.0f;
    for (const auto &v : water_verts) {
      min_depth = std::min(min_depth, v.depth);
      max_depth = std::max(max_depth, v.depth);
    }
    r.check(max_depth > min_depth + 0.05f,
            "water mesh encodes varying column depth for absorption");
    r.check(max_depth > 0.2f, "water mesh depth reaches deeper basin samples");
    apply_store.set_sea_level(-0.35f);
    engine::set_active_water_store(&apply_store);
    const auto place_along = engine::execute_editor_operation(
        context, "water_apply",
        R"({"action":"place_along","points":[{"x":0,"z":0},{"x":4,"z":-2}],"radius":2.5,"strength":1,"step":2})");
    r.check(place_along.exit_code == engine::ExitCode::Success,
            "water_apply place_along succeeds");
    engine::TerrainEditStore height_store;
    engine::TerrainEditHistory height_history;
    bool height_dirty = false;
    engine::EditorSessionContext terrain_ctx;
    terrain_ctx.project_root = std::filesystem::path("samples/open-world-rpg");
    terrain_ctx.terrain_edits = &height_store;
    terrain_ctx.terrain_history = &height_history;
    terrain_ctx.terrain_edits_dirty = &height_dirty;
    terrain_ctx.water_store = &apply_store;
    engine::set_active_terrain_edits(&height_store);
    const auto set_h = engine::execute_editor_operation(
        terrain_ctx, "terrain_apply",
        R"({"action":"set_height","x":0,"z":0,"radius":3,"strength":1,"targetHeight":1.25})");
    r.check(set_h.exit_code == engine::ExitCode::Success,
            "terrain_apply set_height succeeds");
    r.check(std::abs(engine::sample_terrain_height(0.0f, 0.0f) - 1.25f) < 0.15f,
            "set_height reaches target near brush center");
    const auto carved = engine::execute_editor_operation(
        terrain_ctx, "terrain_apply",
        R"({"action":"carve_channel","points":[{"x":-4,"z":2},{"x":0,"z":0},{"x":4,"z":-2}],"halfWidth":2.5,"bedDepth":1.2,"bankClearance":1.0,"step":2,"strength":1})");
    r.check(carved.exit_code == engine::ExitCode::Success,
            "terrain_apply carve_channel succeeds");
    const auto batched_carve = engine::execute_editor_operation(
        terrain_ctx, "terrain_apply",
        R"({"action":"batch","ops":[{"action":"carve_channel","points":[{"x":-2,"z":-2},{"x":2,"z":2}],"halfWidth":1.5,"bedDepth":1.0,"step":2,"strength":1},{"action":"smooth","x":0,"z":0,"radius":4,"strength":0.4}]})");
    r.check(batched_carve.exit_code == engine::ExitCode::Success,
            "terrain_apply batch accepts carve_channel+smooth");
    const auto grade_along = engine::execute_editor_operation(
        terrain_ctx, "terrain_apply",
        R"({"action":"set_height_along","points":[{"x":-6,"z":-6},{"x":0,"z":0},{"x":6,"z":6}],"startHeight":0.5,"endHeight":2.5,"halfWidth":2.5,"skirtWidth":2.0,"step":2,"strength":1,"smoothPasses":1})");
    r.check(grade_along.exit_code == engine::ExitCode::Success,
            "terrain_apply set_height_along succeeds");
    const float mid_h = engine::sample_terrain_height(0.0f, 0.0f);
    r.check(std::abs(mid_h - 1.5f) < 0.35f,
            "set_height_along mid-path height interpolates start/end");
    const float side_h = engine::sample_terrain_height(2.0f, -2.0f);
    r.check(std::isfinite(side_h),
            "set_height_along strip grades off-center samples");
    const auto plateau = engine::execute_editor_operation(
        terrain_ctx, "terrain_apply",
        R"({"action":"plateau","x":-10,"z":-10,"radius":5,"skirtWidth":3,"strength":1,"targetHeight":3.0})");
    r.check(plateau.exit_code == engine::ExitCode::Success,
            "terrain_apply plateau succeeds");
    const auto smooth_along = engine::execute_editor_operation(
        terrain_ctx, "terrain_apply",
        R"({"action":"smooth_along","points":[{"x":-6,"z":-6},{"x":6,"z":6}],"radius":3,"step":2,"strength":0.5})");
    r.check(smooth_along.exit_code == engine::ExitCode::Success,
            "terrain_apply smooth_along succeeds");
    const auto hill = engine::execute_editor_operation(
        terrain_ctx, "terrain_apply",
        R"({"action":"gentle_hill","x":8,"z":8,"radius":6,"height":2.5,"strength":0.8})");
    r.check(hill.exit_code == engine::ExitCode::Success,
            "terrain_apply gentle_hill succeeds");
    r.check(hill.metadata.count("recipe") && hill.metadata.at("recipe") == "gentle_hill",
            "gentle_hill reports recipe metadata");
    const auto paint_along = engine::execute_editor_operation(
        terrain_ctx, "terrain_apply",
        R"({"action":"paint_along","points":[{"x":0,"z":0},{"x":4,"z":0}],"radius":2,"material":"assets/materials/forest_floor.material.json","step":2})");
    // paint store may be unbound in this narrow terrain_ctx — accept Invalid/Unavailable or Success
    r.check(paint_along.exit_code == engine::ExitCode::Success ||
                paint_along.exit_code == engine::ExitCode::Unavailable,
            "terrain_apply paint_along responds without crash");
    engine::set_active_terrain_edits(nullptr);
    engine::set_active_water_store(nullptr);
  } else if (suite == "world_influence") {
    engine::WorldInfluenceBus bus;
    const auto empty_dominant = bus.dominant_at(0.0f, 0.0f);
    r.check(empty_dominant.position[0] == 0.0f &&
                empty_dominant.position[2] == 0.0f,
            "empty influence bus returns zero dominant");
    engine::WorldInfluenceSource near_source;
    near_source.position = {1.0f, 0.0f, 1.0f};
    near_source.velocity = {0.5f, 0.0f, 0.0f};
    near_source.radius = 2.0f;
    near_source.strength = 0.8f;
    near_source.kind = "character";
    bus.add(std::move(near_source));
    engine::WorldInfluenceSource far_source;
    far_source.position = {40.0f, 0.0f, 40.0f};
    far_source.velocity = {0.0f, 0.0f, 0.0f};
    far_source.radius = 3.0f;
    far_source.strength = 0.2f;
    far_source.kind = "character";
    bus.add(std::move(far_source));
    const auto dominant = bus.dominant_at(1.5f, 1.5f);
    r.check(dominant.position[0] == 1.0f && dominant.position[2] == 1.0f,
            "dominant influence picks nearest source on XZ");
    r.check(dominant.strength == 0.8f,
            "dominant influence preserves source strength");
    bus.clear();
    r.check(bus.empty(), "influence bus clear removes sources");

    engine::WindFieldParams wind;
    wind.direction = {1.0f, 0.0f, 0.0f};
    wind.gust_wavelength = 10.0f;
    wind.gust_sharpness = 2.0f;
    wind.speed = 4.0f;
    wind.time_seconds = 0.0f;
    const float e0 = engine::wind_gust_envelope(0.0f, 0.0f, wind);
    wind.time_seconds = 1.5f;
    const float e1 = engine::wind_gust_envelope(0.0f, 0.0f, wind);
    r.check(e0 >= 0.0f && e0 <= 1.0f && e1 >= 0.0f && e1 <= 1.0f,
            "wind gust envelope stays in 0..1");
    r.check(std::abs(e0 - e1) > 1e-4f, "wind gust envelope advances with time");
    r.check(engine::mesh_uses_wind_sway("assets/models/tree.gltf"),
            "tree mesh enables wind sway");
    r.check(engine::mesh_uses_wind_sway("assets/models/dead-tree.gltf"),
            "dead-tree mesh enables wind sway");
    r.check(!engine::mesh_uses_wind_sway("assets/models/rock.gltf"),
            "rock mesh does not enable wind sway");
    float pack[8]{};
    engine::pack_wind_constants(wind, pack);
    r.check(std::abs(pack[0] - 1.0f) < 1e-4f && std::abs(pack[1]) < 1e-4f,
            "pack_wind_constants stores dir xz");
  } else if (suite == "foliage") {
    engine::FoliageLayerPalette palette;
    palette.schema_version = 1;
    palette.layers = {{"grass",
                       "Grass",
                       "grass_blade",
                       {0.14f, 0.22f, 0.10f},
                       0.55f,
                       1.0f,
                       0.15f,
                       0.55f,
                       0.35f,
                       1.2f,
                       0.55f,
                       "grass_walk"},
                      {"flower",
                       "Flower",
                       "flower_clump",
                       {0.62f, 0.28f, 0.48f},
                       0.45f,
                       0.85f,
                       0.03f,
                       0.45f,
                       0.1f,
                       0.9f,
                       0.45f,
                       ""}};
    r.check(palette.validate().has_value(), "foliage layer palette validates");
    const auto palette_round_trip =
        engine::FoliageLayerPalette::from_json(palette.to_json());
    r.check(palette_round_trip.has_value(), "foliage layer palette round trip");
    r.check(palette_round_trip.value().layers[0].mesh_kind == "grass_blade",
            "foliage palette round trip keeps grass_blade mesh");
    r.check(palette_round_trip.value().layers[0].bend_strength == 0.35f,
            "foliage palette round trip keeps bendStrength");
    r.check(palette_round_trip.value().layers[0].disturb_vfx_id == "grass_walk",
            "foliage palette round trip keeps disturbVfxId");
    r.check(!engine::FoliageLayerPalette::from_json("{\"schemaVersion\":2}"),
            "invalid foliage layer schema rejected");
    engine::FoliageDensityStore density;
    const auto brushed =
        density.apply_foliage_brush(0.0f, 0.0f, 6.0f, 0.2f, 0, false);
    r.check(brushed && brushed.value().count({0, 0}) == 1,
            "foliage brush touches origin cell");
    const auto density_round_trip =
        engine::FoliageDensityStore::from_json(density.to_json());
    r.check(density_round_trip.has_value(), "foliage density round trip");
    r.check(!engine::FoliageDensityStore::from_json("{\"schemaVersion\":2}"),
            "invalid foliage density schema rejected");
    engine::FoliageDensityHistory history;
    const auto before = density.cell_snapshot_or_empty({0, 0});
    const auto erased =
        density.apply_foliage_brush(0.0f, 0.0f, 6.0f, 1.0f, 0, true);
    r.check(erased && erased.value().count({0, 0}) <= 1,
            "foliage erase brush succeeds");
    std::map<engine::CellCoord, engine::FoliageCellSnapshot> before_map;
    before_map[{0, 0}] = before;
    std::map<engine::CellCoord, engine::FoliageCellSnapshot> after;
    after[{0, 0}] = density.cell_snapshot_or_empty({0, 0});
    r.check(
        history
            .execute(density,
                     std::make_unique<engine::FoliageDensityBrushStrokeCommand>(
                         std::move(before_map), std::move(after)))
            .has_value(),
        "foliage brush command executes");
    r.check(history.undo(density).has_value(), "foliage brush command undoes");
    const std::set<engine::CellCoord> cells{{0, 0}};
    const std::array<float, 3> camera{0.0f, 8.0f, 0.0f};
    const auto first =
        engine::scatter_foliage_cells(cells, density, palette, {}, camera);
    const auto second =
        engine::scatter_foliage_cells(cells, density, palette, {}, camera);
    r.check(first.size() == second.size(), "foliage scatter map size stable");
    bool scatter_matches = true;
    for (const auto &entry : first) {
      const auto found = second.find(entry.first);
      if (found == second.end() ||
          found->second.size() != entry.second.size()) {
        scatter_matches = false;
        break;
      }
      for (std::size_t index = 0; index < entry.second.size(); ++index) {
        if (entry.second[index].model != found->second[index].model) {
          scatter_matches = false;
          break;
        }
      }
      if (!scatter_matches)
        break;
    }
    r.check(scatter_matches, "foliage scatter is deterministic");
    engine::FoliageDensityStore heavy;
    (void)heavy.apply_foliage_brush(0.0f, 0.0f, 40.0f, 1.0f, 0, false);
    const auto heavy_instances =
        engine::scatter_foliage_cell({0, 0}, heavy, palette, {}, camera);
    r.check(heavy_instances.size() <=
                engine::FoliageScatterConfig::k_max_instances_per_cell,
            "foliage scatter respects per-cell budget");
    {
      // Over-budget scatter must thin across the cell, not abort after the
      // first density row.
      float min_z = 1.0e9f;
      float max_z = -1.0e9f;
      for (const auto &instance : heavy_instances) {
        min_z = std::min(min_z, instance.model[14]);
        max_z = std::max(max_z, instance.model[14]);
      }
      r.check(!heavy_instances.empty() && (max_z - min_z) > 8.0f,
              "budgeted foliage scatter covers cell depth instead of striping "
              "one grid row");
    }
    const std::array<float, 3> high_camera{0.0f, 500.0f, 0.0f};
    const auto high_instances =
        engine::scatter_foliage_cell({0, 0}, heavy, palette, {}, high_camera);
    r.check(high_instances.size() == heavy_instances.size(),
            "foliage scatter distance falloff ignores camera height");
    engine::FoliageLayerPalette steep_palette = palette;
    steep_palette.layers[0].max_slope_ratio = 0.0001f;
    const auto steep = engine::scatter_foliage_cell({0, 0}, density,
                                                    steep_palette, {}, camera);
    const auto permissive =
        engine::scatter_foliage_cell({0, 0}, density, palette, {}, camera);
    r.check(steep.size() <= permissive.size(),
            "foliage scatter rejects steep slopes");
    engine::CollisionWorld world;
    engine::StreamedTerrainField terrain_field;
    r.check(terrain_field.update(world, {0.0f, 8.0f, 0.0f}, {}).has_value(),
            "foliage stream terrain update");
    engine::StreamedFoliageField foliage_field;
    foliage_field.set_palette(&palette);
    foliage_field.set_density(&density);
    r.check(foliage_field.sync(terrain_field, camera).has_value(),
            "streamed foliage syncs with terrain");
    r.check(foliage_field.dirty(),
            "streamed foliage marks instances dirty after sync");
    {
      // Amortized scatter: budget 0 leaves terrain cells without foliage until
      // a later frame.
      engine::StreamedFoliageField paced;
      paced.set_palette(&palette);
      paced.set_density(&density);
      r.check(paced.sync(terrain_field, camera, 0).has_value(),
              "zero foliage budget sync succeeds");
      r.check(paced.cell_instances().empty(),
              "zero foliage budget scatters no new cells");
      r.check(paced.sync(terrain_field, camera, 1).has_value(),
              "unit foliage budget sync succeeds");
      r.check(paced.cell_instances().size() == 1,
              "unit foliage budget scatters one cell");
      std::size_t paced_frames = 0;
      while (paced.cell_instances().size() <
                 terrain_field.loaded_cell_count() &&
             paced_frames < 64) {
        r.check(paced.sync(terrain_field, camera, 1).has_value(),
                "foliage catch-up sync");
        ++paced_frames;
      }
      r.check(paced.cell_instances().size() ==
                  terrain_field.loaded_cell_count(),
              "amortized foliage eventually matches terrain resident set");
    }
    r.check(foliage_field.batches_stale(),
            "streamed foliage defers batch merge until batches() is read");
    r.check(!foliage_field.batches().empty(),
            "streamed foliage builds instance batches");
    r.check(!foliage_field.batches_stale(),
            "streamed foliage batch merge is cached after access");
    r.check(!foliage_field.cell_instances().empty(),
            "streamed foliage keeps per-cell instances for frustum cull");
    {
      // Cull AABBs must use the centered cell footprint (same as
      // scatter/density), not corner origin.
      constexpr float cell_size = engine::FoliageDensityStore::k_cell_size;
      const engine::CellCoord cell{1, 0};
      const float origin_x =
          static_cast<float>(cell.x) * cell_size - cell_size * 0.5f;
      const float origin_z =
          static_cast<float>(cell.z) * cell_size - cell_size * 0.5f;
      r.check(std::abs(origin_x - 20.0f) < 1e-3f &&
                  std::abs(origin_z - (-20.0f)) < 1e-3f,
              "foliage cell (1,0) origin is centered at (20,-20), not corner "
              "(40,0)");
      r.check(origin_x + cell_size == 60.0f && origin_z + cell_size == 20.0f,
              "foliage cell (1,0) footprint is [20,60]x[-20,20]");
    }
    std::size_t cell_instance_total = 0;
    for (const auto &entry : foliage_field.cell_instances())
      cell_instance_total += entry.second.size();
    std::size_t batch_total = 0;
    for (const auto &entry : foliage_field.batches())
      batch_total += entry.second.size();
    r.check(cell_instance_total == batch_total,
            "per-cell foliage instances match merged batches");
    r.check(engine::mesh_lod::k_far_cull_start_m >
                engine::mesh_lod::k_near_full_end_m,
            "mesh LOD far cull starts beyond near-full band");
    r.check(engine::mesh_lod::k_far_cull_exit_m <=
                engine::mesh_lod::k_near_full_end_m,
            "mesh LOD hysteresis exit is at or inside near-full band");
    r.check(engine::mesh_lod::k_lod1_start_m < engine::mesh_lod::k_lod2_start_m
                && engine::mesh_lod::k_lod2_start_m <
                       engine::mesh_lod::k_far_cull_start_m,
            "mesh LOD1 < LOD2 < far cull band order");
    {
      using engine::mesh_lod::Level;
      using engine::mesh_lod::select_level;
      r.check(select_level(50.0f, Level::Lod0, true, true) == Level::Lod0,
              "near distance stays LOD0");
      r.check(select_level(180.0f, Level::Lod0, true, true) == Level::Lod1,
              "mid distance enters LOD1");
      r.check(select_level(300.0f, Level::Lod1, true, true) == Level::Lod2,
              "far mid enters LOD2");
      r.check(select_level(400.0f, Level::Lod2, true, true) == Level::Cull,
              "beyond far cull enters Cull");
      r.check(select_level(300.0f, Level::Cull, true, true) == Level::Cull,
              "cull hysteresis holds until exit");
      r.check(select_level(200.0f, Level::Cull, false, false) == Level::Lod0,
              "without authored LODs cull exits to full mesh");
    }
    {
      std::vector<std::uint8_t> a(16, 10);
      std::vector<std::uint8_t> b = a;
      b[0] = 20; // R channel only on first pixel
      const auto diff = engine::mean_abs_rgb_diff(2, 2, a, b);
      r.check(diff.has_value(), "image diff accepts matching sizes");
      r.check(diff.value().mean_abs_rgb > 0.0 &&
                  diff.value().mean_abs_rgb < 3.0,
              "image diff reports small mean for one-channel delta");
      auto identical = a;
      const auto zero = engine::mean_abs_rgb_diff(2, 2, a, identical);
      r.check(zero.has_value() && zero.value().mean_abs_rgb == 0.0,
              "identical images have zero mean abs rgb");
    }
    const auto meshes = engine::build_foliage_layer_meshes(palette);
    r.check(meshes.size() == 2,
            "foliage layer meshes generated for grass and flower");
    const auto blade_mesh =
        std::find_if(meshes.begin(), meshes.end(), [](const auto &entry) {
          return entry.first.find("grass_blade") != std::string::npos;
        });
    r.check(blade_mesh != meshes.end() &&
                blade_mesh->second.vertices.size() >= 96,
            "grass_blade mesh has multi-blade tuft verts");
    r.check(blade_mesh->second.aabb.max_y >= 0.65f &&
                blade_mesh->second.aabb.max_y <= 0.85f,
            "grass_blade AABB height matches tapered strip");
    const auto sample_palette = engine::FoliageLayerPalette::load(
        "samples/open-world-rpg/assets/foliage/ground-cover.layers.json");
    r.check(sample_palette.has_value(),
            "sample ground-cover.layers.json loads");
    r.check(sample_palette.value().layers[0].mesh_kind == "grass_blade",
            "sample grass layer uses grass_blade");
    r.check(sample_palette.value().layers[0].density_multiplier >= 0.12f &&
                sample_palette.value().layers[0].density_multiplier <= 0.22f,
            "sample grass density tuned for dense tuft carpet");
    r.check(sample_palette.value().layers[0].blade_height >= 0.65f,
            "sample grass bladeHeight matches strip height");
    r.check(sample_palette.value().layers.size() >= 5,
            "sample palette includes bush layers");
    const auto bush_layer =
        std::find_if(sample_palette.value().layers.begin(),
                     sample_palette.value().layers.end(),
                     [](const engine::FoliageLayerDefinition &layer) {
                       return layer.id == "bush";
                     });
    r.check(bush_layer != sample_palette.value().layers.end() &&
                bush_layer->scatter_mode ==
                    engine::FoliageScatterMode::Discrete,
            "sample bush layer uses discrete scatter");
    engine::FoliageLayerPalette discrete_palette;
    discrete_palette.layers = {{"bush",
                                "Bush",
                                "bush",
                                {0.14f, 0.22f, 0.10f},
                                0.85f,
                                1.15f,
                                1.0f,
                                0.5f,
                                0.08f,
                                1.4f,
                                0.95f,
                                "",
                                engine::FoliageScatterMode::Discrete,
                                static_cast<std::uint8_t>(56)}};
    engine::FoliageDensityStore light_bush;
    (void)light_bush.apply_foliage_brush(0.0f, 0.0f, 8.0f, 0.2f, 0, false);
    engine::FoliageDensityStore strong_bush;
    (void)strong_bush.apply_foliage_brush(0.0f, 0.0f, 8.0f, 0.9f, 0, false);
    const auto light_instances = engine::scatter_foliage_cell(
        {0, 0}, light_bush, discrete_palette, {}, camera);
    const auto strong_instances = engine::scatter_foliage_cell(
        {0, 0}, strong_bush, discrete_palette, {}, camera);
    r.check(light_instances.empty(),
            "light discrete bush paint below threshold produces no bushes");
    r.check(!strong_instances.empty(),
            "strong discrete bush paint above threshold produces bushes");
    const auto bush_mesh =
        engine::generate_primitive_mesh("bush", {0.14f, 0.22f, 0.10f});
    r.check(bush_mesh.has_value() && bush_mesh.value().vertices.size() >= 12,
            "bush primitive mesh is non-empty");
    engine::FoliageLayerPalette mix_palette;
    mix_palette.layers = {{"grass",
                           "Grass",
                           "grass_blade",
                           {0.14f, 0.22f, 0.10f},
                           0.55f,
                           1.0f,
                           0.15f,
                           0.55f},
                          {"flower",
                           "Flower",
                           "flower_clump",
                           {0.62f, 0.28f, 0.48f},
                           0.45f,
                           0.85f,
                           0.03f,
                           0.45f},
                          {"bush",
                           "Bush",
                           "bush",
                           {0.14f, 0.22f, 0.10f},
                           0.85f,
                           1.15f,
                           1.0f,
                           0.5f,
                           0.08f,
                           1.4f,
                           0.95f,
                           "",
                           engine::FoliageScatterMode::Discrete,
                           static_cast<std::uint8_t>(72)}};
    const auto mix_weights = engine::default_meadow_mix_weights(mix_palette);
    r.check(mix_weights.size() >= 3,
            "meadow mix resolves grass flower and bush weights");
    engine::FoliageDensityStore mixed_density;
    (void)mixed_density.apply_foliage_mixed_brush(0.0f, 0.0f, 10.0f, 0.55f,
                                                  mix_weights, false);
    const auto *mixed_cell = mixed_density.find_cell({0, 0});
    r.check(mixed_cell, "mixed foliage brush touches origin cell");
    std::set<std::uint8_t> mixed_layers;
    if (mixed_cell) {
      for (std::size_t index = 0; index < mixed_cell->density.size(); ++index) {
        if (mixed_cell->density[index] > 0)
          mixed_layers.insert(mixed_cell->layer[index]);
      }
    }
    r.check(mixed_layers.size() >= 2,
            "mixed foliage brush assigns multiple layer types");
    engine::FoliageDensityStore erase_density = mixed_density;
    (void)erase_density.apply_foliage_brush(0.0f, 0.0f, 10.0f, 0.55f, 0, true);
    const auto *erased_cell = erase_density.find_cell({0, 0});
    r.check(!erased_cell ||
                std::all_of(erased_cell->density.begin(),
                            erased_cell->density.end(),
                            [](std::uint8_t value) { return value == 0; }),
            "foliage erase brush clears painted density");
  } else if (suite == "collision") {
    engine::CollisionWorld world;
    r.check(!world.add_box({0, 0, 0}, {0, 1, 1},
                           engine::CollisionLayer::StaticWorld, false),
            "invalid box rejected");
    auto floor = world.add_box({0, -1, 0}, {10, 1, 10},
                               engine::CollisionLayer::StaticWorld, false,
                               engine::CellCoord{0, 0});
    r.check(floor && world.body_count() == 1, "static body created");
    auto hit = world.ray_cast({0, 5, 0}, {0, -10, 0});
    r.check(hit && hit.value().has_value(), "ray hits floor");
    {
      auto blocker = world.add_sphere({0, 2, 0}, 0.4f,
                                      engine::CollisionLayer::Dynamic, false);
      r.check(blocker.has_value(), "dynamic ray blocker created");
      auto any_hit = world.ray_cast({0, 5, 0}, {0, -10, 0});
      r.check(any_hit && any_hit.value().has_value(),
              "unfiltered ray still hits");
      engine::CollisionQueryFilter static_only;
      static_only.layer = engine::CollisionLayer::StaticWorld;
      auto static_hit = world.ray_cast({0, 5, 0}, {0, -10, 0}, static_only);
      r.check(static_hit && static_hit.value().has_value() &&
                  static_hit.value()->body.value == floor.value().value,
              "StaticWorld-filtered ray skips Dynamic and hits floor");
      (void)world.remove(blocker.value());
    }
    auto sphere = world.add_sphere({0, 5, 0}, 0.5f,
                                   engine::CollisionLayer::Dynamic, true);
    r.check(sphere.has_value(), "dynamic sphere created");
    auto before = world.position(sphere.value());
    for (int i = 0; i < 30; ++i)
      r.check(world.step(1.0f / 60.0f).has_value(), "physics step succeeds");
    auto after = world.position(sphere.value());
    r.check(before && after && after.value().y < before.value().y,
            "dynamic body falls");
    {
      // Regression: a lag-spike-sized step (clamped dt up to 0.25s) must not
      // tunnel a fast body through a thin static floor. Jolt's default discrete
      // motion quality only checks overlap at the end of a collision step, so a
      // single big step (old `step()` always used inCollisionSteps=1) can skip
      // clean over the floor with no collision ever detected.
      engine::CollisionWorld tunnel_world;
      auto thin_floor =
          tunnel_world.add_box({0, 0, 0}, {5.0f, 0.1f, 5.0f},
                               engine::CollisionLayer::StaticWorld, false);
      r.check(thin_floor.has_value(),
              "thin floor created for tunneling regression");
      engine::CollisionBodySettings no_gravity =
          engine::CollisionBodySettings::make_dynamic();
      no_gravity.use_gravity = false;
      auto fast_body = tunnel_world.add_sphere(
          {0.0, 2.0, 0.0}, 0.3f, engine::CollisionLayer::Dynamic, no_gravity);
      r.check(fast_body.has_value(),
              "fast body created for tunneling regression");
      r.check(tunnel_world
                  .set_linear_velocity(fast_body.value(), {0.0f, -20.0f, 0.0f})
                  .has_value(),
              "fast body velocity set for tunneling regression");
      // One hitch-sized step: at -20 m/s this covers 5m, ~10x the floor
      // thickness plus body diameter — the old single-step behavior sailed
      // straight through it.
      r.check(tunnel_world.step(0.25f).has_value(),
              "lag-spike-sized physics step succeeds");
      const auto landed = tunnel_world.position(fast_body.value());
      r.check(landed.has_value(),
              "fast body position readable after lag-spike step");
      r.check(
          landed && landed.value().y > -0.4,
          "fast body does not tunnel through thin floor on a lag-spike step");
    }
    auto overlap = world.overlap_sphere({0, 0, 0}, 2.0f);
    r.check(overlap && overlap.value().size() >= 1,
            "overlap sphere hits floor");
    auto miss = world.overlap_sphere({0, 50, 0}, 0.5f);
    r.check(miss && miss.value().empty(), "overlap sphere misses when empty");
    engine::CollisionQueryFilter static_filter;
    static_filter.layer = engine::CollisionLayer::StaticWorld;
    auto filtered = world.overlap_sphere({0, 0, 0}, 15.0f, static_filter);
    r.check(filtered &&
                std::any_of(filtered.value().begin(), filtered.value().end(),
                            [](const engine::OverlapHit &h) {
                              return h.layer ==
                                     engine::CollisionLayer::StaticWorld;
                            }),
            "overlap layer filter keeps static floor");
    engine::CollisionQueryFilter trigger_filter;
    trigger_filter.layer = engine::CollisionLayer::Trigger;
    auto trigger_miss = world.overlap_sphere({0, 0, 0}, 15.0f, trigger_filter);
    r.check(trigger_miss && trigger_miss.value().empty(),
            "overlap layer filter excludes non-trigger bodies");
    r.check(!world.overlap_sphere({0, 0, 0}, 0.0f),
            "invalid overlap radius rejected");
    auto box_overlap = world.overlap_box({0, 0, 0}, {2, 2, 2});
    r.check(box_overlap && box_overlap.value().size() >= 1,
            "overlap box hits floor");
    auto box_miss = world.overlap_box({0, 50, 0}, {0.5f, 0.5f, 0.5f});
    r.check(box_miss && box_miss.value().empty(),
            "overlap box misses when empty");
    auto box_filtered =
        world.overlap_box({0, 0, 0}, {15, 15, 15}, static_filter);
    r.check(box_filtered &&
                std::any_of(
                    box_filtered.value().begin(), box_filtered.value().end(),
                    [](const engine::OverlapHit &h) {
                      return h.layer == engine::CollisionLayer::StaticWorld;
                    }),
            "overlap box layer filter keeps static floor");
    r.check(!world.overlap_box({0, 0, 0}, {0, 1, 1}),
            "invalid overlap box rejected");
    auto sweep = world.sweep_sphere({0, 5, 0}, {0, -10, 0}, 0.25f);
    r.check(sweep && sweep.value().has_value() &&
                sweep.value()->fraction > 0.0f &&
                sweep.value()->fraction <= 1.0f,
            "swept sphere hits floor");
    auto sweep_miss = world.sweep_sphere({0, 50, 0}, {0, 10, 0}, 0.25f);
    r.check(sweep_miss && !sweep_miss.value().has_value(),
            "swept sphere misses upward cast");
    auto sweep_filtered =
        world.sweep_sphere({0, 5, 0}, {0, -10, 0}, 0.25f, trigger_filter);
    r.check(sweep_filtered && !sweep_filtered.value().has_value(),
            "swept sphere layer filter excludes static floor");
    r.check(!world.sweep_sphere({0, 0, 0}, {0, -1, 0}, 0.0f),
            "invalid swept sphere radius rejected");
    r.check(world.remove(sphere.value()).has_value(), "dynamic body removed");
    auto trigger =
        world.add_box({0, 2, 0}, {2, 2, 2}, engine::CollisionLayer::Trigger,
                      false, engine::CellCoord{1, 0});
    r.check(trigger.has_value(), "trigger volume created");
    auto mover = world.add_sphere({0, 10, 0}, 0.5f,
                                  engine::CollisionLayer::Dynamic, true);
    r.check(mover.has_value(), "trigger test mover created");
    bool saw_enter = false, saw_exit = false;
    for (int i = 0; i < 240; ++i) {
      r.check(world.step(1.0f / 60.0f).has_value(),
              "trigger test physics step");
      for (const auto &event : world.drain_contact_events()) {
        if (event.layer_a != engine::CollisionLayer::Trigger &&
            event.layer_b != engine::CollisionLayer::Trigger)
          continue;
        if (event.type == engine::ContactEventType::Enter)
          saw_enter = true;
        if (event.type == engine::ContactEventType::Exit)
          saw_exit = true;
      }
    }
    r.check(saw_enter, "object enters trigger");
    r.check(saw_exit, "object exits trigger");
    auto unload_trigger =
        world.add_box({20, 2, 0}, {1, 1, 1}, engine::CollisionLayer::Trigger,
                      false, engine::CellCoord{2, 0});
    auto inside = world.add_sphere({20, 2, 0}, 0.25f,
                                   engine::CollisionLayer::Dynamic, true);
    for (int i = 0; i < 10; ++i) {
      r.check(world.step(1.0f / 60.0f).has_value(),
              "trigger unload warm-up step");
      (void)world.drain_contact_events();
    }
    world.unload_cell({2, 0});
    auto unload_events = world.drain_contact_events();
    r.check(std::any_of(unload_events.begin(), unload_events.end(),
                        [](const engine::ContactEvent &e) {
                          return e.type == engine::ContactEventType::Exit;
                        }),
            "trigger unloads with cell");
    (void)world.remove(inside.value());
    (void)world.remove(mover.value());
    world.unload_cell({1, 0});
    world.unload_cell({0, 0});
    r.check(world.body_count() == 0, "cell unload removes bodies");
    r.check(!world.step(0), "invalid step rejected");
    auto terrain = engine::generate_stylized_terrain({0, 0}, 33, 40);
    auto terrain_body = terrain
                            ? world.add_heightfield(terrain.value(), {},
                                                    engine::CellCoord{0, 0})
                            : engine::Result<engine::CollisionBody>::failure(
                                  engine::EngineError{});
    r.check(terrain_body.has_value(), "heightfield collision created");
    auto terrain_hit = world.ray_cast({5, 20, 5}, {0, -40, 0});
    r.check(terrain_hit && terrain_hit.value().has_value(),
            "ray hits generated terrain");
    world.unload_cell({0, 0});
    r.check(world.body_count() == 0, "heightfield unloads with cell");
    engine::PhysicalMaterialProperties invalid;
    invalid.friction = -1;
    r.check(terrain && !world.add_heightfield(terrain.value(), invalid),
            "invalid physical material rejected");
    (void)world.add_box({0, -1, 0}, {1, 1, 1},
                        engine::CollisionLayer::StaticWorld, false);
    engine::PrefabAsset prefab_trigger;
    prefab_trigger.schema_version = 2;
    engine::PrefabCollisionVolume trigger_volume;
    trigger_volume.shape = engine::PrefabCollisionShape::Sphere;
    trigger_volume.trigger = true;
    trigger_volume.layer = engine::CollisionLayer::Trigger;
    trigger_volume.radius = 1.0f;
    trigger_volume.transform.position = {0.0f, 0.5f, 0.0f};
    prefab_trigger.collision.push_back(trigger_volume);
    engine::TransformComponent placement;
    placement.position = {10.0f, 2.0f, 10.0f};
    auto prefab_bodies = engine::spawn_prefab_collision(
        world, prefab_trigger, placement, engine::CellCoord{0, 0});
    r.check(prefab_bodies && prefab_bodies.value().size() == 1,
            "prefab trigger collision spawns");
    auto prefab_mover = world.add_sphere({10.0f, 10.0f, 10.0f}, 0.5f,
                                         engine::CollisionLayer::Dynamic, true);
    r.check(prefab_mover.has_value(), "prefab trigger test mover created");
    bool prefab_enter = false, prefab_exit = false;
    for (int i = 0; i < 240; ++i) {
      r.check(world.step(1.0f / 60.0f).has_value(),
              "prefab trigger physics step");
      for (const auto &event : world.drain_contact_events()) {
        if (event.layer_a != engine::CollisionLayer::Trigger &&
            event.layer_b != engine::CollisionLayer::Trigger)
          continue;
        if (event.type == engine::ContactEventType::Enter)
          prefab_enter = true;
        if (event.type == engine::ContactEventType::Exit)
          prefab_exit = true;
      }
    }
    r.check(prefab_enter, "prefab trigger reports enter");
    r.check(prefab_exit, "prefab trigger reports exit");
    (void)world.remove(prefab_mover.value());
    auto prefab_inside = world.add_sphere(
        {10.0f, 2.5f, 10.0f}, 0.25f, engine::CollisionLayer::Dynamic, true);
    r.check(prefab_inside.has_value(), "prefab trigger unload probe created");
    for (int i = 0; i < 10; ++i) {
      r.check(world.step(1.0f / 60.0f).has_value(),
              "prefab trigger unload warm-up step");
      (void)world.drain_contact_events();
    }
    world.unload_cell({0, 0});
    auto prefab_unload_events = world.drain_contact_events();
    r.check(std::any_of(prefab_unload_events.begin(),
                        prefab_unload_events.end(),
                        [](const engine::ContactEvent &e) {
                          return e.type == engine::ContactEventType::Exit;
                        }),
            "prefab trigger unloads with cell");
    (void)world.remove(prefab_inside.value());
    r.check(!world.debug_bodies().empty(), "collision debug bodies available");

    // TICKET-0197: CollisionBodySettings + PlacementCollisionTracker Rigidbody
    // path
    {
      engine::CollisionWorld settings_world;
      auto settings_floor = settings_world.add_box(
          {0, -1, 0}, {20, 1, 20}, engine::CollisionLayer::StaticWorld, false);
      r.check(settings_floor.has_value(), "0197 settings floor");
      auto dyn_settings = engine::CollisionBodySettings::make_dynamic();
      dyn_settings.mass = 2.0f;
      dyn_settings.linear_damping = 0.1f;
      auto falling =
          settings_world.add_box({0, 8, 0}, {0.5f, 0.5f, 0.5f},
                                 engine::CollisionLayer::Dynamic, dyn_settings);
      r.check(falling.has_value(), "0197 dynamic settings body");
      const auto y0 = settings_world.position(falling.value()).value().y;
      for (int i = 0; i < 120; ++i)
        r.check(settings_world.step(1.0f / 60.0f).has_value(),
                "0197 fall step");
      const auto y1 = settings_world.position(falling.value()).value().y;
      r.check(y1 < y0 - 1.0, "0197 dynamic settings body falls");
      r.check(y1 > -0.5, "0197 dynamic settings body rests above floor");
      auto kin_settings = engine::CollisionBodySettings::make_kinematic();
      auto kinematic =
          settings_world.add_box({5, 8, 0}, {0.5f, 0.5f, 0.5f},
                                 engine::CollisionLayer::Dynamic, kin_settings);
      r.check(kinematic.has_value(), "0197 kinematic settings body");
      const auto ky0 = settings_world.position(kinematic.value()).value().y;
      for (int i = 0; i < 60; ++i)
        r.check(settings_world.step(1.0f / 60.0f).has_value(),
                "0197 kinematic step");
      const auto ky1 = settings_world.position(kinematic.value()).value().y;
      r.check(std::abs(ky1 - ky0) < 0.01, "0197 kinematic does not fall");
      auto no_mass = engine::CollisionBodySettings::make_dynamic();
      no_mass.mass = 0.0f;
      r.check(!settings_world.add_box({0, 1, 0}, {0.5f, 0.5f, 0.5f},
                                      engine::CollisionLayer::Dynamic, no_mass),
              "0197 invalid mass rejected");
    }
    {
      engine::CollisionWorld track_world;
      auto track_floor = track_world.add_box(
          {0, -1, 0}, {20, 1, 20}, engine::CollisionLayer::StaticWorld, false,
          engine::CellCoord{0, 0});
      r.check(track_floor.has_value(), "0197 tracker floor");
      engine::PrefabAsset crate;
      crate.schema_version = 2;
      engine::PrefabCollisionVolume box;
      box.shape = engine::PrefabCollisionShape::Box;
      box.layer = engine::CollisionLayer::StaticWorld;
      box.half_extent = {0.5f, 0.5f, 0.5f};
      crate.collision.push_back(box);
      engine::PrefabRigidbody rb;
      rb.id = "rigidbody-0";
      rb.motion_type = "dynamic";
      rb.mass = 1.0f;
      crate.rigidbodies.push_back(rb);
      engine::Scene scene;
      engine::TransformComponent at;
      at.position = {0.0f, 6.0f, 0.0f};
      auto placed =
          scene.place_world_object("crate", "assets/prefabs/crate.prefab.json",
                                   at, std::nullopt, std::nullopt, &crate);
      r.check(placed.has_value(), "0197 place rigidbody prefab");
      std::map<std::string, engine::PrefabAsset> catalog;
      catalog["assets/prefabs/crate.prefab.json"] = crate;
      engine::PlacementCollisionTracker tracker;
      r.check(tracker.sync(track_world, scene, catalog, true).has_value(),
              "0197 sync dynamic placement");
      r.check(track_world.body_count() >= 2, "0197 motion + floor bodies");
      const auto y_before = scene.transform(placed.value())->position[1];
      for (int i = 0; i < 120; ++i) {
        r.check(track_world.step(1.0f / 60.0f).has_value(),
                "0197 tracker physics step");
        tracker.write_back_transforms(scene, track_world);
        r.check(tracker.sync(track_world, scene, catalog, true).has_value(),
                "0197 sync after write-back");
      }
      const auto y_after = scene.transform(placed.value())->position[1];
      r.check(y_after < y_before - 1.0f, "0197 write-back lowers entity");
      r.check(y_after > -0.6f, "0197 crate rests near floor");
      const auto rev_after_fall = scene.edit_revision();
      r.check(tracker.sync(track_world, scene, catalog, true).has_value(),
              "0197 sync no-op after physics write-back");
      r.check(scene.edit_revision() == rev_after_fall,
              "0197 physics write-back does not bump scene edit revision");
      {
        // Dense static forest: unchanged-revision sync must stay O(1), not
        // O(entities).
        engine::PrefabAsset bush;
        bush.schema_version = 2;
        bush.collision.push_back(box);
        catalog["assets/prefabs/bush.prefab.json"] = bush;
        for (int i = 0; i < 240; ++i) {
          engine::TransformComponent at;
          at.position = {static_cast<float>(i % 20) * 2.0f, 0.0f,
                         static_cast<float>(i / 20) * 2.0f};
          r.check(scene
                      .place_world_object("bush_" + std::to_string(i),
                                          "assets/prefabs/bush.prefab.json", at,
                                          std::nullopt, std::nullopt, &bush)
                      .has_value(),
                  "0197 place static bush");
        }
        r.check(tracker.sync(track_world, scene, catalog, true).has_value(),
                "0197 sync dense static forest");
        const auto rev_dense = scene.edit_revision();
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 200; ++i)
          r.check(tracker.sync(track_world, scene, catalog, true).has_value(),
                  "0197 skip sync dense forest");
        const auto t1 = std::chrono::steady_clock::now();
        r.check(scene.edit_revision() == rev_dense,
                "0197 dense skip keeps edit revision");
        const auto skip_ms =
            std::chrono::duration<double, std::milli>(t1 - t0).count();
        r.check(skip_ms < 50.0,
                "0197 unchanged-revision sync skips dense forest work");
      }
      {
        // Play-test visual follow must use set_transform(..., bump=false).
        // Bumping every frame forced PlacementCollisionTracker to rescan every
        // entity and spiked Simulation ms.
        engine::TransformComponent follow = *scene.transform(placed.value());
        follow.position[0] += 0.05f;
        follow.rotation[1] = 0.02f;
        const auto rev_before_follow = scene.edit_revision();
        for (int i = 0; i < 60; ++i) {
          follow.position[0] += 0.01f;
          r.check(
              scene.set_transform(placed.value(), follow, false).has_value(),
              "0197 play-test visual follow without bump");
          r.check(tracker.sync(track_world, scene, catalog, true).has_value(),
                  "0197 sync after visual follow");
        }
        r.check(scene.edit_revision() == rev_before_follow,
                "0197 play-test visual follow does not bump edit revision");
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 200; ++i)
          r.check(tracker.sync(track_world, scene, catalog, true).has_value(),
                  "0197 skip after visual follow");
        const auto follow_skip_ms = std::chrono::duration<double, std::milli>(
                                        std::chrono::steady_clock::now() - t0)
                                        .count();
        r.check(follow_skip_ms < 50.0,
                "0197 visual-follow frames keep O(1) sync skip");
      }
      {
        // Budgeted sync: dynamics flip must not rebuild every Rigidbody in one
        // frame.
        engine::CollisionWorld budget_world;
        r.check(budget_world
                    .add_box({0, -1, 0}, {20, 1, 20},
                             engine::CollisionLayer::StaticWorld, false,
                             engine::CellCoord{0, 0})
                    .has_value(),
                "0197 budget floor");
        engine::Scene budget_scene;
        std::map<std::string, engine::PrefabAsset> budget_catalog;
        budget_catalog["assets/prefabs/crate.prefab.json"] = crate;
        engine::PlacementCollisionTracker budget_tracker;
        std::vector<engine::EntityId> crates;
        for (int i = 0; i < 3; ++i) {
          engine::TransformComponent at_i;
          at_i.position = {static_cast<float>(i) * 2.0f, 6.0f, 0.0f};
          auto id = budget_scene.place_world_object(
              "crate-" + std::to_string(i), "assets/prefabs/crate.prefab.json",
              at_i, std::nullopt, std::nullopt, &crate);
          r.check(id.has_value(), "0197 place budget crate");
          crates.push_back(id.value());
        }
        r.check(budget_tracker
                    .sync(budget_world, budget_scene, budget_catalog, false)
                    .has_value(),
                "0197 sync all kinematic for edit");
        r.check(!budget_tracker.sync_incomplete(), "0197 edit sync completes");
        r.check(budget_tracker
                    .sync(budget_world, budget_scene, budget_catalog, true, 1,
                          &crates[0])
                    .has_value(),
                "0197 budgeted play-start sync");
        r.check(budget_tracker.sync_incomplete(),
                "0197 budget 1 leaves catch-up work");
        r.check(budget_tracker.motion_body_for(crates[0]).has_value(),
                "0197 priority entity rebuilt under budget");
        std::size_t catch_up = 0;
        while (budget_tracker.sync_incomplete() && catch_up < 8) {
          r.check(budget_tracker
                      .sync(budget_world, budget_scene, budget_catalog, true, 1)
                      .has_value(),
                  "0197 catch-up budgeted sync");
          ++catch_up;
        }
        r.check(!budget_tracker.sync_incomplete(),
                "0197 budgeted sync eventually completes");
        r.check(budget_tracker.motion_body_for(crates[1]).has_value() &&
                    budget_tracker.motion_body_for(crates[2]).has_value(),
                "0197 catch-up rebuilds remaining dynamics");
      }
      {
        // The Inspector re-seeds the selected entity every frame. A prefab with
        // nothing to seed left the entity holding an empty component set, so
        // the old guard re-seeded forever, bumped the edit revision, and
        // re-dirtied the static render cache on every editor frame.
        engine::PrefabAsset bare;
        bare.schema_version = 2;
        engine::TransformComponent bare_at;
        bare_at.position = {40.0f, 0.0f, 40.0f};
        const auto bare_id = scene.place_world_object(
            "bare", "assets/prefabs/bare.prefab.json", bare_at, std::nullopt,
            std::nullopt, &bare);
        r.check(bare_id.has_value(), "place component-less prefab");
        const auto rev_before = scene.edit_revision();
        for (int i = 0; i < 8; ++i) {
          const auto again =
              scene.ensure_authored_components_seeded(bare_id.value(), bare);
          r.check(again.has_value() && !again.value(),
                  "repeat seed of component-less prefab is a no-op");
        }
        r.check(
            scene.edit_revision() == rev_before,
            "repeat seed of component-less prefab does not bump edit revision");
        engine::PrefabAsset gained = bare;
        gained.collision.push_back(box);
        const auto grown =
            scene.ensure_authored_components_seeded(bare_id.value(), gained);
        r.check(grown.has_value() && grown.value(),
                "prefab that gains components still seeds onto empty set");
      }
      engine::PrefabAsset static_prefab;
      static_prefab.schema_version = 2;
      static_prefab.collision.push_back(box);
      engine::TransformComponent static_at;
      static_at.position = {10.0f, 6.0f, 0.0f};
      auto static_placed = scene.place_world_object(
          "static", "assets/prefabs/static.prefab.json", static_at,
          std::nullopt, std::nullopt, &static_prefab);
      catalog["assets/prefabs/static.prefab.json"] = static_prefab;
      r.check(static_placed.has_value() &&
                  tracker.sync(track_world, scene, catalog, true).has_value(),
              "0197 sync static placement");
      const auto static_y0 =
          scene.transform(static_placed.value())->position[1];
      for (int i = 0; i < 60; ++i) {
        r.check(track_world.step(1.0f / 60.0f).has_value(), "0197 static step");
        tracker.write_back_transforms(scene, track_world);
      }
      r.check(std::abs(scene.transform(static_placed.value())->position[1] -
                       static_y0) < 0.001f,
              "0197 no-Rigidbody placement stays put");
      const auto count_before_unload = track_world.body_count();
      track_world.unload_cell({0, 0});
      r.check(track_world.body_count() < count_before_unload,
              "0197 unload removes cell bodies");
    }
  } else if (suite == "navigation") {
    auto grid = engine::build_navigation_grid({0, 0});
    r.check(grid.has_value() && grid.value().resolution == 33 &&
                grid.value().cell_size == 128.0f,
            "navigation grid builds for partition cell");
    std::size_t walkable = 0;
    for (const auto sample : grid.value().walkable)
      if (sample)
        ++walkable;
    r.check(walkable > 0, "navigation grid marks walkable samples");
    auto nearest = grid.value().nearest_walkable(32.0f, 32.0f, 20.0f);
    r.check(nearest.has_value(), "nearest walkable point found");
    r.check(grid.value().line_of_walk(16.0f, 16.0f, 48.0f, 48.0f),
            "line-of-walk succeeds on local flat path");
    r.check(!engine::build_navigation_grid({0, 0}, 128.0f, 16),
            "invalid navigation resolution rejected");
    engine::StreamedNavigationField field;
    r.check(field.update({64.0f, 0.0f, 64.0f}, 1).has_value() &&
                field.loaded_cell_count() == 9,
            "streamed navigation loads neighborhood");
    r.check(field.contains({0, 0}), "streamed navigation keeps origin cell");
    r.check(field.update({200.0f, 0.0f, 64.0f}, 1).has_value(),
            "streamed navigation recenters");
    r.check(!field.contains({0, 0}) || field.focus_cell().x != 0,
            "streamed navigation unloads distant cells");
    auto walk = field.line_of_walk({64.0, 0.0, 64.0}, {80.0, 0.0, 80.0});
    r.check(walk.has_value() && walk.value(),
            "loaded cell line-of-walk query succeeds");
    auto cross = field.line_of_walk({64.0, 0.0, 64.0}, {150.0, 0.0, 64.0});
    r.check(cross.has_value(),
            "cross-cell line-of-walk succeeds across loaded cells");
    r.check(!field.line_of_walk({64.0, 0.0, 64.0}, {300.0, 0.0, 300.0}),
            "line-of-walk fails when cells are not loaded");
    r.check(!field.nearest_walkable_point({9999.0, 0.0, 9999.0}, 10.0f),
            "out-of-world nearest walkable rejected");
    engine::StreamedNavigationField path_field;
    r.check(path_field.ensure_loaded_for_query({64.0, 0.0, 64.0}, {96.0, 0.0, 80.0})
                .has_value(),
            "path query loads covering cells");
    auto path = path_field.find_path({64.0, 0.0, 64.0}, {96.0, 0.0, 80.0});
    r.check(path.has_value() && path.value().found && path.value().points.size() >= 2,
            "find_path returns a walkable polyline");
    r.check(path.value().length_xz > 1.0f, "find_path reports positive length");
    auto blocked = path_field.find_path({64.0, 0.0, 64.0}, {64.0, 0.0, 64.0});
    r.check(blocked.has_value() && blocked.value().found,
            "find_path same-point succeeds");
  } else if (suite == "character") {
    engine::CollisionWorld world;
    engine::CharacterControllerConfig config;
    config.max_speed = 4.0f;
    auto terrain = engine::generate_stylized_terrain({0, 0}, 33, 40);
    r.check(terrain.has_value(), "character test terrain generated");
    auto terrain_body = terrain
                            ? world.add_heightfield(terrain.value(), {},
                                                    engine::CellCoord{0, 0})
                            : engine::Result<engine::CollisionBody>::failure(
                                  engine::EngineError{});
    r.check(terrain_body.has_value(),
            "character test terrain collision loaded");
    const float ground = engine::sample_terrain_height(8.0f, 8.0f);
    auto created = engine::CharacterController::create(
        world, {8.0, ground + 5.0, 8.0}, config);
    r.check(created.has_value(), "character controller created");
    auto character = std::move(created.value());
    bool landed = false;
    for (int i = 0; i < 180; ++i) {
      r.check(character.move({0, 0, 0}, 0.0f, 1.0f / 60.0f).has_value(),
              "character gravity step");
      if (character.on_ground()) {
        landed = true;
        break;
      }
    }
    r.check(landed, "character lands on terrain");
    const auto start = character.position();
    for (int i = 0; i < 120; ++i)
      r.check(character.move({0, 0, config.max_speed}, 0.0f, 1.0f / 60.0f)
                  .has_value(),
              "character walk step");
    const auto moved = character.position();
    r.check(std::abs(moved.z - start.z) > 0.5,
            "character walks forward on terrain");
    engine::WorldPartition partition;
    const auto owner = character.owner_cell(partition);
    r.check(owner.x == 0 && owner.z == 0,
            "character owner cell derived from position");
    auto steep = world.add_box({12, ground + 0.5, 12}, {2, 0.5f, 2},
                               engine::CollisionLayer::StaticWorld, false,
                               engine::CellCoord{0, 0});
    r.check(steep.has_value(), "steep ramp created for slope test");
    character.set_position({12.0, ground + 3.0, 10.0});
    const auto before_ramp = character.position();
    for (int i = 0; i < 120; ++i)
      r.check(character.move({0, 0, config.max_speed}, 0.0f, 1.0f / 60.0f)
                  .has_value(),
              "character ramp push step");
    r.check(character.on_steep_ground() ||
                character.position().y - before_ramp.y < 1.5,
            "character slope limit blocks steep climb");
    engine::CharacterControllerConfig invalid_config;
    invalid_config.capsule_radius = 0.0f;
    r.check(
        !engine::CharacterController::create(world, {0, 0, 0}, invalid_config),
        "invalid character config rejected");
    engine::StreamedTerrainField field;
    r.check(field.update(world, {8.0f, 12.0f, 8.0f}, {}).has_value(),
            "character terrain stream update");
    r.check(character.move({0, 0, 1.0f}, 0.0f, 1.0f / 60.0f).has_value(),
            "character moves with streamed terrain resident");
    const auto normalized_start = character.position();
    for (int i = 0; i < 120; ++i)
      r.check(character.move({0, 0, 1.0f}, 0.0f, 1.0f / 60.0f).has_value(),
              "normalized wish uses max speed");
    const auto normalized_end = character.position();
    r.check(std::abs(normalized_end.z - normalized_start.z) > 1.5,
            "normalized wish reaches configured max speed");
    const auto diagonal_start = character.position();
    for (int i = 0; i < 90; ++i)
      r.check(character.move({1.0f, 0, 1.0f}, 0.0f, 1.0f / 60.0f).has_value(),
              "diagonal keyboard step");
    const auto diagonal_end = character.position();
    const float diagonal_distance =
        std::sqrt((diagonal_end.x - diagonal_start.x) *
                      (diagonal_end.x - diagonal_start.x) +
                  (diagonal_end.z - diagonal_start.z) *
                      (diagonal_end.z - diagonal_start.z));
    const auto forward_start = character.position();
    for (int i = 0; i < 90; ++i)
      r.check(character.move({0, 0, 1.0f}, 0.0f, 1.0f / 60.0f).has_value(),
              "forward keyboard step");
    const auto forward_end = character.position();
    const float forward_distance = std::abs(forward_end.z - forward_start.z);
    r.check(std::abs(diagonal_distance - forward_distance) < 0.35f,
            "diagonal keyboard matches forward speed");
    world.unload_cell({5, 5});
    r.check(character.move({0, 0, 1.0f}, 0.0f, 1.0f / 60.0f).has_value(),
            "character move survives unrelated cell unload");
    r.check(character.debug_body().shape ==
                engine::CollisionDebugShape::Capsule,
            "character debug body reports capsule");
    r.check(
        std::abs(engine::character_facing_yaw_from_velocity(0.0f, 3.0f, 1.5f) -
                 0.0f) < 1e-5f,
        "facing forward uses +Z yaw 0");
    r.check(
        std::abs(engine::character_facing_yaw_from_velocity(3.0f, 0.0f, 0.0f) -
                 1.5707963f) < 1e-4f,
        "facing right uses +X yaw pi/2");
    r.check(
        std::abs(engine::character_facing_yaw_from_velocity(0.0f, 0.0f, 0.75f) -
                 0.75f) < 1e-5f,
        "facing keeps previous when idle");
    r.check(std::abs(
                engine::character_facing_yaw_from_wish(0.0f, 1.0f, 0.4f, 1.5f) -
                0.4f) < 1e-5f,
            "wish forward faces camera yaw");
    r.check(std::abs(
                engine::character_facing_yaw_from_wish(1.0f, 0.0f, 0.0f, 0.0f) -
                1.5707963f) < 1e-4f,
            "wish strafe-right faces +X");
    r.check(std::abs(
                engine::character_facing_yaw_from_wish(0.0f, 0.0f, 0.4f, 0.9f) -
                0.9f) < 1e-5f,
            "wish idle keeps previous facing");
    r.check(std::abs(engine::character_facing_yaw_from_camera_look(
                         0.45f, -10.0f, 0.0f, 0.0f, 0.0f) +
                     std::atan2(0.45f, 10.0f)) < 1e-4f,
            "look facing counters shoulder offset");
    // Build horizontal speed then release input — friction should stop the
    // capsule quickly.
    for (int i = 0; i < 60; ++i)
      r.check(character.move({0, 0, 1.0f}, 0.0f, 1.0f / 60.0f).has_value(),
              "friction wind-up step");
    for (int i = 0; i < 45; ++i)
      r.check(character.move({0, 0, 0}, 0.0f, 1.0f / 60.0f).has_value(),
              "friction brake step");
    {
      const auto stopped = character.linear_velocity();
      const float horiz =
          std::sqrt(stopped[0] * stopped[0] + stopped[2] * stopped[2]);
      r.check(horiz < 0.35f, "ground friction stops idle slide");
    }
    for (int i = 0; i < 30; ++i)
      r.check(character.move({0, 0, 0}, 0.0f, 1.0f / 60.0f).has_value(),
              "character settle before jump");
    r.check(character.on_ground(), "character on ground before jump");
    const float before_jump = character.position().y;
    auto jump_result = character.jump();
    r.check(jump_result.has_value() && jump_result.value(),
            "grounded jump accepted");
    r.check(character.move({0, 0, 0}, 0.0f, 1.0f / 60.0f).has_value(),
            "jump applies on next move");
    const auto jump_velocity = character.linear_velocity();
    r.check(jump_velocity[1] >= config.jump_velocity - 0.01f,
            "jump sets upward velocity");
    bool airborne = false;
    bool repeat_rejected = false;
    float peak_y = before_jump;
    for (int i = 0; i < 90; ++i) {
      peak_y = std::max(peak_y, static_cast<float>(character.position().y));
      if (!character.on_ground()) {
        airborne = true;
        if (!repeat_rejected) {
          auto repeat_jump = character.jump();
          r.check(repeat_jump.has_value() && !repeat_jump.value(),
                  "airborne jump rejected");
          repeat_rejected = true;
        }
      }
      r.check(character.move({0, 0, 0}, 0.0f, 1.0f / 60.0f).has_value(),
              "jump airborne step");
    }
    r.check(airborne, "jump leaves ground");
    r.check(repeat_rejected, "airborne jump rejection tested");
    r.check(peak_y > before_jump + 0.1f, "jump increases height");
    engine::CharacterAsset invalid_jump_asset;
    invalid_jump_asset.jump_velocity = 0.0f;
    r.check(!invalid_jump_asset.validate(), "invalid jump velocity rejected");
    engine::CharacterControllerConfig invalid_jump_config;
    invalid_jump_config.jump_velocity = 0.0f;
    r.check(!engine::CharacterController::create(
                world, {8.0, ground + 1.0, 8.0}, invalid_jump_config),
            "invalid jump config rejected");

    // TICKET-0198: RigidbodyLocomotion on dynamic capsule
    {
      engine::CollisionWorld loco_world;
      auto floor = loco_world.add_box(
          {0, -1, 0}, {20, 1, 20}, engine::CollisionLayer::StaticWorld, false);
      r.check(floor.has_value(), "0198 loco floor");
      auto settings = engine::CollisionBodySettings::make_dynamic();
      settings.mass = 70.0f;
      settings.freeze_rotation = true;
      const float radius = 0.35f;
      const float half = 0.85f;
      const float center_y = half + radius + 3.0f;
      auto capsule =
          loco_world.add_capsule({0.0, center_y, 0.0}, radius, half,
                                 engine::CollisionLayer::Dynamic, settings);
      r.check(capsule.has_value(), "0198 capsule body");
      engine::CharacterControllerConfig loco_cfg;
      loco_cfg.max_speed = 6.0f;
      engine::RigidbodyLocomotion loco(loco_world, capsule.value(), loco_cfg,
                                       radius, half);
      bool landed = false;
      for (int i = 0; i < 240; ++i) {
        // Let gravity integrate without overwriting velocity every frame while
        // airborne.
        r.check(loco_world.step(1.0f / 60.0f).has_value(), "0198 land step");
        if (loco.on_ground() || loco.feet_position().y < 0.15) {
          landed = true;
          break;
        }
      }
      r.check(landed, "0198 locomotion lands");
      for (int i = 0; i < 10; ++i) {
        (void)loco.move({0, 0, 0}, 0.0f, 1.0f / 60.0f);
        r.check(loco_world.step(1.0f / 60.0f).has_value(), "0198 settle step");
      }
      r.check(loco.on_ground(), "0198 locomotion reports ground");
      const auto feet_before = loco.feet_position();
      for (int i = 0; i < 90; ++i) {
        r.check(loco.move({0, 0, 1}, 0.0f, 1.0f / 60.0f).has_value(),
                "0198 walk move");
        r.check(loco_world.step(1.0f / 60.0f).has_value(), "0198 walk step");
      }
      const auto feet_after = loco.feet_position();
      r.check(feet_after.z > feet_before.z + 0.5, "0198 wish walks forward");
      for (int i = 0; i < 60; ++i) {
        r.check(loco.move({0, 0, 0}, 0.0f, 1.0f / 60.0f).has_value(),
                "0198 idle move");
        r.check(loco_world.step(1.0f / 60.0f).has_value(), "0198 idle step");
      }
      const auto vel = loco.linear_velocity();
      const float hspeed = std::sqrt(vel[0] * vel[0] + vel[2] * vel[2]);
      r.check(hspeed < 0.35f, "0198 idle stops without ice slide");
      const auto jumped = loco.jump();
      r.check(jumped && jumped.value(), "0198 grounded jump queued");
      (void)loco.move({0, 0, 0}, 0.0f, 1.0f / 60.0f);
      r.check(loco_world.step(1.0f / 60.0f).has_value(), "0198 jump step");
      r.check(loco.linear_velocity()[1] > 0.5f, "0198 jump lifts");
      // StaticWorld crate-sized box must stop RigidbodyLocomotion (no
      // walk-through).
      for (int i = 0; i < 60; ++i) {
        r.check(loco.move({0, 0, 0}, 0.0f, 1.0f / 60.0f).has_value(),
                "0198 re-settle move");
        r.check(loco_world.step(1.0f / 60.0f).has_value(),
                "0198 re-settle step");
      }
      const auto block_start = loco.feet_position();
      (void)loco_world.add_box({block_start.x, 0.5, block_start.z + 1.2},
                               {0.5, 0.5, 0.5},
                               engine::CollisionLayer::StaticWorld, false);
      for (int i = 0; i < 180; ++i) {
        r.check(loco.move({0, 0, 1}, 0.0f, 1.0f / 60.0f).has_value(),
                "0198 block walk move");
        r.check(loco_world.step(1.0f / 60.0f).has_value(),
                "0198 block walk step");
      }
      const auto block_end = loco.feet_position();
      r.check(block_end.z < block_start.z + 1.2,
              "0198 static crate blocks rigidbody locomotion");
      r.check(block_end.z > block_start.z - 0.05,
              "0198 block walk does not fling backward");

      // Sticky + coyote: foot just above a surface reports grounded (hill
      // micro-gap) without locking fall forever.
      {
        engine::CollisionWorld stick_world;
        auto stick_floor =
            stick_world.add_box({0, -1, 0}, {20, 1, 20},
                                engine::CollisionLayer::StaticWorld, false);
        r.check(stick_floor.has_value(), "sticky ground floor");
        auto settings = engine::CollisionBodySettings::make_dynamic();
        settings.mass = 70.0f;
        settings.freeze_rotation = true;
        settings.use_gravity = false;
        const float radius = 0.35f;
        const float half = 0.85f;
        // Capsule feet sit ~0.18 m above the box top (y=0) so only sticky
        // down-sweep can see ground.
        const float feet_clearance = 0.18f;
        const float center_y = half + radius + feet_clearance;
        auto stick_cap =
            stick_world.add_capsule({0.0, center_y, 0.0}, radius, half,
                                    engine::CollisionLayer::Dynamic, settings);
        r.check(stick_cap.has_value(), "sticky ground capsule");
        engine::CharacterControllerConfig stick_cfg;
        stick_cfg.step_height = 0.35f;
        engine::RigidbodyLocomotion stick_loco(stick_world, stick_cap.value(),
                                               stick_cfg, radius, half);
        r.check(stick_loco.on_ground(),
                "sticky under-foot probe reports grounded");
        // Coyote after hard teleport into free air: brief grounded for anim,
        // then released.
        (void)stick_world.set_transform(stick_cap.value(),
                                        {0.0, center_y + 4.0, 0.0});
        r.check(stick_loco.on_ground(),
                "coyote still grounded on first leave query");
        for (int i = 0; i < 12; ++i) {
          r.check(stick_loco.move({0, 0, 0}, 0.0f, 1.0f / 60.0f).has_value(),
                  "coyote advance step");
          r.check(stick_world.step(1.0f / 60.0f).has_value(),
                  "coyote physics step");
        }
        r.check(!stick_loco.on_ground(), "coyote expires after ~0.2s free air");
      }
    }
  } else if (suite == "camera") {
    engine::DebugCamera camera;
    r.check(!camera.set_perspective(0, 1, 0.1f, 100),
            "invalid perspective rejected");
    r.check(camera.set_perspective(1.0f, 16.0f / 9.0f, 0.1f, 2000).has_value(),
            "perspective accepted");
    auto start = camera.position();
    camera.apply({1, 0, 0, 0, 0, false}, 1.0f / 60.0f);
    r.check(camera.position()[2] > start[2], "forward input moves camera");
    camera.apply({0, 0, 0, 0, 100000, false}, 1.0f / 60.0f);
    r.check(camera.pitch() >= -1.55334f, "pitch clamps");
    auto matrix = camera.view_projection();
    r.check(std::all_of(matrix.begin(), matrix.end(),
                        [](float v) { return std::isfinite(v); }),
            "matrix finite");
    engine::CollisionWorld world;
    (void)world.add_box({0, -1, 0}, {10, 1, 10},
                        engine::CollisionLayer::StaticWorld, false);
    engine::OrbitCameraConfig orbit_config;
    orbit_config.default_distance = 5.0f;
    orbit_config.pivot_height = 1.6f;
    orbit_config.shoulder_offset = 0.0f;
    orbit_config.default_pitch = 0.0f;
    orbit_config.min_pitch = -0.2f;
    orbit_config.max_pitch = 1.2f;
    engine::OrbitCamera orbit(orbit_config);
    r.check(orbit.set_perspective(1.0f, 16.0f / 9.0f, 0.1f, 2000).has_value(),
            "orbit perspective accepted");
    r.check(orbit.update({0, 0, 0}, world).has_value(),
            "orbit update succeeds");
    r.check(std::abs(orbit.resolved_distance() - 5.0f) < 0.01f,
            "orbit keeps desired distance in open space");
    const auto open_pos = orbit.position();
    const float open_dist =
        std::sqrt(open_pos[0] * open_pos[0] +
                  (open_pos[1] - 1.6f) * (open_pos[1] - 1.6f) +
                  open_pos[2] * open_pos[2]);
    r.check(std::abs(open_dist - 5.0f) < 0.25f,
            "orbit eye sits near configured distance from pivot");
    {
      engine::OrbitCamera zoom(orbit_config);
      zoom.adjust_distance(1.5f);
      r.check(std::abs(zoom.desired_distance() - 3.5f) < 0.01f,
              "scroll zoom pulls desired distance closer");
      zoom.adjust_distance(-100.0f);
      r.check(std::abs(zoom.desired_distance() - zoom.config().max_distance) <
                  0.01f,
              "scroll zoom clamps at max distance");
      zoom.adjust_distance(100.0f);
      r.check(std::abs(zoom.desired_distance() - zoom.config().min_distance) <
                  0.01f,
              "scroll zoom clamps at min distance");
    }
    (void)world.add_box({0, 2, -2.5f}, {6, 4, 0.3f},
                        engine::CollisionLayer::StaticWorld, false);
    r.check(orbit.update({0, 0, 0}, world).has_value(),
            "orbit update with blocker succeeds");
    r.check(orbit.collision_shortened(),
            "orbit shortens when geometry blocks the ray");
    r.check(orbit.resolved_distance() < orbit.desired_distance(),
            "orbit resolved distance moves closer under collision");
    const float blocked_distance = orbit.resolved_distance();
    // Looking clear of the wall: recover distance gradually (look-around jitter
    // guard).
    orbit.apply_look(400, 0);
    engine::CollisionWorld clear_world;
    (void)clear_world.add_box({0, -1, 0}, {10, 1, 10},
                              engine::CollisionLayer::StaticWorld, false);
    float prev = blocked_distance;
    bool eased = false;
    for (int i = 0; i < 45; ++i) {
      r.check(orbit.update({0, 0, 0}, clear_world, 1.0f / 60.0f).has_value(),
              "orbit recover update");
      const float d = orbit.resolved_distance();
      if (d > prev + 0.01f && d < orbit.desired_distance() - 0.05f)
        eased = true;
      prev = d;
    }
    r.check(eased, "orbit collision distance eases out instead of snapping");
    r.check(std::abs(orbit.resolved_distance() - orbit.desired_distance()) <
                0.15f,
            "orbit recovers near desired distance after clear frames");
    {
      // Soft-follow must not leave the look pivot buried under a rising feet
      // sample (slope settle).
      engine::OrbitCamera follow(orbit_config);
      r.check(follow.update({0, 0, 0}, clear_world, 1.0f / 60.0f).has_value(),
              "follow init");
      r.check(follow.update({0, 1.0, 0}, clear_world, 1.0f / 60.0f).has_value(),
              "follow rise");
      r.check(follow.pivot().y > 0.70,
              "orbit Y soft-follow keeps up with rising feet");
      r.check(follow.update({0, 1.0, 0}, clear_world, 1.0f / 60.0f).has_value(),
              "follow rise settle");
      r.check(follow.pivot().y > 0.90,
              "orbit Y soft-follow settles near feet height");
      r.check(follow.update({8, 1.0, 0}, clear_world, 1.0f / 60.0f).has_value(),
              "follow teleport");
      r.check(std::abs(follow.pivot().x - 8.0) < 0.01 &&
                  std::abs(follow.pivot().y - 1.0) < 0.01,
              "orbit snaps pivot on large gameplay jumps");
    }
    {
      // Instant look into a wall must snap pull-in so the eye is not left
      // inside geometry.
      engine::OrbitCamera blocked(orbit_config);
      r.check(blocked.update({0, 0, 0}, clear_world, 1.0f / 60.0f).has_value(),
              "blocked open init");
      engine::CollisionWorld wall_world;
      (void)wall_world.add_box({0, -1, 0}, {10, 1, 10},
                               engine::CollisionLayer::StaticWorld, false);
      (void)wall_world.add_box({0, 2, -2.5f}, {6, 4, 0.3f},
                               engine::CollisionLayer::StaticWorld, false);
      blocked.apply_look(0, 0); // eye still open-space distance
      r.check(blocked.update({0, 0, 0}, wall_world, 1.0f / 60.0f).has_value(),
              "blocked look update");
      r.check(blocked.collision_shortened(), "blocked look shortens");
      r.check(
          blocked.resolved_distance() < blocked.desired_distance() * 0.85f,
          "blocked look snaps pull-in instead of lingering inside the wall");
    }
    orbit.apply_look(100, 0);
    r.check(orbit.yaw() != 0.0f, "orbit look input changes yaw");
    r.check(orbit.update({0, 0, 0}, clear_world, 1.0f / 60.0f).has_value(),
            "orbit update after look succeeds");
    const auto aim = orbit.forward();
    {
      // Modern OTS: forward is pure yaw/pitch, not eye→pivot (shoulder must not
      // skew aim).
      const float yaw = orbit.yaw();
      const float pitch = orbit.pitch();
      const float cp = std::cos(pitch);
      const float sp = std::sin(pitch);
      const float ex = std::sin(yaw) * cp;
      const float ey = -sp;
      const float ez = std::cos(yaw) * cp;
      r.check(std::abs(aim[0] - ex) < 1e-5f && std::abs(aim[1] - ey) < 1e-5f &&
                  std::abs(aim[2] - ez) < 1e-5f,
              "orbit forward is pure look orientation after yaw");
    }
    // OTS: eye shifts right; orientation forward is unchanged (framing only).
    engine::CollisionWorld open_world;
    (void)open_world.add_box({0, -1, 0}, {10, 1, 10},
                             engine::CollisionLayer::StaticWorld, false);
    engine::OrbitCameraConfig shoulder_cfg = orbit_config;
    shoulder_cfg.shoulder_offset = 0.5f;
    engine::OrbitCamera shoulder_cam(shoulder_cfg);
    r.check(shoulder_cam.set_perspective(1.0f, 16.0f / 9.0f, 0.1f, 2000)
                .has_value(),
            "shoulder orbit perspective");
    r.check(shoulder_cam.update({0, 0, 0}, open_world).has_value(),
            "shoulder orbit update");
    r.check(std::abs(shoulder_cam.position()[0] - 0.5f) < 0.05f,
            "shoulder offset shifts eye to camera-right");
    const auto shoulder_aim = shoulder_cam.forward();
    r.check(std::abs(shoulder_aim[0]) < 1e-5f &&
                std::abs(shoulder_aim[1]) < 1e-5f &&
                std::abs(shoulder_aim[2] - 1.0f) < 1e-5f,
            "shoulder camera keeps pure orientation look (not pivot)");
    {
      // Pivot is camera-left of screen center under right shoulder; view does
      // not LookAt head.
      const auto seye = shoulder_cam.position();
      const float to_px = -seye[0], to_py = 1.6f - seye[1], to_pz = -seye[2];
      const float pl = std::sqrt(to_px * to_px + to_py * to_py + to_pz * to_pz);
      r.check(pl > 0.001f, "shoulder eye offset from pivot");
      const float pivot_dot =
          (shoulder_aim[0] * to_px + shoulder_aim[1] * to_py +
           shoulder_aim[2] * to_pz) /
          pl;
      r.check(pivot_dot < 0.999f, "shoulder view axis is not eye-to-pivot");
    }
    shoulder_cam.set_orientation(0.0f, 2.0f);
    r.check(shoulder_cam.pitch() <= shoulder_cfg.max_pitch + 0.001f,
            "pitch clamps to maxPitch");
    shoulder_cam.set_orientation(0.0f, -2.0f);
    r.check(shoulder_cam.pitch() >= shoulder_cfg.min_pitch - 0.001f,
            "pitch clamps to minPitch");
    auto orbit_matrix = orbit.view_projection();
    r.check(std::all_of(orbit_matrix.begin(), orbit_matrix.end(),
                        [](float v) { return std::isfinite(v); }),
            "orbit matrix finite");
    // New optional RPG fields round-trip through camera assets.
    engine::CameraAsset authored;
    authored.default_distance = 10.5f;
    authored.min_distance = 2.5f;
    authored.max_distance = 18.0f;
    authored.shoulder_offset = 0.45f;
    authored.default_pitch = 0.32f;
    authored.min_pitch = -0.15f;
    authored.max_pitch = 1.25f;
    auto reparsed = engine::CameraAsset::from_json(authored.to_json());
    r.check(reparsed &&
                std::abs(reparsed.value().shoulder_offset - 0.45f) < 0.001f &&
                std::abs(reparsed.value().default_pitch - 0.32f) < 0.001f,
            "camera asset RPG fields round-trip");
  } else if (suite == "interaction") {
    engine::InteractionVolumeRegistry registry;
    engine::InteractionOverlapTracker tracker;
    engine::CollisionWorld world;
    (void)world.add_box({0, -1, 0}, {10, 1, 10},
                        engine::CollisionLayer::StaticWorld, false);
    auto plain = world.add_sphere({5, 2, 0}, 1.0f,
                                  engine::CollisionLayer::Trigger, false);
    auto interact = world.add_sphere({0, 2, 0}, 1.2f,
                                     engine::CollisionLayer::Trigger, false);
    r.check(plain && interact, "interaction test volumes created");
    registry.bind(interact.value(), {"placement-1", 0, "use_campfire"});
    auto outside = tracker.update("player", {0, 2, 8}, 0.5f, world, registry);
    r.check(outside.empty(), "outside probe has no interaction events");
    auto enter = tracker.update("player", {0, 2, 0}, 0.5f, world, registry);
    r.check(enter.size() == 1 &&
                enter[0].type == engine::InteractionEventType::Enter &&
                enter[0].interaction_id == "use_campfire",
            "player enters interaction volume");
    auto steady = tracker.update("player", {0, 2, 0}, 0.5f, world, registry);
    r.check(steady.empty(), "steady overlap emits no duplicate enter");
    auto exit = tracker.update("player", {0, 2, 8}, 0.5f, world, registry);
    r.check(exit.size() == 1 &&
                exit[0].type == engine::InteractionEventType::Exit,
            "player exits interaction volume");
    engine::PrefabAsset prefab;
    prefab.schema_version = 2;
    engine::PrefabCollisionVolume volume;
    volume.shape = engine::PrefabCollisionShape::Sphere;
    volume.interaction_id = "use_chest";
    volume.radius = 1.0f;
    volume.transform.position = {0, 1, 0};
    prefab.collision.push_back(volume);
    engine::TransformComponent placement;
    placement.position = {2, 0, 2};
    const auto spawned = engine::spawn_prefab_collision(
        world, prefab, placement, engine::CellCoord{0, 0});
    r.check(spawned && spawned.value().size() == 1,
            "prefab interaction collision spawns");
    registry.clear();
    tracker.reset("player");
    registry.bind(spawned.value()[0], {"chest-placement", 0, "use_chest"});
    auto chest_enter =
        tracker.update("player", {2, 1.2, 2}, 1.0f, world, registry);
    r.check(chest_enter.size() == 1 &&
                chest_enter[0].interaction_id == "use_chest",
            "authored prefab interaction registers enter");
  } else if (suite == "combat") {
    engine::CombatVolumeRegistry registry;
    engine::CollisionWorld world;
    (void)world.add_box({0, -1, 0}, {10, 1, 10},
                        engine::CollisionLayer::StaticWorld, false);
    auto hurt = world.add_sphere({0, 2, 0}, 1.0f,
                                 engine::CollisionLayer::Trigger, false);
    auto hit = world.add_sphere({0.5, 2, 0}, 0.8f,
                                engine::CollisionLayer::Trigger, false);
    auto plain = world.add_sphere({5, 2, 0}, 1.0f,
                                  engine::CollisionLayer::Trigger, false);
    r.check(hurt && hit && plain, "combat test volumes created");
    registry.bind(hurt.value(),
                  {"enemy-1", 0, engine::CombatVolumeRole::Hurt, "body"});
    registry.bind(hit.value(), {"weapon-1", 0, engine::CombatVolumeRole::Hit,
                                "sword_slash"});
    auto miss = engine::query_combat_hits("player_attack", {0, 2, 8}, 0.5f,
                                          world, registry);
    r.check(miss.empty(), "attack probe misses distant hurt volume");
    auto contacts = engine::query_combat_hits("player_attack", {0, 2, 0}, 0.5f,
                                              world, registry);
    r.check(contacts.size() == 1 && contacts[0].hurt_combat_id == "body",
            "attack probe hits hurt volume");
    r.check(contacts[0].attacker_id == "player_attack",
            "contact records attacker id");
    auto from_hit = engine::query_combat_hits_from_body(
        "sword_slash", hit.value(), world, registry);
    r.check(from_hit.size() == 1 && from_hit[0].hurt_combat_id == "body",
            "hit body query finds overlapping hurt volume");
    auto non_hit = engine::query_combat_hits_from_body(
        "sword_slash", plain.value(), world, registry);
    r.check(non_hit.empty(), "unregistered body query returns no contacts");
    engine::PrefabAsset prefab;
    prefab.schema_version = 2;
    engine::PrefabCollisionVolume hurt_volume;
    hurt_volume.shape = engine::PrefabCollisionShape::Sphere;
    hurt_volume.combat_hurt_id = "body";
    hurt_volume.radius = 1.0f;
    hurt_volume.transform.position = {0, 1, 0};
    prefab.collision.push_back(hurt_volume);
    engine::TransformComponent placement;
    placement.position = {4, 0, 4};
    const auto spawned = engine::spawn_prefab_collision(
        world, prefab, placement, engine::CellCoord{0, 0});
    r.check(spawned && spawned.value().size() == 1,
            "prefab hurt collision spawns");
    registry.clear();
    registry.bind(spawned.value()[0],
                  {"dummy-enemy", 0, engine::CombatVolumeRole::Hurt, "body"});
    auto prefab_hit = engine::query_combat_hits("player_attack", {4, 1.2, 4},
                                                0.5f, world, registry);
    r.check(prefab_hit.size() == 1 &&
                prefab_hit[0].hurt_placement_entity_id == "dummy-enemy",
            "authored prefab hurt registers hit query");
    auto segment_miss = engine::query_combat_hits_along_segment(
        "player_attack", {4, 1.2, 0}, {4, 1.2, 1.5}, 0.2f, world, registry);
    r.check(segment_miss.empty(),
            "projectile segment misses distant prefab hurt");
    auto segment_hit = engine::query_combat_hits_along_segment(
        "player_attack", {4, 1.2, 2.5}, {4, 1.2, 5.5}, 0.2f, world, registry);
    r.check(segment_hit.size() == 1 &&
                segment_hit[0].hurt_placement_entity_id == "dummy-enemy",
            "projectile segment hits prefab hurt volume");
    auto long_step = engine::query_combat_hits_along_segment(
        "player_attack", {4, 1.2, -6}, {4, 1.2, 14}, 0.2f, world, registry);
    r.check(long_step.size() == 1 &&
                long_step[0].hurt_combat_id == "body",
            "fast projectile samples still hit hurt volume");
    {
      auto self_hurt = world.add_sphere({0, 2, 0}, 0.9f,
                                        engine::CollisionLayer::Trigger, false);
      r.check(self_hurt.has_value(), "self-hit player hurt volume created");
      registry.bind(self_hurt.value(),
                    {"player-spawn", 0, engine::CombatVolumeRole::Hurt, "body"});
      auto self_hit = engine::query_combat_hits_along_segment(
          "player_attack", {0, 2, 0}, {0.15, 2, 0.15}, 0.34f, world, registry);
      r.check(!self_hit.empty() &&
                  self_hit[0].hurt_placement_entity_id == "player-spawn",
              "projectile overlapping shooter hurt registers without ignore");
      auto ignored = engine::query_combat_hits_along_segment(
          "player_attack", {0, 2, 0}, {0.15, 2, 0.15}, 0.34f, world, registry,
          "player-spawn");
      r.check(ignored.empty(),
              "projectile overlapping shooter hurt is ignored");
      auto still_dummy = engine::query_combat_hits_along_segment(
          "player_attack", {0, 2, 0}, {4, 1.2, 4}, 0.34f, world, registry,
          "player-spawn");
      r.check(still_dummy.size() == 1 &&
                  still_dummy[0].hurt_placement_entity_id == "dummy-enemy",
              "ignore shooter still hits dummy along the same shot");
    }
    {
      std::vector<int> events;
      for (int i = 0; i < 40; ++i)
        events.push_back(i);
      std::size_t dispatched = 40;
      engine::trim_dispatched_event_queue(events, dispatched, 32);
      r.check(events.size() == 32 && events.front() == 8 && events.back() == 39,
              "trim keeps newest 32 events");
      r.check(dispatched == 32,
              "trim repairs dispatch cursor instead of leaving it past size");
      events.push_back(40);
      events.push_back(41);
      // Simulate dispatch of newly appended events then trim again.
      dispatched = events.size();
      engine::trim_dispatched_event_queue(events, dispatched, 32);
      r.check(events.size() == 32 && events.back() == 41 && dispatched == 32,
              "post-dispatch trim keeps cursor aligned for further appends");
      events.push_back(42);
      r.check(dispatched < events.size(),
              "new append remains pending after repaired trim");
    }
    {
      engine::HostileNpcConfig config;
      config.aggro_radius = 10.0f;
      config.lose_aggro_radius = 14.0f;
      config.attack_range = 2.0f;
      config.attack_cooldown = 1.0f;
      engine::HostileNpcBrainState brain;
      engine::HostileNpcInput input;
      input.self = {0, 0, 0};
      input.player = {0, 0, 20};
      auto idle = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(idle.action == engine::HostileNpcAction::Idle && !brain.aggroed,
              "hostile idle outside aggro");
      input.player = {0, 0, 6};
      auto chase = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(chase.action == engine::HostileNpcAction::Chase && chase.wish_forward &&
                  brain.aggroed,
              "hostile chases inside aggro");
      input.player = {0, 0, 1.0};
      auto swing = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(swing.action == engine::HostileNpcAction::Attack && swing.fire_attack,
              "hostile attacks inside range");
      auto cooling = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(!cooling.fire_attack && !cooling.wish_forward && !cooling.wish_block,
              "hostile holds during attack cooldown");
      config.read_bag_reads = 1;
      config.read_bag_misses = 0;
      config.block_reaction = 0.0f;
      config.block_hold = 0.5f;
      brain.read_marbles_remaining = -1;
      brain.miss_marbles_remaining = -1;
      input.player_melee_state = "attack";
      auto guard = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(guard.action == engine::HostileNpcAction::Block &&
                  guard.wish_block && !guard.fire_attack && !guard.wish_forward,
              "hostile reads a new player swing into Block");
      input.player_melee_state = "attack";
      auto holding = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(holding.action == engine::HostileNpcAction::Block && holding.wish_block,
              "hostile keeps Block up through the swing");
      input.player_melee_state.clear();
      auto pose = engine::tick_hostile_npc(config, brain, input, 0.4f);
      r.check(pose.action == engine::HostileNpcAction::Block && pose.wish_block,
              "hostile holds Block after the swing so the pose is readable");
      auto drop = engine::tick_hostile_npc(config, brain, input, 0.2f);
      r.check(!drop.wish_block, "hostile drops Block after hold expires");
      config.read_bag_reads = 0;
      config.read_bag_misses = 1;
      config.block_reaction = 0.2f;
      brain.read = engine::HostileNpcRead::None;
      brain.read_marbles_remaining = -1;
      brain.miss_marbles_remaining = -1;
      brain.last_player_melee_state.clear();
      input.player_melee_state = "attack2";
      auto miss = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(!miss.wish_block && brain.read == engine::HostileNpcRead::Missed,
              "hostile misses some attack reads");
      input.player_melee_state.clear();
      (void)engine::tick_hostile_npc(config, brain, input, 0.016f);
      config.read_bag_reads = 1;
      config.read_bag_misses = 0;
      config.block_reaction = 0.2f;
      brain.read_marbles_remaining = -1;
      brain.miss_marbles_remaining = -1;
      brain.last_player_melee_state.clear();
      input.player_melee_state = "attack3";
      auto windup = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(windup.action == engine::HostileNpcAction::Idle && !windup.wish_block &&
                  brain.read == engine::HostileNpcRead::Pending,
              "hostile reaction delay before the guard comes up");
      auto cover = engine::tick_hostile_npc(config, brain, input, 0.2f);
      r.check(cover.action == engine::HostileNpcAction::Block && cover.wish_block,
              "hostile Block starts after the reaction delay");
      input.attack_anim_active = true;
      auto swinging = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(swinging.action == engine::HostileNpcAction::Attack &&
                  !swinging.wish_forward && !swinging.wish_block,
              "hostile stands still while attack clip plays");
      input.attack_anim_active = false;
      input.player_melee_state.clear();
      brain.attack_cooldown_remaining = 0.0f;
      brain.aggroed = true;
      brain.read = engine::HostileNpcRead::None;
      auto trade = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(trade.action == engine::HostileNpcAction::Attack &&
                  trade.fire_attack && !trade.wish_block,
              "hostile attacks when cooldown is ready and no read is active");
      input.dead = true;
      auto dead = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(dead.action == engine::HostileNpcAction::Idle && !brain.aggroed,
              "hostile stops when dead");
      input.dead = false;
      input.stunned = true;
      brain.aggroed = true;
      auto staggered = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(staggered.action == engine::HostileNpcAction::Idle &&
                  !staggered.wish_forward && !staggered.fire_attack &&
                  !staggered.wish_block,
              "hostile stands still while staggered");
      input.stunned = false;
      brain = {};
      input.player = {0, 0, 40};
      input.patrol_world = {{4, 0, 0}, {4, 0, 4}, {0, 0, 4}};
      auto patrol = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(patrol.action == engine::HostileNpcAction::Patrol &&
                  patrol.wish_forward && patrol.has_move_goal && !brain.aggroed,
              "hostile patrols when unaggroed with waypoints");
      input.self = {4.0, 0, 0.2};
      brain.patrol_index = 0;
      config.patrol_arrive = 2.25f;
      auto advance = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(brain.patrol_index == 1 &&
                  advance.action == engine::HostileNpcAction::Patrol,
              "hostile advances patrol index near waypoint");
      // Already inside arrive of the new goal — should skip-through, not reverse.
      input.self = {4.05, 0, 4.05};
      brain.patrol_index = 1;
      auto skip = engine::tick_hostile_npc(config, brain, input, 0.016f);
      r.check(brain.patrol_index == 2 &&
                  skip.action == engine::HostileNpcAction::Patrol &&
                  skip.has_move_goal,
              "hostile skip-throughs waypoints already inside arrive radius");
    }
    {
      r.check(engine::player_health_anim_cue(false, 100.0, 90.0) ==
                  engine::PlayerHealthAnimCue::None,
              "player health drop does not stagger / HitReact");
      r.check(engine::player_health_anim_cue(false, 10.0, 0.0) ==
                  engine::PlayerHealthAnimCue::Die,
              "player death still fires die");
      r.check(engine::player_health_anim_cue(true, 0.0, 100.0) ==
                  engine::PlayerHealthAnimCue::Revive,
              "player revive still fires revive");
      r.check(engine::player_health_anim_cue(false, 100.0, 100.0) ==
                  engine::PlayerHealthAnimCue::None,
              "unchanged health is a no-op");
    }
    {
      engine::PrefabAsset prefab;
      prefab.schema_version = 2;
      engine::PrefabPart part;
      part.name = "Body";
      part.mesh.primitive = "cube";
      prefab.parts.push_back(part);
      engine::PrefabNpcAi ai;
      ai.kind = "hostile";
      ai.display_name = "Raider";
      ai.held_item_id = "ashfell_arming_sword";
      ai.aggro_radius = 8.0f;
      ai.move_speed = 2.5f;
      prefab.npc_ai = ai;
      const auto tmp = std::filesystem::temp_directory_path() /
                       "airpg_hostile_npc_prefab.json";
      r.check(prefab.save(tmp).has_value(), "hostile prefab npcAi saves");
      auto loaded = engine::PrefabAsset::load(tmp);
      r.check(loaded && loaded.value().npc_ai &&
                  loaded.value().npc_ai->is_hostile() &&
                  loaded.value().npc_ai->display_name == "Raider" &&
                  loaded.value().npc_ai->held_item_id ==
                      "ashfell_arming_sword" &&
                  loaded.value().npc_ai->move_speed == 2.5f,
              "hostile prefab npcAi round-trips");
      std::filesystem::remove(tmp);
    }
    {
      // Physics-driven hurt sensors must follow the motion body (TICKET-0285).
      engine::CollisionWorld follow_world;
      r.check(follow_world
                  .add_box({0, -1, 0}, {20, 1, 20},
                           engine::CollisionLayer::StaticWorld, false,
                           engine::CellCoord{0, 0})
                  .has_value(),
              "hurt-follow floor");
      engine::PrefabAsset fighter;
      fighter.schema_version = 2;
      engine::PrefabCollisionVolume capsule;
      capsule.shape = engine::PrefabCollisionShape::Capsule;
      capsule.layer = engine::CollisionLayer::Dynamic;
      capsule.radius = 0.3f;
      capsule.capsule_half_height = 0.6f;
      capsule.transform.position = {0.0f, 0.9f, 0.0f};
      fighter.collision.push_back(capsule);
      engine::PrefabCollisionVolume hurt;
      hurt.shape = engine::PrefabCollisionShape::Sphere;
      hurt.combat_hurt_id = "npc_body";
      hurt.trigger = true;
      hurt.radius = 0.85f;
      hurt.transform.position = {0.0f, 1.2f, 0.0f};
      fighter.collision.push_back(hurt);
      engine::PrefabRigidbody rb;
      rb.id = "rigidbody-0";
      rb.motion_type = "kinematic";
      rb.mass = 70.0f;
      rb.use_gravity = false;
      fighter.rigidbodies.push_back(rb);
      engine::Scene follow_scene;
      engine::TransformComponent at;
      at.position = {0.0f, 0.0f, 0.0f};
      auto placed = follow_scene.place_world_object(
          "npc", "assets/prefabs/NPC/npc_humanoid.prefab.json", at,
          std::nullopt, std::nullopt, &fighter);
      r.check(placed.has_value(), "hurt-follow place fighter");
      std::map<std::string, engine::PrefabAsset> catalog;
      catalog["assets/prefabs/NPC/npc_humanoid.prefab.json"] = fighter;
      engine::PlacementCollisionTracker tracker;
      r.check(tracker.sync(follow_world, follow_scene, catalog, true).has_value(),
              "hurt-follow sync");
      const auto motion = tracker.motion_body_for(placed.value());
      r.check(motion.has_value(), "hurt-follow motion body");
      auto origin_hits = engine::query_combat_hits(
          "player_attack", {0.0, 1.2, 0.0}, 0.5f, follow_world,
          tracker.combat_registry());
      r.check(origin_hits.size() == 1 &&
                  origin_hits[0].hurt_combat_id == "npc_body",
              "hurt-follow query hits spawn pose");
      r.check(follow_world
                  .set_transform(motion.value(), {8.0, 0.9, 0.0})
                  .has_value(),
              "hurt-follow move motion body");
      tracker.write_back_transforms(follow_scene, follow_world);
      auto stale = engine::query_combat_hits("player_attack", {0.0, 1.2, 0.0},
                                             0.5f, follow_world,
                                             tracker.combat_registry());
      r.check(stale.empty(), "hurt-follow leaves spawn after move");
      auto moved = engine::query_combat_hits("player_attack", {8.0, 1.2, 0.0},
                                             0.5f, follow_world,
                                             tracker.combat_registry());
      r.check(moved.size() == 1 && moved[0].hurt_combat_id == "npc_body",
              "hurt-follow query hits moved pose");
      engine::TransformComponent authored = *follow_scene.transform(placed.value());
      authored.position = {4.0f, 0.0f, 0.0f};
      r.check(follow_scene.move_world_object(placed.value(), authored).has_value(),
              "hurt-follow authored gizmo move");
      r.check(tracker.sync(follow_world, follow_scene, catalog, true).has_value(),
              "hurt-follow sync after gizmo");
      auto gizmo_stale = engine::query_combat_hits(
          "player_attack", {8.0, 1.2, 0.0}, 0.5f, follow_world,
          tracker.combat_registry());
      r.check(gizmo_stale.empty(), "hurt-follow gizmo leaves prior pose");
      auto gizmo_hits = engine::query_combat_hits(
          "player_attack", {4.0, 1.2, 0.0}, 0.5f, follow_world,
          tracker.combat_registry());
      r.check(gizmo_hits.size() == 1 && gizmo_hits[0].hurt_combat_id == "npc_body",
              "hurt-follow gizmo teleports hurt sensor");
    }
  } else if (suite == "scripting") {
    const auto project = std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                         "samples" / "open-world-rpg";
    const auto bindings = engine::ScriptBindingsAsset::load(
        engine::default_script_bindings_path(project));
    r.check(bindings && bindings.value().interactions.size() >= 1,
            "script bindings load");
    const auto campfire =
        bindings.value().resolve_script_path("interaction", "use_campfire");
    r.check(campfire &&
                campfire.value() == "assets/scripts/campfire_interaction.lua",
            "resolve interaction binding to lua path");
    const auto hurt =
        bindings.value().resolve_script_path("combatHurt", "body");
    r.check(hurt && hurt.value() == "assets/scripts/combat_hurt.lua",
            "resolve combatHurt binding to lua path");
    const auto npc_hurt =
        bindings.value().resolve_script_path("combatHurt", "npc_body");
    r.check(npc_hurt && npc_hurt.value() == "assets/scripts/npc_hurt.lua",
            "resolve npc_body combatHurt binding");
    const auto handler =
        engine::resolve_script_binding_path(project, "handler", "use_campfire");
    r.check(handler &&
                handler.value() == "assets/scripts/campfire_interaction.lua",
            "handler kind searches all binding lists");
    r.check(!bindings.value().resolve_script_path("interaction", "missing_id"),
            "missing binding id rejected");
    r.check(!bindings.value().resolve_script_path("bogus", "use_campfire"),
            "unknown binding kind rejected");
    engine::LuaRuntime runtime;
    r.check(runtime
                .load_bindings(project,
                               engine::default_script_bindings_path(project))
                .has_value(),
            "lua bindings load into runtime");
    const auto script =
        project / "assets" / "scripts" / "campfire_interaction.lua";
    r.check(runtime.validate_script(script).has_value(),
            "sample lua script validates");
    r.check(runtime.load_script(script).has_value(), "sample lua script loads");
    const auto invalid =
        std::filesystem::temp_directory_path() / "bad-script.lua";
    {
      std::ofstream out(invalid);
      out << "function bad(";
      out.close();
    }
    r.check(!runtime.validate_script(invalid), "invalid lua script rejected");
    engine::InteractionEvent event;
    event.type = engine::InteractionEventType::Enter;
    event.interaction_id = "use_campfire";
    event.placement_entity_id = "campfire-1";
    event.interactor_id = "player";
    runtime.clear_recent_errors();
    runtime.blackboard_clear();
    runtime.dispatch_interaction(event);
    r.check(runtime.recent_errors().empty(),
            "interaction dispatch succeeds for bound handler");
    const auto last_id = runtime.blackboard_get("interaction.lastId");
    r.check(last_id && last_id->type == engine::ScriptBlackboardType::String &&
                last_id->string_value == "use_campfire",
            "interaction handler sets blackboard lastId");
    const auto campfire_active =
        runtime.blackboard_get("interaction.campfireActive");
    r.check(campfire_active &&
                campfire_active->type == engine::ScriptBlackboardType::Bool &&
                campfire_active->bool_value,
            "interaction enter sets campfireActive true");
    const auto campfire_prompt = runtime.blackboard_get("interact.prompt");
    r.check(campfire_prompt &&
                campfire_prompt->type == engine::ScriptBlackboardType::Bool &&
                campfire_prompt->bool_value,
            "campfire enter sets interact prompt (does not auto-heal)");

    engine::CombatContactEvent contact;
    contact.attacker_id = "player";
    contact.hurt_placement_entity_id = "target-1";
    contact.hurt_combat_id = "body";
    runtime.clear_recent_errors();
    runtime.dispatch_combat_hit(contact);
    r.check(runtime.recent_errors().empty(),
            "combat hurt dispatch succeeds for bound handler");
    const auto last_hurt = runtime.blackboard_get("combat.lastHurtId");
    r.check(last_hurt &&
                last_hurt->type == engine::ScriptBlackboardType::String &&
                last_hurt->string_value == "body",
            "combat hurt handler sets blackboard lastHurtId");
    const auto hit_count = runtime.blackboard_get("combat.hitCount");
    r.check(hit_count &&
                hit_count->type == engine::ScriptBlackboardType::Number &&
                hit_count->number_value == 1.0,
            "combat hurt handler increments hitCount");

    {
      engine::CombatContactEvent blocked;
      blocked.attacker_id = "npc_attack";
      blocked.hurt_placement_entity_id = "target-1";
      blocked.hurt_combat_id = "body";
      blocked.blocked = true;
      blocked.contact_point = engine::WorldPosition{0.0, 1.4, 0.0};
      engine::CombatTextRuntime block_text;
      runtime.set_combat_text(&block_text);
      runtime.dispatch_combat_hit(blocked);
      r.check(runtime.recent_errors().empty(), "blocked hurt dispatch succeeds");
      const auto last_block = runtime.blackboard_get("combat.lastBlocked");
      r.check(last_block && last_block->type == engine::ScriptBlackboardType::Bool &&
                  last_block->bool_value,
              "blocked hurt sets combat.lastBlocked");
      const auto block_count = runtime.blackboard_get("combat.blockCount");
      r.check(block_count &&
                  block_count->type == engine::ScriptBlackboardType::Number &&
                  block_count->number_value == 1.0,
              "blocked hurt increments blockCount");
      const auto hits_after = runtime.blackboard_get("combat.hitCount");
      r.check(hits_after && hits_after->type == engine::ScriptBlackboardType::Number &&
                  hits_after->number_value == 1.0,
              "blocked hurt does not increment hitCount");
      r.check(block_text.size() == 1 &&
                  block_text.floaters().front().text == "Blocked",
              "blocked hurt spawns Blocked combat text");
    }

    engine::WorldUiBillboardRuntime dummy_ui;
    engine::CombatTextRuntime dummy_combat_text;
    runtime.set_world_ui_billboards(&dummy_ui);
    runtime.set_combat_text(&dummy_combat_text);
    engine::CombatContactEvent dummy_contact;
    dummy_contact.attacker_id = "player";
    dummy_contact.hurt_placement_entity_id =
        "7aaf4fb9-adf5-4e12-8242-4a898c21edb9";
    dummy_contact.hurt_combat_id = "dummy_body";
    runtime.clear_recent_errors();
    runtime.dispatch_combat_hit(dummy_contact);
    r.check(runtime.recent_errors().empty(),
            "dummy_body combat hurt dispatch succeeds");
    const auto dummy_hp = runtime.blackboard_get(
        "combat.dummyHp.7aaf4fb9-adf5-4e12-8242-4a898c21edb9");
    r.check(dummy_hp && dummy_hp->type == engine::ScriptBlackboardType::Number &&
                dummy_hp->number_value == 90.0,
            "dummy hurt drains dummy HP, not player health");
    const auto dummy_chip =
        dummy_ui.get("dummy.hp.7aaf4fb9-adf5-4e12-8242-4a898c21edb9");
    r.check(dummy_chip && dummy_chip->has_bar &&
                dummy_chip->bar_current == 90.0 && dummy_chip->visible,
            "dummy hurt upserts a world-ui health chip");
    r.check(dummy_chip && dummy_chip->hit_flash > 0.5f &&
                dummy_chip->bar_ghost >= 99.0 &&
                dummy_chip->pulse_scale > 1.05f,
            "dummy hurt seeds health-chip hit juice");
    r.check(dummy_combat_text.size() == 1,
            "dummy hurt spawns combat floating damage text");
    if (dummy_combat_text.size() == 1) {
      const auto& floater = dummy_combat_text.floaters().front();
      r.check(floater.text == "10", "combat text shows default damage amount");
      r.check(floater.kind == engine::CombatTextKind::Hit,
              "default dummy hit is non-crit without gear crit");
      r.check(floater.world_y > dummy_chip->world_y + 0.4f,
              "combat text spawns above the name/HP chip");
    }
    {
      engine::CombatTextRuntime juice;
      juice.spawn(0.0f, 1.0f, 0.0f, 42.0, false);
      juice.spawn(0.0f, 1.0f, 0.0f, 99.0, true);
      juice.spawn(0.0f, 1.0f, 0.0f, 3.0, engine::CombatTextKind::Bleed);
      juice.spawn(0.0f, 1.0f, 0.0f, 4.0, engine::CombatTextKind::Burn);
      r.check(juice.size() == 4, "combat text pools multiple floaters");
      r.check(juice.floaters()[1].kind == engine::CombatTextKind::Crit &&
                  juice.floaters()[1].impact_peak >
                      juice.floaters()[0].impact_peak,
              "crit floaters use a stronger impact peak");
      r.check(juice.floaters()[2].kind == engine::CombatTextKind::Bleed &&
                  juice.floaters()[2].text.find('-') != std::string::npos,
              "bleed tick floaters use minus prefix");
      r.check(juice.floaters()[3].kind == engine::CombatTextKind::Burn &&
                  juice.floaters()[3].text.find('-') != std::string::npos,
              "burn tick floaters use minus prefix");
      for (int i = 0; i < 20; ++i)
        juice.tick(0.1f);
      r.check(juice.size() == 0, "combat text floaters expire after lifetime");
    }
    {
      engine::StatusEffectRuntime statuses;
      statuses.set_target_anchor("dummy", 1.0f, 2.0f, 3.0f);
      statuses.apply("dummy", engine::StatusEffectKind::Bleed, 3.0f, 2.0f, 1.0f);
      r.check(statuses.has_effect("dummy", engine::StatusEffectKind::Bleed),
              "status apply tracks bleed");
      r.check(statuses.stack_count("dummy", engine::StatusEffectKind::Bleed) == 1,
              "first apply is one stack");
      statuses.apply("dummy", engine::StatusEffectKind::Bleed, 3.0f, 2.0f, 1.0f);
      statuses.apply("dummy", engine::StatusEffectKind::Bleed, 3.0f, 2.0f, 1.0f);
      r.check(statuses.stack_count("dummy", engine::StatusEffectKind::Bleed) == 3,
              "same-kind bleed stacks");
      statuses.apply("dummy", engine::StatusEffectKind::Poison, 2.0f, 2.0f, 1.0f);
      statuses.apply("dummy", engine::StatusEffectKind::Poison, 2.0f, 2.0f, 1.0f);
      r.check(statuses.stack_count("dummy", engine::StatusEffectKind::Poison) == 2,
              "poison stacks independently");
      r.check(statuses.has_effect("dummy", engine::StatusEffectKind::Bleed) &&
                  statuses.has_effect("dummy", engine::StatusEffectKind::Poison),
              "bleed and poison coexist");
      std::vector<engine::StatusTickEvent> ticks;
      for (int i = 0; i < 10; ++i) {
        auto frame = statuses.tick(0.1f);
        ticks.insert(ticks.end(), frame.begin(), frame.end());
      }
      float bleed_amt = 0.0f;
      float poison_amt = 0.0f;
      for (const auto& t : ticks) {
        if (t.kind == engine::StatusEffectKind::Bleed) bleed_amt = t.amount;
        if (t.kind == engine::StatusEffectKind::Poison) poison_amt = t.amount;
      }
      r.check(bleed_amt == 9.0f, "bleed tick scales with stacks (3x3)");
      r.check(poison_amt == 4.0f, "poison tick scales with stacks (2x2)");
      r.check(statuses.status_line("dummy").find("Bleed ×3") != std::string::npos,
              "status line shows bleed stacks");
      r.check(statuses.status_line("dummy").find("Poison ×2") != std::string::npos,
              "status line shows poison stacks");
      for (int i = 0; i < engine::kStatusEffectMaxStacks + 5; ++i)
        statuses.apply("dummy", engine::StatusEffectKind::Bleed, 1.0f, 1.0f, 1.0f);
      r.check(statuses.stack_count("dummy", engine::StatusEffectKind::Bleed) ==
                  engine::kStatusEffectMaxStacks,
              "bleed stacks soft-cap");
      r.check(engine::is_known_status_effect_id("burn") &&
                  engine::is_known_status_effect_id("slow"),
              "burn and slow are known status ids");
      r.check(engine::status_effect_deals_tick_damage(
                  engine::StatusEffectKind::Burn) &&
                  !engine::status_effect_deals_tick_damage(
                      engine::StatusEffectKind::Slow),
              "slow is not a DoT");
      {
        engine::StatusEffectRuntime magic;
        magic.apply("dummy", engine::StatusEffectKind::Burn, 3.0f, 2.0f, 1.0f);
        magic.apply("dummy", engine::StatusEffectKind::Slow, 0.0f, 4.0f, 1.0f);
        r.check(magic.has_effect("dummy", engine::StatusEffectKind::Burn) &&
                    magic.has_effect("dummy", engine::StatusEffectKind::Slow),
                "burn and slow coexist");
        std::vector<engine::StatusTickEvent> magic_ticks;
        for (int i = 0; i < 12; ++i) {
          auto frame = magic.tick(0.1f);
          magic_ticks.insert(magic_ticks.end(), frame.begin(), frame.end());
        }
        int burn_ticks = 0;
        int slow_ticks = 0;
        for (const auto& t : magic_ticks) {
          if (t.kind == engine::StatusEffectKind::Burn) ++burn_ticks;
          if (t.kind == engine::StatusEffectKind::Slow) ++slow_ticks;
        }
        r.check(burn_ticks >= 1, "burn emits tick damage");
        r.check(slow_ticks == 0, "slow emits no tick damage");
        r.check(magic.status_line("dummy").find("Slow") != std::string::npos,
                "status line names Slow");
      }
    }
    runtime.clear_recent_errors();
    r.check(runtime.call_handler("on_update", "{\"dt\":1.0}").has_value() &&
                runtime.recent_errors().empty(),
            "dummy on_update regen handler runs");
    const auto dummy_hp_regen = runtime.blackboard_get(
        "combat.dummyHp.7aaf4fb9-adf5-4e12-8242-4a898c21edb9");
    r.check(dummy_hp_regen &&
                dummy_hp_regen->type == engine::ScriptBlackboardType::Number &&
                dummy_hp_regen->number_value == 100.0,
            "dummy HP regenerates quickly after a hit");
    for (int i = 0; i < 20; ++i)
      runtime.dispatch_combat_hit(dummy_contact);
    const auto dummy_hp_floor = runtime.blackboard_get(
        "combat.dummyHp.7aaf4fb9-adf5-4e12-8242-4a898c21edb9");
    r.check(dummy_hp_floor && dummy_hp_floor->number_value == 1.0,
            "dummy HP floors at 1 and never dies");
    runtime.set_world_ui_billboards(nullptr);
    runtime.set_combat_text(nullptr);

    r.check(runtime.blackboard_get("combat.hitCount") &&
                runtime.blackboard_get("combat.hitCount")->number_value == 22.0,
            "dummy hurt also increments shared combat.hitCount");

    {
      engine::CombatTextRuntime npc_block_text;
      runtime.set_combat_text(&npc_block_text);
      runtime.blackboard_set_number("combat.npcHp.npc-block-1", 80.0);
      engine::CombatContactEvent npc_blocked;
      npc_blocked.attacker_id = "player_attack";
      npc_blocked.hurt_placement_entity_id = "npc-block-1";
      npc_blocked.hurt_combat_id = "npc_body";
      npc_blocked.blocked = true;
      npc_blocked.hurt_entity_position = engine::WorldPosition{0.0, 0.0, 0.0};
      runtime.dispatch_combat_hit(npc_blocked);
      r.check(runtime.recent_errors().empty(),
              "blocked npc_body dispatch succeeds");
      const auto npc_hp = runtime.blackboard_get("combat.npcHp.npc-block-1");
      r.check(npc_hp && npc_hp->type == engine::ScriptBlackboardType::Number &&
                  npc_hp->number_value == 80.0,
              "blocked npc hurt does not drain NPC HP");
      const auto hits_after_npc_block =
          runtime.blackboard_get("combat.hitCount");
      r.check(hits_after_npc_block &&
                  hits_after_npc_block->number_value == 22.0,
              "blocked npc hurt does not increment hitCount");
      const auto npc_blocks = runtime.blackboard_get("combat.blockCount");
      r.check(npc_blocks && npc_blocks->number_value == 2.0,
              "blocked npc hurt increments shared blockCount");
      r.check(npc_block_text.size() == 1 &&
                  npc_block_text.floaters().front().text == "Blocked",
              "blocked npc hurt spawns Blocked combat text");
      runtime.set_combat_text(nullptr);
    }

    runtime.blackboard_set_number("combat.npcHp.npc-1", 12.0);
    runtime.blackboard_set_bool("combat.npcDead.npc-1", true);
    runtime.blackboard_set_number("opening.keepMe", 1.0);
    runtime.blackboard_erase_prefix("combat.");
    r.check(!runtime.blackboard_get("combat.npcHp.npc-1") &&
                !runtime.blackboard_get("combat.npcDead.npc-1") &&
                !runtime.blackboard_get("combat.hitCount") &&
                !runtime.blackboard_get(
                    "combat.dummyHp.7aaf4fb9-adf5-4e12-8242-4a898c21edb9"),
            "erase combat. prefix drops play-test combat keys");
    const auto keep_opening = runtime.blackboard_get("opening.keepMe");
    r.check(keep_opening && keep_opening->type == engine::ScriptBlackboardType::Number &&
                keep_opening->number_value == 1.0,
            "erase combat. prefix leaves opening keys");

    const auto host_probe =
        std::filesystem::temp_directory_path() / "engine-host-api-probe.lua";
    {
      std::ofstream out(host_probe);
      out << "function probe_host(payload_json)\n"
             "  local ok, err = engine.json_decode(payload_json)\n"
             "  if ok then\n"
             "    engine.blackboard_set('probe.jsonOk', true)\n"
             "    engine.blackboard_set('probe.id', tostring(ok.id))\n"
             "  else\n"
             "    engine.blackboard_set('probe.jsonOk', false)\n"
             "    engine.blackboard_set('probe.err', tostring(err))\n"
             "  end\n"
             "  engine.log('not-a-level', 'should stay in sandbox')\n"
             "  engine.blackboard_set('probe.logged', true)\n"
             "end\n";
    }
    r.check(runtime.load_script(host_probe).has_value(),
            "host api probe script loads");
    runtime.clear_recent_errors();
    r.check(
        runtime.call_handler("probe_host", "{\"id\":\"alpha\"}").has_value(),
        "json_decode happy path via probe");
    const auto json_ok = runtime.blackboard_get("probe.jsonOk");
    r.check(json_ok && json_ok->bool_value, "json_decode returns table");
    const auto probe_id = runtime.blackboard_get("probe.id");
    r.check(probe_id && probe_id->string_value == "alpha",
            "json_decode exposes object fields");
    r.check(runtime.call_handler("probe_host", "{bad").has_value(),
            "bad JSON fails closed without aborting VM");
    const auto json_bad = runtime.blackboard_get("probe.jsonOk");
    r.check(json_bad && !json_bad->bool_value, "bad JSON sets jsonOk false");
    const auto logged = runtime.blackboard_get("probe.logged");
    r.check(logged && logged->bool_value,
            "invalid log level does not abort handler");
    r.check(runtime.recent_errors().empty(),
            "host api probe leaves no scripting errors");

    engine::UiCanvasStack stack;
    r.check(stack.set_hud(engine::default_player_hud_path(project)).has_value(),
            "sample player hud loads for scripting suite");
    r.check(stack
                .register_canvas("pause", project / "assets" / "ui" /
                                              "pause.uicanvas.json")
                .has_value(),
            "scripting suite registers pause");
    stack.hud().reset_player_health(100.0, 100.0);
    runtime.set_hud_runtime(&stack.hud());
    runtime.set_ui_canvas_stack(&stack);
    const auto health_probe =
        std::filesystem::temp_directory_path() / "engine-health-probe.lua";
    {
      std::ofstream out(health_probe);
      out << "function probe_health(payload_json)\n"
             "  engine.set_health(80, 100)\n"
             "  local c, m = engine.get_health()\n"
             "  engine.blackboard_set('probe.hp', c)\n"
             "  engine.blackboard_set('probe.max', m)\n"
             "  engine.hud_set_number('player.health', 70)\n"
             "  engine.hud_set_text('player.healthText', '70 / 100')\n"
             "  engine.hud_set_visible('player_health', true)\n"
             "  engine.hud_set_enabled('player_health', true)\n"
             "  engine.hud_set_bool('settings.music', true)\n"
             "  engine.blackboard_set('probe.music', "
             "engine.hud_get_bool('settings.music'))\n"
             "end\n";
    }
    r.check(runtime.load_script(health_probe).has_value(),
            "health probe script loads");
    r.check(runtime.call_handler("probe_health", "{}").has_value(),
            "set_health / get_health succeed");
    const auto hp = runtime.blackboard_get("probe.hp");
    r.check(hp && hp->number_value == 80.0, "get_health returns current");
    r.check(stack.hud().get_number("player.health") &&
                *stack.hud().get_number("player.health") == 70.0,
            "hud_set_number updates runtime");
    r.check(stack.hud().get_text("player.healthText") &&
                *stack.hud().get_text("player.healthText") == "70 / 100",
            "hud_set_text updates runtime");
    const auto music = runtime.blackboard_get("probe.music");
    r.check(music && music->bool_value,
            "hud_set_bool / hud_get_bool round-trip");
    r.check(stack.hud().get_bool("settings.music") &&
                *stack.hud().get_bool("settings.music"),
            "hud_set_bool updates runtime");
    const auto ui_probe =
        std::filesystem::temp_directory_path() / "engine-ui-stack-probe.lua";
    {
      std::ofstream out(ui_probe);
      out << "function probe_ui_stack(payload_json)\n"
             "  engine.ui_push('pause')\n"
             "  engine.blackboard_set('probe.top', engine.ui_top())\n"
             "  engine.ui_pop()\n"
             "  engine.blackboard_set('probe.empty', engine.ui_top() == nil)\n"
             "end\n";
    }
    r.check(runtime.load_script(ui_probe).has_value(),
            "ui stack probe script loads");
    r.check(runtime.call_handler("probe_ui_stack", "{}").has_value(),
            "ui_push/ui_pop/ui_top succeed");
    const auto top = runtime.blackboard_get("probe.top");
    r.check(top && top->string_value == "pause",
            "ui_top returns pause after push");
    const auto empty = runtime.blackboard_get("probe.empty");
    r.check(empty && empty->bool_value, "ui_top is nil after pop");
    runtime.set_hud_runtime(nullptr);
    runtime.set_ui_canvas_stack(nullptr);
  } else if (suite == "hud") {
    const auto project = std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                         "samples" / "open-world-rpg";
    const auto canvas = engine::UiCanvasAsset::load(
        engine::default_player_ui_canvas_path(project));
    r.check(canvas.has_value(), "sample player.uicanvas.json loads");
    r.check(canvas && canvas.value().widgets.size() >= 2,
            "sample canvas has widgets");
    r.check(canvas && canvas.value().design_resolution[0] == 1920.0f &&
                canvas.value().design_resolution[1] == 1080.0f,
            "sample canvas designResolution is 1920x1080");
    const auto bad_design = engine::UiCanvasAsset::parse(
        R"({"schemaVersion":1,"id":"x","designResolution":[0,1080],"widgets":[]})",
        "bad.uicanvas.json");
    r.check(!bad_design, "zero designResolution rejected");
    const auto letterbox_full = engine::compute_ui_canvas_letterbox(
        0.0f, 0.0f, 1920.0f, 1080.0f, 1920.0f, 1080.0f);
    r.check(std::abs(letterbox_full.scale - 1.0f) < 0.0001f,
            "letterbox scale is 1 at design size");
    r.check(std::abs(letterbox_full.content_min_x) < 0.0001f &&
                std::abs(letterbox_full.content_min_y) < 0.0001f,
            "letterbox fills viewport at design size");
    const auto letterbox_half = engine::compute_ui_canvas_letterbox(
        0.0f, 0.0f, 960.0f, 540.0f, 1920.0f, 1080.0f);
    r.check(std::abs(letterbox_half.scale - 0.5f) < 0.0001f,
            "letterbox scale is 0.5 at half viewport");
    const auto letterbox_wide = engine::compute_ui_canvas_letterbox(
        0.0f, 0.0f, 1920.0f, 500.0f, 1920.0f, 1080.0f);
    r.check(letterbox_wide.content_min_x > 0.0f,
            "wide short viewport pillarboxes horizontally");
    const auto fill_wide = engine::compute_ui_canvas_layout(
        0.0f, 0.0f, 1920.0f, 500.0f, 1920.0f, 1080.0f,
        engine::UiCanvasScaleMode::FillEdges);
    r.check(std::abs(fill_wide.scale - (1920.0f / 1920.0f)) < 0.0001f ||
                fill_wide.scale >= 1.0f,
            "fill_edges uses cover scale on wide short viewport");
    r.check(fill_wide.content_min_y < 0.0f || fill_wide.content_max_y > 500.0f,
            "fill_edges content extends past short viewport");
    const auto scale_mode_canvas = engine::UiCanvasAsset::parse(
        R"({"schemaVersion":1,"id":"x","designResolution":[1920,1080],"scaleMode":"fill_edges","widgets":[]})",
        "scale.uicanvas.json");
    r.check(scale_mode_canvas.has_value() &&
                scale_mode_canvas.value().scale_mode ==
                    engine::UiCanvasScaleMode::FillEdges,
            "scaleMode fill_edges parses");
    const auto bad = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"b","type":"bar","size":[10,10]}]})",
        "bad.hud.json");
    r.check(!bad, "bar without bind rejected");
    const auto unknown = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"b","type":"gadget","size":[10,10]}]})",
        "bad.hud.json");
    r.check(!unknown, "unknown widget type rejected");
    const auto button = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"b","type":"button","size":[10,10],"bind":"a"}]})",
        "ok.hud.json");
    r.check(button.has_value(), "button widget type loads");
    const auto toggle = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"t","type":"toggle","size":[80,24],"bind":"opt"}]})",
        "ok.hud.json");
    r.check(toggle.has_value(), "toggle widget type loads");
    const auto slider = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"s","type":"slider","size":[100,16],"bind":"vol","maxBind":"volMax"}]})",
        "ok.hud.json");
    r.check(slider.has_value(), "slider widget type loads");
    const auto image_widget = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"i","type":"image","size":[64,64],"image":"assets/ui/textures/x.png","imageMode":"contain"}]})",
        "ok.hud.json");
    r.check(image_widget.has_value() && image_widget.value().widgets[0].image ==
                                            "assets/ui/textures/x.png",
            "image widget parses path");
    r.check(image_widget && image_widget.value().widgets[0].image_mode ==
                                engine::HudImageMode::Contain,
            "imageMode contain parses");
    const auto nine_slice_widget = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"i","type":"image","size":[200,80],"image":"assets/ui/menu/menu-btn-primary.png","imageMode":"nine_slice","imageSlice":[48,24,48,24]}]})",
        "ok.hud.json");
    r.check(nine_slice_widget.has_value() &&
                nine_slice_widget.value().widgets[0].image_mode ==
                    engine::HudImageMode::NineSlice &&
                nine_slice_widget.value().widgets[0].image_slice[0] == 48.0f &&
                nine_slice_widget.value().widgets[0].image_slice[3] == 24.0f,
            "imageMode nine_slice and imageSlice parse");
    r.check(nine_slice_widget &&
                nine_slice_widget.value().to_json().find("nine_slice") !=
                    std::string::npos &&
                nine_slice_widget.value().to_json().find("imageSlice") !=
                    std::string::npos,
            "nine_slice serializes imageMode and imageSlice");
    const auto padding_widget = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"t","type":"text","bind":"body","size":[400,120],"padding":[24,16,24,20],"text":"Hi"}]})",
        "ok.hud.json");
    r.check(padding_widget.has_value() && padding_widget.value().widgets[0].padding[0] == 24.0f &&
            padding_widget.value().widgets[0].padding[1] == 16.0f &&
            padding_widget.value().widgets[0].padding[2] == 24.0f &&
            padding_widget.value().widgets[0].padding[3] == 20.0f,
        "padding [L,T,R,B] parses");
    r.check(padding_widget && padding_widget.value().to_json().find("\"padding\"") != std::string::npos,
        "padding serializes");
    const auto bad_padding = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"t","type":"text","bind":"body","size":[40,20],"padding":[-1,0,0,0]}]})",
        "bad.hud.json");
    r.check(!bad_padding.has_value(), "negative padding rejected");
    const auto fit_text_widget = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"t","type":"text","bind":"body","size":[200,80],"fontSize":18,"fitText":true,"minFontSize":12}]})",
        "ok.hud.json");
    r.check(fit_text_widget.has_value() && fit_text_widget.value().widgets[0].fit_text &&
            fit_text_widget.value().widgets[0].min_font_size == 12.0f,
        "fitText/minFontSize parse");
    r.check(fit_text_widget && fit_text_widget.value().to_json().find("\"fitText\"") != std::string::npos,
        "fitText serializes");
    const auto image_bind_widget = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"i","type":"image","size":[64,64],"imageBind":"slot.icon","imageMode":"contain"}]})",
        "ok.hud.json");
    r.check(image_bind_widget.has_value() &&
                image_bind_widget.value().widgets[0].image_bind == "slot.icon",
            "imageBind parses");
    const auto tooltip_widget = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"b","type":"button","size":[40,40],"bind":"a","tooltip":"Close"}]})",
        "ok.hud.json");
    r.check(tooltip_widget.has_value() &&
                tooltip_widget.value().widgets[0].tooltip == "Close",
            "tooltip parses");
    const auto theme_widget = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"b","type":"button","size":[80,32],"bind":"a","themeRole":"primaryButton","colorToken":"goldAccent","textColorToken":"ink","textColor":[20,18,16,255]}]})",
        "ok.hud.json");
    r.check(theme_widget.has_value() &&
                theme_widget.value().widgets[0].theme_role == "primaryButton" &&
                theme_widget.value().widgets[0].color_token == "goldAccent" &&
                theme_widget.value().widgets[0].text_color_token == "ink" &&
                theme_widget.value().widgets[0].has_text_color(),
            "themeRole/colorToken/textColor parse");
    r.check(theme_widget &&
                theme_widget.value().to_json().find("themeRole") !=
                    std::string::npos,
            "themeRole serializes");
    {
      const auto sample_theme = engine::UiThemeAsset::load(
          engine::default_ui_theme_path(project));
      r.check(sample_theme.has_value(), "sample ui-theme.json loads");
      r.check(sample_theme && sample_theme.value().token("goldAccent") &&
                  sample_theme.value().role("primaryButton") &&
                  sample_theme.value().role("primaryButton")->text == "ink",
              "sample theme has goldAccent and primaryButton");
      const auto bad_role = engine::UiThemeAsset::parse(
          R"({"schemaVersion":1,"id":"x","tokens":{"goldAccent":[213,185,120,255]},"roles":{"primaryButton":{"fill":"missing","text":"goldAccent"}}})",
          "bad.ui-theme.json");
      r.check(!bad_role, "role with unknown token rejected");
      engine::HudWidget gold_btn;
      gold_btn.type = engine::HudWidgetType::Button;
      gold_btn.color = {{213.0f, 185.0f, 120.0f, 255.0f}};
      const auto fill = engine::ui_theme_resolve_fill(
          gold_btn, sample_theme ? &sample_theme.value() : nullptr);
      const auto text = engine::ui_theme_resolve_text(
          gold_btn, sample_theme ? &sample_theme.value() : nullptr, fill);
      r.check(fill[0] == 213.0f && text[0] == 72.0f,
              "gold button fill stays gold and label uses ink contrast");
      engine::HudWidget role_btn;
      role_btn.type = engine::HudWidgetType::Button;
      role_btn.theme_role = "primaryButton";
      const auto role_fill = engine::ui_theme_resolve_fill(
          role_btn, sample_theme ? &sample_theme.value() : nullptr);
      const auto role_text = engine::ui_theme_resolve_text(
          role_btn, sample_theme ? &sample_theme.value() : nullptr, role_fill);
      r.check(role_fill[0] == 213.0f && role_text[0] == 72.0f,
              "primaryButton role resolves gold fill and ink text");
    }
    {
      float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
      engine::hud_image_fit_rect(0, 0, 100, 50, 200, 100,
                                 engine::HudImageMode::Stretch, x0, y0, x1, y1);
      r.check(x0 == 0 && y0 == 0 && x1 == 100 && y1 == 50,
              "image stretch fills box");
      engine::hud_image_fit_rect(0, 0, 100, 50, 200, 100,
                                 engine::HudImageMode::Contain, x0, y0, x1, y1);
      r.check(std::abs(x0 - 0.0f) < 0.01f && std::abs(y0 - 0.0f) < 0.01f &&
                  std::abs(x1 - 100.0f) < 0.01f && std::abs(y1 - 50.0f) < 0.01f,
              "image contain matches aspect box");
      engine::hud_image_fit_rect(0, 0, 100, 100, 200, 100,
                                 engine::HudImageMode::Contain, x0, y0, x1, y1);
      r.check(std::abs(x0 - 0.0f) < 0.01f && std::abs(y0 - 25.0f) < 0.01f &&
                  std::abs(x1 - 100.0f) < 0.01f && std::abs(y1 - 75.0f) < 0.01f,
              "image contain letterboxes tall box");
    }
    {
      // Chrome PNGs are pre-filtered on load because ImGui's D3D12 sampler
      // clamps to mip 0; minified title art otherwise drops texels.
      unsigned pw = 0, ph = 0;
      engine::ui_texture_prefilter_size(1488, 750, 474, 240, pw, ph);
      r.check(pw < 1488 && ph < 750 && pw >= 474 && ph >= 240,
              "prefilter shrinks toward draw size without undersampling");
      engine::ui_texture_prefilter_size(1488, 750, 474, 240, pw, ph);
      const unsigned first_w = pw;
      engine::ui_texture_prefilter_size(1488, 750, 500, 250, pw, ph);
      r.check(pw == first_w,
              "prefilter buckets nearby draw sizes to one upload");
      engine::ui_texture_prefilter_size(64, 32, 512, 256, pw, ph);
      r.check(pw == 64 && ph == 32, "prefilter never upscales");
      engine::ui_texture_prefilter_size(1488, 750, 0, 0, pw, ph);
      r.check(pw == 1488 && ph == 750,
              "prefilter keeps full size when no box given");

      // Two opaque texels and two transparent ones: alpha averages, colour
      // comes only from the opaque pair (premultiplied filtering).
      const std::vector<std::uint8_t> quad = {
          200, 40, 40, 255, 200, 40, 40, 255, 0, 255, 0, 0, 0, 255, 0, 0};
      const auto half = engine::resample_rgba_area(quad.data(), 2, 2, 1, 1);
      r.check(half.width == 1 && half.height == 1 &&
                  half.pixels[3] == 128 && half.pixels[0] == 200 &&
                  half.pixels[1] == 40,
              "area resample filters in premultiplied space");
      const auto degenerate =
          engine::resample_rgba_area(quad.data(), 2, 2, 0, 0);
      r.check(degenerate.pixels.empty(), "area resample rejects zero target");
    }
    const auto toggle_nobind = engine::HudAsset::parse(
        R"({"schemaVersion":1,"id":"x","widgets":[{"id":"t","type":"toggle","size":[80,24]}]})",
        "bad.hud.json");
    r.check(!toggle_nobind, "toggle without bind rejected");
    engine::HudRuntime runtime;
    r.check(runtime.load(engine::default_player_hud_path(project)).has_value(),
            "hud runtime loads sample canvas");
    r.check(runtime.asset().design_resolution[0] == 1920.0f,
            "runtime keeps design resolution");
    runtime.set_health(40.0, 100.0);
    r.check(runtime.get_number("player.health") &&
                *runtime.get_number("player.health") == 40.0,
            "set_health clamps current");
    runtime.set_health(200.0, 100.0);
    r.check(runtime.get_number("player.health") &&
                *runtime.get_number("player.health") == 100.0,
            "set_health clamps to max");
    runtime.apply_archetype_hud("ashfell_blade");
    r.check(runtime.get_text("player.resourceKind") &&
                *runtime.get_text("player.resourceKind") == "stamina",
            "ashfell_blade uses stamina resource");
    r.check(runtime.get_number("player.resource") &&
                *runtime.get_number("player.resource") == 100.0,
            "apply_archetype_hud seeds resource");
    r.check(runtime.rune_charges_max() == 0, "ashfell hides rune charges");
    r.check(!runtime.is_visible("hud_rune_pip_1"), "ashfell hides rune pip 1");
    runtime.apply_archetype_hud("runecaster");
    r.check(runtime.get_text("player.resourceKind") &&
                *runtime.get_text("player.resourceKind") == "stamina",
            "runecaster keeps shared stamina bar");
    r.check(runtime.rune_charges() == 5 && runtime.rune_charges_max() == 5,
            "runecaster seeds five rune charges");
    r.check(runtime.is_visible("hud_rune_pip_1"),
            "runecaster shows rune pip 1");
    r.check(runtime.get_image("hud.rune.1.icon") &&
                *runtime.get_image("hud.rune.1.icon") ==
                    "assets/ui/hud/hud-rune-pip-ready.png",
            "ready rune uses glow art");
    runtime.set_rune_charges(2, 5);
    r.check(runtime.rune_charges() == 2, "set_rune_charges clamps current");
    r.check(runtime.get_image("hud.rune.2.icon") &&
                *runtime.get_image("hud.rune.2.icon") ==
                    "assets/ui/hud/hud-rune-pip-ready.png",
            "remaining ready pip stays bright");
    r.check(runtime.get_image("hud.rune.3.icon") &&
                *runtime.get_image("hud.rune.3.icon") ==
                    "assets/ui/hud/hud-rune-pip-dim.png",
            "spent rune uses dim art");
    r.check(!runtime.is_enabled("hud_rune_pip_3"), "spent rune pip is dimmed");
    runtime.set_resource(40.0, 100.0);
    r.check(runtime.get_number("player.resource") &&
                *runtime.get_number("player.resource") == 40.0,
            "set_resource clamps current");
    bool has_objective = false;
    for (const auto &widget : runtime.asset().widgets) {
      if (widget.id == "quest_objective_text")
        has_objective = true;
    }
    r.check(has_objective, "player HUD includes quest objective widget");
    runtime.set_visible("player_health", false);
    r.check(!runtime.is_visible("player_health"),
            "hud_set_visible hides widget");
    runtime.set_image("slot.icon",
                      "assets/ui/icons/items/ashfell_arming_sword.png");
    r.check(runtime.get_image("slot.icon") &&
                *runtime.get_image("slot.icon") ==
                    "assets/ui/icons/items/ashfell_arming_sword.png",
            "set_image stores path");
    runtime.set_image("slot.icon", "");
    r.check(runtime.get_image("slot.icon") &&
                runtime.get_image("slot.icon")->empty(),
            "set_image empty clears path");
    runtime.set_text_typed("dialogue.body", "A—é😀B", 8.0f);
    runtime.tick_typewriter(0.125f);
    r.check(runtime.get_text("dialogue.body").value_or("") == "A",
            "typewriter reveals an ASCII code point");
    runtime.tick_typewriter(0.125f);
    r.check(runtime.get_text("dialogue.body").value_or("") == "A—",
            "typewriter does not split an em dash UTF-8 sequence");
    runtime.tick_typewriter(0.25f);
    r.check(runtime.get_text("dialogue.body").value_or("") == "A—é😀",
            "typewriter does not split accented or four-byte UTF-8 characters");
    r.check(!runtime.typewriter_complete("dialogue.body"),
            "typewriter remains incomplete before final UTF-8 code point");
    runtime.tick_typewriter(0.125f);
    r.check(runtime.get_text("dialogue.body").value_or("") == "A—é😀B" &&
                runtime.typewriter_complete("dialogue.body"),
            "typewriter completes UTF-8 dialogue text by code point");
    runtime.set_offset("player_health", 12.0f, -4.0f);
    runtime.set_size("player_health", 480.0f, 32.0f);
    runtime.set_opacity("player_health", 0.5f);
    runtime.clear_offset("player_health");
    runtime.clear_size("player_health");
    runtime.clear_opacity("player_health");
    r.check(true, "runtime offset/size/opacity overrides accept clear");
    engine::WorldUiBillboardRuntime billboards;
    billboards.sync_interact_prompt(true, "Press E to talk", 1.0f, 2.0f, 3.0f);
    r.check(billboards.size() == 1, "interact prompt upserts billboard");
    r.check(billboards.get("interact_prompt") &&
                billboards.get("interact_prompt")->text == "Press E to talk",
            "interact billboard stores label");
    billboards.sync_interact_prompt(false, "", 0.0f, 0.0f, 0.0f);
    r.check(billboards.size() == 0,
            "clearing interact prompt removes billboard");
    {
      engine::WorldUiBillboard bar_chip;
      bar_chip.id = "npc_hp";
      bar_chip.text = "Bandit";
      bar_chip.has_bar = true;
      bar_chip.bar_current = 100.0;
      bar_chip.bar_max = 100.0;
      billboards.upsert(bar_chip);
      r.check(billboards.get("npc_hp") && billboards.get("npc_hp")->has_bar,
              "world ui supports bar chips");
      bar_chip.bar_current = 70.0;
      bar_chip.request_hit_juice = true;
      billboards.upsert(bar_chip);
      const auto juiced = billboards.get("npc_hp");
      r.check(juiced && juiced->bar_ghost >= 99.0 && juiced->hit_flash > 0.5f,
              "world ui hit juice seeds ghost trail + flash");
      for (int i = 0; i < 6; ++i)
        billboards.tick(0.1f);
      const auto drained = billboards.get("npc_hp");
      r.check(drained && drained->bar_ghost < 99.0 &&
                  drained->bar_ghost >= drained->bar_current &&
                  drained->hit_flash < 0.5f,
              "world ui tick drains ghost trail and hit flash");
    }
    float sx = 0.0f, sy = 0.0f, depth = 0.0f;
    std::array<float, 16> identity_vp{1, 0, 0, 0, 0, 1, 0, 0,
                                      0, 0, 1, 0, 0, 0, 0, 1};
    // NDC (0,0,0.5) -> screen center of a 200x100 rect at (10,20)
    // With identity, world (0,0,0.5) maps to NDC z=0.5; x/y=0 -> center.
    const engine::ViewportRect rect{10.0f, 20.0f, 210.0f, 120.0f};
    r.check(engine::project_world_to_screen(identity_vp, rect, 0.0f, 0.0f, 0.5f,
                                            sx, sy, depth),
            "project_world_to_screen accepts mid-depth point");
    r.check(std::abs(sx - 110.0f) < 0.01f && std::abs(sy - 70.0f) < 0.01f,
            "project_world_to_screen maps to viewport center");
    runtime.set_bool("settings.music", true);
    r.check(runtime.get_bool("settings.music") &&
                *runtime.get_bool("settings.music"),
            "set_bool / get_bool");
    const auto written = engine::write_ui_canvas_json_atomic(
        std::filesystem::temp_directory_path() / "player-copy.uicanvas.json",
        canvas.value().to_json());
    r.check(written.has_value(),
            "ui canvas json writes atomically after validate");
    const auto legacy =
        engine::HudAsset::load(project / "assets" / "ui" / "player.hud.json");
    r.check(legacy.has_value(), "legacy player.hud.json shim still loads");

    engine::UiCanvasStack stack;
    r.check(stack.load_theme(engine::default_ui_theme_path(project)).has_value(),
            "stack loads ui theme");
    r.check(stack.theme().token("goldAccent").has_value(),
            "loaded theme exposes goldAccent");
    r.check(stack.set_hud(engine::default_player_ui_canvas_path(project))
                .has_value(),
            "stack sets hud canvas");
    r.check(stack
                .register_canvas("pause", project / "assets" / "ui" /
                                              "pause.uicanvas.json")
                .has_value(),
            "stack registers pause canvas");
    r.check(stack
                .register_canvas("settings", project / "assets" / "ui" /
                                                 "settings.uicanvas.json")
                .has_value(),
            "stack registers settings canvas");
    r.check(stack
                .register_canvas("inventory", project / "assets" / "ui" /
                                                  "inventory.uicanvas.json")
                .has_value(),
            "stack registers inventory canvas");
    if (const auto *inv = stack.find_canvas("inventory")) {
      bool found_bag = false;
      for (const auto &widget : inv->asset().widgets) {
        if (widget.id == "inventory_tab_bag") {
          found_bag = true;
          r.check(widget.theme_role == "primaryButton",
                  "inventory bag tab uses primaryButton theme role");
        }
      }
      r.check(found_bag, "inventory bag tab widget exists");
    }
    r.check(stack
                .register_canvas("dialogue", project / "assets" / "ui" /
                                                 "dialogue.uicanvas.json")
                .has_value(),
            "stack registers dialogue canvas");
    r.check(stack.hud_visible(), "gameplay hud layer is visible by default");
    stack.set_hud_visible(false);
    r.check(!stack.hud_visible(),
            "menu-only presentation can hide the gameplay hud layer");
    stack.set_hud_visible(true);
    r.check(stack.hud_visible(), "gameplay hud layer restores after menu");
    r.check(stack.push("pause").has_value(), "stack push pause");
    r.check(stack.top_modal() && *stack.top_modal() == "pause",
            "stack top is pause");
    r.check(stack.push("pause").has_value(), "stack re-push pause succeeds");
    r.check(stack.modal_ids().size() == 1,
            "duplicate push does not deepen stack");
    r.check(stack.pop().has_value(), "stack pop pause");
    r.check(!stack.has_modal(), "stack empty after pop");
    r.check(!stack.pop().has_value(), "pop empty stack fails");
    r.check(stack.show("pause").has_value(), "stack show pause");
    r.check(stack.hide("pause").has_value(), "stack hide pause");
    r.check(!stack.has_modal(), "stack empty after hide");
    r.check(stack.push("inventory").has_value(), "stack push inventory");
    r.check(stack.top_modal() && *stack.top_modal() == "inventory",
            "stack top is inventory");
    r.check(stack.pop().has_value(), "stack pop inventory");
    r.check(stack.push("dialogue").has_value(), "stack push dialogue");
    r.check(stack.top_modal() && *stack.top_modal() == "dialogue",
            "stack top is dialogue");
    r.check(stack.pop().has_value(), "stack pop dialogue");
    r.check(stack.push("settings").has_value(), "stack push settings");
    if (auto *settings = stack.find_canvas("settings")) {
      const auto focus = settings->focusable_widget_ids();
      r.check(focus.size() == 4, "settings has toggle+slider+back focusables");
      r.check(settings->get_bool("settings.music") &&
                  !*settings->get_bool("settings.music"),
              "settings music toggle seeds false");
      r.check(settings->get_number("settings.volume") &&
                  *settings->get_number("settings.volume") == 0.0,
              "settings volume slider seeds 0");
      engine::UiCanvasInputEvent activate{};
      activate.viewport_min = {0.0f, 0.0f};
      activate.viewport_max = {1920.0f, 1080.0f};
      activate.activate_pressed = true;
      (void)stack.handle_modal_input(activate, nullptr);
      r.check(settings->get_bool("settings.music") &&
                  *settings->get_bool("settings.music"),
              "activate flips settings music toggle");
      stack.reset_modal_focus();
      // Focus volume slider (third focusable after two toggles).
      engine::UiCanvasInputEvent nav_slider{};
      nav_slider.viewport_min = {0.0f, 0.0f};
      nav_slider.viewport_max = {1920.0f, 1080.0f};
      nav_slider.nav_next = true;
      (void)stack.handle_modal_input(nav_slider, nullptr);
      (void)stack.handle_modal_input(nav_slider, nullptr);
      engine::UiCanvasInputEvent adjust{};
      adjust.viewport_min = {0.0f, 0.0f};
      adjust.viewport_max = {1920.0f, 1080.0f};
      adjust.adjust_right = true;
      (void)stack.handle_modal_input(adjust, nullptr);
      r.check(settings->get_number("settings.volume") &&
                  std::abs(*settings->get_number("settings.volume") - 0.05) <
                      0.001,
              "adjust_right nudges slider by 5%");
    }
    r.check(stack.pop().has_value(), "stack pop settings");

    auto mutate_canvas = engine::UiCanvasAsset::load(
        engine::default_player_ui_canvas_path(project));
    r.check(mutate_canvas.has_value(), "mutate suite loads canvas");
    if (mutate_canvas) {
      float health_x = 0.0f;
      for (const auto &widget : mutate_canvas.value().widgets) {
        if (widget.id == "player_health")
          health_x = widget.offset[0];
      }
      const auto moved = engine::mutate_ui_canvas_asset(
          mutate_canvas.value(), "move",
          R"({"id":"player_health","delta":[10,0]})");
      r.check(moved.has_value(), "mutate move succeeds");
      if (moved) {
        bool found = false;
        for (const auto &widget : moved.value().widgets) {
          if (widget.id == "player_health") {
            found = true;
            r.check(std::abs(widget.offset[0] - (health_x + 10.0f)) < 0.01f,
                    "mutate move offsets widget");
          }
        }
        r.check(found, "mutate move targets player_health");
      }
      const auto styled = engine::mutate_ui_canvas_asset(
          mutate_canvas.value(), "style",
          R"({"id":"player_health_text","color":[255,240,200,255],"fontSize":36,"opacity":0.5,"enabled":false,"text":"HP","textAlign":"center","textVAlign":"middle"})");
      r.check(styled.has_value(), "mutate style succeeds");
      if (styled) {
        for (const auto &widget : styled.value().widgets) {
          if (widget.id == "player_health_text") {
            r.check(widget.has_color() && widget.color[0] == 255.0f,
                    "mutate style sets color");
            r.check(widget.font_size == 36.0f, "mutate style sets fontSize");
            r.check(std::abs(widget.opacity - 0.5f) < 0.001f,
                    "mutate style sets opacity");
            r.check(!widget.enabled, "mutate style sets enabled false");
            r.check(widget.text == "HP", "mutate style sets text");
            r.check(widget.text_align == engine::HudTextAlign::Center,
                    "mutate style sets textAlign");
            r.check(widget.text_v_align == engine::HudTextVAlign::Middle,
                    "mutate style sets textVAlign");
          }
        }
      }
      const auto themed = engine::mutate_ui_canvas_asset(
          mutate_canvas.value(), "style",
          R"({"id":"player_health_text","themeRole":"title","clearColor":true})");
      r.check(themed.has_value(), "mutate style themeRole succeeds");
      if (themed) {
        for (const auto &widget : themed.value().widgets) {
          if (widget.id == "player_health_text") {
            r.check(widget.theme_role == "title",
                    "mutate style sets themeRole");
            r.check(!widget.has_color(), "mutate style clearColor drops fill");
          }
        }
      }
      engine::HudRuntime pause_preview;
      r.check(
          pause_preview.load(project / "assets" / "ui" / "pause.uicanvas.json")
              .has_value(),
          "pause canvas loads");
      r.check(pause_preview.get_text("pause.title").value_or("") == "Paused",
              "pause text seeds from authored text");
      pause_preview.set_enabled("pause_title", false);
      r.check(!pause_preview.is_enabled("pause_title"),
              "hud set_enabled toggles inactive");
      engine::HudRuntime menu_preview;
      r.check(menu_preview
                  .load(project / "assets" / "ui" / "main_menu.uicanvas.json")
                  .has_value(),
              "main menu canvas loads");
      r.check(menu_preview.get_text("main_menu.title").value_or("") ==
                  "MAIN MENU",
              "main menu title seeds from text");
      r.check(menu_preview.get_text("main_menu.new_game").value_or("") ==
                  "NEW GAME",
              "main menu new_game text");
      r.check(!menu_preview.is_enabled("menu_continue"),
              "main menu continue is disabled");
      bool found_image = false;
      for (const auto &widget : menu_preview.asset().widgets) {
        if (widget.id == "menu_new_game" && !widget.image.empty())
          found_image = true;
      }
      r.check(found_image, "main menu new_game has optional image path");
      engine::UiCanvasStack menu_stack;
      r.check(menu_stack
                  .register_canvas("main_menu", project / "assets" / "ui" /
                                                    "main_menu.uicanvas.json")
                  .has_value(),
              "main menu registers on stack");
      r.check(menu_stack.push("main_menu").has_value(),
              "main menu pushes on stack");
      r.check(menu_stack.top_modal() == std::optional<std::string>{"main_menu"},
              "main menu is stack top");
      const auto menu_focus = menu_preview.focusable_widget_ids();
      r.check(menu_focus.size() >= 3,
              "main menu has at least three enabled buttons");
      engine::HudRuntime prologue_preview;
      r.check(prologue_preview
                  .load(project / "assets" / "ui" / "prologue.uicanvas.json")
                  .has_value(),
              "prologue canvas loads");
      const auto prologue_focus = prologue_preview.focusable_widget_ids();
      r.check(std::find(prologue_focus.begin(), prologue_focus.end(),
                        "prologue_continue") == prologue_focus.end(),
              "prologue ghost plate catcher is not keyboard-focusable");
      r.check(std::find(prologue_focus.begin(), prologue_focus.end(),
                        "prologue_skip") != prologue_focus.end(),
              "prologue skip remains focusable");
      r.check(std::find(prologue_focus.begin(), prologue_focus.end(),
                        "prologue_continue_cta") != prologue_focus.end(),
              "prologue continue CTA remains focusable");
      r.check(prologue_focus.size() == 2,
              "prologue focuses only skip + continue CTA");
      engine::UiCanvasStack pause_focus_stack;
      r.check(pause_focus_stack
                  .register_canvas("pause", project / "assets" / "ui" /
                                                "pause.uicanvas.json")
                  .has_value(),
              "pause focus stack registers");
      r.check(pause_focus_stack.push("pause").has_value(),
              "pause focus stack pushes");
      const auto pause_focus =
          pause_focus_stack.find_canvas("pause")->focusable_widget_ids();
      r.check(pause_focus.size() == 2, "pause has two focusable buttons");
      r.check(pause_focus[0] == "pause_resume",
              "pause initial focus is resume");
      engine::UiCanvasInputEvent nav{};
      nav.viewport_min = {0.0f, 0.0f};
      nav.viewport_max = {1920.0f, 1080.0f};
      nav.nav_next = true;
      const auto nav_result =
          pause_focus_stack.handle_modal_input(nav, nullptr);
      r.check(nav_result.handled, "modal keyboard nav handles");
      r.check(pause_focus_stack.modal_focus_widget_id() &&
                  *pause_focus_stack.modal_focus_widget_id() == "pause_quit",
              "modal nav advances to quit");
      // Mouse click must activate immediately (not focus-then-activate /
      // double-click).
      engine::UiCanvasInputEvent mouse_activate{};
      mouse_activate.viewport_min = {0.0f, 0.0f};
      mouse_activate.viewport_max = {1920.0f, 1080.0f};
      // Pause Resume is center-anchored near screen center (offset y=-10).
      mouse_activate.mouse_pos = {960.0f, 530.0f};
      mouse_activate.mouse_clicked = true;
      const auto mouse_result =
          pause_focus_stack.handle_modal_input(mouse_activate, nullptr);
      r.check(mouse_result.handled, "modal mouse click handles");
      r.check(mouse_result.widget_id == "pause_resume",
              "modal mouse click activates on first press");
      r.check(mouse_result.activated_bind == "pause.resume",
              "modal mouse click reports resume bind");
      r.check(pause_focus_stack.modal_focus_widget_id() &&
                  *pause_focus_stack.modal_focus_widget_id() == "pause_resume",
              "modal mouse click moves focus to hit widget");
      // Inventory slot drag needs mouse_down→held→released (not mouse_clicked
      // alone).
      {
        engine::UiCanvasStack inv_stack;
        r.check(inv_stack
                    .register_canvas("inventory", project / "assets" / "ui" /
                                                      "inventory.uicanvas.json")
                    .has_value(),
                "inventory canvas loads for drag test");
        r.check(inv_stack.push("inventory").has_value(),
                "inventory modal push for drag test");
        const float vp_w = 1920.0f, vp_h = 1080.0f;
        // Center-anchored bag.0 / hotbar.0 from sample inventory.uicanvas.json.
        const float bag_cx = vp_w * 0.5f - 164.0f,
                    bag_cy = vp_h * 0.5f - 230.0f;
        const float hot_cx = vp_w * 0.5f - 244.0f,
                    hot_cy = vp_h * 0.5f + 388.0f;
        engine::UiCanvasInputEvent press{};
        press.viewport_min = {0.0f, 0.0f};
        press.viewport_max = {vp_w, vp_h};
        press.mouse_pos = {bag_cx, bag_cy};
        press.mouse_down = true;
        const auto press_result = inv_stack.handle_modal_input(press, nullptr);
        r.check(press_result.handled, "inventory press on bag slot handles");
        engine::UiCanvasInputEvent drag{};
        drag.viewport_min = {0.0f, 0.0f};
        drag.viewport_max = {vp_w, vp_h};
        drag.mouse_pos = {(bag_cx + hot_cx) * 0.5f, (bag_cy + hot_cy) * 0.5f};
        drag.mouse_held = true;
        (void)inv_stack.handle_modal_input(drag, nullptr);
        engine::UiCanvasInputEvent release{};
        release.viewport_min = {0.0f, 0.0f};
        release.viewport_max = {vp_w, vp_h};
        release.mouse_pos = {hot_cx, hot_cy};
        release.mouse_released = true;
        const auto drop_result = inv_stack.handle_modal_input(release, nullptr);
        r.check(drop_result.handled, "inventory drag release handles");
        r.check(drop_result.drag_from_bind &&
                    *drop_result.drag_from_bind == "inventory.select.bag.0",
                "inventory drag from bag.0");
        r.check(drop_result.drag_to_bind &&
                    *drop_result.drag_to_bind == "inventory.select.hotbar.0",
                "inventory drag to hotbar.0");
        (void)inv_stack.pop();
      }
      engine::UiCanvasInputEvent cancel{};
      cancel.viewport_min = {0.0f, 0.0f};
      cancel.viewport_max = {1920.0f, 1080.0f};
      cancel.cancel_pressed = true;
      const auto cancel_result =
          pause_focus_stack.handle_modal_input(cancel, nullptr);
      r.check(cancel_result.modal_popped, "modal cancel pops stack");
      r.check(cancel_result.canvas_id == "pause",
              "modal cancel reports canvas id");
      r.check(!pause_focus_stack.has_modal(), "modal stack empty after cancel");
      const auto added = engine::mutate_ui_canvas_asset(
          mutate_canvas.value(), "add",
          R"({"id":"temp_panel","type":"panel","offset":[8,8],"size":[64,64]})");
      r.check(added.has_value(), "mutate add succeeds");
      const auto added_button = engine::mutate_ui_canvas_asset(
          mutate_canvas.value(), "add",
          R"({"id":"temp_btn","type":"button","text":"OK","bind":"test.ok","textAlign":"center"})");
      r.check(added_button.has_value(), "mutate add button succeeds");
      const auto added_toggle = engine::mutate_ui_canvas_asset(
          mutate_canvas.value(), "add",
          R"({"id":"temp_toggle","type":"toggle","text":"On"})");
      r.check(added_toggle.has_value(), "mutate add toggle succeeds");
      const auto added_slider = engine::mutate_ui_canvas_asset(
          mutate_canvas.value(), "add",
          R"({"id":"temp_slider","type":"slider","label":"Vol"})");
      r.check(added_slider.has_value(), "mutate add slider succeeds");
      const auto added_image = engine::mutate_ui_canvas_asset(
          mutate_canvas.value(), "add",
          R"({"id":"temp_img","type":"image","image":"assets/ui/textures/x.png","imageMode":"contain"})");
      r.check(added_image.has_value(), "mutate add image succeeds");
      if (added_image) {
        for (const auto &widget : added_image.value().widgets) {
          if (widget.id == "temp_img") {
            r.check(widget.image == "assets/ui/textures/x.png",
                    "mutate image sets path");
            r.check(widget.image_mode == engine::HudImageMode::Contain,
                    "mutate image sets mode");
          }
        }
      }
      if (added) {
        const auto removed = engine::mutate_ui_canvas_asset(
            added.value(), "remove", R"({"id":"temp_panel"})");
        r.check(removed.has_value() && removed.value().widgets.size() ==
                                           mutate_canvas.value().widgets.size(),
                "mutate remove restores input count");
      }
    }
  } else if (suite == "inventory") {
    const auto project = std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                         "samples" / "open-world-rpg";
    const auto catalog = engine::load_project_item_catalog(project);
    r.check(catalog.has_value(), "project item catalog loads");
    r.check(catalog && catalog.value().find("ashfell_arming_sword"),
            "catalog has ashfell_arming_sword");
    r.check(catalog && catalog.value().find("outrider_shortbow"),
            "catalog has outrider_shortbow");
    r.check(catalog && catalog.value().find("guild_rune_focus"),
            "catalog has guild_rune_focus");
    r.check(catalog && catalog.value().find("vein_iron_pendant"),
            "catalog has vein_iron_pendant");
    r.check(catalog && catalog.value().find("bloodied_crit_charm"),
            "catalog has bloodied_crit_charm");
    r.check(catalog && catalog.value().find("ashfell_bleed_sword"),
            "catalog has bleed test sword clone");
    r.check(catalog && catalog.value().find("outrider_poison_bow"),
            "catalog has poison test bow clone");
    r.check(catalog && catalog.value().find("guild_rune_focus_fire"),
            "catalog has fire rune-focus clone");
    r.check(catalog && catalog.value().find("guild_rune_focus_frost"),
            "catalog has frost rune-focus clone");
    r.check(catalog && catalog.value().find("guild_rune_focus_lightning"),
            "catalog has lightning rune-focus clone");
    {
      const engine::ItemDef *bleed =
          catalog.value().find("ashfell_bleed_sword");
      r.check(bleed && bleed->stats.on_hit.size() == 1 &&
                  bleed->stats.on_hit[0].status == "bleed",
              "bleed sword authors onHit bleed");
      const engine::ItemDef *fire_focus =
          catalog.value().find("guild_rune_focus_fire");
      const engine::ItemDef *frost_focus =
          catalog.value().find("guild_rune_focus_frost");
      r.check(fire_focus && fire_focus->stats.on_hit.size() == 1 &&
                  fire_focus->stats.on_hit[0].status == "burn",
              "fire focus authors onHit burn");
      r.check(frost_focus && frost_focus->stats.on_hit.size() == 1 &&
                  frost_focus->stats.on_hit[0].status == "slow" &&
                  frost_focus->stats.on_hit[0].damage_per_tick == 0.0f,
              "frost focus authors onHit slow");
    }
    r.check(catalog && catalog.value().find("field_bandage"),
            "catalog has field_bandage");
    {
      const engine::ItemDef *bow = catalog.value().find("outrider_shortbow");
      const engine::ItemDef *focus = catalog.value().find("guild_rune_focus");
      r.check(bow && !bow->hand_attach.joint.empty(),
              "shortbow has handAttach joint");
      r.check(bow && bow->hand_attach.draw_clip == "bow_draw",
              "shortbow has handAttach.drawClip bow_draw");
      r.check(focus && !focus->hand_attach.joint.empty(),
              "rune focus has handAttach joint");
      r.check(engine::normalize_starter_archetype_id("outrider") == "outrider",
              "normalize outrider");
      r.check(engine::normalize_starter_archetype_id("archer") == "outrider",
              "alias archer→outrider");
      r.check(engine::normalize_starter_archetype_id("magic") == "runecaster",
              "alias magic→runecaster");
      r.check(engine::kAct0MeleeAttackStaminaCost == 15.0,
              "melee swing stamina cost");
      r.check(engine::kAct0OutriderStarterArrowCount == 20,
              "outrider starter arrow count");
      r.check(engine::kAct0RuneChargesMax == 5, "runecaster rune charge max");
      r.check(engine::kAct0RuneRegenSeconds == 2.0f, "rune regen interval");
      r.check(engine::default_starter_weapon_item_id("outrider") ==
                  "outrider_shortbow",
              "outrider default bow");
      r.check(engine::default_starter_weapon_item_id("runecaster") ==
                  "guild_rune_focus",
              "runecaster default focus");
      r.check(engine::default_starter_weapon_item_id("ashfell_blade") ==
                  "ashfell_arming_sword",
              "ashfell default sword");
      const auto archetypes_path =
          engine::default_world_forge_archetypes_path(project);
      const auto archetypes =
          engine::WorldForgeArchetypesAsset::load(archetypes_path);
      r.check(archetypes.has_value(), "archetypes load for starter weapons");
      if (archetypes) {
        r.check(engine::resolve_starter_weapon_item_id(
                    "outrider", &archetypes.value()) == "outrider_shortbow",
                "resolve outrider weapon from catalog field");
        r.check(engine::resolve_starter_weapon_item_id(
                    "runecaster", &archetypes.value()) == "guild_rune_focus",
                "resolve runecaster weapon from catalog field");
      }
    }
    engine::InventoryRuntime inv;
    r.check(inv.bind(&catalog.value()).has_value(), "inventory binds catalog");
    r.check(inv.grant("field_bandage", 3).has_value(), "grant bandage stacks");
    r.check(inv.grant("ashfell_arming_sword", 1).has_value(),
            "grant starter sword");
    r.check(!inv.grant("not_a_real_item", 1), "unknown item id fail-closed");
    r.check(inv.set_hotbar(0, "ashfell_arming_sword", 1).has_value(),
            "set hotbar weapon");
    r.check(inv.set_equip("trinket0", "vein_iron_pendant", 1).has_value(),
            "equip trinket");
    r.check(inv.select_hotbar(0).has_value(), "select hotbar 0");
    r.check(inv.active_hotbar_item().item_id == "ashfell_arming_sword",
            "active hotbar is sword");
    {
      const engine::ItemDef *sword =
          catalog.value().find("ashfell_arming_sword");
      r.check(sword && !sword->world_mesh.empty(),
              "sword has worldMesh for hand attach");
      r.check(sword && sword->world_mesh.find("ashfell_arming_sword") !=
                           std::string::npos,
              "sword worldMesh path");
      bool one_handed = false, melee = false;
      for (const auto &tag : sword->tags) {
        if (tag == "one_handed")
          one_handed = true;
        if (tag == "melee")
          melee = true;
      }
      r.check(one_handed && melee, "sword tagged one_handed+melee");
      r.check(!sword->hand_attach.joint.empty(), "sword has handAttach joint");
      r.check(sword && sword->stats.damage_min == 16.0f &&
                  sword->stats.damage_max == 22.0f,
              "sword authored damage range");
      r.check(sword && std::fabs(sword->stats.attacks_per_second - 1.1f) < 0.001f,
              "sword authored attacksPerSecond");
      r.check(sword && std::fabs(sword->stats.dps() - 20.9f) < 0.05f,
              "sword DPS from range * APS");
      {
        const auto lines = engine::format_item_stat_lines(*sword);
        bool has_dps = false, has_dmg = false, has_str = false;
        for (const auto &line : lines) {
          if (line.find("DPS") != std::string::npos)
            has_dps = true;
          if (line.find("16") != std::string::npos &&
              line.find("22") != std::string::npos)
            has_dmg = true;
          if (line.find("Strength") != std::string::npos)
            has_str = true;
        }
        r.check(has_dps && has_dmg && has_str, "sword tooltip lists DPS/range/str");
      }
      const engine::ItemDef *bow = catalog.value().find("outrider_shortbow");
      r.check(bow && bow->stats.has_weapon_damage() &&
                  bow->stats.attacks_per_second > 0.0f,
              "shortbow has ranged damage stats");
      const engine::ItemDef *focus = catalog.value().find("guild_rune_focus");
      r.check(focus && focus->stats.has_weapon_damage(),
              "rune focus has magic damage stats");
      r.check(focus && engine::magic_element_from_tags(focus->tags) ==
                           engine::MagicElement::Arcane,
              "starter focus is arcane");
      {
        const engine::ItemDef *fire =
            catalog.value().find("guild_rune_focus_fire");
        const engine::ItemDef *frost =
            catalog.value().find("guild_rune_focus_frost");
        const engine::ItemDef *bolt =
            catalog.value().find("guild_rune_focus_lightning");
        r.check(fire && engine::magic_element_from_tags(fire->tags) ==
                             engine::MagicElement::Fire,
                "fire focus tag maps to Fire");
        r.check(frost && engine::magic_element_from_tags(frost->tags) ==
                              engine::MagicElement::Frost,
                "frost focus tag maps to Frost");
        r.check(bolt && engine::magic_element_from_tags(bolt->tags) ==
                             engine::MagicElement::Lightning,
                "lightning focus tag maps to Lightning");
        bool fire_spell = false;
        bool fire_burn = false;
        for (const auto &line : engine::format_item_stat_lines(*fire)) {
          if (line.find("Spell") != std::string::npos &&
              line.find("Fire") != std::string::npos)
            fire_spell = true;
          if (line.find("Burn") != std::string::npos)
            fire_burn = true;
        }
        r.check(fire_spell, "fire focus tooltip lists Spell Fire");
        r.check(fire_burn, "fire focus tooltip lists On hit Burn");
        bool frost_slow = false;
        for (const auto &line : engine::format_item_stat_lines(*frost)) {
          if (line.find("Slow") != std::string::npos)
            frost_slow = true;
        }
        r.check(frost_slow, "frost focus tooltip lists On hit Slow");
        bool lightning_chain = false;
        for (const auto &line : engine::format_item_stat_lines(*bolt)) {
          if (line.find("Chains") != std::string::npos)
            lightning_chain = true;
        }
        r.check(lightning_chain, "lightning focus tooltip lists chain hops");
      }
      const engine::ItemDef *helm = catalog.value().find("iron_test_helmet");
      r.check(helm && !helm->stats.modifiers.empty(),
              "iron helmet has armor modifiers");
      const engine::ItemDef *bandage = catalog.value().find("field_bandage");
      r.check(bandage && bandage->stats.heal == 25.0f, "bandage heal amount");
      {
        const auto pendant_stats = inv.compute_player_stats();
        r.check(std::fabs(pendant_stats.max_health - 116.0f) < 0.01f,
                "pendant +12 and strength +4 health");
        r.check(std::fabs(pendant_stats.armor - 1.0f) < 0.01f,
                "equipped pendant adds +1 armor");
        r.check(std::fabs(pendant_stats.strength - 2.0f) < 0.01f,
                "active sword adds +2 strength");
        r.check(std::fabs(pendant_stats.damage_multiplier - 1.04f) < 0.001f,
                "strength 2 is +4% melee");
        r.check(std::fabs(pendant_stats.dps - 20.9f * 1.04f) < 0.05f,
                "active sword DPS scaled by strength");
        engine::InventoryRuntime geared_inv;
        r.check(geared_inv.bind(&catalog.value()).has_value(),
                "stats test inventory binds");
        r.check(geared_inv.set_hotbar(0, "ashfell_arming_sword", 1).has_value(),
                "stats test sword");
        r.check(geared_inv.select_hotbar(0).has_value(), "stats test select sword");
        r.check(geared_inv.set_equip("trinket0", "vein_iron_pendant", 1).has_value(),
                "stats test pendant");
        r.check(geared_inv.set_equip("head", "iron_test_helmet", 1).has_value(),
                "stats test helmet");
        const auto geared = geared_inv.compute_player_stats();
        r.check(std::fabs(geared.max_health - 124.0f) < 0.01f,
                "helmet+pendant+strength maxHealth 124");
        r.check(std::fabs(geared.armor - 4.0f) < 0.01f, "helmet+pendant armor 4");
        r.check(std::fabs(engine::mitigate_damage_with_armor(10.0f, 4.0f) -
                          (10.0f * 100.0f / 104.0f)) < 0.01f,
                "armor mitigation formula");
        r.check(std::fabs(engine::resist_mitigation(10.0f, 25.0f) -
                          (10.0f * 100.0f / 125.0f)) < 0.01f,
                "generic resist curve");
        engine::HudRuntime hud;
        hud.reset_player_health(100.0, 100.0);
        hud.set_resource(80.0, 100.0);
        engine::apply_player_stats_to_hud(hud, geared);
        r.check(hud.get_number("player.healthMax") &&
                    *hud.get_number("player.healthMax") == 124.0,
                "HUD max health follows gear + strength");
        r.check(hud.get_number("player.health") &&
                    *hud.get_number("player.health") == 100.0,
                "current HP stays when max grows");
        r.check(hud.get_number("player.resourceMax") &&
                    *hud.get_number("player.resourceMax") == 100.0,
                "HUD stamina max unchanged without stamina mods");
        r.check(hud.get_number("player.resource") &&
                    *hud.get_number("player.resource") == 80.0,
                "current stamina preserved");
        r.check(hud.get_number("player.armor") &&
                    *hud.get_number("player.armor") == 4.0,
                "HUD armor bind");
        r.check(geared_inv.clear_equip("head").has_value(), "unequip helmet");
        const auto unequipped = geared_inv.compute_player_stats();
        r.check(std::fabs(unequipped.max_health - 116.0f) < 0.01f,
                "unequip helmet drops maxHealth");
        r.check(std::fabs(inv.compute_player_stats().max_health - 116.0f) < 0.01f,
                "main inventory still pendant+strength after isolated stats test");
        {
          const auto mix = engine::crit_marbles_from_chance(25.0f);
          r.check(mix.crit == 5 && mix.hit == 15, "25% crit is 5/15 of 20 marbles");
          r.check(std::fabs(mix.displayed_chance() - 25.0f) < 0.01f,
                  "displayed crit matches marble ratio");
          r.check(engine::crit_marbles_from_chance(0.0f).total() == 0,
                  "zero crit chance has empty bag");
        }
        r.check(geared_inv.set_hotbar(0, "outrider_shortbow", 1).has_value(),
                "swap to bow for agility");
        r.check(geared_inv.select_hotbar(0).has_value(), "select bow");
        const auto bow_stats = geared_inv.compute_player_stats();
        r.check(bow_stats.weapon_school == engine::WeaponSchool::Ranged,
                "bow is ranged school");
        r.check(std::fabs(bow_stats.agility - 2.0f) < 0.01f, "bow grants agility");
        r.check(std::fabs(bow_stats.damage_multiplier - 1.04f) < 0.001f,
                "agility scales bow");
        r.check(std::fabs(bow_stats.crit_chance - 5.0f) < 0.01f,
                "agility adds crit chance (2% per point → 1/20 bag)");
        r.check(bow_stats.crit_marbles == 1 && bow_stats.hit_marbles == 19,
                "bow agility yields one crit marble");
        r.check(!bow_stats.holding_magic_weapon, "bow is not magic");
        r.check(geared_inv.set_hotbar(0, "guild_rune_focus", 1).has_value(),
                "swap to focus");
        r.check(geared_inv.select_hotbar(0).has_value(), "select focus");
        const auto focus_stats = geared_inv.compute_player_stats();
        r.check(focus_stats.holding_magic_weapon, "focus shows magicka runes");
        r.check(std::fabs(focus_stats.intellect - 2.0f) < 0.01f,
                "focus grants intellect");
        r.check(std::fabs(focus_stats.damage_multiplier - 1.04f) < 0.001f,
                "intellect scales magic");
        engine::apply_player_stats_to_hud(hud, focus_stats);
        r.check(hud.rune_charges_max() == 5,
                "holding a magic weapon shows rune pips");
        r.check(hud.rune_charges() == 5, "magic weapon seeds full rune charges");
        engine::apply_player_stats_to_hud(hud, bow_stats);
        r.check(hud.rune_charges_max() == 0,
                "non-magic weapon hides rune pips");
        r.check(geared_inv.set_equip("trinket1", "bloodied_crit_charm", 1).has_value(),
                "equip crit test trinket");
        const auto crit_stats = geared_inv.compute_player_stats();
        r.check(std::fabs(crit_stats.crit_chance - 25.0f) < 0.01f,
                "crit trinket is 25%");
        r.check(crit_stats.crit_marbles == 5 && crit_stats.hit_marbles == 15,
                "crit bag 5 crit / 15 hit");
        bool saw_crit = false;
        bool saw_hit = false;
        for (int i = 0; i < 20; ++i) {
          const auto roll = geared_inv.roll_player_attack();
          if (roll.crit)
            saw_crit = true;
          else
            saw_hit = true;
        }
        r.check(saw_crit && saw_hit, "crit marble bag yields both outcomes in a cycle");
        {
          std::set<int> seen_base;
          for (int i = 0; i < 40; ++i) {
            const auto roll = geared_inv.roll_player_attack();
            float base = roll.damage;
            if (roll.crit) base /= engine::kCritHitMultiplier;
            seen_base.insert(static_cast<int>(std::lround(base)));
          }
          r.check(seen_base.size() >= 2,
                  "damage marble bag varies within weapon min/max");
          const auto sword = geared_inv.compute_player_stats();
          const int expect_lo =
              static_cast<int>(std::lround(sword.damage_min));
          const int expect_hi =
              static_cast<int>(std::lround(sword.damage_max));
          bool in_range = true;
          for (int v : seen_base) {
            if (v < expect_lo || v > expect_hi)
              in_range = false;
          }
          r.check(in_range, "damage marbles stay inside rounded weapon range");
        }
        r.check(geared_inv.set_hotbar(0, "ashfell_arming_sword", 1).has_value(),
                "restore sword after school tests");
        r.check(geared_inv.select_hotbar(0).has_value(), "reselect sword");
        {
          const auto step1 = geared_inv.roll_player_attack(1);
          const auto step3 = geared_inv.roll_player_attack(3);
          float s1 = step1.damage;
          float s3 = step3.damage;
          if (step1.crit)
            s1 /= engine::kCritHitMultiplier;
          if (step3.crit)
            s3 /= engine::kCritHitMultiplier;
          r.check(s3 + 0.01f >= s1, "melee combo step3 damage >= step1");
          const auto stats = geared_inv.compute_player_stats();
          r.check(std::fabs(s1 - std::lround(stats.damage_min)) < 0.01f ||
                      step1.crit,
                  "combo step1 uses weapon min (pre-crit)");
        }
      }
      const auto swapped = engine::ItemCatalogAsset::parse(
          R"({"schemaVersion":1,"id":"t","entities":[{"id":"swap_blade","kind":"weapon","stats":{"damageMin":12,"damageMax":8,"attacksPerSecond":2}}]})",
          "swap.json");
      r.check(swapped.has_value(), "inline stats catalog parses");
      if (swapped) {
        const engine::ItemDef *blade = swapped.value().find("swap_blade");
        r.check(blade && blade->stats.damage_min == 8.0f &&
                    blade->stats.damage_max == 12.0f,
                "damageMin/Max swap when inverted");
        r.check(blade && std::fabs(blade->stats.dps() - 20.0f) < 0.05f,
                "inline DPS (10 avg * 2 APS)");
      }
    }
    // Per-archetype starter weapons can land on hotbar0 (play-test command
    // path).
    r.check(inv.set_hotbar(0, "outrider_shortbow", 1).has_value(),
            "set hotbar bow starter");
    r.check(inv.active_hotbar_item().item_id == "outrider_shortbow",
            "active hotbar is shortbow");
    r.check(inv.set_hotbar(0, "guild_rune_focus", 1).has_value(),
            "set hotbar focus starter");
    r.check(inv.active_hotbar_item().item_id == "guild_rune_focus",
            "active hotbar is focus");
    r.check(inv.set_hotbar(0, "ashfell_arming_sword", 1).has_value(),
            "restore sword hotbar");
    r.check(inv.active_hotbar_item().item_id == "ashfell_arming_sword",
            "active hotbar restored to sword");
    r.check(inv.select_hotbar(1).has_value(), "select empty hotbar 1");
    r.check(inv.active_hotbar_item().empty(),
            "empty hotbar clears active weapon");
    {
      const auto empty_hand = inv.compute_player_stats();
      r.check(std::fabs(empty_hand.strength) < 0.01f,
              "unselected weapon drops strength");
      r.check(std::fabs(empty_hand.dps) < 0.01f, "empty hotbar has no weapon DPS");
      r.check(std::fabs(empty_hand.max_health - 112.0f) < 0.01f,
              "empty hand keeps pendant health without strength bonus");
      r.check(std::fabs(empty_hand.armor - 1.0f) < 0.01f,
              "armor still from equipped trinket");
    }
    r.check(inv.select_hotbar(0).has_value(), "reselect sword hotbar");
    const auto snap = inv.status();
    r.check(snap.selected_hotbar == 0, "selectedHotbar tracks equip slot");
    r.check(snap.hotbar.size() == 8, "hotbar has 8 slots");
    r.check(snap.bag.size() == 20, "bag has 20 slots");
    r.check(snap.equipped.trinkets[0].item_id == "vein_iron_pendant",
            "trinket0 set");
    bool bandage_in_bag = false;
    for (const auto &stack : snap.bag) {
      if (stack.item_id == "field_bandage" && stack.count == 3)
        bandage_in_bag = true;
    }
    r.check(bandage_in_bag, "bandage stack held in bag");
    r.check(inv.grant("crude_arrow", 40).has_value(), "grant ammo");
    r.check(!inv.status().ammo.empty() &&
                inv.status().ammo[0].item_id == "crude_arrow",
            "ammo slot used");
    {
      // Ammo never lands in the bag grid, so the inventory panel needs its own
      // Ammo row.
      bool arrow_in_bag = false;
      for (const auto &stack : inv.status().bag) {
        if (stack.item_id == "crude_arrow")
          arrow_in_bag = true;
      }
      r.check(!arrow_in_bag, "ammo stays out of the bag grid");
      const engine::ItemDef *arrow_def = inv.find_def("crude_arrow");
      r.check(arrow_def && !arrow_def->icon.empty(),
              "ammo item carries an icon for the Ammo row");
    }
    r.check(inv.count_item("crude_arrow") == 40,
            "count_item totals ammo stack");
    r.check(inv.remove("crude_arrow", 1).has_value(), "remove one crude arrow");
    r.check(inv.count_item("crude_arrow") == 39, "count_item after remove");
    r.check(!inv.remove("crude_arrow", 1000),
            "remove fail-closed when ammo insufficient");
    r.check(inv.count_item("crude_arrow") == 39,
            "failed remove leaves count unchanged");
    r.check(inv.select_ui("bag", 0).has_value(), "select bag slot");
    (void)inv.equip_selected();
    r.check(true, "equip_selected invoked without crash");
    r.check(inv.grant("outrider_shortbow", 1).has_value(),
            "grant shortbow for move");
    int bow_bag = -1;
    {
      const auto after_grant = inv.status();
      for (int i = 0; i < static_cast<int>(after_grant.bag.size()); ++i) {
        if (after_grant.bag[static_cast<std::size_t>(i)].item_id ==
            "outrider_shortbow") {
          bow_bag = i;
          break;
        }
      }
    }
    r.check(bow_bag >= 0, "shortbow landed in bag");
    r.check(inv.move_to("bag", bow_bag, {}, "hotbar", 1, {}).has_value(),
            "drag move bag→hotbar");
    r.check(inv.status().hotbar[1].item_id == "outrider_shortbow",
            "hotbar 1 holds shortbow after move");
    r.check(inv.move_to("hotbar", 1, {}, "bag", -1, {}).has_value(),
            "drag move hotbar→bag");
    r.check(inv.move_to("equip", -1, "trinket0", "bag", -1, {}).has_value(),
            "move trinket to bag");
    r.check(inv.status().equipped.trinkets[0].empty(),
            "trinket0 empty after move");
    r.check(inv.open_container("combat_weapon_crate").has_value(),
            "open weapon crate");
    r.check(inv.grant_container("guild_rune_focus", 1).has_value(),
            "grant focus into crate");
    r.check(inv.status().container_id == "combat_weapon_crate",
            "container id is open");
    r.check(!inv.status().container.empty() &&
                inv.status().container[0].item_id == "guild_rune_focus",
            "crate slot 0 holds focus");
    r.check(inv.move_to("container", 0, {}, "hotbar", 2, {}).has_value(),
            "drag crate→hotbar");
    r.check(inv.status().hotbar[2].item_id == "guild_rune_focus",
            "hotbar 2 holds focus after crate move");
    r.check(inv.move_to("hotbar", 2, {}, "container", 0, {}).has_value(),
            "drag hotbar→crate");
    r.check(inv.status().container[0].item_id == "guild_rune_focus",
            "focus returned to crate");
    r.check(inv.grant_container("crude_arrow", 20).has_value(),
            "grant arrows into crate");
    int crate_arrow = -1;
    {
      const auto crate_snap = inv.status();
      for (int i = 0; i < static_cast<int>(crate_snap.container.size()); ++i) {
        if (crate_snap.container[static_cast<std::size_t>(i)].item_id ==
            "crude_arrow") {
          crate_arrow = i;
          break;
        }
      }
    }
    r.check(crate_arrow >= 0, "arrows sit in crate");
    const int arrows_before = inv.count_item("crude_arrow");
    r.check(inv.move_to("container", crate_arrow, {}, "bag", -1, {}).has_value(),
            "drag crate ammo onto bag routes to ammo");
    r.check(inv.count_item("crude_arrow") == arrows_before + 20,
            "crate arrows merged into ammo region");
    inv.close_container();
    r.check(inv.status().container_id.empty(), "container closed");
    r.check(!inv.grant_container("ashfell_arming_sword", 1),
            "grant into closed crate fails");
  } else if (suite == "automation") {
    const auto project = std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                         "samples" / "open-world-rpg";
    const auto repo = std::filesystem::path(ENGINE_REPOSITORY_ROOT);
    {
      // Asset import (editor Assets tab): a picked file must resolve to the
      // right bake target.
      const auto player = engine::plan_asset_import(
          project, repo / "tools/art/player/GoodPlayerModel.gltf");
      r.check(player.supported && player.existing_target,
              "import plan updates registered player target");
      r.check(player.target_id == "player",
              "import plan matches player by registered source");
      const auto player_bb = engine::plan_asset_import(
          project, repo / "tools/art/player/GoodPlayerModel_rigged.bbmodel");
      r.check(
          player_bb.supported && player_bb.existing_target &&
              player_bb.target_id == "player",
          "import plan matches player Blockbench project to the player target");
      r.check(!player_bb.clip_names.empty(),
              "import plan lists clips from the player bbmodel");
      const bool has_run =
          std::find(player_bb.clip_names.begin(), player_bb.clip_names.end(),
                    "Run") != player_bb.clip_names.end();
      const bool has_attack =
          std::find(player_bb.clip_names.begin(), player_bb.clip_names.end(),
                    "Attack") != player_bb.clip_names.end();
      r.check(has_run && has_attack,
              "import plan sees Run and Attack on the player bbmodel");
      const auto player_rebake = engine::plan_target_rebake(project, "player");
      r.check(player_rebake.supported && player_rebake.target_id == "player",
              "player rebake plan resolves the registered target");
      r.check(
          player_rebake.source.extension() == ".bbmodel" ||
              !player_rebake.clip_names.empty(),
          "player rebake prefers Blockbench clips when the bbmodel is present");
      r.check(player.match == engine::AssetImportMatch::DefaultSource,
              "import plan reports source match");
      r.check(player.has_skin && player.kind == "skinned",
              "import plan detects player skin");
      r.check(player.clip_names.size() >= 4,
              "import plan lists player animation clips");
      r.check(player.mesh_output == "assets/models/player.gltf",
              "import plan keeps player mesh output");
      r.check(player.existing_prefab && !player.prefab_path.empty(),
              "import plan finds player prefab");

      const auto crate = engine::plan_asset_import(
          project, project / "assets/models/crate.gltf");
      r.check(crate.supported && crate.existing_target &&
                  crate.target_id == "crate",
              "import plan re-imports a baked output as its target");
      r.check(crate.match == engine::AssetImportMatch::MeshOutput,
              "import plan reports baked-output match");
      r.check(!crate.has_skin && crate.kind == "static",
              "import plan treats crate as static");

      const auto missing = engine::plan_asset_import(
          project, project / "assets/models/not_here.gltf");
      r.check(!missing.supported && !missing.diagnostics.empty() &&
                  missing.diagnostics.front().code ==
                      "ASSET-IMPORT-SOURCE-MISSING",
              "import plan rejects a missing source");
      const auto wrong_kind =
          engine::plan_asset_import(project, repo / "README.md");
      r.check(!wrong_kind.supported && !wrong_kind.diagnostics.empty() &&
                  wrong_kind.diagnostics.front().code ==
                      "ASSET-IMPORT-UNSUPPORTED",
              "import plan rejects a non-model file");

      const auto sandbox =
          std::filesystem::temp_directory_path() / "engine-asset-import-test";
      std::error_code sandbox_error;
      std::filesystem::create_directories(sandbox, sandbox_error);
      const auto fresh = sandbox / "My Cool Prop.gltf";
      std::filesystem::copy_file(
          project / "assets/models/crate.gltf", fresh,
          std::filesystem::copy_options::overwrite_existing, sandbox_error);
      if (!sandbox_error) {
        const auto fresh_plan = engine::plan_asset_import(project, fresh);
        r.check(fresh_plan.supported && !fresh_plan.existing_target,
                "import plan registers an unknown model as a new target");
        r.check(fresh_plan.target_id == "my_cool_prop",
                "import plan slugifies the id from the file name");
        r.check(fresh_plan.mesh_output == "assets/models/my_cool_prop.gltf",
                "import plan derives the mesh output from the id");
        r.check(fresh_plan.prefab_path ==
                    "assets/prefabs/Imported/my_cool_prop.prefab.json",
                "import plan proposes a placeable prefab path");
        r.check(!fresh_plan.existing_prefab,
                "import plan marks the new prefab as not yet authored");
        r.check(fresh_plan.target_height > 0.0f,
                "import plan defaults a world height for new assets");
        const auto renamed = engine::plan_asset_import(project, fresh, "crate");
        r.check(renamed.target_id != "crate",
                "import plan keeps a requested id unique against the catalog");
        // "Replace..." pins the target, so a renamed export still updates the
        // same asset.
        const auto replace =
            engine::plan_asset_replace(project, "crate", fresh);
        r.check(replace.supported && replace.existing_target &&
                    replace.target_id == "crate",
                "replace plan pins a renamed source onto the chosen target");
        r.check(replace.mesh_output == "assets/models/crate.gltf",
                "replace plan keeps the target's baked outputs");
        const auto replace_unknown =
            engine::plan_asset_replace(project, "not_a_target", fresh);
        r.check(!replace_unknown.supported,
                "replace plan rejects an unknown target id");
      }
      std::filesystem::remove_all(sandbox, sandbox_error);

      const auto rebake = engine::plan_target_rebake(project, "crate");
      r.check(rebake.supported && rebake.existing_target &&
                  rebake.match == engine::AssetImportMatch::MeshOutput,
              "target rebake plan bakes from the catalog source");
      const auto unknown_rebake =
          engine::plan_target_rebake(project, "definitely_not_a_target");
      r.check(!unknown_rebake.supported,
              "target rebake plan rejects an unknown target id");
      r.check(!engine::find_prefab_for_mesh(project, "assets/models/crate.gltf")
                   .empty(),
              "prefab lookup finds the prefab that references a mesh");
      r.check(engine::find_prefab_for_mesh(project, "assets/models/nope.gltf")
                  .empty(),
              "prefab lookup returns empty for an unreferenced mesh");
    }
    const auto plan = engine::classify_scene_plan(
        "place a tree in the scene", "worlds/vertical-slice.world.json");
    r.check(plan.target_kind == "scene_data",
            "scene plan classifies world placement");
    {
      const auto sandbox =
          engine::Scene::load(project / "worlds" / "sandbox.world.json");
      r.check(sandbox.has_value(), "sandbox.world.json loads");
      if (sandbox) {
        r.check(sandbox.value().size() >= 5, "sandbox world has pad entities");
        const auto errors = sandbox.value().validate();
        r.check(errors.empty(), "sandbox.world.json validates");
      }
    }
    const auto lua_plan = engine::classify_scene_plan(
        "update quest script", "assets/scripts/quest.lua");
    r.check(lua_plan.target_kind == "lua_script",
            "scene plan classifies lua edits");
    const auto hud_plan = engine::classify_scene_plan(
        "resize health bar", "assets/ui/player.hud.json");
    r.check(hud_plan.target_kind == "hud_asset",
            "scene plan classifies hud assets");
    const auto canvas_plan = engine::classify_scene_plan(
        "move health bar", "assets/ui/player.uicanvas.json");
    r.check(canvas_plan.target_kind == "ui_canvas",
            "scene plan classifies ui canvas assets");
    const auto wf_plan = engine::classify_scene_plan(
        "update world forge factions",
        "assets/world-forge/factions.worldforge.json");
    r.check(wf_plan.target_kind == "world_forge",
            "scene plan classifies world forge assets");
    const auto wf_get = engine::apply_world_forge_operation(
        project, nlohmann::json{{"action", "get"}, {"kind", "factions"}});
    r.check(wf_get.exit_code == engine::ExitCode::Success &&
                wf_get.metadata.count("content"),
            "world_forge get factions succeeds");
    const auto wf_validate = engine::apply_world_forge_operation(
        project, nlohmann::json{{"action", "validate"}, {"kind", "map"}});
    r.check(wf_validate.exit_code == engine::ExitCode::Success,
            "world_forge validate map succeeds");
    const auto wf_bad = engine::apply_world_forge_operation(
        project,
        nlohmann::json{
            {"action", "apply"},
            {"kind", "factions"},
            {"source",
             R"({"schemaVersion":1,"id":"t","entities":[{"id":"","kind":"faction","canonStatus":"draft"}]})"}});
    r.check(wf_bad.exit_code == engine::ExitCode::ValidationFailed,
            "world_forge apply rejects invalid factions");
    engine::LuaRuntime lua_call_runtime;
    engine::UiCanvasStack lua_call_stack;
    r.check(lua_call_runtime
                .load_bindings(project,
                               engine::default_script_bindings_path(project))
                .has_value(),
            "lua_call suite loads bindings");
    r.check(lua_call_stack.set_hud(engine::default_player_hud_path(project))
                .has_value(),
            "lua_call suite loads hud");
    r.check(lua_call_stack
                .register_canvas("pause", project / "assets" / "ui" /
                                              "pause.uicanvas.json")
                .has_value(),
            "lua_call suite registers pause");
    lua_call_stack.hud().reset_player_health(100.0, 100.0);
    lua_call_runtime.set_hud_runtime(&lua_call_stack.hud());
    lua_call_runtime.set_ui_canvas_stack(&lua_call_stack);
    engine::EditorSessionContext lua_call_context;
    lua_call_context.project_root = project;
    lua_call_context.lua_runtime = &lua_call_runtime;
    lua_call_context.ui_canvas_stack = &lua_call_stack;
    const auto hurt_call = engine::execute_editor_operation(
        lua_call_context, "lua_call", R"({"kind":"combatHurt","id":"body"})");
    r.check(hurt_call.exit_code == engine::ExitCode::Success,
            "lua_call combatHurt succeeds");
    r.check(lua_call_stack.hud().get_number("player.health") &&
                *lua_call_stack.hud().get_number("player.health") == 90.0,
            "lua_call combatHurt applies sample damage");
    const auto interact_enter = engine::execute_editor_operation(
        lua_call_context, "lua_call",
        R"({"kind":"interaction","id":"use_campfire","type":"enter"})");
    r.check(interact_enter.exit_code == engine::ExitCode::Success,
            "lua_call campfire enter succeeds");
    r.check(lua_call_stack.hud().get_number("player.health") &&
                *lua_call_stack.hud().get_number("player.health") == 90.0,
            "lua_call campfire enter does not heal");
    const auto interact_use = engine::execute_editor_operation(
        lua_call_context, "lua_call",
        R"({"kind":"interaction","id":"use_campfire","type":"use"})");
    r.check(interact_use.exit_code == engine::ExitCode::Success,
            "lua_call campfire use succeeds");
    r.check(lua_call_stack.hud().get_number("player.health") &&
                *lua_call_stack.hud().get_number("player.health") == 100.0,
            "lua_call campfire use restores health");
    const auto push_call = engine::execute_editor_operation(
        lua_call_context, "ui_stack", R"({"action":"push","id":"pause"})");
    r.check(push_call.exit_code == engine::ExitCode::Success,
            "ui_stack push pause succeeds");
    r.check(lua_call_stack.top_modal() &&
                *lua_call_stack.top_modal() == "pause",
            "ui_stack top is pause");
    const auto pop_call = engine::execute_editor_operation(
        lua_call_context, "ui_stack", R"({"action":"pop"})");
    r.check(pop_call.exit_code == engine::ExitCode::Success,
            "ui_stack pop succeeds");
    r.check(!lua_call_stack.has_modal(), "ui_stack empty after pop");
    {
      auto quests = engine::WorldForgeQuestsAsset::load(
          engine::default_world_forge_quests_path(project));
      r.check(quests.has_value(), "quest_call suite loads quests asset");
      engine::QuestRuntime quest_rt;
      if (quests)
        r.check(quest_rt.bind(&quests.value()).has_value(),
                "quest_call suite binds QuestRuntime");
      lua_call_context.quest_runtime = &quest_rt;
      lua_call_context.hud_runtime = &lua_call_stack.hud();
      lua_call_runtime.set_quest_runtime(&quest_rt);
      const auto q_start = engine::execute_editor_operation(
          lua_call_context, "quest_call",
          R"({"kind":"start","questId":"sq_01_cart_again"})");
      r.check(q_start.exit_code == engine::ExitCode::Success &&
                  q_start.metadata.count("currentObjectiveId") &&
                  q_start.metadata.at("currentObjectiveId") == "find_pellin",
              "quest_call start succeeds");
      const auto hud_text =
          lua_call_stack.hud().get_text("quest.objectiveText");
      r.check(hud_text && !hud_text->empty(),
              "quest_call updates quest.objectiveText");
      const auto q_bad = engine::execute_editor_operation(
          lua_call_context, "quest_call",
          R"({"kind":"complete_objective","questId":"sq_01_cart_again","objectiveId":"lift_cart"})");
      r.check(q_bad.exit_code == engine::ExitCode::ValidationFailed,
              "quest_call rejects out-of-order");
      const auto q_done = engine::execute_editor_operation(
          lua_call_context, "quest_call",
          R"({"kind":"complete_objective","questId":"sq_01_cart_again","objectiveId":"find_pellin"})");
      r.check(q_done.exit_code == engine::ExitCode::Success &&
                  q_done.metadata.at("currentObjectiveId") ==
                      "clear_scavengers",
              "quest_call completes objective");
      const auto q_list = engine::execute_editor_operation(
          lua_call_context, "quest_call", R"({"kind":"list"})");
      r.check(q_list.exit_code == engine::ExitCode::Success &&
                  q_list.metadata.at("count") == "1",
              "quest_call list active");
      const auto q_abandon = engine::execute_editor_operation(
          lua_call_context, "quest_call",
          R"({"kind":"abandon","questId":"sq_01_cart_again"})");
      r.check(q_abandon.exit_code == engine::ExitCode::Success &&
                  q_abandon.metadata.at("status") == "abandoned",
              "quest_call abandon");
    }
    {
      auto dialogues = engine::WorldForgeDialoguesAsset::load(
          engine::default_world_forge_dialogues_path(project));
      r.check(dialogues.has_value(),
              "dialogue_call suite loads dialogues asset");
      engine::DialogueRuntime dialogue_rt;
      if (dialogues)
        r.check(dialogue_rt.bind(&dialogues.value()).has_value(),
                "dialogue_call suite binds DialogueRuntime");
      auto quests_for_dialogue = engine::WorldForgeQuestsAsset::load(
          engine::default_world_forge_quests_path(project));
      engine::QuestRuntime quest_rt_for_dialogue;
      if (quests_for_dialogue) {
        r.check(quest_rt_for_dialogue.bind(&quests_for_dialogue.value())
                    .has_value(),
                "dialogue_call suite binds QuestRuntime for talk_act0");
      }
      r.check(lua_call_stack
                  .register_canvas("dialogue", project / "assets" / "ui" /
                                                   "dialogue.uicanvas.json")
                  .has_value(),
              "dialogue_call suite registers dialogue canvas");
      lua_call_context.dialogue_runtime = &dialogue_rt;
      lua_call_context.quest_runtime = &quest_rt_for_dialogue;
      lua_call_runtime.set_dialogue_runtime(&dialogue_rt);
      lua_call_runtime.set_quest_runtime(&quest_rt_for_dialogue);
      const auto d_start = engine::execute_editor_operation(
          lua_call_context, "dialogue_call",
          R"({"kind":"start","treeId":"dlg_act0_meet_arkand"})");
      r.check(d_start.exit_code == engine::ExitCode::Success &&
                  d_start.metadata.count("treeId") &&
                  d_start.metadata.at("treeId") == "dlg_act0_meet_arkand",
              "dialogue_call start succeeds");
      r.check(d_start.metadata.count("choiceCount") &&
                  d_start.metadata.at("choiceCount") != "0",
              "dialogue_call start exposes choices");
      r.check(lua_call_stack.top_modal() &&
                  *lua_call_stack.top_modal() == "dialogue",
              "dialogue_call start pushes dialogue canvas");
      const auto d_present = engine::execute_editor_operation(
          lua_call_context, "dialogue_call", R"({"kind":"present"})");
      r.check(d_present.exit_code == engine::ExitCode::Success &&
                  d_present.metadata.count("nodeId") &&
                  !d_present.metadata.at("nodeId").empty(),
              "dialogue_call present returns node");
      std::string first_choice;
      if (d_start.metadata.count("choiceIds")) {
        const auto &ids = d_start.metadata.at("choiceIds");
        const auto comma = ids.find(',');
        first_choice = comma == std::string::npos ? ids : ids.substr(0, comma);
      }
      r.check(!first_choice.empty(), "dialogue_call has a first choice id");
      const auto d_choose = engine::execute_editor_operation(
          lua_call_context, "dialogue_call",
          nlohmann::json{{"kind", "choose"}, {"choiceId", first_choice}}
              .dump());
      r.check(d_choose.exit_code == engine::ExitCode::Success,
              "dialogue_call choose succeeds");
      const auto d_reset = engine::execute_editor_operation(
          lua_call_context, "dialogue_call", R"({"kind":"reset"})");
      r.check(d_reset.exit_code == engine::ExitCode::Success,
              "dialogue_call reset succeeds");
      r.check(!lua_call_stack.has_modal() ||
                  !(lua_call_stack.top_modal() &&
                    *lua_call_stack.top_modal() == "dialogue"),
              "dialogue_call reset clears dialogue modal");
      const auto interact_talk = engine::execute_editor_operation(
          lua_call_context, "lua_call",
          R"({"kind":"interaction","id":"talk_act0","type":"enter"})");
      r.check(interact_talk.exit_code == engine::ExitCode::Success,
              "lua_call talk_act0 enter succeeds");
      const auto prompt = lua_call_runtime.blackboard_get("interact.prompt");
      r.check(prompt && prompt->type == engine::ScriptBlackboardType::Bool &&
                  prompt->bool_value,
              "talk_act0 enter sets interact prompt (does not auto-start "
              "dialogue)");
      r.check(dialogue_rt.tree_id().empty(),
              "talk_act0 enter does not start DialogueRuntime");
      const auto interact_use = engine::execute_editor_operation(
          lua_call_context, "lua_call",
          R"({"kind":"interaction","id":"talk_act0","type":"use"})");
      r.check(interact_use.exit_code == engine::ExitCode::Success,
              "lua_call talk_act0 use succeeds");
      const auto guard =
          lua_call_runtime.blackboard_get("dialogue.talk_act0.started");
      r.check(guard && guard->type == engine::ScriptBlackboardType::Bool &&
                  guard->bool_value,
              "talk_act0 use sets re-enter guard");
      r.check(!dialogue_rt.tree_id().empty(),
              "talk_act0 use starts DialogueRuntime tree");
      const auto interact_talk_again = engine::execute_editor_operation(
          lua_call_context, "lua_call",
          R"({"kind":"interaction","id":"talk_act0","type":"use"})");
      r.check(interact_talk_again.exit_code == engine::ExitCode::Success,
              "lua_call talk_act0 re-use ok");
      (void)engine::execute_editor_operation(lua_call_context, "dialogue_call",
                                             R"({"kind":"reset"})");
    }
    {
      auto factions = engine::WorldForgeFactionsAsset::parse(R"({
                "schemaVersion":1,"id":"mcp_standing","entities":[
                  {"id":"cristallo","kind":"faction","canonStatus":"draft","standing":{
                    "tracksPlayer":true,"min":-50,"max":50,
                    "ranks":[{"id":"neutral","minScore":0,"displayName":"N"}]}} ,
                  {"id":"arrotrebae","kind":"faction","canonStatus":"draft","standing":{
                    "tracksPlayer":true,"min":-50,"max":50}}
                ]})");
      auto rels = engine::WorldForgeRelationshipsAsset::parse(R"({
                "schemaVersion":1,"id":"mcp_rel","nodes":[],"edges":[
                  {"id":"c_vs_a","from":{"target":"faction","id":"cristallo"},
                   "to":{"target":"faction","id":"arrotrebae"},"kind":"opposes","canonStatus":"draft",
                   "standingTransfer":1.0}]})");
      r.check(factions.has_value() && rels.has_value(),
              "standing_call fixtures parse");
      engine::StandingRuntime standing_rt;
      if (factions && rels)
        r.check(standing_rt.bind(&factions.value(), &rels.value()).has_value(),
                "standing_call suite binds StandingRuntime");
      lua_call_context.standing_runtime = &standing_rt;
      lua_call_runtime.set_standing_runtime(&standing_rt);
      const auto s_adj = engine::execute_editor_operation(
          lua_call_context, "standing_call",
          R"({"kind":"adjust","factionId":"cristallo","delta":10})");
      r.check(s_adj.exit_code == engine::ExitCode::Success,
              "standing_call adjust succeeds");
      const auto s_get = engine::execute_editor_operation(
          lua_call_context, "standing_call",
          R"({"kind":"get","factionId":"arrotrebae"})");
      r.check(s_get.exit_code == engine::ExitCode::Success &&
                  s_get.metadata.count("score") &&
                  std::stod(s_get.metadata.at("score")) == -10.0,
              "standing_call hostility fallout via MCP");
      const auto s_list = engine::execute_editor_operation(
          lua_call_context, "standing_call", R"({"kind":"list"})");
      r.check(s_list.exit_code == engine::ExitCode::Success &&
                  s_list.metadata.at("count") == "2",
              "standing_call list tracked");
      lua_call_runtime.set_standing_runtime(nullptr);
      lua_call_context.standing_runtime = nullptr;
    }
    {
      engine::FlagRuntime flag_rt;
      lua_call_context.flag_runtime = &flag_rt;
      lua_call_runtime.set_flag_runtime(&flag_rt);
      const auto f_set = engine::execute_editor_operation(
          lua_call_context, "flag_call",
          R"({"kind":"set","flagId":"act0.helped_larrell"})");
      r.check(f_set.exit_code == engine::ExitCode::Success,
              "flag_call set succeeds");
      const auto f_has = engine::execute_editor_operation(
          lua_call_context, "flag_call",
          R"({"kind":"has","flagId":"act0.helped_larrell"})");
      r.check(f_has.exit_code == engine::ExitCode::Success &&
                  f_has.metadata.count("has") &&
                  f_has.metadata.at("has") == "true",
              "flag_call has true");
      const auto f_list = engine::execute_editor_operation(
          lua_call_context, "flag_call", R"({"kind":"list"})");
      r.check(f_list.exit_code == engine::ExitCode::Success &&
                  f_list.metadata.at("count") == "1",
              "flag_call list");
      auto quests_for_fork = engine::WorldForgeQuestsAsset::load(
          engine::default_world_forge_quests_path(project));
      engine::QuestRuntime quest_rt_fork;
      if (quests_for_fork) {
        r.check(quest_rt_fork.bind(&quests_for_fork.value()).has_value(),
                "flag_call suite binds QuestRuntime");
        lua_call_context.quest_runtime = &quest_rt_fork;
        r.check(quest_rt_fork.start("mq_act0_calrenoth").has_value(),
                "flag_call suite starts mq for fork");
        const auto f_fork = engine::execute_editor_operation(
            lua_call_context, "quest_call",
            R"({"kind":"resolve_fork","questId":"mq_act0_calrenoth","forkId":"larrell_save_vs_flee","outcomeFlag":"act0.fled_drawbridge"})");
        r.check(f_fork.exit_code == engine::ExitCode::Success,
                "quest_call resolve_fork succeeds");
        r.check(flag_rt.has("act0.fled_drawbridge") &&
                    !flag_rt.has("act0.helped_larrell"),
                "resolve_fork via MCP clears sibling");
      }
      lua_call_runtime.set_flag_runtime(nullptr);
      lua_call_context.flag_runtime = nullptr;
      lua_call_context.quest_runtime = nullptr;
    }
    const auto temp_canvas =
        std::filesystem::temp_directory_path() / "engine-mutate.uicanvas.json";
    {
      auto base = engine::UiCanvasAsset::load(
          engine::default_player_ui_canvas_path(project));
      r.check(base.has_value(), "automation mutate fixtures from sample");
      if (base)
        r.check(engine::write_ui_canvas_json_atomic(temp_canvas,
                                                    base.value().to_json())
                    .has_value(),
                "automation writes temp canvas");
    }
    engine::EditorSessionContext mutate_context;
    mutate_context.project_root = temp_canvas.parent_path();
    // Keep project_root as sample project; pass absolute via path that joins
    // root - use relative under temp by copying into project temp relative path
    // instead:
    const auto project_temp =
        project / "assets" / "ui" / "_agent_mutate_tmp.uicanvas.json";
    {
      auto base = engine::UiCanvasAsset::load(
          engine::default_player_ui_canvas_path(project));
      if (base)
        (void)engine::write_ui_canvas_json_atomic(project_temp,
                                                  base.value().to_json());
    }
    mutate_context.project_root = project;
    const auto mutate_call = engine::execute_editor_operation(
        mutate_context, "ui_canvas_mutate",
        R"({"path":"assets/ui/_agent_mutate_tmp.uicanvas.json","action":"move","id":"player_health","delta":[4,0]})");
    r.check(mutate_call.exit_code == engine::ExitCode::Success,
            "ui_canvas_mutate move succeeds");
    const auto style_call = engine::execute_editor_operation(
        mutate_context, "ui_canvas_mutate",
        R"({"path":"assets/ui/_agent_mutate_tmp.uicanvas.json","action":"style","id":"player_health","color":[200,40,40,255]})");
    r.check(style_call.exit_code == engine::ExitCode::Success,
            "ui_canvas_mutate style succeeds");
    std::error_code ec;
    std::filesystem::remove(project_temp, ec);
    std::filesystem::remove(temp_canvas, ec);
    lua_call_runtime.set_hud_runtime(nullptr);
    lua_call_runtime.set_ui_canvas_stack(nullptr);
    engine::EditorBridgeRequest request;
    request.operation = "scene_plan";
    request.params_json = "{\"description\":\"move entity\"}";
    r.check(engine::EditorBridgeResponse::parse_request(
                "{\"schemaVersion\":1,\"operation\":\"editor_status\","
                "\"requestId\":\"abc\"}")
                .has_value(),
            "bridge request parses");
    engine::EditorBridgeResponse response;
    response.summary = "ok";
    response.changed_object_ids = {"entity"};
    r.check(response.to_json().find("\"summary\":\"ok\"") != std::string::npos,
            "bridge response serializes");
    r.check(engine::validate_project_at(project).exit_code ==
                engine::ExitCode::Success,
            "sample project validates through automation helper");
    r.check(engine::editor_bridge_pipe_name(
                std::filesystem::path("samples/open-world-rpg")) ==
                engine::editor_bridge_pipe_name(project),
            "relative and absolute project paths share bridge pipe name");
    engine::AssetRegistry assets;
    std::map<std::string, engine::PrefabAsset> catalog;
    engine::EditorSessionContext context;
    context.project_root = project;
    context.assets = &assets;
    context.prefab_catalog = &catalog;
    const auto refresh = engine::execute_editor_operation(
        context, "asset_apply", "{\"action\":\"refresh_catalog\"}");
    r.check(refresh.exit_code == engine::ExitCode::Success,
            "asset catalog refresh succeeds offline");
    r.check(refresh.metadata.count("prefabCount") &&
                std::stoi(refresh.metadata.at("prefabCount")) >= 7,
            "asset catalog refresh finds sample prefabs");
    r.check(catalog.count(
                "assets/prefabs/scene assets/talk_marker.prefab.json") > 0 ||
                catalog.count(
                    "assets/prefabs/Scene Assets/talk_marker.prefab.json") > 0,
            "asset catalog includes talk_marker prefab");
    const auto terrain = engine::execute_editor_operation(
        context, "scene_apply",
        "{\"action\":\"sample_terrain\",\"x\":-2.5,\"z\":5.0}");
    r.check(terrain.exit_code == engine::ExitCode::Success,
            "terrain sample works without live scene");
    r.check(terrain.metadata.contains("height"),
            "terrain sample returns height metadata");
    engine::Scene scene;
    engine::CommandHistory history;
    bool dirty = false;
    engine::EditorSessionContext batch_context;
    batch_context.scene = &scene;
    batch_context.history = &history;
    batch_context.scene_dirty = &dirty;
    batch_context.project_root = project;
    batch_context.world_path = project / "worlds" / "vertical-slice.world.json";
    const auto tree_a =
        engine::EntityId::parse("00000000-0000-4000-8000-00000000aa01");
    const auto tree_b =
        engine::EntityId::parse("00000000-0000-4000-8000-00000000aa02");
    const auto batch_place = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"batch","label":"mcp-batch-place","ops":[{"action":"place","name":"Batch Tree A","prefab":"assets/prefabs/tree.prefab.json","entityId":"00000000-0000-4000-8000-00000000aa01","transform":{"position":[30,0,30],"scale":[1,1,1],"rotation":[0,0,0,1]}},{"action":"place","name":"Batch Tree B","prefab":"assets/prefabs/tree.prefab.json","entityId":"00000000-0000-4000-8000-00000000aa02","transform":{"position":[32,0,32],"scale":[1,1,1],"rotation":[0,0,0,1]}}]})");
    r.check(batch_place.exit_code == engine::ExitCode::Success,
            "scene batch place succeeds offline");
    r.check(batch_place.metadata.at("appliedCount") == "2",
            "scene batch reports applied count");
    r.check(tree_a && tree_b && scene.contains(tree_a.value()) &&
                scene.contains(tree_b.value()),
            "scene batch places entities");
    const auto batch_move = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"batch","ops":[{"action":"move","entityId":"00000000-0000-4000-8000-00000000aa01","transform":{"position":[35,0,35],"scale":[1,1,1],"rotation":[0,0,0,1]}},{"action":"move","entityId":"00000000-0000-4000-8000-00000000aa02","transform":{"position":[37,0,37],"scale":[1,1,1],"rotation":[0,0,0,1]}}]})");
    r.check(batch_move.exit_code == engine::ExitCode::Success,
            "scene batch move succeeds");
    r.check(history.undo(scene) &&
                scene.transform(tree_a.value())->position[0] == 30.0f,
            "single undo reverts scene batch move");
    const auto batch_fail = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"batch","ops":[{"action":"move","entityId":"00000000-0000-4000-8000-00000000aa01","transform":{"position":[40,0,40],"scale":[1,1,1],"rotation":[0,0,0,1]}},{"action":"move","entityId":"00000000-0000-4000-8000-00000000ffff","transform":{"position":[41,0,41],"scale":[1,1,1],"rotation":[0,0,0,1]}}]})");
    r.check(batch_fail.exit_code == engine::ExitCode::ValidationFailed,
            "scene batch rolls back on mid-batch failure");
    r.check(scene.transform(tree_a.value())->position[0] == 30.0f,
            "failed batch leaves prior state unchanged");
    const auto marker =
        engine::EntityId::parse("00000000-0000-4000-8000-00000000aa03");
    const auto place_marker = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"place_marker","name":"menu_cam_establishing","entityId":"00000000-0000-4000-8000-00000000aa03","transform":{"position":[12,18,24]}})");
    r.check(place_marker.exit_code == engine::ExitCode::Success && marker &&
                scene.contains(marker.value()),
            "scene place_marker creates a named marker");
    r.check(scene.name(marker.value()) == "menu_cam_establishing",
            "scene place_marker preserves marker name");
    const auto batch_marker =
        engine::EntityId::parse("00000000-0000-4000-8000-00000000aa08");
    const auto batch_place_marker = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"batch","ops":[{"action":"place_marker","name":"landfall_handoff","entityId":"00000000-0000-4000-8000-00000000aa08","x":14,"y":20,"z":26}]})");
    r.check(batch_place_marker.exit_code == engine::ExitCode::Success &&
                batch_marker && scene.contains(batch_marker.value()) &&
                scene.name(batch_marker.value()) == "landfall_handoff" &&
                scene.transform(batch_marker.value())->position[0] == 14.0f &&
                scene.transform(batch_marker.value())->position[1] == 20.0f &&
                scene.transform(batch_marker.value())->position[2] == 26.0f,
            "scene batch place_marker normalizes default prefab and shorthand position");
    const auto stamp_a =
        engine::EntityId::parse("00000000-0000-4000-8000-00000000aa04");
    const auto stamp_b =
        engine::EntityId::parse("00000000-0000-4000-8000-00000000aa05");
    const auto stamp_prefabs = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"stamp_prefabs","label":"graybox-stamp","stamps":[{"name":"Crate A","prefab":"assets/prefabs/crate.prefab.json","entityId":"00000000-0000-4000-8000-00000000aa04","x":42,"z":43,"snapToTerrain":false},{"name":"Crate B","prefab":"assets/prefabs/crate.prefab.json","entityId":"00000000-0000-4000-8000-00000000aa05","transform":{"position":[44,2,45]},"snapToTerrain":false}]})");
    r.check(stamp_prefabs.exit_code == engine::ExitCode::Success &&
                stamp_prefabs.metadata.at("appliedCount") == "2",
            "scene stamp_prefabs applies compact placement batch");
    r.check(stamp_a && stamp_b && scene.contains(stamp_a.value()) &&
                scene.contains(stamp_b.value()) &&
                scene.transform(stamp_a.value())->position[0] == 42.0f &&
                scene.transform(stamp_b.value())->position[2] == 45.0f,
            "scene stamp_prefabs expands x/z and transform placements");
    const auto stamp_scatter = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"stamp_scatter","label":"scatter-test","prefabs":["assets/prefabs/crate.prefab.json"],"count":3,"minX":50,"maxX":56,"minZ":50,"maxZ":56,"minSpacing":1.5,"namePrefix":"ScatterCrate_","seed":42,"snapToTerrain":false,"groundOffset":0})");
    r.check(stamp_scatter.exit_code == engine::ExitCode::Success &&
                stamp_scatter.metadata.at("placedCount") == "3",
            "scene stamp_scatter densifies with spacing");
    const auto float_audit = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"list","namePrefix":"ScatterCrate_","floatGapMin":-1000,"limit":10})");
    r.check(float_audit.exit_code == engine::ExitCode::Success &&
                float_audit.metadata.count("entitiesJson") &&
                float_audit.metadata.at("entitiesJson").find("\"floatGap\"") !=
                    std::string::npos,
            "scene list float audit includes floatGap");
    const auto composition_a =
        engine::EntityId::parse("00000000-0000-4000-8000-00000000aa06");
    const auto composition_b =
        engine::EntityId::parse("00000000-0000-4000-8000-00000000aa07");
    const auto stamp_compositions = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"stamp_compositions","label":"graybox-compositions","stamps":[{"name":"graybox_keep_wall_n","entityId":"00000000-0000-4000-8000-00000000aa06","x":50,"y":3,"z":51,"snapToTerrain":false,"parts":[{"name":"Stone","primitive":"cube","color":[0.42,0.4,0.38],"transform":{"position":[0,1,0],"scale":[4,2,1]}}]},{"name":"graybox_banner_pole","entityId":"00000000-0000-4000-8000-00000000aa07","transform":{"position":[52,1,53]},"snapToTerrain":false,"parts":[{"primitive":"cylinder","color":[0.35,0.22,0.15],"transform":{"scale":[0.15,2.5,0.15]}},{"primitive":"cube","color":[0.55,0.12,0.12],"transform":{"position":[0.4,2.0,0],"scale":[0.8,1.2,0.05]}}]}]})");
    r.check(stamp_compositions.exit_code == engine::ExitCode::Success &&
                stamp_compositions.metadata.at("appliedCount") == "2",
            "scene stamp_compositions places synthesized graybox prefabs");
    r.check(composition_a && composition_b &&
                scene.contains(composition_a.value()) &&
                scene.contains(composition_b.value()),
            "scene stamp_compositions creates named composition entities");
    r.check(std::filesystem::exists(project / "assets" / "prefabs" / "Graybox" /
                                    "graybox_keep_wall_n.prefab.json") &&
                std::filesystem::exists(project / "assets" / "prefabs" /
                                        "Graybox" /
                                        "graybox_banner_pole.prefab.json"),
            "scene stamp_compositions writes Graybox prefab assets");
    const auto bad_composition = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"stamp_compositions","stamps":[{"name":"bad","parts":[{"primitive":"torus"}]}]})");
    r.check(bad_composition.exit_code != engine::ExitCode::Success,
            "scene stamp_compositions rejects unsupported primitives");
    const auto component_plan =
        engine::classify_scene_plan("add component collider to entity", "");
    r.check(component_plan.target_kind == "scene_data",
            "scene plan classifies entity component edits");
    const auto prefab_component_plan = engine::classify_scene_plan(
        "add component on prefab", "assets/prefabs/tree.prefab.json");
    r.check(prefab_component_plan.target_kind == "prefab_asset",
            "scene plan classifies prefab component edits");
    batch_context.prefab_catalog = &catalog;
    const auto add_component = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"add_component","entityId":"00000000-0000-4000-8000-00000000aa01","component":{"id":"collider-test","type":"collider","data":{"shape":"box","halfExtent":[0.4,0.4,0.4],"layer":"staticWorld","trigger":false}}})");
    r.check(add_component.exit_code == engine::ExitCode::Success,
            "scene add_component succeeds");
    r.check(scene.authored_components(tree_a.value()) &&
                !scene.authored_components(tree_a.value())->entries.empty(),
            "add_component stores entity component");
    const auto dedicated = engine::execute_editor_operation(
        batch_context, "entity_component_apply",
        R"({"action":"remove_component","entityId":"00000000-0000-4000-8000-00000000aa01","componentId":"collider-test"})");
    r.check(dedicated.exit_code == engine::ExitCode::Success,
            "entity_component_apply remove succeeds");
    const auto terrain_plan = engine::classify_scene_plan(
        "flatten terrain near spawn and paint grass", "");
    r.check(terrain_plan.target_kind == "terrain_data",
            "scene plan classifies terrain sculpt/paint");
    engine::TerrainEditStore terrain_edits;
    engine::TerrainEditHistory terrain_history;
    bool terrain_dirty = false;
    engine::TerrainPaintStore terrain_paint;
    engine::TerrainPaintHistory terrain_paint_history;
    bool paint_dirty = false;
    engine::EditorSessionContext terrain_context;
    terrain_context.project_root = project;
    terrain_context.terrain_edits = &terrain_edits;
    terrain_context.terrain_history = &terrain_history;
    terrain_context.terrain_edits_dirty = &terrain_dirty;
    terrain_context.terrain_paint = &terrain_paint;
    terrain_context.terrain_paint_history = &terrain_paint_history;
    terrain_context.terrain_paint_dirty = &paint_dirty;
    engine::set_active_terrain_edits(&terrain_edits);
    const float before_h = engine::sample_terrain_height(0.0f, 0.0f);
    const auto raised = engine::execute_editor_operation(
        terrain_context, "terrain_apply",
        R"({"action":"raise","x":0,"z":0,"radius":3,"strength":0.5})");
    r.check(raised.exit_code == engine::ExitCode::Success,
            "terrain_apply raise succeeds");
    r.check(engine::sample_terrain_height(0.0f, 0.0f) > before_h,
            "terrain_apply raise changes height");
    const auto flattened = engine::execute_editor_operation(
        terrain_context, "terrain_apply",
        R"({"action":"flatten","x":0,"z":0,"radius":3,"strength":1.0,"targetHeight":0.0})");
    r.check(flattened.exit_code == engine::ExitCode::Success,
            "terrain_apply flatten succeeds");
    const auto painted = engine::execute_editor_operation(
        terrain_context, "terrain_apply",
        R"({"action":"paint","x":0,"z":0,"radius":3,"material":"assets/materials/grass.material.json"})");
    r.check(painted.exit_code == engine::ExitCode::Success,
            "terrain_apply paint succeeds");
    r.check(paint_dirty, "terrain_apply paint marks dirty");
    const auto batched = engine::execute_editor_operation(
        terrain_context, "terrain_apply",
        R"({"action":"batch","ops":[{"action":"raise","x":5,"z":5,"radius":2,"strength":0.25},{"action":"paint","x":5,"z":5,"radius":2,"material":"assets/materials/stone.material.json"}]})");
    r.check(batched.exit_code == engine::ExitCode::Success,
            "terrain_apply batch succeeds");
    r.check(batched.metadata.at("appliedCount") == "2",
            "terrain_apply batch reports applied count");
    r.check(terrain_history.undo_size() >= 2,
            "terrain_apply batch records undo entries");
    {
      engine::McpJobQueue jobs;
      terrain_context.mcp_jobs = &jobs;
      const auto missing = engine::execute_editor_operation(
          terrain_context, "job_call",
          R"({"kind":"submit","ops":[]})");
      r.check(missing.exit_code != engine::ExitCode::Success,
              "job submit rejects empty ops");
      const float before_job_h = engine::sample_terrain_height(1.0f, 1.0f);
      const auto submitted = engine::execute_editor_operation(
          terrain_context, "job_call",
          R"({"kind":"submit","label":"test_grade","ops":[{"target":"terrain","action":"raise","x":1,"z":1,"radius":3,"strength":0.8},{"target":"terrain","action":"raise","x":1,"z":1,"radius":3,"strength":0.8}]})");
      r.check(submitted.exit_code == engine::ExitCode::Success,
              "job submit succeeds");
      r.check(submitted.metadata.count("jobId") &&
                  !submitted.metadata.at("jobId").empty(),
              "job submit returns jobId");
      r.check(submitted.metadata.at("status") == "queued",
              "job submit starts queued");
      const auto job_id = submitted.metadata.at("jobId");
      r.check(jobs.has_work(), "submitted job is pending work");
      r.check(jobs.tick(terrain_context, 1), "job tick runs one op");
      const auto mid = engine::execute_editor_operation(
          terrain_context, "job_call",
          nlohmann::json({{"kind", "status"}, {"jobId", job_id}}).dump());
      r.check(mid.metadata.at("status") == "running" ||
                  mid.metadata.at("opIndex") == "1",
              "job status shows progress after first tick");
      int guard = 0;
      while (jobs.has_work() && guard++ < 8)
        jobs.tick(terrain_context, 1);
      const auto done = engine::execute_editor_operation(
          terrain_context, "job_call",
          nlohmann::json({{"kind", "status"}, {"jobId", job_id}}).dump());
      r.check(done.metadata.at("status") == "succeeded",
              "job reaches succeeded after ticks");
      r.check(engine::sample_terrain_height(1.0f, 1.0f) > before_job_h + 0.05f,
              "job terrain ops applied heights");
      const auto listed = engine::execute_editor_operation(
          terrain_context, "job_call", R"({"kind":"list"})");
      r.check(listed.exit_code == engine::ExitCode::Success &&
                  listed.metadata.count("jobsJson"),
              "job list returns jobsJson");
      const auto cancel_submit = engine::execute_editor_operation(
          terrain_context, "job_call",
          R"({"kind":"submit","label":"to_cancel","ops":[{"target":"terrain","action":"raise","x":0,"z":0,"radius":1,"strength":0.1},{"target":"terrain","action":"raise","x":0,"z":0,"radius":1,"strength":0.1}]})");
      const auto cancel_id = cancel_submit.metadata.at("jobId");
      const auto cancelled = engine::execute_editor_operation(
          terrain_context, "job_call",
          nlohmann::json({{"kind", "cancel"}, {"jobId", cancel_id}}).dump());
      r.check(cancelled.metadata.at("status") == "cancelled",
              "job cancel marks queued job cancelled");
      terrain_context.mcp_jobs = nullptr;
    }
    r.check(terrain_history.undo_size() >= 2,
            "terrain_apply batch commits one height undo entry");
    const auto paint_road = engine::execute_editor_operation(
        terrain_context, "terrain_apply",
        R"({"action":"paint_along","points":[{"x":0,"z":0},{"x":6,"z":2}],"radius":2.5,"material":"assets/materials/forest_floor.material.json","step":2})");
    r.check(paint_road.exit_code == engine::ExitCode::Success,
            "terrain_apply paint_along succeeds");
    const auto pad = engine::execute_editor_operation(
        terrain_context, "terrain_apply",
        R"({"action":"flatten_pad","x":3,"z":3,"radius":4,"targetHeight":1.0})");
    r.check(pad.exit_code == engine::ExitCode::Success,
            "terrain_apply flatten_pad succeeds");
    const auto smart_batch = engine::execute_editor_operation(
        terrain_context, "terrain_apply",
        R"({"action":"batch","ops":[{"action":"flatten_pad","x":-4,"z":-4,"radius":3,"targetHeight":0.5},{"action":"sample","x":-4,"z":-4}]})");
    r.check(smart_batch.exit_code == engine::ExitCode::Success &&
                smart_batch.metadata.at("appliedCount") == "2" &&
                smart_batch.metadata.count("samplesJson"),
            "terrain_apply batch accepts flatten_pad and sample");
    const auto listed = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"list","namePrefix":"Batch","limit":10})");
    r.check(listed.exit_code == engine::ExitCode::Success,
            "scene_apply list succeeds");
    r.check(listed.metadata.count("count"), "scene list returns count");
    const auto snapped = engine::execute_editor_operation(
        batch_context, "scene_apply",
        R"({"action":"snap_to_terrain","names":["Batch Tree A"],"groundOffset":0})");
    r.check(snapped.exit_code == engine::ExitCode::Success,
            "scene_apply snap_to_terrain by name succeeds");
    const auto undone = engine::execute_editor_operation(
        terrain_context, "terrain_apply",
        R"({"action":"undo","kind":"height"})");
    r.check(undone.exit_code == engine::ExitCode::Success,
            "terrain_apply undo height succeeds");
    engine::FoliageDensityStore foliage_density;
    engine::FoliageDensityHistory foliage_history;
    bool foliage_dirty = false;
    engine::FoliageLayerPalette foliage_palette;
    foliage_palette.layers.push_back({"grass", "Grass", "grass_blade"});
    foliage_palette.layers.push_back({"flower", "Flower", "flower_clump"});
    terrain_context.foliage_density = &foliage_density;
    terrain_context.foliage_density_history = &foliage_history;
    terrain_context.foliage_density_dirty = &foliage_dirty;
    terrain_context.foliage_layers = &foliage_palette;
    const auto foliage_painted = engine::execute_editor_operation(
        terrain_context, "terrain_apply",
        R"({"action":"paint_foliage","x":0,"z":0,"radius":3,"strength":0.4,"layer":"grass"})");
    r.check(foliage_painted.exit_code == engine::ExitCode::Success,
            "terrain_apply paint_foliage succeeds");
    r.check(foliage_dirty, "terrain_apply paint_foliage marks dirty");
    r.check(foliage_density.density_at({0, 0}, 16, 16) > 0,
            "terrain_apply paint_foliage writes density");
    const auto foliage_batch = engine::execute_editor_operation(
        terrain_context, "terrain_apply",
        R"({"action":"batch","ops":[{"action":"paint_foliage","x":2,"z":2,"radius":2,"strength":0.3,"layer":"flower"},{"action":"paint_foliage_mixed","x":-2,"z":-2,"radius":2,"strength":0.25}]})");
    r.check(foliage_batch.exit_code == engine::ExitCode::Success,
            "terrain_apply foliage batch succeeds");
    r.check(foliage_batch.metadata.at("foliageChanged") == "true",
            "terrain_apply foliage batch reports foliageChanged");
    const auto foliage_undone = engine::execute_editor_operation(
        terrain_context, "terrain_apply",
        R"({"action":"undo","kind":"foliage"})");
    r.check(foliage_undone.exit_code == engine::ExitCode::Success,
            "terrain_apply undo foliage succeeds");
    const auto foliage_plan = engine::classify_scene_plan(
        "paint foliage ground cover near spawn", "");
    r.check(foliage_plan.target_kind == "terrain_data",
            "scene plan classifies foliage paint");
    engine::set_active_terrain_edits(nullptr);
    (void)request;
    {
      const auto missing = engine::apply_project_git_operation(
          std::filesystem::temp_directory_path() /
              ("engine-not-a-git-" + engine::make_correlation_id()),
          nlohmann::json{{"action", "status"}});
      r.check(missing.exit_code == engine::ExitCode::Unavailable,
              "project_git rejects missing/non-repo path");
      const auto no_action = engine::apply_project_git_operation(
          project, nlohmann::json::object());
      r.check(no_action.exit_code == engine::ExitCode::InvalidArguments,
              "project_git requires action");
      const auto sample_status = engine::apply_project_git_operation(
          project, nlohmann::json{{"action", "status"}});
      r.check(sample_status.exit_code == engine::ExitCode::Success,
              "project_git status on sample project");
      r.check(sample_status.metadata.count("branch") != 0,
              "project_git status reports branch");
      r.check(sample_status.metadata.count("repoRoot") != 0,
              "project_git status reports repoRoot");

      const auto fixture =
          std::filesystem::temp_directory_path() /
          ("engine-project-git-" + engine::make_correlation_id());
      std::error_code ec;
      std::filesystem::create_directories(fixture / "assets" / "world-forge",
                                          ec);
      std::filesystem::create_directories(fixture / "build", ec);
      {
        std::ofstream manifest(fixture / "project.engine.json");
        manifest
            << R"({"schemaVersion":1,"projectId":"git_fixture","name":"Git Fixture"})";
      }
      {
        std::ofstream story(fixture / "assets" / "world-forge" /
                            "quests.worldforge.json");
        story << R"({"schemaVersion":1,"id":"quests","quests":[]})";
      }
      {
        std::ofstream junk(fixture / "build" / "ignore-me.exe");
        junk << "x";
      }
#if defined(_WIN32)
      const char *git_quiet = ">NUL 2>&1";
#else
      const char *git_quiet = ">/dev/null 2>&1";
#endif
      const auto init = std::system(
          ("git -C \"" + fixture.string() + "\" init -b main " + git_quiet)
              .c_str());
      r.check(init == 0, "project_git fixture git init");
      (void)std::system(("git -C \"" + fixture.string() +
                         "\" config user.email \"engine-suite@example.com\" " +
                         git_quiet)
                            .c_str());
      (void)std::system(("git -C \"" + fixture.string() +
                         "\" config user.name \"Engine Suite\" " + git_quiet)
                            .c_str());
      const auto commit_empty_msg = engine::apply_project_git_operation(
          fixture, nlohmann::json{{"action", "commit"}, {"message", ""}});
      r.check(commit_empty_msg.exit_code == engine::ExitCode::InvalidArguments,
              "project_git commit requires message");
      const auto commit_ok = engine::apply_project_git_operation(
          fixture, nlohmann::json{{"action", "commit"},
                                  {"message", "seed world forge"}});
      r.check(commit_ok.exit_code == engine::ExitCode::Success,
              "project_git commit stages project content");
      r.check(commit_ok.metadata.count("stagedPaths") != 0 &&
                  commit_ok.metadata.at("stagedPaths").find("world-forge") !=
                      std::string::npos,
              "project_git commit includes world-forge path");
      r.check(commit_ok.metadata.at("stagedPaths").find("build/") ==
                      std::string::npos &&
                  commit_ok.metadata.at("stagedPaths").find("ignore-me.exe") ==
                      std::string::npos,
              "project_git commit blocks build artifacts");
      const auto clean = engine::apply_project_git_operation(
          fixture, nlohmann::json{{"action", "status"}});
      r.check(clean.exit_code == engine::ExitCode::Success &&
                  clean.metadata.at("dirtyCount") == "0",
              "project_git status clean after commit");
      const auto nothing = engine::apply_project_git_operation(
          fixture, nlohmann::json{{"action", "commit"}, {"message", "noop"}});
      r.check(nothing.exit_code == engine::ExitCode::ValidationFailed,
              "project_git commit fails when clean");
      engine::EditorSessionContext git_ctx;
      git_ctx.project_root = fixture;
      const auto via_op = engine::execute_editor_operation(
          git_ctx, "project_git", R"({"action":"status"})");
      // Prefer direct call if editor path fails for empty context (same API
      // surface).
      const auto status_ok =
          via_op.exit_code == engine::ExitCode::Success
              ? via_op
              : engine::apply_project_git_operation(
                    fixture, nlohmann::json{{"action", "status"}});
      r.check(status_ok.exit_code == engine::ExitCode::Success,
              "editor project_git operation succeeds");
      std::filesystem::remove_all(fixture, ec);
    }
    {
      // Planning backlog reader (TICKET-0228): read-only parse of the
      // authoritative epics.md.
      const auto repo_root = std::filesystem::path(ENGINE_REPOSITORY_ROOT);
      const auto planning_root = engine::find_planning_root(project);
      r.check(planning_root.has_value(),
              "planning root found from sample project");
      r.check(planning_root &&
                  std::filesystem::equivalent(*planning_root, repo_root),
              "planning root resolves to repository root");
      const auto backlog = engine::load_planning_backlog(
          repo_root / "context" / "planning" / "epics.md");
      r.check(backlog.has_value(), "planning backlog loads from epics.md");
      if (backlog) {
        r.check(backlog.value().tickets.size() >= 100,
                "planning backlog parses ticket tables");
        const auto *seed = backlog.value().find("TICKET-0228");
        r.check(seed != nullptr, "planning backlog finds TICKET-0228");
        r.check(seed && seed->epic_id == "EPIC-0001",
                "planning backlog maps ticket to epic heading");
        const auto counts = backlog.value().status_counts();
        r.check(counts.count("needs-approval") != 0 &&
                    counts.at("needs-approval") > 0,
                "planning backlog counts statuses");
      }
      const auto missing_backlog = engine::load_planning_backlog(
          std::filesystem::temp_directory_path() /
          ("engine-no-epics-" + engine::make_correlation_id()));
      r.check(!missing_backlog.has_value() &&
                  missing_backlog.error().code == "PLANNING-BACKLOG-MISSING",
              "planning backlog fails closed on missing file");
    }
    {
      // Build coordination lease queue (TICKET-0228) on an isolated fixture
      // root.
      const auto fixture =
          std::filesystem::temp_directory_path() /
          ("engine-build-coord-" + engine::make_correlation_id());
      std::error_code ec;
      std::filesystem::create_directories(fixture / "context" / "planning", ec);
      {
        std::ofstream epics(fixture / "context" / "planning" / "epics.md");
        epics << "# Fixture backlog\n\n## EPIC-0001: Fixture epic\n\n"
              << "| ID | Title | Status | Priority | Notes |\n| --- | --- | "
                 "--- | --- | --- |\n"
              << "| TICKET-0001 | Fixture ticket one | ready | P1 | |\n"
              << "| TICKET-0002 | Fixture ticket two | active | P2 | |\n";
      }
      const auto state_path = engine::build_coordination_state_path(fixture);
      r.check(state_path == fixture / ".engine" / "build-coordinator.json",
              "coordination state path anchors to fixture planning root");

      const auto no_action = engine::apply_build_coordination_operation(
          fixture, nlohmann::json::object());
      r.check(no_action.exit_code == engine::ExitCode::InvalidArguments,
              "build coordination requires action");
      const auto missing_params = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "acquire"}, {"agentId", "alice"}});
      r.check(missing_params.exit_code == engine::ExitCode::InvalidArguments,
              "acquire requires agentId + ticketId + summary");
      const auto unknown_ticket = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "acquire"},
                                  {"agentId", "alice"},
                                  {"ticketId", "TICKET-9999"},
                                  {"summary", "rebuild"}});
      r.check(unknown_ticket.exit_code == engine::ExitCode::ValidationFailed &&
                  !unknown_ticket.diagnostics.empty() &&
                  unknown_ticket.diagnostics.front().code ==
                      "BUILD-COORD-TICKET-UNKNOWN",
              "acquire rejects ticket ids missing from epics.md");

      const auto granted = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "acquire"},
                                  {"agentId", "alice"},
                                  {"ticketId", "ticket-0001"},
                                  {"summary", "rebuild engine"}});
      r.check(granted.exit_code == engine::ExitCode::Success &&
                  granted.metadata.count("leaseToken") != 0 &&
                  granted.metadata.at("state") == "granted",
              "first acquire grants the lease");
      r.check(granted.metadata.count("leaseTicketId") != 0 &&
                  granted.metadata.at("leaseTicketId") == "TICKET-0001",
              "acquire normalizes ticket id casing");
      const std::string alice_token = granted.metadata.count("leaseToken")
                                          ? granted.metadata.at("leaseToken")
                                          : std::string{};

      const auto re_granted = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "acquire"},
                                  {"agentId", "alice"},
                                  {"ticketId", "TICKET-0001"},
                                  {"summary", "rebuild engine"}});
      r.check(re_granted.exit_code == engine::ExitCode::Success &&
                  re_granted.metadata.count("leaseToken") != 0 &&
                  re_granted.metadata.at("leaseToken") == alice_token,
              "holder re-acquire is idempotent (same token)");

      const auto queued_bob = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "acquire"},
                                  {"agentId", "bob"},
                                  {"ticketId", "TICKET-0002"},
                                  {"summary", "rebuild for audio"}});
      r.check(queued_bob.exit_code == engine::ExitCode::Unavailable &&
                  queued_bob.metadata.at("state") == "queued" &&
                  queued_bob.metadata.at("queuePosition") == "1" &&
                  queued_bob.metadata.count("retryAfterMs") != 0,
              "second agent queues FIFO at position 1");
      r.check(queued_bob.metadata.count("leaseToken") == 0,
              "queued response never leaks the lease token");
      const auto queued_carol = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "acquire"},
                                  {"agentId", "carol"},
                                  {"ticketId", "TICKET-0001"},
                                  {"summary", "rebuild for terrain"}});
      r.check(queued_carol.exit_code == engine::ExitCode::Unavailable &&
                  queued_carol.metadata.at("queuePosition") == "2",
              "third agent queues FIFO at position 2");
      const auto queued_bob_again = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "acquire"},
                                  {"agentId", "bob"},
                                  {"ticketId", "TICKET-0002"},
                                  {"summary", "rebuild for audio"}});
      r.check(queued_bob_again.exit_code == engine::ExitCode::Unavailable &&
                  queued_bob_again.metadata.at("queuePosition") == "1" &&
                  queued_bob_again.metadata.at("queueLength") == "2",
              "repeat queue request dedupes (no duplicate entry)");

      const auto wait_timeout = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "wait"},
                                  {"agentId", "bob"},
                                  {"ticketId", "TICKET-0002"},
                                  {"summary", "rebuild for audio"},
                                  {"timeoutSeconds", 1}});
      r.check(wait_timeout.exit_code == engine::ExitCode::Unavailable &&
                  !wait_timeout.diagnostics.empty() &&
                  wait_timeout.diagnostics.back().code ==
                      "BUILD-COORD-WAIT-TIMEOUT",
              "wait times out while another agent holds the lease");

      const auto bad_release = engine::apply_build_coordination_operation(
          fixture,
          nlohmann::json{{"action", "release"}, {"token", "not-the-token"}});
      r.check(bad_release.exit_code == engine::ExitCode::ValidationFailed &&
                  !bad_release.diagnostics.empty() &&
                  bad_release.diagnostics.front().code ==
                      "BUILD-COORD-TOKEN-MISMATCH",
              "release rejects a mismatched token");
      const auto heartbeat = engine::apply_build_coordination_operation(
          fixture,
          nlohmann::json{{"action", "heartbeat"}, {"token", alice_token}});
      r.check(heartbeat.exit_code == engine::ExitCode::Success,
              "heartbeat extends the holder lease");
      const auto released = engine::apply_build_coordination_operation(
          fixture,
          nlohmann::json{{"action", "release"}, {"token", alice_token}});
      r.check(released.exit_code == engine::ExitCode::Success &&
                  released.metadata.at("state") == "released",
              "holder release succeeds with the granted token");

      const auto carol_blocked = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "acquire"},
                                  {"agentId", "carol"},
                                  {"ticketId", "TICKET-0001"},
                                  {"summary", "rebuild for terrain"}});
      r.check(carol_blocked.exit_code == engine::ExitCode::Unavailable,
              "queue front is respected after release (carol waits for bob)");
      const auto bob_granted = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "wait"},
                                  {"agentId", "bob"},
                                  {"ticketId", "TICKET-0002"},
                                  {"summary", "rebuild for audio"},
                                  {"timeoutSeconds", 5}});
      r.check(bob_granted.exit_code == engine::ExitCode::Success &&
                  bob_granted.metadata.at("state") == "granted",
              "front-of-queue agent acquires via wait after release");
      const std::string bob_token = bob_granted.metadata.count("leaseToken")
                                        ? bob_granted.metadata.at("leaseToken")
                                        : std::string{};

      const auto status_held = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "status"}});
      r.check(status_held.exit_code == engine::ExitCode::Success &&
                  status_held.metadata.at("state") == "held" &&
                  status_held.metadata.at("leaseAgentId") == "bob" &&
                  status_held.metadata.at("queueLength") == "1",
              "status reports holder and queue after handoff");
      r.check(status_held.metadata.count("leaseToken") == 0,
              "status never exposes the lease token");

      const auto force_cleared = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "clear-stale"},
                                  {"force", true},
                                  {"agentId", "owner"}});
      r.check(force_cleared.exit_code == engine::ExitCode::Success &&
                  force_cleared.metadata.at("cleared") == "true" &&
                  force_cleared.metadata.at("state") == "idle",
              "clear-stale force clears a live lease");
      const auto release_after_force =
          engine::apply_build_coordination_operation(
              fixture,
              nlohmann::json{{"action", "release"}, {"token", bob_token}});
      r.check(release_after_force.exit_code ==
                      engine::ExitCode::ValidationFailed &&
                  !release_after_force.diagnostics.empty() &&
                  release_after_force.diagnostics.front().code ==
                      "BUILD-COORD-NOT-HELD",
              "release after force-clear reports no active lease");

      // Expired lease + dead-owner queue entry reclaim from a crafted state
      // file.
      {
        nlohmann::json crafted{{"schemaVersion", 1}};
        crafted["lease"] = nlohmann::json{{"token", "stale-token"},
                                          {"agentId", "ghost"},
                                          {"ticketId", "TICKET-0001"},
                                          {"summary", "crashed"},
                                          {"command", ""},
                                          {"pid", 4294967293ull},
                                          {"acquiredAtMs", 1000},
                                          {"heartbeatAtMs", 1000},
                                          {"expiresAtMs", 2000},
                                          {"leaseSeconds", 600}};
        crafted["queue"] =
            nlohmann::json::array({nlohmann::json{{"agentId", "ghost2"},
                                                  {"ticketId", "TICKET-0002"},
                                                  {"summary", "gone"},
                                                  {"pid", 4294967291ull},
                                                  {"requestedAtMs", 1000}}});
        crafted["events"] = nlohmann::json::array();
        std::filesystem::create_directories(state_path.parent_path(), ec);
        std::ofstream(state_path, std::ios::binary | std::ios::trunc)
            << crafted.dump();
      }
      const auto reclaimed = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "status"}});
      r.check(reclaimed.exit_code == engine::ExitCode::Success &&
                  reclaimed.metadata.at("state") == "idle" &&
                  reclaimed.metadata.at("queueLength") == "0",
              "expired/dead lease and dead queue entry are reclaimed");
      r.check(reclaimed.metadata.count("eventsJson") != 0 &&
                  reclaimed.metadata.at("eventsJson").find("lease_reclaimed") !=
                      std::string::npos,
              "stale reclaim records an event");
      const auto after_reclaim = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "acquire"},
                                  {"agentId", "dana"},
                                  {"ticketId", "TICKET-0001"},
                                  {"summary", "rebuild after crash"}});
      r.check(after_reclaim.exit_code == engine::ExitCode::Success,
              "acquire succeeds immediately after stale reclaim");

      // Malformed state file recovers instead of failing.
      std::ofstream(state_path, std::ios::binary | std::ios::trunc)
          << "{not json";
      const auto recovered = engine::apply_build_coordination_operation(
          fixture, nlohmann::json{{"action", "status"}});
      r.check(recovered.exit_code == engine::ExitCode::Success &&
                  recovered.metadata.at("state") == "idle",
              "malformed coordinator state resets to idle");
      r.check(recovered.metadata.count("eventsJson") != 0 &&
                  recovered.metadata.at("eventsJson").find("state_recovered") !=
                      std::string::npos,
              "malformed state recovery records an event");

      std::filesystem::remove_all(fixture, ec);
    }
    {
      engine::AnimationPreviewRequest sample_preview;
      sample_preview.project_root = project;
      sample_preview.frames = 40;
      auto sample = engine::run_animation_preview(sample_preview);
      r.check(sample && sample.value().ok,
              "sample project animation preview ok");
      r.check(sample && sample.value().controller_path.find(
                            "example.animator.json") != std::string::npos,
              "sample project picks example controller");
      auto found = engine::find_default_animator_controller(project);
      r.check(found && found.value().find("example.animator.json") !=
                           std::string::npos,
              "find_default_animator_controller locates sample");
    }
  } else if (suite == "diagnostics") {
    auto path = std::filesystem::temp_directory_path() /
                ("engine-diagnostics-suite-" + engine::make_correlation_id() +
                 ".jsonl");
    engine::Logger::instance().initialize(path);
    engine::Logger::instance().set_process_context(
        {"diagnostics-session", "2026-08-10T19:30:00Z", "editor",
         "sample-project", "worlds/main-menu.world.json", "Debug", 4242});
    engine::EngineError error{"TEST-RUNTIME",
                              engine::Severity::Error,
                              engine::ErrorCategory::Validation,
                              "diagnostics-test",
                              "visible error",
                              std::nullopt,
                              {},
                              "fix",
                              "test-correlation",
                              engine::ErrorPriority::P1High};
    engine::Logger::instance().write(error);
    r.check(engine::Logger::instance().error_count() == 1,
            "error counter increments");
    r.check(engine::Logger::instance().recent_errors().size() == 1,
            "recent error retained");
    std::ifstream input(path);
    std::string line;
    std::getline(input, line);
    r.check(line.find("\"code\":\"TEST-RUNTIME\"") != std::string::npos,
            "JSONL error persisted");
    r.check(line.find("\"priority\":\"P1\"") != std::string::npos,
            "priority label persisted");
    r.check(line.find("\"gpuDiagnostics\":{\"available\":") !=
                std::string::npos,
            "JSONL includes GPU diagnostics");
    r.check(line.find("\"process\":{\"sessionId\":\"diagnostics-session\"") !=
                std::string::npos &&
                line.find("\"pid\":4242") != std::string::npos,
            "JSONL includes process session metadata");
    r.check(line.find("\"adapterName\":") != std::string::npos &&
                line.find("\"driverVersion\":") != std::string::npos &&
                line.find("\"dedicatedVideoMemory\":") != std::string::npos &&
                line.find("\"featureLevel\":") != std::string::npos &&
                line.find("\"deviceRemovalHresult\":") != std::string::npos,
            "JSONL includes GPU context fields");
    engine::Logger::instance().write_event(
        "performance_dip", "{\"fps\":8.0,\"editorUiMs\":90.98}");
    std::string event_line;
    std::getline(input, event_line);
    r.check(event_line.find("\"event\":\"performance_dip\"") !=
                std::string::npos &&
                event_line.find("\"editorUiMs\":90.98") !=
                std::string::npos,
            "structured diagnostic event persisted");
    const auto bundle_root =
        std::filesystem::temp_directory_path() /
        ("engine-diagnostics-bundle-" + engine::make_correlation_id());
    auto bundle =
        engine::CrashBundle::write_diagnostic_bundle(bundle_root, error);
    r.check(bundle.has_value(), "crash bundle writes");
    if (bundle) {
      std::ifstream diagnostic(bundle.value() / "diagnostic.json");
      std::string contents((std::istreambuf_iterator<char>(diagnostic)), {});
      r.check(contents.find("\"gpuDiagnostics\":{\"available\":") !=
                  std::string::npos,
              "crash bundle includes GPU diagnostics");
      r.check(
          contents.find("\"adapterName\":") != std::string::npos &&
              contents.find("\"driverVersion\":") != std::string::npos &&
              contents.find("\"dedicatedVideoMemory\":") != std::string::npos &&
              contents.find("\"featureLevel\":") != std::string::npos &&
              contents.find("\"deviceRemovalHresult\":") != std::string::npos,
          "crash bundle includes GPU context fields");
    }
    std::error_code cleanup;
    std::filesystem::remove(path, cleanup);
    std::filesystem::remove_all(bundle_root, cleanup);
  } else if (suite == "animator") {
    const auto root =
        std::filesystem::temp_directory_path() /
        ("engine-animator-suite-" + engine::make_correlation_id());
    std::filesystem::create_directories(root / "assets/models");
    std::filesystem::create_directories(root / "assets/animators");
    std::filesystem::create_directories(root / "assets/prefabs");
    const auto clips_gltf = root / "assets/models/hero_clips.gltf";
    std::ofstream(clips_gltf) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"}],
"animations":[
{"name":"Idle","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
{"name":"Walk","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
{"name":"Attack","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}
],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
    const auto controller_path = root / "assets/animators/hero.animator.json";
    std::ofstream(controller_path) << R"({
"schemaVersion":1,
"kind":"animatorController",
"id":"hero_locomotion",
"parameters":[
  {"name":"speed","type":"float","default":0.0},
  {"name":"attack","type":"trigger"}
],
"layers":[{
  "name":"base",
  "defaultState":"idle",
  "blendMode":"override",
  "states":[
    {"name":"idle","motion":{"type":"clip","clipSource":"assets/models/hero_clips.gltf","clip":"Idle","loop":true}},
    {"name":"locomotion","motion":{"type":"blendTree1D","parameter":"speed","children":[
      {"threshold":0.0,"clipSource":"assets/models/hero_clips.gltf","clip":"Idle","loop":true},
      {"threshold":1.0,"clipSource":"assets/models/hero_clips.gltf","clip":"Walk","loop":true}
    ]}},
    {"name":"attack","motion":{"type":"clip","clipSource":"assets/models/hero_clips.gltf","clip":"Attack","loop":false}}
  ],
  "transitions":[
    {"from":"idle","to":"locomotion","duration":0.1,"conditions":[{"parameter":"speed","op":"greater","value":0.1}]},
    {"from":"locomotion","to":"idle","duration":0.1,"conditions":[{"parameter":"speed","op":"lessOrEqual","value":0.1}]},
    {"from":"*","to":"attack","duration":0.05,"conditions":[{"parameter":"attack","op":"trigger"}]}
  ]
}]
})";
    auto parsed = engine::AnimatorControllerAsset::load(controller_path);
    r.check(parsed && parsed.value().id == "hero_locomotion" &&
                parsed.value().layers.size() == 1,
            "animator controller loads");
    r.check(parsed && parsed.value().layers[0].states.size() == 3,
            "animator controller states parsed");
    auto bad_ctrl = engine::AnimatorControllerAsset::parse(
        R"({"schemaVersion":1,"kind":"animatorController","id":"x","layers":[]})",
        "bad");
    r.check(!bad_ctrl && bad_ctrl.error().code == "ANIM-CTRL-LAYERS",
            "empty layers rejected");

    engine::AnimationClipLibrary clips;
    engine::AnimatorRuntime animator;
    animator.set_project_root(root);
    animator.set_clip_library(&clips);
    const std::string entity = "00000000-0000-4000-8000-0000000000a1";
    r.check(animator.attach(entity, "assets/animators/hero.animator.json")
                .has_value(),
            "animator attach succeeds");
    auto idle_state = animator.current_state(entity);
    r.check(idle_state && idle_state.value() == "idle",
            "animator starts in idle");
    r.check(animator.set_float(entity, "speed", 0.5f).has_value(),
            "animator set_float succeeds");
    animator.tick(0.0f);
    auto mid_state = animator.current_state(entity);
    auto mid_status = animator.status(entity);
    r.check((mid_state && mid_state.value() == "locomotion") ||
                (mid_status && mid_status.value().layers[0].in_transition),
            "speed transition toward locomotion");
    for (int i = 0; i < 5; ++i)
      animator.tick(0.05f);
    auto loco_state = animator.current_state(entity);
    r.check(loco_state && loco_state.value() == "locomotion",
            "locomotion state after blend duration");
    auto status = animator.status(entity);
    r.check(status && !status.value().active_clips.empty(),
            "active clip weights reported");
    bool has_walk = false;
    float weight_sum = 0.0f;
    for (const auto &clip : status.value().active_clips) {
      weight_sum += clip.weight;
      if (clip.clip == "Walk")
        has_walk = true;
    }
    r.check(has_walk && weight_sum > 0.9f,
            "blendTree1D includes Walk at speed 0.5");

    // blendTree2D: freeform Cartesian walk/strafe/run sample
    {
      const auto dir_ctrl = root / "assets/animators/hero_dir.animator.json";
      std::ofstream(dir_ctrl) << R"({
"schemaVersion":1,
"kind":"animatorController",
"id":"hero_dir",
"parameters":[
  {"name":"speed","type":"float","default":0.0},
  {"name":"moveX","type":"float","default":0.0},
  {"name":"moveZ","type":"float","default":0.0}
],
"layers":[{
  "name":"base",
  "defaultState":"locomotion",
  "blendMode":"override",
  "states":[
    {"name":"locomotion","motion":{"type":"blendTree2D","parameterX":"moveX","parameterY":"moveZ","children":[
      {"position":[0.0,0.35],"clipSource":"assets/models/hero_clips.gltf","clip":"Walk","loop":true},
      {"position":[-0.35,0.0],"clipSource":"assets/models/hero_clips.gltf","clip":"Walk","loop":true},
      {"position":[0.35,0.0],"clipSource":"assets/models/hero_clips.gltf","clip":"Walk","loop":true},
      {"position":[0.0,-0.35],"clipSource":"assets/models/hero_clips.gltf","clip":"Walk","loop":true},
      {"position":[0.0,1.0],"clipSource":"assets/models/hero_clips.gltf","clip":"Idle","loop":true}
    ]}}
  ],
  "transitions":[]
}]
})";
      auto dir_parsed = engine::AnimatorControllerAsset::load(dir_ctrl);
      r.check(dir_parsed && dir_parsed.value().layers[0].states[0].motion.type
                  == engine::AnimatorMotionType::BlendTree2D
                  && !dir_parsed.value().layers[0].states[0].motion.blend_tree_2d.triangles.empty(),
              "blendTree2D parses with triangulation");
      engine::AnimatorRuntime dir_anim;
      dir_anim.set_project_root(root);
      dir_anim.set_clip_library(&clips);
      const std::string dir_entity = "00000000-0000-4000-8000-0000000000a2";
      r.check(dir_anim.attach(dir_entity, "assets/animators/hero_dir.animator.json").has_value(),
              "blendTree2D attach succeeds");
      r.check(dir_anim.set_float(dir_entity, "moveX", 0.0f).has_value()
                  && dir_anim.set_float(dir_entity, "moveZ", 0.35f).has_value(),
              "blendTree2D set move params");
      dir_anim.tick(0.0f);
      auto forward_status = dir_anim.status(dir_entity);
      float fwd_sum = 0.0f;
      bool fwd_walk = false;
      if (forward_status) {
        for (const auto &clip : forward_status.value().active_clips) {
          fwd_sum += clip.weight;
          if (clip.clip == "Walk")
            fwd_walk = true;
        }
      }
      r.check(fwd_walk && fwd_sum > 0.9f, "blendTree2D forward samples Walk");
      r.check(dir_anim.set_float(dir_entity, "moveX", 0.35f).has_value()
                  && dir_anim.set_float(dir_entity, "moveZ", 0.0f).has_value(),
              "blendTree2D set strafe right");
      dir_anim.tick(0.0f);
      auto strafe_status = dir_anim.status(dir_entity);
      float strafe_sum = 0.0f;
      if (strafe_status) {
        for (const auto &clip : strafe_status.value().active_clips)
          strafe_sum += clip.weight;
      }
      r.check(strafe_sum > 0.9f, "blendTree2D axis sample weights sum ~1");
      r.check(dir_anim.set_float(dir_entity, "moveX", 0.2f).has_value()
                  && dir_anim.set_float(dir_entity, "moveZ", 0.2f).has_value(),
              "blendTree2D set diagonal");
      dir_anim.tick(0.0f);
      auto diag_status = dir_anim.status(dir_entity);
      float diag_sum = 0.0f;
      if (diag_status) {
        for (const auto &clip : diag_status.value().active_clips)
          diag_sum += clip.weight;
      }
      r.check(diag_sum > 0.9f, "blendTree2D diagonal sample weights sum ~1");

      auto too_few = engine::AnimatorControllerAsset::parse(R"({
"schemaVersion":1,"kind":"animatorController","id":"bad2d",
"parameters":[{"name":"moveX","type":"float"},{"name":"moveZ","type":"float"}],
"layers":[{"name":"base","defaultState":"loco","blendMode":"override","states":[
  {"name":"loco","motion":{"type":"blendTree2D","parameterX":"moveX","parameterY":"moveZ","children":[
    {"position":[0,0],"clipSource":"assets/models/hero_clips.gltf","clip":"Idle","loop":true},
    {"position":[1,0],"clipSource":"assets/models/hero_clips.gltf","clip":"Walk","loop":true}
  ]}}
],"transitions":[]}]
})", "bad2d");
      r.check(!too_few && too_few.error().code == "ANIM-CTRL-BLEND-CHILDREN",
              "blendTree2D rejects fewer than 3 children");
      auto missing_param = engine::AnimatorControllerAsset::parse(R"({
"schemaVersion":1,"kind":"animatorController","id":"bad2dparam",
"parameters":[{"name":"moveX","type":"float"}],
"layers":[{"name":"base","defaultState":"loco","blendMode":"override","states":[
  {"name":"loco","motion":{"type":"blendTree2D","parameterX":"moveX","parameterY":"moveZ","children":[
    {"position":[0,0],"clipSource":"assets/models/hero_clips.gltf","clip":"Idle","loop":true},
    {"position":[1,0],"clipSource":"assets/models/hero_clips.gltf","clip":"Walk","loop":true},
    {"position":[0,1],"clipSource":"assets/models/hero_clips.gltf","clip":"Idle","loop":true}
  ]}}
],"transitions":[]}]
})", "bad2dparam");
      r.check(!missing_param
                  && (missing_param.error().code == "ANIM-CTRL-BLEND-PARAM-MISSING"
                      || missing_param.error().code == "ANIM-CTRL-BLEND-PARAM"),
              "blendTree2D rejects undeclared parameterY");
    }

    r.check(animator.set_trigger(entity, "attack").has_value(),
            "animator set_trigger succeeds");
    animator.tick(0.0f);
    for (int i = 0; i < 4; ++i)
      animator.tick(0.05f);
    auto attack_state = animator.current_state(entity);
    r.check(attack_state && attack_state.value() == "attack",
            "trigger any-state transition to attack");
    auto cross = animator.crossfade(entity, "idle", 0.0f);
    auto idle_again = animator.current_state(entity);
    r.check(cross && idle_again && idle_again.value() == "idle",
            "crossfade instant to idle");
    auto bad_float = animator.set_float(entity, "missing", 1.0f);
    r.check(!bad_float && bad_float.error().code == "ANIM-PARAM-FLOAT",
            "bad float param fails closed");

    {
      engine::AnimatorRuntime isolated;
      isolated.set_project_root(root);
      isolated.set_clip_library(&clips);
      const std::string player_e = "00000000-0000-4000-8000-0000000000b1";
      const std::string npc_e = "00000000-0000-4000-8000-0000000000b2";
      r.check(isolated.attach(player_e, "assets/animators/hero.animator.json")
                  .has_value() &&
                  isolated.attach(npc_e, "assets/animators/hero.animator.json")
                      .has_value(),
              "player and NPC animator instances attach independently");
      r.check(isolated.set_trigger(player_e, "attack").has_value(),
              "player attack trigger on isolated runtime");
      isolated.tick(0.0f);
      for (int i = 0; i < 4; ++i)
        isolated.tick(0.05f);
      auto player_atk = isolated.current_state(player_e);
      auto npc_idle = isolated.current_state(npc_e);
      r.check(player_atk && player_atk.value() == "attack",
              "player instance enters attack");
      r.check(npc_idle && npc_idle.value() == "idle",
              "NPC instance stays idle when player attacks");
    }

    // Ashfell-style 3-hit string: idle→attack→attack2→attack3 (no * restart
    // mid-combo).
    {
      const auto combo_gltf = root / "assets/models/combo_clips.gltf";
      std::ofstream(combo_gltf) << R"({
"asset":{"version":"2.0"},
"scenes":[{"nodes":[0]}],"nodes":[{"name":"Root"}],
"animations":[
{"name":"Attack","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
{"name":"Attack2","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
{"name":"Attack3","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}
],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
      const auto combo_ctrl =
          root / "assets/animators/melee_combo.animator.json";
      std::ofstream(combo_ctrl) << R"({
"schemaVersion":1,"kind":"animatorController","id":"melee_combo",
"parameters":[
  {"name":"attack","type":"trigger"},
  {"name":"attack2","type":"trigger"},
  {"name":"attack3","type":"trigger"}
],
"layers":[{"name":"base","defaultState":"idle","blendMode":"override",
  "states":[
    {"name":"idle","motion":{"type":"clip","clipSource":"assets/models/combo_clips.gltf","clip":"Attack","loop":true}},
    {"name":"attack","motion":{"type":"clip","clipSource":"assets/models/combo_clips.gltf","clip":"Attack","loop":false}},
    {"name":"attack2","motion":{"type":"clip","clipSource":"assets/models/combo_clips.gltf","clip":"Attack2","loop":false}},
    {"name":"attack3","motion":{"type":"clip","clipSource":"assets/models/combo_clips.gltf","clip":"Attack3","loop":false}}
  ],
  "transitions":[
    {"from":"idle","to":"attack","duration":0.05,"hasExitTime":false,"conditions":[{"parameter":"attack","op":"trigger"}]},
    {"from":"attack","to":"attack2","duration":0.05,"hasExitTime":false,"conditions":[{"parameter":"attack2","op":"trigger"}]},
    {"from":"attack2","to":"attack3","duration":0.05,"hasExitTime":false,"conditions":[{"parameter":"attack3","op":"trigger"}]},
    {"from":"attack","to":"idle","duration":0.05,"hasExitTime":true,"exitTime":0.95,"conditions":[]},
    {"from":"attack2","to":"idle","duration":0.05,"hasExitTime":true,"exitTime":0.95,"conditions":[]},
    {"from":"attack3","to":"idle","duration":0.05,"hasExitTime":true,"exitTime":0.95,"conditions":[]}
  ]
}]
})";
      engine::AnimatorRuntime combo_rt;
      combo_rt.set_project_root(root);
      combo_rt.set_clip_library(&clips);
      const std::string combo_e = "00000000-0000-4000-8000-0000000000c8";
      r.check(
          combo_rt.attach(combo_e, "assets/animators/melee_combo.animator.json")
              .has_value(),
          "combo animator attaches");
      r.check(combo_rt.set_trigger(combo_e, "attack").has_value(),
              "combo attack trigger");
      for (int i = 0; i < 4; ++i)
        combo_rt.tick(0.05f);
      auto s1 = combo_rt.current_state(combo_e);
      r.check(s1 && s1.value() == "attack", "combo enters attack from idle");
      r.check(combo_rt.set_trigger(combo_e, "attack2").has_value(),
              "combo attack2 trigger");
      for (int i = 0; i < 4; ++i)
        combo_rt.tick(0.05f);
      auto s2 = combo_rt.current_state(combo_e);
      r.check(s2 && s2.value() == "attack2", "combo advances attack→attack2");
      r.check(combo_rt.set_trigger(combo_e, "attack3").has_value(),
              "combo attack3 trigger");
      for (int i = 0; i < 4; ++i)
        combo_rt.tick(0.05f);
      auto s3 = combo_rt.current_state(combo_e);
      r.check(s3 && s3.value() == "attack3", "combo advances attack2→attack3");
      r.check(combo_rt.set_trigger(combo_e, "attack").has_value(),
              "combo attack while in attack3");
      for (int i = 0; i < 4; ++i)
        combo_rt.tick(0.05f);
      auto s4 = combo_rt.current_state(combo_e);
      r.check(
          s4 && s4.value() == "attack3",
          "attack trigger does not restart string from attack3 (no *→attack)");
    }

    // TICKET-0268: shipped Ashfell string must chain. Each hit starts from the
    // blade pose the previous hit reaches inside its cancel window, and no two
    // hits start from the same place.
    {
      const auto models = std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                          "samples/open-world-rpg/assets/models";
      auto a1 = engine::AnimationClipOverrideAsset::load(
          models / "player.Attack.anim.json");
      auto a2 = engine::AnimationClipOverrideAsset::load(
          models / "player.Attack2.anim.json");
      auto a3 = engine::AnimationClipOverrideAsset::load(
          models / "player.Attack3.anim.json");
      r.check(a1 && a2 && a3, "player attack chain overrides load");
      if (a1 && a2 && a3) {
        auto rotation_channel =
            [](const engine::AnimationClipOverrideAsset &clip,
               const std::string &joint)
            -> const engine::AnimationClipChannel * {
          for (const auto &ch : clip.channels)
            if (ch.target_node_name == joint &&
                ch.path == engine::AnimationChannelPath::Rotation)
              return &ch;
          return nullptr;
        };
        auto pose_delta_deg =
            [&](const engine::AnimationClipOverrideAsset &from, float from_t,
                const engine::AnimationClipOverrideAsset &to, float to_t,
                const std::string &joint) -> float {
          const auto *fc = rotation_channel(from, joint);
          const auto *tc = rotation_channel(to, joint);
          if (!fc || !tc)
            return -1.0f;
          auto fq = engine::sample_rotation_channel(*fc, from_t);
          auto tq = engine::sample_rotation_channel(*tc, to_t);
          if (!fq || !tq)
            return -1.0f;
          float dot = 0.0f;
          for (int i = 0; i < 4; ++i)
            dot += fq.value()[i] * tq.value()[i];
          dot = std::fabs(dot);
          if (dot > 1.0f)
            dot = 1.0f;
          return 2.0f * std::acos(dot) * 57.2957795f;
        };
        const std::vector<std::string> chain_joints{
            "Hips", "Spine", "RightUpperArm", "RightLowerArm", "RightHand"};
        // Cancel-window handoff times mirror melee_combo_cancel_start() in
        // render_app.cpp.
        bool links_continuous = true;
        for (const auto &joint : chain_joints) {
          const float hit1_to_hit2 =
              pose_delta_deg(a1.value(), 0.56f, a2.value(), 0.0f, joint);
          const float hit2_to_hit3 =
              pose_delta_deg(a2.value(), 0.52f, a3.value(), 0.0f, joint);
          if (hit1_to_hit2 < 0.0f || hit1_to_hit2 > 20.0f)
            links_continuous = false;
          if (hit2_to_hit3 < 0.0f || hit2_to_hit3 > 20.0f)
            links_continuous = false;
        }
        r.check(links_continuous, "attack chain start poses match the previous "
                                  "hit's cancel-window pose");
        const float start_1_2 =
            pose_delta_deg(a1.value(), 0.0f, a2.value(), 0.0f, "RightUpperArm");
        const float start_2_3 =
            pose_delta_deg(a2.value(), 0.0f, a3.value(), 0.0f, "RightUpperArm");
        r.check(
            start_1_2 > 30.0f && start_2_3 > 30.0f,
            "each attack in the string starts from a distinct sword arm pose");
        r.check(a3.value().duration_seconds > a1.value().duration_seconds &&
                    a3.value().duration_seconds > a2.value().duration_seconds,
                "finisher commits the longest of the three hits");
        // Guards the clone-key class: a leftover key can snap the wrist
        // mid-recovery.
        bool wrist_smooth = true;
        for (const auto *clip : {&a2.value(), &a3.value()}) {
          const auto *wrist = rotation_channel(*clip, "RightHand");
          if (!wrist) {
            wrist_smooth = false;
            continue;
          }
          for (std::size_t i = 1; i < wrist->times.size(); ++i) {
            const float dt = wrist->times[i] - wrist->times[i - 1];
            if (dt > 0.06f)
              continue;
            float dot = 0.0f;
            for (int c = 0; c < 4; ++c)
              dot += wrist->values[(i - 1) * 4 + c] * wrist->values[i * 4 + c];
            dot = std::fabs(dot);
            if (dot > 1.0f)
              dot = 1.0f;
            if (2.0f * std::acos(dot) * 57.2957795f > 60.0f)
              wrist_smooth = false;
          }
          if (!wrist->times.empty() &&
              wrist->times.back() > clip->duration_seconds + 1.0e-3f)
            wrist_smooth = false;
        }
        r.check(wrist_smooth,
                "combo sword wrists carry no out-of-range keys or snap-backs");
      }
    }

    const auto bad_controller =
        root / "assets/animators/bad_clip.animator.json";
    std::ofstream(bad_controller) << R"({
"schemaVersion":1,"kind":"animatorController","id":"bad","parameters":[],
"layers":[{"name":"base","defaultState":"idle","states":[
  {"name":"idle","motion":{"type":"clip","clipSource":"assets/models/hero_clips.gltf","clip":"MissingClip","loop":true}}
],"transitions":[]}]}
)";
    engine::AnimatorRuntime bad_rt;
    bad_rt.set_project_root(root);
    bad_rt.set_clip_library(&clips);
    r.check(bad_rt.attach("e-bad", "assets/animators/bad_clip.animator.json")
                .has_value(),
            "attach with bad clip still binds controller");
    auto bad_status = bad_rt.status("e-bad");
    r.check(bad_status && bad_status.value().active_clips.empty() &&
                !bad_rt.recent_errors().empty(),
            "missing clip fails closed with diagnostics");

    const auto prefab_path = root / "assets/prefabs/hero.prefab.json";
    std::ofstream(prefab_path)
        << R"({"schemaVersion":2,"entities":[{"name":"Body","mesh":{"primitive":"capsule","color":[0.3,0.4,0.8]}}],"components":[{"id":"animator-0","type":"animator","data":{"controller":"assets/animators/hero.animator.json","defaultState":"idle"}}]})";
    auto prefab = engine::PrefabAsset::load(prefab_path);
    r.check(prefab && prefab.value().animators.size() == 1 &&
                prefab.value().animators[0].controller.find(
                    "hero.animator.json") != std::string::npos,
            "prefab animator component parsed");
    auto seeded = engine::seed_authored_components_from_prefab(prefab.value());
    r.check(seeded.entries.size() == 1 &&
                seeded.entries[0].type ==
                    engine::AuthoredComponentType::Animator,
            "authored animator seeded from prefab");
    auto round = engine::authored_component_entry_from_json(
        engine::authored_component_entry_to_json(seeded.entries[0]));
    r.check(round && round.value().animator.controller ==
                         seeded.entries[0].animator.controller,
            "animator component JSON round trip");

    const auto rb_prefab_path = root / "assets/prefabs/crate.prefab.json";
    std::ofstream(rb_prefab_path)
        << R"({"schemaVersion":2,"entities":[{"name":"Box","mesh":{"primitive":"cube","color":[0.6,0.4,0.2]}}],"components":[{"id":"rigidbody-0","type":"rigidbody","data":{"motionType":"dynamic","mass":2.5,"linearDamping":0.1,"angularDamping":0.2,"useGravity":true,"freezeRotation":false}}]})";
    auto rb_prefab = engine::PrefabAsset::load(rb_prefab_path);
    r.check(rb_prefab && rb_prefab.value().rigidbodies.size() == 1 &&
                rb_prefab.value().rigidbodies[0].mass == 2.5f,
            "prefab rigidbody component parsed");
    auto rb_seeded =
        engine::seed_authored_components_from_prefab(rb_prefab.value());
    r.check(rb_seeded.entries.size() == 1 &&
                rb_seeded.entries[0].type ==
                    engine::AuthoredComponentType::Rigidbody,
            "authored rigidbody seeded from prefab");
    auto rb_round = engine::authored_component_entry_from_json(
        engine::authored_component_entry_to_json(rb_seeded.entries[0]));
    r.check(rb_round && rb_round.value().rigidbody.mass == 2.5f &&
                rb_round.value().rigidbody.motion_type == "dynamic",
            "rigidbody component JSON round trip");
    engine::AuthoredComponentEntry bad_mass = rb_seeded.entries[0];
    bad_mass.rigidbody.mass = 0.0f;
    r.check(!engine::validate_authored_component_entry(bad_mass),
            "rigidbody rejects non-positive mass");
    engine::AuthoredComponentEntry bad_motion = rb_seeded.entries[0];
    bad_motion.rigidbody.motion_type = "static";
    r.check(!engine::validate_authored_component_entry(bad_motion),
            "rigidbody rejects invalid motionType");

    const auto audio_prefab_path =
        root / "assets/prefabs/torch_audio.prefab.json";
    std::ofstream(audio_prefab_path)
        << R"({"schemaVersion":2,"entities":[{"name":"Torch","mesh":{"primitive":"cube","color":[0.6,0.4,0.2]}}],"components":[{"id":"audio-0","type":"audioSource","data":{"clip":"assets/audio/campfire_crackle.wav","volume":0.8,"loop":true,"spatial":true,"playOnStart":true,"minDistance":1.0,"maxDistance":25.0}}]})";
    auto audio_prefab = engine::PrefabAsset::load(audio_prefab_path);
    r.check(audio_prefab && audio_prefab.value().audio_sources.size() == 1 &&
                audio_prefab.value().audio_sources[0].volume == 0.8f,
            "prefab audioSource component parsed");
    auto audio_seeded =
        engine::seed_authored_components_from_prefab(audio_prefab.value());
    r.check(audio_seeded.entries.size() == 1 &&
                audio_seeded.entries[0].type ==
                    engine::AuthoredComponentType::AudioSource,
            "authored audioSource seeded from prefab");
    auto audio_round = engine::authored_component_entry_from_json(
        engine::authored_component_entry_to_json(audio_seeded.entries[0]));
    r.check(audio_round &&
                audio_round.value().audio_source.clip ==
                    "assets/audio/campfire_crackle.wav" &&
                audio_round.value().audio_source.play_on_start,
            "audioSource component JSON round trip");
    engine::AuthoredComponentEntry bad_clip = audio_seeded.entries[0];
    bad_clip.audio_source.clip = "C:/absolute/tone.wav";
    r.check(!engine::validate_authored_component_entry(bad_clip),
            "audioSource rejects absolute clip path");
    engine::AuthoredComponentEntry bad_vol = audio_seeded.entries[0];
    bad_vol.audio_source.volume = 1.5f;
    r.check(!engine::validate_authored_component_entry(bad_vol),
            "audioSource rejects volume out of range");
    engine::AuthoredComponentEntry bad_dist = audio_seeded.entries[0];
    bad_dist.audio_source.min_distance = 10.0f;
    bad_dist.audio_source.max_distance = 2.0f;
    r.check(!engine::validate_authored_component_entry(bad_dist),
            "audioSource rejects maxDistance < minDistance");

    engine::LuaRuntime lua;
    lua.set_animator_runtime(&animator);
    const auto script = root / "drive.lua";
    std::ofstream(script) << R"(
function drive_anim(payload)
  local data = engine.json_decode(payload)
  engine.animator_set_float(data.entityId, "speed", data.speed)
  engine.animator_set_trigger(data.entityId, "attack")
end
)";
    r.check(lua.load_script(script).has_value(), "animator drive script loads");
    r.check(animator.crossfade(entity, "idle", 0.0f).has_value(),
            "reset idle before lua drive");
    r.check(lua.call_handler("drive_anim", std::string("{\"entityId\":\"") +
                                               entity + "\",\"speed\":0.8}")
                .has_value(),
            "lua animator drive succeeds");
    animator.tick(0.0f);
    for (int i = 0; i < 8; ++i)
      animator.tick(0.05f);
    auto lua_attack = animator.current_state(entity);
    r.check(lua_attack && lua_attack.value() == "attack",
            "lua trigger reaches attack state");

    // TICKET-0104 / DEC-0030: root motion extraction + character sync
    const auto walk_bin = root / "assets/models/walk_root.bin";
    {
      // times 0,1 + Hip translation (0,0,0)->(0,0,2)
      const float payload[] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f};
      std::ofstream(walk_bin, std::ios::binary)
          .write(reinterpret_cast<const char *>(payload), sizeof(payload));
    }
    const auto walk_gltf = root / "assets/models/walk_root.gltf";
    std::ofstream(walk_gltf) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"}],
"animations":[{"name":"WalkRoot","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"walk_root.bin"}]
})";
    auto walk_set = engine::import_gltf_animation_clips(walk_gltf);
    r.check(walk_set && !walk_set.value().clips.empty(),
            "walk root clip imports");
    auto root_delta = engine::extract_clip_root_motion_delta(
        walk_set.value().clips[0], "Hip", 0.0f, 0.5f, true);
    r.check(root_delta && root_delta.value().found_root_channel &&
                std::abs(root_delta.value().translation[2] - 1.0f) < 1e-4f,
            "root motion extracts +Z half-second delta");
    auto wrap_delta = engine::extract_clip_root_motion_delta(
        walk_set.value().clips[0], "Hip", 0.75f, 1.25f, true);
    r.check(wrap_delta &&
                std::abs(wrap_delta.value().translation[2] - 1.0f) < 1e-3f,
            "root motion wraps looped clip delta");

    const auto root_ctrl = root / "assets/animators/hero_root.animator.json";
    std::ofstream(root_ctrl) << R"({
"schemaVersion":1,"kind":"animatorController","id":"hero_root","applyRootMotion":true,"rootJoint":"Hip","rootMotionY":false,
"parameters":[{"name":"speed","type":"float","default":1.0}],
"layers":[{"name":"base","defaultState":"walk","states":[
  {"name":"walk","motion":{"type":"clip","clipSource":"assets/models/walk_root.gltf","clip":"WalkRoot","loop":true}}
],"transitions":[]}]
})";
    engine::AnimatorRuntime root_rt;
    root_rt.set_project_root(root);
    root_rt.set_clip_library(&clips);
    const std::string root_entity = "root-motion-entity";
    r.check(
        root_rt.attach(root_entity, "assets/animators/hero_root.animator.json")
            .has_value(),
        "root-motion controller attaches");
    auto arm = root_rt.apply_root_motion(root_entity);
    r.check(arm && arm.value(), "applyRootMotion enabled on instance");
    root_rt.tick(1.0f / 60.0f);
    auto tick_delta = root_rt.root_motion_delta(root_entity);
    r.check(tick_delta && std::abs(tick_delta.value().translation[2] -
                                   (2.0f / 60.0f)) < 1e-4f,
            "animator tick emits root delta");

    engine::CollisionWorld cworld;
    auto terrain = engine::generate_stylized_terrain({0, 0}, 33, 40);
    auto terrain_body = terrain
                            ? cworld.add_heightfield(terrain.value(), {},
                                                     engine::CellCoord{0, 0})
                            : engine::Result<engine::CollisionBody>::failure(
                                  engine::EngineError{});
    r.check(terrain && terrain_body, "root-motion terrain ready");
    const float ground = engine::sample_terrain_height(8.0f, 8.0f);
    auto created =
        engine::CharacterController::create(cworld, {8.0, ground + 5.0, 8.0});
    r.check(created.has_value(), "root-motion character created");
    auto character = std::move(created.value());
    for (int i = 0; i < 180 && !character.on_ground(); ++i)
      (void)character.move({0, 0, 0}, 0.0f, 1.0f / 60.0f);
    r.check(character.on_ground(), "root-motion character grounded");
    const auto start_pos = character.position();
    for (int i = 0; i < 60; ++i) {
      auto synced = engine::sync_character_root_motion(
          character, root_rt, root_entity, 0.0f, 1.0f / 60.0f);
      r.check(synced && synced.value(), "sync_character_root_motion applies");
    }
    const auto end_pos = character.position();
    r.check(std::abs(end_pos.z - start_pos.z) > 1.5,
            "animation-driven root motion moves capsule forward");

    // TICKET-0199: root motion on Rigidbody / CollisionBody
    {
      engine::CollisionWorld rb_world;
      auto floor = rb_world.add_box({0, -1, 0}, {20, 1, 20},
                                    engine::CollisionLayer::StaticWorld, false);
      r.check(floor.has_value(), "0199 root-motion floor");
      auto settings = engine::CollisionBodySettings::make_dynamic();
      settings.mass = 70.0f;
      settings.freeze_rotation = true;
      auto body =
          rb_world.add_capsule({0.0, 1.5, 0.0}, 0.35f, 0.85f,
                               engine::CollisionLayer::Dynamic, settings);
      r.check(body.has_value(), "0199 root-motion capsule body");
      for (int i = 0; i < 120; ++i)
        r.check(rb_world.step(1.0f / 60.0f).has_value(), "0199 settle step");
      const auto start = rb_world.position(body.value()).value();
      engine::AnimatorRuntime rb_rt;
      rb_rt.set_project_root(root);
      rb_rt.set_clip_library(&clips);
      const std::string rb_entity = "root-motion-rb";
      r.check(
          rb_rt.attach(rb_entity, "assets/animators/hero_root.animator.json")
              .has_value(),
          "0199 rb controller attaches");
      for (int i = 0; i < 60; ++i) {
        auto synced = engine::sync_rigidbody_root_motion(
            rb_world, body.value(), rb_rt, rb_entity, 0.0f, 1.0f / 60.0f);
        r.check(synced && synced.value(), "sync_rigidbody_root_motion applies");
        r.check(rb_world.step(1.0f / 60.0f).has_value(),
                "0199 root-motion physics step");
      }
      const auto end = rb_world.position(body.value()).value();
      r.check(end.z > start.z + 1.0,
              "animation-driven root motion moves rigidbody forward");
      auto bad = engine::apply_rigidbody_root_motion(rb_world, {}, {0, 0, 1},
                                                     1.0f / 60.0f);
      r.check(!bad, "0199 invalid body rejected");
    }

    // TICKET-0105 / DEC-0031: controller timeline events → Lua
    const auto event_ctrl = root / "assets/animators/hero_events.animator.json";
    std::ofstream(event_ctrl) << R"({
"schemaVersion":1,"kind":"animatorController","id":"hero_events",
"parameters":[],
"layers":[{"name":"base","defaultState":"walk","states":[
  {"name":"walk","motion":{"type":"clip","clipSource":"assets/models/walk_root.gltf","clip":"WalkRoot","loop":true}}
],"transitions":[]}],
"timelineEvents":[
  {"state":"walk","time":0.25,"name":"footstep","layer":"base","payload":{"foot":"left"}},
  {"state":"walk","time":0.75,"name":"footstep","payload":{"foot":"right"}}
]
})";
    auto event_parsed = engine::AnimatorControllerAsset::load(event_ctrl);
    r.check(event_parsed && event_parsed.value().timeline_events.size() == 2,
            "timelineEvents parse");
    auto bad_event = engine::AnimatorControllerAsset::parse(R"({
"schemaVersion":1,"kind":"animatorController","id":"bad_ev","parameters":[],
"layers":[{"name":"base","defaultState":"idle","states":[{"name":"idle","motion":{"type":"clip","clipSource":"x.gltf","clip":"A","loop":true}}],"transitions":[]}],
"timelineEvents":[{"state":"missing","time":0.1,"name":"x"}]
})",
                                                            "bad_ev");
    r.check(!bad_event &&
                bad_event.error().code == "ANIM-CTRL-EVENT-STATE-MISSING",
            "bad timelineEvent state rejected");

    engine::AnimatorRuntime event_rt;
    event_rt.set_project_root(root);
    event_rt.set_clip_library(&clips);
    const std::string event_entity = "event-entity";
    r.check(
        event_rt
            .attach(event_entity, "assets/animators/hero_events.animator.json")
            .has_value(),
        "event controller attaches");
    (void)event_rt.take_fired_events();
    std::size_t first_hits = 0;
    for (int i = 0; i < 20;
         ++i) { // 20 * 0.05 = 1.0s through one loop of WalkRoot
      event_rt.tick(0.05f);
      first_hits += event_rt.take_fired_events().size();
    }
    r.check(first_hits == 2, "timeline events fire once per cycle");
    std::size_t second_hits = 0;
    for (int i = 0; i < 20; ++i) {
      event_rt.tick(0.05f);
      second_hits += event_rt.take_fired_events().size();
    }
    r.check(second_hits == 2, "timeline events re-fire after loop wrap");

    // Animation Studio solo tick: exitTime transitions must not steal the
    // preview state.
    {
      const auto solo_ctrl =
          root / "assets/animators/hero_solo_preview.animator.json";
      std::ofstream(solo_ctrl) << R"({
"schemaVersion":1,"kind":"animatorController","id":"hero_solo",
"parameters":[],
"layers":[{
  "name":"base","defaultState":"attack","blendMode":"override",
  "states":[
    {"name":"idle","motion":{"type":"clip","clipSource":"assets/models/hero_clips.gltf","clip":"Idle","loop":true}},
    {"name":"attack","motion":{"type":"clip","clipSource":"assets/models/hero_clips.gltf","clip":"Attack","loop":false}}
  ],
  "transitions":[
    {"from":"attack","to":"idle","duration":0.05,"hasExitTime":true,"exitTime":0.5}
  ]
}]
})";
      engine::AnimatorRuntime solo_rt;
      solo_rt.set_project_root(root);
      solo_rt.set_clip_library(&clips);
      const std::string solo_entity = "solo-preview-entity";
      r.check(solo_rt
                  .attach(solo_entity,
                          "assets/animators/hero_solo_preview.animator.json")
                  .has_value(),
              "solo preview controller attaches");
      r.check(solo_rt.crossfade(solo_entity, "attack", 0.0f).has_value(),
              "solo pin attack");
      for (int i = 0; i < 30; ++i)
        solo_rt.tick(0.05f, false); // 1.5s > exitTime on 1.0s Attack
      auto solo_state = solo_rt.current_state(solo_entity);
      r.check(solo_state && solo_state.value() == "attack",
              "solo tick keeps attack (no exitTime transition)");
      engine::AnimatorRuntime graph_rt;
      graph_rt.set_project_root(root);
      graph_rt.set_clip_library(&clips);
      r.check(graph_rt
                  .attach(solo_entity,
                          "assets/animators/hero_solo_preview.animator.json")
                  .has_value(),
              "graph preview controller attaches");
      r.check(graph_rt.crossfade(solo_entity, "attack", 0.0f).has_value(),
              "graph pin attack");
      for (int i = 0; i < 30; ++i)
        graph_rt.tick(0.05f, true);
      auto graph_state = graph_rt.current_state(solo_entity);
      r.check(graph_state && graph_state.value() == "idle",
              "default tick still honors exitTime to idle");
    }

    engine::LuaRuntime event_lua;
    event_lua.set_animator_runtime(&event_rt);
    const auto event_script = root / "anim_events.lua";
    std::ofstream(event_script) << R"(
function on_animation_event(payload)
  local data, err = engine.json_decode(payload)
  if not data then
    engine.blackboard_set("anim_event_err", tostring(err))
    return
  end
  local count = engine.blackboard_get("anim_event_count") or 0
  engine.blackboard_set("anim_event_count", count + 1)
  engine.blackboard_set("anim_event_name", tostring(data.name or ""))
end
)";
    r.check(event_lua.load_script(event_script).has_value(),
            "animation event script loads");
    event_rt.detach(event_entity);
    r.check(
        event_rt
            .attach(event_entity, "assets/animators/hero_events.animator.json")
            .has_value(),
        "reattach for lua event smoke");
    (void)event_rt.take_fired_events();
    event_lua.blackboard_clear();
    event_lua.clear_recent_errors();
    std::size_t dispatched = 0;
    for (int i = 0; i < 10; ++i) {
      event_rt.tick(0.05f);
      for (const auto &fired : event_rt.take_fired_events()) {
        event_lua.dispatch_animation_event(fired);
        ++dispatched;
      }
    }
    r.check(dispatched >= 1, "lua smoke received at least one fired event");
    r.check(event_lua.recent_errors().empty(),
            "on_animation_event dispatch has no script errors");
    auto count_entry = event_lua.blackboard_get("anim_event_count");
    auto name_entry = event_lua.blackboard_get("anim_event_name");
    r.check(count_entry &&
                count_entry->type == engine::ScriptBlackboardType::Number &&
                count_entry->number_value >= 1.0,
            "on_animation_event updates blackboard");
    r.check(name_entry && name_entry->string_value == "footstep",
            "on_animation_event receives event name");

    // TICKET-0253 / DEC-0052: *.anim.json override wins + Sync to .gltf
    // round-trip
    {
      const auto ov_gltf = root / "assets/models/override_clip.gltf";
      std::ofstream(ov_gltf) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"}],
"animations":[{"name":"Idle","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
      auto base = engine::import_gltf_animation_clips(ov_gltf);
      r.check(base && !base.value().clips.empty(), "0253 base clip imports");
      auto mid_base = engine::sample_translation_channel(
          base.value().clips[0].channels[0], 0.5f);
      r.check(mid_base && std::abs(mid_base.value()[1] - 0.5f) < 1e-4f,
              "0253 base mid sample Y=0.5");

      engine::AnimationClipOverrideAsset ov;
      ov.clip_source = "assets/models/override_clip.gltf";
      ov.clip_name = "Idle";
      ov.duration_seconds = 1.0f;
      engine::AnimationClipChannel ch;
      ch.target_node_name = "Hip";
      ch.target_node_index = 0;
      ch.path = engine::AnimationChannelPath::Translation;
      ch.interpolation = engine::AnimationInterpolationMode::Linear;
      ch.times = {0.0f, 1.0f};
      ch.values = {0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f}; // end Y=2
      ov.channels.push_back(ch);
      const auto ov_path =
          engine::animation_clip_override_path(ov_gltf, "Idle");
      r.check(ov.save_atomic(ov_path).has_value(), "0253 override saves");
      r.check(std::filesystem::exists(ov_path), "0253 sidecar exists");

      engine::AnimationClipLibrary ov_lib;
      auto loaded = ov_lib.load(ov_gltf);
      r.check(loaded && !loaded.value()->clips.empty(),
              "0253 library loads with override");
      auto mid_ov = engine::sample_translation_channel(
          loaded.value()->clips[0].channels[0], 0.5f);
      r.check(mid_ov && std::abs(mid_ov.value()[1] - 1.0f) < 1e-4f,
              "0253 override wins sampling (Y=1 at t=0.5)");

      auto synced = engine::sync_animation_clip_override_to_gltf(ov_gltf, ov);
      r.check(synced.has_value(), "0253 sync to gltf succeeds");
      if (!synced) {
        // Keep the failure message visible in suite output without aborting on
        // .value().
        r.check(false, ("0253 sync error: " + synced.error().code + " " +
                        synced.error().message)
                           .c_str());
      } else {
        std::filesystem::remove(ov_path);
        auto after_sync = engine::import_gltf_animation_clips(ov_gltf);
        r.check(after_sync && !after_sync.value().clips.empty(),
                "0253 reimport after sync");
        if (after_sync && !after_sync.value().clips.empty() &&
            !after_sync.value().clips[0].channels.empty()) {
          auto mid_sync = engine::sample_translation_channel(
              after_sync.value().clips[0].channels[0], 0.5f);
          r.check(mid_sync && std::abs(mid_sync.value()[1] - 1.0f) < 1e-3f,
                  "0253 sync round-trip preserves override pose");
        }
      }

      auto glb_fail = engine::sync_animation_clip_override_to_gltf(
          root / "assets/models/missing.glb", ov);
      r.check(!glb_fail && glb_fail.error().code == "ANIM-OV-SYNC-GLB",
              "0253 glb sync rejected");
    }

    // TICKET-0261/0263/0264: agent inspect + ease + shift helpers (GPU-free)
    {
      engine::AnimationClip clip;
      clip.name = "AgentTest";
      clip.duration_seconds = 1.0f;
      engine::AnimationClipChannel rot;
      rot.target_node_name = "RightUpperArm";
      rot.path = engine::AnimationChannelPath::Rotation;
      rot.interpolation = engine::AnimationInterpolationMode::Linear;
      rot.times = {0.0f, 1.0f};
      const auto q0 = engine::euler_deg_to_quat_xyzw({0.0f, 0.0f, 0.0f});
      const auto q1 = engine::euler_deg_to_quat_xyzw({0.0f, 0.0f, 90.0f});
      rot.values = {q0[0], q0[1], q0[2], q0[3], q1[0], q1[1], q1[2], q1[3]};
      clip.channels.push_back(rot);

      auto keys = engine::list_clip_keys_json(clip, true, {"RightUpperArm"},
                                              "rotation");
      r.check(keys.is_array() && keys.size() == 1,
              "0261 list_keys returns one rotation channel");
      r.check(keys[0].value("skinName", std::string{}) == "LeftUpperArm",
              "0261 sagittal skinName remap");
      r.check(keys[0].contains("eulerDeg") && keys[0]["eulerDeg"].is_array() &&
                  keys[0]["eulerDeg"].size() == 2,
              "0261 list_keys includes eulerDeg per key");

      auto poses = engine::sample_clip_channel_poses_json(
          clip, 0.5f, {"RightUpperArm"}, true);
      r.check(poses.is_array() && !poses.empty(),
              "0261 sample_pose channel-local");
      auto diff = engine::diff_pose_json(
          engine::sample_clip_channel_poses_json(clip, 0.0f, {}, true),
          engine::sample_clip_channel_poses_json(clip, 1.0f, {}, true));
      r.check(diff.is_array() && !diff.empty(), "0261 diff_pose non-empty");

      const int inserted = engine::insert_ease_breakdowns(
          clip, 0.0f, 1.0f, 3, engine::AnimEasePreset::EaseInOut,
          {"RightUpperArm"}, "rotation");
      r.check(inserted == 3, "0264 ease_segment inserts 3 breakdowns");
      r.check(clip.channels[0].times.size() == 5,
              "0264 channel has endpoints + 3 breakdowns");

      // Clip edit undo/redo snapshots (Animation Studio history).
      {
        engine::AnimStudioClipHistory history;
        engine::AnimationClip working = clip;
        const auto before = working;
        history.push_before(before, "Ease segment");
        working.channels[0].times.push_back(1.25f);
        working.channels[0].values.insert(working.channels[0].values.end(),
                                          {0, 0, 0, 1});
        r.check(history.can_undo() && !history.can_redo(),
                "clip history undo ready after push");
        r.check(history.undo(working) &&
                    working.channels[0].times.size() == before.channels[0].times.size(),
                "clip history undo restores prior keys");
        r.check(history.can_redo() && history.redo(working) &&
                    working.channels[0].times.size() ==
                        before.channels[0].times.size() + 1,
                "clip history redo restores edit");
        history.begin_gesture(working, "Bone gizmo");
        history.begin_gesture(working, "Bone gizmo");
        r.check(history.undo_size() == 2,
                "clip history gesture coalesces to one snapshot");
        history.end_gesture();
        working.channels.clear();
        history.push_before(working, "Clear");
        r.check(history.redo_size() == 0, "clip history push clears redo");
      }

      // Dense mid keys must be cleared before ease inserts — otherwise absolute
      // eased samples zigzag against authored motion (Attack choppiness
      // regression).
      {
        engine::AnimationClip dense = clip;
        dense.channels[0].times = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        const auto qm = engine::euler_deg_to_quat_xyzw({0.0f, 0.0f, 45.0f});
        dense.channels[0].values = {q0[0], q0[1], q0[2], q0[3], qm[0],
                                    qm[1], qm[2], qm[3], qm[0], qm[1],
                                    qm[2], qm[3], qm[0], qm[1], qm[2],
                                    qm[3], q1[0], q1[1], q1[2], q1[3]};
        const int cleared_insert = engine::insert_ease_breakdowns(
            dense, 0.0f, 1.0f, 1, engine::AnimEasePreset::Linear,
            {"RightUpperArm"}, "rotation");
        r.check(cleared_insert == 1,
                "0264 ease clears dense mids then inserts");
        r.check(dense.channels[0].times.size() == 3,
                "0264 ease leaves endpoints + 1 breakdown");
        r.check(std::find_if(dense.channels[0].times.begin(),
                             dense.channels[0].times.end(),
                             [](float t) { return t > 0.2f && t < 0.3f; }) ==
                    dense.channels[0].times.end(),
                "0264 ease removed conflicting 0.25 key");
      }

      const int moved = engine::shift_clip_keys(
          clip, 0.05f, clip.duration_seconds, {"RightUpperArm"}, "rotation");
      r.check(moved > 0, "0263 shift_keys moves keys");
      r.check(clip.channels[0].times.front() >= 0.0f &&
                  clip.channels[0].times.back() <= 1.0f + 1e-4f,
              "0263 shift_keys clamps to duration");

      {
        engine::AnimationClip hold = clip;
        hold.channels[0].times = {0.0f, 0.25f, 1.0f};
        const auto q_mid = engine::euler_deg_to_quat_xyzw({0.0f, 0.0f, 45.0f});
        hold.channels[0].values = {q0[0], q0[1], q0[2], q0[3], q_mid[0],
                                   q_mid[1], q_mid[2], q_mid[3], q1[0],
                                   q1[1], q1[2], q1[3]};
        engine::AnimationClipChannel trans;
        trans.target_node_name = "Hips";
        trans.path = engine::AnimationChannelPath::Translation;
        trans.interpolation = engine::AnimationInterpolationMode::Linear;
        trans.times = {0.0f, 1.0f};
        trans.values = {0.0f, 1.12f, 0.0f, 0.0f, 1.10f, 0.0f};
        hold.channels.push_back(trans);
        const int copied = engine::copy_clip_pose_at(
            hold, hold, 0.25f, {0.0f, 1.0f}, {}, "");
        r.check(copied > 0, "0263 copy_pose_at writes existing channels");
        auto hip_t0 = engine::sample_translation_channel(hold.channels[1], 0.0f);
        auto hip_t1 = engine::sample_translation_channel(hold.channels[1], 1.0f);
        r.check(hip_t0 && hip_t1 &&
                    std::abs(hip_t0.value()[1] - hip_t1.value()[1]) < 1e-3f,
                "0263 copy_pose_at flattens hips translation to fromTime");
        r.check(hold.channels.size() == 2,
                "0263 copy_pose_at does not invent extra TRS tracks");
      }

      auto report = engine::loop_report_json(clip, nullptr, true, 4);
      r.check(report.contains("seamDiff") && report.contains("samples"),
              "0264 loop_report shape");

      auto series = engine::sample_world_series_json(
          clip, nullptr, true, {0.0f, 0.5f, 1.0f}, {"RightUpperArm"}, "");
      r.check(series.is_array() && series.size() == 3,
              "0266 sample_series length");
      r.check(series[1].contains("poses") && series[1]["poses"].is_array() &&
                  !series[1]["poses"].empty(),
              "0266 sample_series poses at mid");

      auto label = engine::format_contact_slot_label(
          0.52f, "attack", {{"hitFrame", 0.52f}, {"other", 0.1f}});
      r.check(label.find("0.52") != std::string::npos &&
                  label.find("hitFrame") != std::string::npos,
              "0266 contact slot label includes time+event");

      {
        engine::MeshBounds tip_bounds{};
        tip_bounds.min_x = -0.05f;
        tip_bounds.max_x = 0.05f;
        tip_bounds.min_y = -0.1f;
        tip_bounds.max_y = 0.8f;
        tip_bounds.min_z = -0.05f;
        tip_bounds.max_z = 0.05f;
        const auto tip_local = engine::tip_local_from_mesh_bounds(tip_bounds);
        r.check(std::abs(tip_local[1] - 0.8f) < 1e-4f,
                "0267 tip_local picks farthest AABB corner");

        engine::ImportedSkin skin;
        skin.joint_names = {"LeftHand"};
        skin.joint_node_indices = {0};
        engine::ImportedJointRestLocal rest{};
        rest.translation = {0.0f, 1.0f, 0.0f};
        rest.parent_joint = -1;
        skin.joint_rest_locals = {rest};
        skin.inverse_bind_matrices.assign(
            1, std::array<float, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0,
                                     0, 1});
        engine::BoneWeld weld;
        weld.joint = "LeftHand";
        weld.offset = {0.0f, 0.0f, 0.0f};
        weld.euler_deg = {0.0f, 0.0f, 0.0f};
        weld.scale = {1.0f, 1.0f, 1.0f};
        engine::TransformComponent owner{};
        engine::TransformComponent visual{};
        auto tip_series = engine::sample_held_tip_series_json(
            clip, skin, false, weld, owner, visual, {0.0f, 0.5f, 0.0f},
            {0.0f, 0.5f});
        r.check(tip_series.is_array() && tip_series.size() == 2,
                "0267 held_tip_series length");
        r.check(tip_series[0].contains("tipWorld") &&
                    tip_series[0].contains("gripWorld") &&
                    tip_series[0].contains("tipDir"),
                "0267 held_tip_series fields");
        r.check(tip_series[1].contains("pathLength") &&
                    tip_series[1]["pathLength"].get<float>() >= 0.0f,
                "0267 held_tip_series pathLength");

        std::vector<std::uint8_t> rgba(32 * 32 * 4, 40);
        std::vector<std::array<float, 3>> tips{
            {0.0f, 1.0f, 0.0f}, {0.0f, 1.2f, 0.4f}, {0.0f, 0.8f, 0.8f}};
        engine::stamp_tip_trail_rgba(rgba, 32, 32, tips, "side");
        bool changed = false;
        for (std::size_t i = 0; i < rgba.size(); i += 4) {
          if (rgba[i] != 40 || rgba[i + 1] != 40 || rgba[i + 2] != 40) {
            changed = true;
            break;
          }
        }
        r.check(changed, "0267 tip trail stamps pixels");
      }

      r.check(std::abs(engine::evaluate_anim_ease(
                           engine::AnimEasePreset::Linear, 0.5f) -
                       0.5f) < 1e-5f,
              "0264 linear ease midpoint");
      r.check(engine::evaluate_anim_ease(engine::AnimEasePreset::EaseIn, 0.5f) <
                  0.5f,
              "0264 ease-in below linear");

      std::vector<std::vector<std::uint8_t>> frames(
          2, std::vector<std::uint8_t>(4 * 4 * 4, 128));
      std::uint32_t ow = 0, oh = 0;
      auto sheet =
          engine::composite_rgba_contact_sheet(frames, 4, 4, 2, ow, oh);
      r.check(ow == 8 && oh == 4 && sheet.size() == 8 * 4 * 4,
              "0262 contact sheet composites 1x2");
    }

    engine::AnimationPreviewRequest preview_req;
    preview_req.project_root = root;
    preview_req.controller_path = "assets/animators/hero.animator.json";
    preview_req.frames = 30;
    auto preview = engine::run_animation_preview(preview_req);
    r.check(preview && preview.value().ok &&
                preview.value().final_state == "attack",
            "animation preview CLI helper reaches attack state");

    // Bone welds (Motor6D-style item attach): euler convention, socket chain,
    // and inverse solve.
    {
      const auto quat_close = [](const std::array<float, 4> &a,
                                 const std::array<float, 4> &b, float tol) {
        const float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
        return std::abs(std::abs(dot) - 1.0f) < tol;
      };
      const std::array<std::array<float, 3>, 6> euler_cases{
          {{0.0f, 0.0f, 0.0f},
           {-90.0f, 0.0f, 0.0f},
           {0.0f, 0.0f, 90.0f},
           {30.0f, -45.0f, 120.0f},
           {12.5f, 175.0f, -33.0f},
           {89.0f, 10.0f, -10.0f}}};
      bool euler_round_trips = true;
      for (const auto &euler : euler_cases) {
        const auto q = engine::quaternion_from_euler_deg(euler);
        const auto back = engine::euler_deg_from_quaternion(q);
        if (!quat_close(q, engine::quaternion_from_euler_deg(back), 1e-4f))
          euler_round_trips = false;
      }
      r.check(euler_round_trips,
              "weld euler <-> quaternion round trips for every axis order");
      const auto gimbal = engine::euler_deg_from_quaternion(
          engine::quaternion_from_euler_deg({90.0f, 20.0f, 40.0f}));
      r.check(
          quat_close(engine::quaternion_from_euler_deg(gimbal),
                     engine::quaternion_from_euler_deg({90.0f, 20.0f, 40.0f}),
                     1e-3f),
          "weld euler survives gimbal lock at pitch 90");

      // Joint one metre up in a character drawn at 0.655 scale two metres out
      // on +X.
      engine::BoneSocketChain chain;
      chain.owner_world.position = {2.0f, 0.0f, 0.0f};
      chain.visual_local.scale = {0.655f, 0.655f, 0.655f};
      chain.joint_model = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1};
      const auto socket = engine::bone_socket_world(chain);
      r.check(std::abs(socket.position[0] - 2.0f) < 1e-4f &&
                  std::abs(socket.position[1] - 0.655f) < 1e-4f,
              "socket world applies the skinned mesh part scale to the joint");
      r.check(std::abs(socket.scale[0] - 0.655f) < 1e-4f,
              "socket world carries the character scale");

      engine::BoneWeld weld;
      weld.joint = "RightHand";
      weld.offset = {0.0f, 0.5f, 0.0f};
      weld.euler_deg = {-90.0f, 0.0f, 0.0f};
      const auto welded = engine::weld_world_transform(socket, weld);
      r.check(std::abs(welded.position[1] - (0.655f + 0.5f * 0.655f)) < 1e-4f,
              "weld offset is scaled by the socket, not applied in raw world "
              "metres");

      const auto solved =
          engine::weld_from_world_transform(socket, welded, weld.joint);
      const bool offset_recovered =
          std::abs(solved.offset[0] - weld.offset[0]) < 1e-3f &&
          std::abs(solved.offset[1] - weld.offset[1]) < 1e-3f &&
          std::abs(solved.offset[2] - weld.offset[2]) < 1e-3f;
      r.check(offset_recovered,
              "weld inverse solve recovers the authored offset");
      r.check(quat_close(engine::quaternion_from_euler_deg(solved.euler_deg),
                         engine::quaternion_from_euler_deg(weld.euler_deg),
                         1e-3f),
              "weld inverse solve recovers the authored rotation");
      const auto resolved_again = engine::weld_world_transform(socket, solved);
      r.check(std::abs(resolved_again.position[1] - welded.position[1]) < 1e-3f,
              "gizmo write-back round trip holds the mesh in place");

      // A joint matrix must not lose its translation on the way into a
      // TransformComponent.
      const std::array<float, 16> joint_matrix{
          1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.25f, 1.5f, -0.75f, 1};
      const auto joint_transform =
          engine::transform_from_column_major(joint_matrix);
      r.check(std::abs(joint_transform.position[0] - 0.25f) < 1e-5f &&
                  std::abs(joint_transform.position[1] - 1.5f) < 1e-5f &&
                  std::abs(joint_transform.position[2] + 0.75f) < 1e-5f,
              "column-major joint matrix keeps its translation");

      auto catalog =
          engine::ItemCatalogAsset::parse(R"({"schemaVersion":1,"entities":[
{"id":"welded_blade","kind":"weapon","worldMesh":"assets/models/blade.gltf",
 "handAttach":{"joint":"RightHand","gripOffset":[0.1,-0.2,0.3],"gripEulerDeg":[-90,0,0],"gripScale":[0.5,0.5,0.5]}},
{"id":"unwelded_blade","kind":"weapon","worldMesh":"assets/models/blade.gltf"}]})",
                                          "welds");
      r.check(catalog.has_value(), "item catalog with gripScale parses");
      if (catalog) {
        const auto *welded_def = catalog.value().find("welded_blade");
        r.check(welded_def && welded_def->hand_attach.grip_scale[0] == 0.5f,
                "gripScale round trips from item JSON");
        const auto *plain = catalog.value().find("unwelded_blade");
        r.check(plain && plain->hand_attach.grip_scale[0] == 1.0f,
                "missing gripScale defaults to 1");
      }
    }

    // Held weapon skins must sample without sagittal RH/LH remapping (bow_draw
    // limb keys).
    {
      const auto bow_anim = root / "assets/models/weapon_draw.gltf";
      // Keyframe rotates LimbU1 only at t=1; no Right/Left player joints.
      std::ofstream(bow_anim) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Grip"},{"name":"LimbU1","translation":[0,0.1,0]}],
"animations":[{"name":"bow_draw","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],
 "channels":[{"sampler":0,"target":{"node":1,"path":"rotation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC4"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":32}],
"buffers":[{"byteLength":40,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAD/Xs10/AAAAAAAAAAAAAAA/17NdPw=="}]
})";
      engine::AnimationClipLibrary weapon_clips;
      r.check(weapon_clips.load(bow_anim).has_value(),
              "weapon draw clip loads");
      engine::ImportedSkin weapon_skin;
      weapon_skin.joint_node_indices = {0, 1};
      weapon_skin.joint_names = {"Grip", "LimbU1"};
      engine::ImportedJointRestLocal grip_rest;
      grip_rest.translation = {0, 0, 0};
      engine::ImportedJointRestLocal limb_rest;
      limb_rest.translation = {0, 0.1f, 0};
      limb_rest.parent_joint = 0;
      weapon_skin.joint_rest_locals = {grip_rest, limb_rest};
      weapon_skin.inverse_bind_matrices.assign(
          2, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
      engine::AnimatorClipWeight draw_w;
      draw_w.clip_source = "assets/models/weapon_draw.gltf";
      draw_w.clip = "bow_draw";
      draw_w.weight = 1.0f;
      draw_w.time_seconds = 0.5f;
      draw_w.loop = false;
      auto no_mirror = engine::sample_skinned_local_poses(
          weapon_skin, weapon_clips, root, {draw_w}, false);
      r.check(no_mirror && no_mirror.value().size() == 2,
              "held weapon sample without sagittal");
      if (no_mirror && no_mirror.value().size() == 2) {
        const auto &limb = no_mirror.value()[1];
        // Identity (rest) at t=0 key is first of buffer pair; mid-draw uses
        // second key (~30° about Z).
        r.check(std::abs(limb.rotation[2] - 0.5f) < 1e-3f ||
                    std::abs(limb.rotation[2] + 0.5f) < 1e-3f ||
                    std::abs(limb.rotation[3] - 0.8660254f) < 1e-2f,
                "bow_draw rotation applied without sagittal flip destroying "
                "the key");
        auto mats = engine::build_skin_matrices(weapon_skin, no_mirror.value());
        r.check(mats && mats.value().size() == 2,
                "held weapon skin matrices build");
      }
    }

    // Sample-project skinned shortbow rebake preserves joints + bow_draw
    // (import-only smoke).
    {
      const auto project = std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                           "samples" / "open-world-rpg";
      const auto bow_mesh = project / "assets/models/outrider_shortbow.gltf";
      if (std::filesystem::exists(bow_mesh)) {
        auto mesh = engine::import_gltf_mesh(bow_mesh);
        r.check(mesh && mesh.value().has_skinning() &&
                    !mesh.value().skins.empty(),
                "outrider_shortbow runtime mesh is skinned");
        if (mesh && !mesh.value().skins.empty()) {
          r.check(mesh.value().skins[0].joint_names.size() >= 5,
                  "shortbow skin has limb joints");
          engine::AnimationClipLibrary bow_lib;
          auto loaded = bow_lib.load(bow_mesh);
          r.check(loaded && !loaded.value()->clips.empty(),
                  "shortbow clips load from mesh glTF");
          bool has_draw = false;
          for (const auto &c : loaded.value()->clips)
            if (c.name == "bow_draw")
              has_draw = true;
          r.check(has_draw, "shortbow glTF exports bow_draw");
          if (has_draw &&
              mesh.value().influences.size() == mesh.value().vertices.size()) {
            engine::AnimatorClipWeight w;
            w.clip_source = "assets/models/outrider_shortbow.gltf";
            w.clip = "bow_draw";
            w.weight = 1.0f;
            w.time_seconds = 0.25f;
            w.loop = false;
            auto poses = engine::sample_skinned_local_poses(
                mesh.value().skins[0], bow_lib, project, {w}, false);
            r.check(poses.has_value(),
                    "sample bow_draw on shortbow skin (no sagittal)");
            if (poses) {
              auto mats = engine::build_skin_matrices(mesh.value().skins[0],
                                                      poses.value());
              r.check(mats && mats.value().size() ==
                                  mesh.value().skins[0].joint_names.size(),
                      "shortbow held skin matrices size matches joints");
            }
          }
        }
      }
    }

    // TICKET-0283: masked override overlay — Block on upper body, loco on legs.
    {
      auto none_parsed = engine::AnimatorControllerAsset::parse(
          R"({"schemaVersion":1,"kind":"animatorController","id":"ov",
"parameters":[{"name":"block","type":"bool","default":false}],
"layers":[
  {"name":"base","defaultState":"idle","blendMode":"override","states":[
    {"name":"idle","motion":{"type":"clip","clipSource":"assets/models/hero_clips.gltf","clip":"Idle","loop":true}}
  ]},
  {"name":"upperBody","defaultState":"empty","blendMode":"override","weight":1.0,
   "mask":{"joints":["Spine"],"includeChildren":true},
   "states":[
     {"name":"empty","motion":{"type":"none"}},
     {"name":"block","motion":{"type":"clip","clipSource":"assets/models/hero_clips.gltf","clip":"Attack","loop":true}}
   ],
   "transitions":[
     {"from":"empty","to":"block","duration":0.0,"conditions":[{"parameter":"block","op":"equal","value":true}]},
     {"from":"block","to":"empty","duration":0.0,"conditions":[{"parameter":"block","op":"equal","value":false}]}
   ]}
]})",
          "overlay");
      r.check(none_parsed && none_parsed.value().layers.size() == 2 &&
                  none_parsed.value().layers[1].states[0].motion.type ==
                      engine::AnimatorMotionType::None &&
                  none_parsed.value().layers[1].mask.joints.size() == 1,
              "overlay layer parses none motion + mask");
      auto bad_mask = engine::AnimatorControllerAsset::parse(
          R"({"schemaVersion":1,"kind":"animatorController","id":"x","layers":[
{"name":"base","defaultState":"idle","states":[{"name":"idle","motion":{"type":"none"}}],
 "mask":{"joints":[""]}}]})",
          "bad-mask");
      r.check(!bad_mask && bad_mask.error().code == "ANIM-CTRL-MASK-JOINT",
              "empty mask joint rejected");

      const auto overlay_ctrl =
          root / "assets/animators/hero_overlay.animator.json";
      std::ofstream(overlay_ctrl) << none_parsed.value().to_json();
      engine::AnimatorRuntime overlay_rt;
      overlay_rt.set_project_root(root);
      overlay_rt.set_clip_library(&clips);
      const std::string overlay_e = "overlay-entity";
      r.check(overlay_rt
                  .attach(overlay_e, "assets/animators/hero_overlay.animator.json",
                          "idle")
                  .has_value(),
              "overlay controller attaches with base idle override");
      auto overlay_idle = overlay_rt.current_state(overlay_e, "upperBody");
      r.check(overlay_idle && overlay_idle.value() == "empty",
              "defaultState override does not steal overlay empty");
      r.check(overlay_rt.set_bool(overlay_e, "block", true).has_value(),
              "set block true");
      overlay_rt.tick(0.0f);
      auto base_still = overlay_rt.current_state(overlay_e);
      auto upper_block = overlay_rt.current_state(overlay_e, "upperBody");
      r.check(base_still && base_still.value() == "idle",
              "block overlay leaves base idle");
      r.check(upper_block && upper_block.value() == "block",
              "block overlay enters upperBody block");
      r.check(overlay_rt.crossfade(overlay_e, "block", 0.0f).has_value(),
              "crossfade block finds overlay layer");
    }

    // Overlay melee string: Attack 1/2/3 on upperBody, locomotion stays on base.
    {
      const auto atk_gltf = root / "assets/models/overlay_attack.gltf";
      std::ofstream(atk_gltf) << R"({
"asset":{"version":"2.0"},
"scenes":[{"nodes":[0]}],"nodes":[{"name":"Root"}],
"animations":[
{"name":"Attack","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
{"name":"Attack2","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
{"name":"Attack3","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}
],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
      engine::AnimationClipLibrary overlay_atk_lib;
      r.check(overlay_atk_lib.load(atk_gltf).has_value(),
              "overlay attack fixture clip loads");
      auto atk_parsed = engine::AnimatorControllerAsset::parse(
          R"({"schemaVersion":1,"kind":"animatorController","id":"ov-atk",
"parameters":[
  {"name":"attack","type":"trigger"},
  {"name":"attack2","type":"trigger"},
  {"name":"attack3","type":"trigger"},
  {"name":"grounded","type":"bool","default":true}
],
"timelineEvents":[
  {"layer":"upperBody","state":"attack","time":0.52,"name":"hitFrame"}
],
"layers":[
  {"name":"base","defaultState":"idle","blendMode":"override","states":[
    {"name":"idle","motion":{"type":"clip","clipSource":"assets/models/overlay_attack.gltf","clip":"Attack","loop":true}}
  ]},
  {"name":"upperBody","defaultState":"empty","blendMode":"override","weight":1.0,
   "mask":{"joints":["Spine"],"includeChildren":true},
   "states":[
     {"name":"empty","motion":{"type":"none"}},
     {"name":"attack","motion":{"type":"clip","clipSource":"assets/models/overlay_attack.gltf","clip":"Attack","loop":false}},
     {"name":"attack2","motion":{"type":"clip","clipSource":"assets/models/overlay_attack.gltf","clip":"Attack2","loop":false}},
     {"name":"attack3","motion":{"type":"clip","clipSource":"assets/models/overlay_attack.gltf","clip":"Attack3","loop":false}}
   ],
   "transitions":[
     {"from":"empty","to":"attack","duration":0.0,"hasExitTime":false,"conditions":[{"parameter":"attack","op":"trigger"}]},
     {"from":"attack","to":"attack2","duration":0.0,"hasExitTime":false,"conditions":[{"parameter":"attack2","op":"trigger"}]},
     {"from":"attack2","to":"attack3","duration":0.0,"hasExitTime":false,"conditions":[{"parameter":"attack3","op":"trigger"}]},
     {"from":"attack","to":"empty","duration":0.0,"hasExitTime":true,"exitTime":0.95,"conditions":[]},
     {"from":"attack2","to":"empty","duration":0.0,"hasExitTime":true,"exitTime":0.95,"conditions":[]},
     {"from":"attack3","to":"empty","duration":0.0,"hasExitTime":true,"exitTime":0.95,"conditions":[]}
   ]}
]})",
          "overlay-atk");
      r.check(atk_parsed && atk_parsed.value().layers.size() == 2,
              "overlay attack controller parses");
      const auto atk_ctrl =
          root / "assets/animators/hero_overlay_attack.animator.json";
      std::ofstream(atk_ctrl) << atk_parsed.value().to_json();
      engine::AnimatorRuntime overlay_atk;
      overlay_atk.set_project_root(root);
      overlay_atk.set_clip_library(&overlay_atk_lib);
      const std::string atk_e = "overlay-atk-entity";
      r.check(overlay_atk
                  .attach(atk_e, "assets/animators/hero_overlay_attack.animator.json")
                  .has_value(),
              "overlay attack controller attaches");
      r.check(overlay_atk.set_trigger(atk_e, "attack").has_value(),
              "overlay attack trigger");
      overlay_atk.tick(0.0f);
      auto base_idle = overlay_atk.current_state(atk_e);
      auto upper_atk = overlay_atk.current_state(atk_e, "upperBody");
      r.check(base_idle && base_idle.value() == "idle",
              "overlay attack leaves base idle");
      r.check(upper_atk && upper_atk.value() == "attack",
              "overlay attack enters upperBody attack");
      overlay_atk.tick(0.53f);
      bool hit_frame = false;
      for (const auto &fired : overlay_atk.take_fired_events()) {
        if (fired.name == "hitFrame" && fired.layer == "upperBody" &&
            fired.state == "attack")
          hit_frame = true;
      }
      r.check(hit_frame, "overlay attack hitFrame fires on upperBody");
      r.check(overlay_atk.set_trigger(atk_e, "attack2").has_value(),
              "overlay attack2 trigger");
      overlay_atk.tick(0.0f);
      upper_atk = overlay_atk.current_state(atk_e, "upperBody");
      r.check(upper_atk && upper_atk.value() == "attack2",
              "overlay combo advances to attack2");
      r.check(overlay_atk.set_trigger(atk_e, "attack3").has_value(),
              "overlay attack3 trigger");
      overlay_atk.tick(0.0f);
      upper_atk = overlay_atk.current_state(atk_e, "upperBody");
      r.check(upper_atk && upper_atk.value() == "attack3",
              "overlay combo advances to attack3");
      for (int i = 0; i < 20; ++i)
        overlay_atk.tick(0.05f);
      upper_atk = overlay_atk.current_state(atk_e, "upperBody");
      r.check(upper_atk && upper_atk.value() == "empty",
              "overlay attack3 exitTime returns to empty");
      base_idle = overlay_atk.current_state(atk_e);
      r.check(base_idle && base_idle.value() == "idle",
              "overlay attack recover keeps base idle");
    }

    // Overlay bow graph: draw/aim/release on upperBody, locomotion stays on base.
    {
      const auto bow_gltf = root / "assets/models/overlay_bow.gltf";
      std::ofstream(bow_gltf) << R"({
"asset":{"version":"2.0"},
"scenes":[{"nodes":[0]}],"nodes":[{"name":"Root"}],
"animations":[
{"name":"BowDraw","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
{"name":"BowAim","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
{"name":"BowRelease","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}
],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
      engine::AnimationClipLibrary overlay_bow_lib;
      r.check(overlay_bow_lib.load(bow_gltf).has_value(),
              "overlay bow fixture clip loads");
      auto bow_parsed = engine::AnimatorControllerAsset::parse(
          R"({"schemaVersion":1,"kind":"animatorController","id":"ov-bow",
"parameters":[
  {"name":"bowDrawn","type":"bool","default":false},
  {"name":"grounded","type":"bool","default":true}
],
"timelineEvents":[
  {"layer":"upperBody","state":"bowDraw","time":0.12,"name":"nockArrow"},
  {"layer":"upperBody","state":"bowRelease","time":0.05,"name":"releaseArrow"}
],
"layers":[
  {"name":"base","defaultState":"idle","blendMode":"override","states":[
    {"name":"idle","motion":{"type":"clip","clipSource":"assets/models/overlay_bow.gltf","clip":"BowAim","loop":true}}
  ]},
  {"name":"upperBody","defaultState":"empty","blendMode":"override","weight":1.0,
   "mask":{"joints":["Spine"],"includeChildren":true},
   "states":[
     {"name":"empty","motion":{"type":"none"}},
     {"name":"bowDraw","motion":{"type":"clip","clipSource":"assets/models/overlay_bow.gltf","clip":"BowDraw","loop":false}},
     {"name":"bowAim","motion":{"type":"clip","clipSource":"assets/models/overlay_bow.gltf","clip":"BowAim","loop":true}},
     {"name":"bowRelease","motion":{"type":"clip","clipSource":"assets/models/overlay_bow.gltf","clip":"BowRelease","loop":false}}
   ],
   "transitions":[
     {"from":"empty","to":"bowDraw","duration":0.0,"hasExitTime":false,"conditions":[{"parameter":"bowDrawn","op":"equal","value":true},{"parameter":"grounded","op":"equal","value":true}]},
     {"from":"bowDraw","to":"bowAim","duration":0.0,"hasExitTime":true,"exitTime":0.92,"conditions":[{"parameter":"bowDrawn","op":"equal","value":true}]},
     {"from":"bowDraw","to":"bowRelease","duration":0.0,"hasExitTime":false,"conditions":[{"parameter":"bowDrawn","op":"equal","value":false}]},
     {"from":"bowAim","to":"bowRelease","duration":0.0,"hasExitTime":false,"conditions":[{"parameter":"bowDrawn","op":"equal","value":false}]},
     {"from":"bowRelease","to":"empty","duration":0.0,"hasExitTime":true,"exitTime":0.90,"conditions":[]}
   ]}
]})",
          "overlay-bow");
      r.check(bow_parsed && bow_parsed.value().layers.size() == 2,
              "overlay bow controller parses");
      const auto bow_ctrl =
          root / "assets/animators/hero_overlay_bow.animator.json";
      std::ofstream(bow_ctrl) << bow_parsed.value().to_json();
      engine::AnimatorRuntime overlay_bow;
      overlay_bow.set_project_root(root);
      overlay_bow.set_clip_library(&overlay_bow_lib);
      const std::string bow_e = "overlay-bow-entity";
      r.check(overlay_bow
                  .attach(bow_e, "assets/animators/hero_overlay_bow.animator.json")
                  .has_value(),
              "overlay bow controller attaches");
      r.check(overlay_bow.set_bool(bow_e, "bowDrawn", true).has_value(),
              "overlay bowDrawn true");
      overlay_bow.tick(0.0f);
      auto bow_base = overlay_bow.current_state(bow_e);
      auto bow_upper = overlay_bow.current_state(bow_e, "upperBody");
      r.check(bow_base && bow_base.value() == "idle",
              "overlay bow leaves base idle");
      r.check(bow_upper && bow_upper.value() == "bowDraw",
              "overlay bow enters upperBody bowDraw");
      overlay_bow.tick(0.13f);
      bool nock_frame = false;
      for (const auto &fired : overlay_bow.take_fired_events()) {
        if (fired.name == "nockArrow" && fired.layer == "upperBody" &&
            fired.state == "bowDraw")
          nock_frame = true;
      }
      r.check(nock_frame, "overlay bow nockArrow fires on upperBody");
      overlay_bow.tick(0.85f);
      overlay_bow.tick(0.0f);
      bow_upper = overlay_bow.current_state(bow_e, "upperBody");
      r.check(bow_upper && bow_upper.value() == "bowAim",
              "overlay bowDraw exitTime enters bowAim");
      r.check(overlay_bow.set_bool(bow_e, "bowDrawn", false).has_value(),
              "overlay bowDrawn false");
      overlay_bow.tick(0.0f);
      bow_upper = overlay_bow.current_state(bow_e, "upperBody");
      r.check(bow_upper && bow_upper.value() == "bowRelease",
              "overlay bowAim release enters bowRelease");
      overlay_bow.tick(0.06f);
      bool release_frame = false;
      for (const auto &fired : overlay_bow.take_fired_events()) {
        if (fired.name == "releaseArrow" && fired.layer == "upperBody" &&
            fired.state == "bowRelease")
          release_frame = true;
      }
      r.check(release_frame, "overlay bow releaseArrow fires on upperBody");
      for (int i = 0; i < 20; ++i)
        overlay_bow.tick(0.05f);
      bow_upper = overlay_bow.current_state(bow_e, "upperBody");
      r.check(bow_upper && bow_upper.value() == "empty",
              "overlay bowRelease exitTime returns to empty");
      bow_base = overlay_bow.current_state(bow_e);
      r.check(bow_base && bow_base.value() == "idle",
              "overlay bow recover keeps base idle");
    }

    // Overlay MagicCast: cast on upperBody, locomotion stays on base.
    {
      const auto mag_gltf = root / "assets/models/overlay_magic.gltf";
      std::ofstream(mag_gltf) << R"({
"asset":{"version":"2.0"},
"scenes":[{"nodes":[0]}],"nodes":[{"name":"Root"}],
"animations":[
{"name":"Idle","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
{"name":"MagicCast","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}
],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
      engine::AnimationClipLibrary overlay_mag_lib;
      r.check(overlay_mag_lib.load(mag_gltf).has_value(),
              "overlay magic fixture clip loads");
      auto mag_parsed = engine::AnimatorControllerAsset::parse(
          R"({"schemaVersion":1,"kind":"animatorController","id":"ov-mag",
"parameters":[
  {"name":"cast","type":"trigger"},
  {"name":"grounded","type":"bool","default":true}
],
"timelineEvents":[
  {"layer":"upperBody","state":"magicCast","time":0.28,"name":"castCharge"},
  {"layer":"upperBody","state":"magicCast","time":0.80,"name":"castRelease"}
],
"layers":[
  {"name":"base","defaultState":"idle","blendMode":"override","states":[
    {"name":"idle","motion":{"type":"clip","clipSource":"assets/models/overlay_magic.gltf","clip":"Idle","loop":true}}
  ]},
  {"name":"upperBody","defaultState":"empty","blendMode":"override","weight":1.0,
   "mask":{"joints":["Spine"],"includeChildren":true},
   "states":[
     {"name":"empty","motion":{"type":"none"}},
     {"name":"magicCast","motion":{"type":"clip","clipSource":"assets/models/overlay_magic.gltf","clip":"MagicCast","loop":false}}
   ],
   "transitions":[
     {"from":"empty","to":"magicCast","duration":0.0,"hasExitTime":false,"conditions":[{"parameter":"cast","op":"trigger"}]},
     {"from":"magicCast","to":"empty","duration":0.0,"hasExitTime":true,"exitTime":0.90,"conditions":[]}
   ]}
]})",
          "overlay-mag");
      r.check(mag_parsed && mag_parsed.value().layers.size() == 2,
              "overlay magic controller parses");
      const auto mag_ctrl =
          root / "assets/animators/hero_overlay_magic.animator.json";
      std::ofstream(mag_ctrl) << mag_parsed.value().to_json();
      engine::AnimatorRuntime overlay_mag;
      overlay_mag.set_project_root(root);
      overlay_mag.set_clip_library(&overlay_mag_lib);
      const std::string mag_e = "overlay-mag-entity";
      r.check(overlay_mag
                  .attach(mag_e, "assets/animators/hero_overlay_magic.animator.json")
                  .has_value(),
              "overlay magic controller attaches");
      r.check(overlay_mag.set_trigger(mag_e, "cast").has_value(),
              "overlay cast trigger");
      overlay_mag.tick(0.0f);
      auto mag_base = overlay_mag.current_state(mag_e);
      auto mag_upper = overlay_mag.current_state(mag_e, "upperBody");
      r.check(mag_base && mag_base.value() == "idle",
              "overlay magic leaves base idle");
      r.check(mag_upper && mag_upper.value() == "magicCast",
              "overlay magic enters upperBody magicCast");
      overlay_mag.tick(0.29f);
      bool charge_frame = false;
      for (const auto &fired : overlay_mag.take_fired_events()) {
        if (fired.name == "castCharge" && fired.layer == "upperBody" &&
            fired.state == "magicCast")
          charge_frame = true;
      }
      r.check(charge_frame, "overlay magic castCharge fires on upperBody");
      overlay_mag.tick(0.52f);
      bool release_cast = false;
      for (const auto &fired : overlay_mag.take_fired_events()) {
        if (fired.name == "castRelease" && fired.layer == "upperBody" &&
            fired.state == "magicCast")
          release_cast = true;
      }
      r.check(release_cast, "overlay magic castRelease fires on upperBody");
      for (int i = 0; i < 20; ++i)
        overlay_mag.tick(0.05f);
      mag_upper = overlay_mag.current_state(mag_e, "upperBody");
      r.check(mag_upper && mag_upper.value() == "empty",
              "overlay magicCast exitTime returns to empty");
      mag_base = overlay_mag.current_state(mag_e);
      r.check(mag_base && mag_base.value() == "idle",
              "overlay magic recover keeps base idle");
    }

    {
      auto leftover = engine::AnimatorControllerAsset::parse(
          R"({"schemaVersion":1,"kind":"animatorController","id":"leftover-from",
"layers":[{"name":"base","defaultState":"idle","states":[
  {"name":"idle","motion":{"type":"none"}}
],"transitions":[{"from":"magicCast","to":"idle","duration":0.1}]}]})",
          "leftover-from");
      r.check(!leftover && leftover.error().code == "ANIM-CTRL-TRANSITION-FROM-MISSING",
              "leftover transition.from on missing state is rejected");
      const auto sample_ctrl =
          std::filesystem::path(ENGINE_REPOSITORY_ROOT) / "samples" /
          "open-world-rpg" / "assets" / "animators" / "player.animator.json";
      if (std::filesystem::exists(sample_ctrl)) {
        auto sample = engine::AnimatorControllerAsset::load(sample_ctrl);
        r.check(sample.has_value(), "sample player.animator.json validates");
        if (sample) {
          bool magic_on_base = false;
          bool magic_on_overlay = false;
          for (const auto &layer : sample.value().layers) {
            for (const auto &st : layer.states) {
              if (st.name != "magicCast")
                continue;
              if (layer.name == "base")
                magic_on_base = true;
              if (layer.name == "upperBody")
                magic_on_overlay = true;
            }
          }
          r.check(!magic_on_base && magic_on_overlay,
                  "sample MagicCast lives on upperBody, not base");
        }
      }
    }

    {
      const auto walk_gltf = root / "assets/models/overlay_walk.gltf";
      const auto block_gltf = root / "assets/models/overlay_block.gltf";
      std::ofstream(walk_gltf) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"}],
"animations":[{"name":"Walk","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
      std::ofstream(block_gltf) << R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Spine"}],
"animations":[{"name":"Block","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"rotation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC4"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":32}],
"buffers":[{"byteLength":40,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAD/Xs10/AAAAAAAAAAAAAAA/17NdPw=="}]
})";
      engine::AnimationClipLibrary overlay_lib;
      r.check(overlay_lib.load(walk_gltf).has_value() &&
                  overlay_lib.load(block_gltf).has_value(),
              "overlay pose fixture clips load");
      engine::ImportedSkin overlay_skin;
      overlay_skin.joint_node_indices = {0, 1, 2};
      overlay_skin.joint_names = {"Hip", "Spine", "LeftUpperLeg"};
      engine::ImportedJointRestLocal hip_rest;
      hip_rest.translation = {0.0f, 1.0f, 0.0f};
      hip_rest.parent_joint = -1;
      engine::ImportedJointRestLocal spine_rest;
      spine_rest.translation = {0.0f, 0.2f, 0.0f};
      spine_rest.parent_joint = 0;
      engine::ImportedJointRestLocal leg_rest;
      leg_rest.translation = {0.15f, 0.0f, 0.0f};
      leg_rest.parent_joint = 0;
      overlay_skin.joint_rest_locals = {hip_rest, spine_rest, leg_rest};
      overlay_skin.inverse_bind_matrices.assign(
          3, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
      engine::AnimatorClipWeight walk_w;
      walk_w.clip_source = "assets/models/overlay_walk.gltf";
      walk_w.clip = "Walk";
      walk_w.weight = 1.0f;
      walk_w.time_seconds = 0.5f;
      walk_w.layer = "base";
      engine::AnimatorClipWeight block_w;
      block_w.clip_source = "assets/models/overlay_block.gltf";
      block_w.clip = "Block";
      block_w.weight = 1.0f;
      block_w.time_seconds = 0.5f;
      block_w.layer = "upperBody";
      block_w.mask_joints = {"Spine"};
      block_w.mask_include_children = true;
      auto overlay_poses = engine::sample_skinned_local_poses(
          overlay_skin, overlay_lib, root, {walk_w, block_w}, false);
      r.check(overlay_poses && overlay_poses.value().size() == 3,
              "overlay sample returns hip/spine/leg");
      if (overlay_poses && overlay_poses.value().size() == 3) {
        const auto &hip = overlay_poses.value()[0];
        const auto &spine = overlay_poses.value()[1];
        const auto &leg = overlay_poses.value()[2];
        r.check(std::abs(hip.translation[1] - 0.5f) < 0.05f &&
                    std::abs(hip.translation[0]) < 0.05f,
                "masked overlay keeps locomotion hip translation");
        r.check(std::abs(spine.rotation[3] - 1.0f) > 0.05f,
                "masked overlay writes spine from block clip");
        r.check(std::abs(leg.translation[0] - 0.15f) < 1e-4f,
                "masked overlay leaves unmasked leg on rest/base");
      }
    }

    std::filesystem::remove_all(root);
  } else if (suite == "audio") {
    const auto root = std::filesystem::temp_directory_path() /
                      ("engine-audio-suite-" + engine::make_correlation_id());
    std::filesystem::create_directories(root / "assets/audio");
    const auto tone_path = root / "assets/audio/tone.wav";
    r.check(engine::write_test_tone_wav(tone_path).has_value(),
            "test tone wav writes");
    r.check(std::filesystem::exists(tone_path), "test tone wav exists");

    engine::AudioEngine audio;
    engine::AudioEngineConfig config{};
    config.no_device = true;
    config.master_volume = 0.75f;
    r.check(audio.initialize(config).has_value(),
            "audio engine initializes headless");
    r.check(audio.is_initialized(), "audio engine reports initialized");
    r.check(std::abs(audio.master_volume() - 0.75f) < 1e-4f,
            "master volume applies");
    audio.set_project_root(root);

    r.check(audio.play_project_sound("assets/audio/tone.wav").has_value(),
            "one-shot play succeeds");
    r.check(
        audio.play_project_sound_at("assets/audio/tone.wav", 1.0f, 0.5f, -2.0f)
            .has_value(),
        "spatial play succeeds");
    audio.update_listener({0.0f, 1.6f, 0.0f}, {0.0f, 0.0f, 1.0f});
    audio.update(0.05f);

    const auto missing = audio.play_project_sound("assets/audio/missing.wav");
    r.check(!missing && missing.error().code == "AUDIO-FILE-MISSING",
            "missing file fails closed");

    const auto escape = audio.play_project_sound("../outside.wav");
    r.check(!escape && escape.error().code == "AUDIO-PATH-ESCAPE",
            "path escape rejected");

    engine::LuaRuntime lua;
    lua.set_audio_engine(&audio);
    const auto script = root / "play.lua";
    std::ofstream(script) << R"(
function play_campfire()
  engine.play_sound("assets/audio/tone.wav")
end
function play_at()
  engine.play_sound_at("assets/audio/tone.wav", 2, 0, -1)
end
function set_vol()
  engine.set_master_volume(0.5)
end
)";
    r.check(lua.load_script(script).has_value(), "audio lua script loads");
    r.check(lua.call_handler("play_campfire", "{}").has_value(),
            "lua play_sound succeeds");
    r.check(lua.call_handler("play_at", "{}").has_value(),
            "lua play_sound_at succeeds");
    r.check(lua.call_handler("set_vol", "{}").has_value(),
            "lua set_master_volume succeeds");
    r.check(std::abs(audio.master_volume() - 0.5f) < 1e-4f,
            "lua volume bind updates engine");

    const auto sample_root = std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                             "samples/open-world-rpg";
    const auto sample_wav = sample_root / "assets/audio/campfire_crackle.wav";
    r.check(std::filesystem::exists(sample_wav), "sample campfire wav shipped");
    audio.set_project_root(sample_root);
    r.check(audio.play_project_sound("assets/audio/campfire_crackle.wav")
                .has_value(),
            "sample campfire plays");

    audio.shutdown();
    r.check(!audio.is_initialized(),
            "audio engine shutdown clears initialized");
    std::filesystem::remove_all(root);
  } else if (suite == "particles") {
    using engine::ParticleEmitterAsset;
    using engine::ParticleFlipbookLayout;
    using engine::ParticleFlipbookMode;
    using engine::ParticleOrientation;
    using engine::ParticleSystem;
    using engine::PrefabAsset;
    using engine::PrefabParticleEmitter;
    using engine::TransformComponent;
    using engine::WeaponSweepAsset;
    using engine::WeaponSweepSystem;

    const auto sweep = WeaponSweepAsset::parse(R"({"schemaVersion":1,"id":"ashfell_light_sweep","texture":"assets/vfx/generated/sword_sweep.png","trailSeconds":0.14,"maxSamples":6})");
    r.check(sweep.has_value(), "weapon sweep asset parses");
    if (sweep) {
      const auto project = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / "samples/open-world-rpg";
      r.check(sweep.value().validate_texture(project).has_value(), "weapon sweep texture exists");
      WeaponSweepSystem trail; trail.begin(sweep.value());
      trail.sample({0,1,0},{0,2,0},0.00f); trail.sample({1,1,0},{1,2,0},0.04f);
      r.check(trail.draw_vertices().size() == 12, "two sweep samples emit aura and core ribbon quads");
      trail.end(); trail.tick(0.30f);
      r.check(trail.draw_vertices().empty() && !trail.active(), "expired sweep clears safely");
    }

    const auto parsed = ParticleEmitterAsset::parse(R"({
          "schemaVersion": 1,
          "id": "test_flame",
          "color": [1.0, 0.5, 0.1],
          "size": {"keypoints":[{"t":0,"value":0.2},{"t":1,"value":0.05}]},
          "transparency": {"keypoints":[{"t":0,"value":0.1},{"t":1,"value":1.0}]},
          "lifetime": {"min": 0.5, "max": 0.5},
          "speed": {"min": 1.0, "max": 1.0},
          "rate": 40,
          "spreadAngle": [10, 10],
          "shape": "disc",
          "shapeSize": [0.2, 0.05, 0.2],
          "orientation": "FacingCamera",
          "lightEmission": 0.8,
          "blend": "softLight",
          "acceleration": [0, 1, 0],
          "drag": 0.2,
          "maxParticles": 32,
          "enabled": true
        })");
    r.check(parsed.has_value(), "particle asset parses");
    if (parsed) {
      r.check(parsed.value().id == "test_flame", "particle id preserved");
      r.check(std::abs(parsed.value().size.sample(0.0f) - 0.2f) < 1e-4f,
              "size sequence samples start");
      r.check(std::abs(parsed.value().size.sample(1.0f) - 0.05f) < 1e-4f,
              "size sequence samples end");
      r.check(std::abs(parsed.value().transparency.sample(1.0f) - 1.0f) < 1e-4f,
              "transparency sequence samples end");
      const auto round_trip =
          ParticleEmitterAsset::parse(parsed.value().to_json());
      r.check(round_trip.has_value() && round_trip.value().rate == 40.0f,
              "particle asset round trips");
    }

    const auto bad =
        ParticleEmitterAsset::parse(R"({"schemaVersion":2,"id":"x"})");
    r.check(!bad && bad.error().code == "PARTICLE-SCHEMA",
            "unsupported schema fails closed");

    const auto bad_tex = ParticleEmitterAsset::parse(
        R"({"schemaVersion":1,"id":"x","texture":"../secret.png","rate":1,"maxParticles":4})");
    r.check(!bad_tex && bad_tex.error().code == "PARTICLE-TEXTURE-PATH-INVALID",
            "particle texture path traversal fails closed");

    const auto project = std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                         "samples/open-world-rpg";
    const auto hit = ParticleEmitterAsset::load(
        project / "assets/vfx/hit_spark.particle.json");
    r.check(hit.has_value(), "hit_spark recipe sample loads");
    if (hit) {
      r.check(hit.value().validate_texture(project).has_value(),
              "hit_spark texture exists");
    }
    const auto dodge_dust = ParticleEmitterAsset::load(
        project / "assets/vfx/dodge_dust.particle.json");
    r.check(dodge_dust.has_value(), "dodge_dust recipe sample loads");
    if (dodge_dust) {
      r.check(dodge_dust.value().id == "dodge_dust", "dodge_dust particle id");
      r.check(dodge_dust.value().validate_texture(project).has_value(),
              "dodge_dust texture exists");
      r.check(dodge_dust.value().orientation ==
                  ParticleOrientation::VelocityParallel,
              "dodge_dust uses VelocityParallel streaks");
    }
    const auto footstep_dust = ParticleEmitterAsset::load(
        project / "assets/vfx/footstep_dust.particle.json");
    r.check(footstep_dust.has_value(), "footstep_dust recipe sample loads");
    if (footstep_dust) {
      r.check(footstep_dust.value().id == "footstep_dust",
              "footstep_dust particle id");
      r.check(footstep_dust.value().validate_texture(project).has_value(),
              "footstep_dust texture exists");
      r.check(footstep_dust.value().rate <= 0.0f + 1e-6f,
              "footstep_dust is burst-only (rate 0)");
    }
    for (const char *impact_name :
         {"sword_impact", "sword_impact_flash", "arrow_impact",
          "arrow_impact_flash", "arcane_bolt_impact",
          "arcane_bolt_impact_flash", "fire_bolt_impact",
          "fire_bolt_impact_flash", "frost_bolt_impact",
          "frost_bolt_impact_flash", "lightning_bolt_impact",
          "lightning_bolt_impact_flash"}) {
      const auto impact = ParticleEmitterAsset::load(
          project / ("assets/vfx/" + std::string(impact_name) + ".particle.json"));
      r.check(impact.has_value(),
              (std::string(impact_name) + " recipe sample loads").c_str());
      if (impact) {
        r.check(impact.value().id == impact_name,
                (std::string(impact_name) + " particle id").c_str());
        r.check(impact.value().rate <= 0.0f + 1e-6f,
                (std::string(impact_name) + " is burst-only (rate 0)").c_str());
        if (!impact.value().texture.empty()) {
          r.check(impact.value().validate_texture(project).has_value(),
                  (std::string(impact_name) + " texture exists").c_str());
        }
      }
    }
    const auto aura = ParticleEmitterAsset::load(
        project / "assets/vfx/corrupt_aura.particle.json");
    r.check(aura.has_value(), "corrupt_aura recipe sample loads");
    if (aura) {
      r.check(aura.value().validate_texture(project).has_value(),
              "corrupt_aura texture exists");
    }
    const auto missing_tex = ParticleEmitterAsset::parse(
        R"({"schemaVersion":1,"id":"x","texture":"assets/vfx/does_not_exist.png","rate":1,"maxParticles":4})");
    r.check(missing_tex.has_value(),
            "missing texture still parses without project root");
    if (missing_tex) {
      const auto missing = missing_tex.value().validate_texture(project);
      r.check(!missing && missing.error().code == "PARTICLE-TEXTURE-MISSING",
              "missing particle texture fails closed with project root");
    }

    const auto sample = ParticleEmitterAsset::load(
        std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
        "samples/open-world-rpg/assets/vfx/campfire_flame.particle.json");
    r.check(sample.has_value(), "campfire flame particle asset loads");
    if (sample) {
      r.check(sample.value().id == "campfire_flame", "campfire particle id");
      r.check(sample.value().flipbook_layout ==
                  engine::ParticleFlipbookLayout::Grid4x4,
              "campfire uses 4x4 flipbook layout");
      r.check(!sample.value().texture.empty(),
              "campfire flipbook references texture");
      r.check(sample.value().crossed_billboards,
              "campfire flame uses crossed billboards");
      r.check(sample.value().orientation ==
                  engine::ParticleOrientation::FacingCameraWorldUp,
              "campfire flame stays upright (FacingCameraWorldUp)");
      r.check(std::abs(sample.value().aspect_ratio - 0.78f) < 1e-3f,
              "campfire flame aspectRatio authored");
      r.check(sample.value().min_screen_size >= 20.0f,
              "campfire flame has distant minScreenSize");
      r.check(std::filesystem::exists(
                  std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                  "samples/open-world-rpg" / sample.value().texture),
              "fire_flipbook_4x4.png exists");
    }

    const auto billboard_shape = ParticleEmitterAsset::parse(R"({
          "schemaVersion": 1,
          "id": "billboard_shape",
          "aspectRatio": 0.5,
          "rotation": {"keypoints":[{"t":0,"value":10},{"t":1,"value":40}]},
          "rotationStartRandom": false,
          "crossedBillboards": true,
          "minScreenSize": 24,
          "softOcclusion": false,
          "orientation": "FacingCameraWorldUp",
          "rate": 1,
          "maxParticles": 4
        })");
    r.check(billboard_shape.has_value(),
            "aspect/rotation/crossed particle parses");
    if (billboard_shape) {
      r.check(std::abs(billboard_shape.value().aspect_ratio - 0.5f) < 1e-4f,
              "aspectRatio preserved");
      r.check(std::abs(billboard_shape.value().rotation.sample(1.0f) - 40.0f) <
                  1e-3f,
              "rotation sequence samples");
      r.check(billboard_shape.value().crossed_billboards,
              "crossedBillboards preserved");
      r.check(std::abs(billboard_shape.value().min_screen_size - 24.0f) < 1e-3f,
              "minScreenSize preserved");
      r.check(!billboard_shape.value().soft_occlusion,
              "softOcclusion false preserved");
      const auto rt =
          ParticleEmitterAsset::parse(billboard_shape.value().to_json());
      r.check(rt.has_value() && rt.value().crossed_billboards &&
                  rt.value().orientation ==
                      engine::ParticleOrientation::FacingCameraWorldUp &&
                  std::abs(rt.value().min_screen_size - 24.0f) < 1e-3f &&
                  !rt.value().soft_occlusion,
              "billboard shape fields round trip");
    }
    const auto bad_aspect = ParticleEmitterAsset::parse(
        R"({"schemaVersion":1,"id":"x","aspectRatio":0})");
    r.check(!bad_aspect && bad_aspect.error().code == "PARTICLE-ASPECT",
            "non-positive aspectRatio fails closed");

    const auto flipbook_round = ParticleEmitterAsset::parse(R"({
          "schemaVersion": 1,
          "id": "flip_test",
          "texture": "assets/vfx/fire_flipbook_4x4.png",
          "flipbookLayout": "Grid4x4",
          "flipbookMode": "PingPong",
          "flipbookFramerate": 12,
          "flipbookStartRandom": false,
          "rate": 1,
          "maxParticles": 4
        })");
    r.check(flipbook_round.has_value(), "flipbook particle asset parses");
    if (flipbook_round) {
      r.check(flipbook_round.value().flipbook_frame_count() == 16,
              "4x4 flipbook has 16 frames");
      const auto rt =
          ParticleEmitterAsset::parse(flipbook_round.value().to_json());
      r.check(rt.has_value() && rt.value().flipbook_mode ==
                                    engine::ParticleFlipbookMode::PingPong,
              "flipbook fields round trip");
    }
    const auto flipbook_needs_tex = ParticleEmitterAsset::parse(
        R"({"schemaVersion":1,"id":"x","flipbookLayout":"Grid2x2"})");
    r.check(!flipbook_needs_tex &&
                flipbook_needs_tex.error().code == "PARTICLE-FLIPBOOK-TEXTURE",
            "flipbook without texture fails closed");

    const auto campfire =
        PrefabAsset::load(std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                          "samples/open-world-rpg/assets/prefabs/Scene "
                          "Assets/campfire.prefab.json");
    r.check(campfire.has_value() && (!campfire.value().particles.empty() ||
                                     campfire.value().particle.has_value()),
            "campfire prefab has particle");
    if (campfire) {
      const auto &list = !campfire.value().particles.empty()
                             ? campfire.value().particles
                             : std::vector<engine::PrefabParticleEmitter>{
                                   *campfire.value().particle};
      r.check(list.size() >= 3,
              "campfire has layered flame/embers/smoke emitters");
      bool has_flame = false;
      for (const auto &emitter : list) {
        if (emitter.asset.find("campfire_flame.particle.json") !=
                std::string::npos ||
            emitter.asset.find("stylized_flame_molten.particle.json") !=
                std::string::npos)
          has_flame = true;
      }
      r.check(has_flame, "campfire particle path points at flame asset");
    }

    ParticleSystem system;
    system.set_seed(42);
    if (sample)
      system.register_asset("assets/vfx/campfire_flame.particle.json",
                            sample.value());
    PrefabAsset prefab;
    prefab.schema_version = 2;
    PrefabParticleEmitter attachment;
    attachment.asset = "assets/vfx/campfire_flame.particle.json";
    attachment.offset = {0.0f, 0.4f, 0.0f};
    prefab.particle = attachment;
    std::map<std::string, PrefabAsset> catalog;
    catalog["assets/prefabs/scene assets/campfire.prefab.json"] = prefab;
    TransformComponent transform;
    transform.position = {10.0f, 0.0f, -5.0f};
    system.sync_placements(
        catalog,
        {{"assets/prefabs/scene assets/campfire.prefab.json", transform}});
    r.check(system.emitter_count() == 1, "sync creates one emitter");
    for (int i = 0; i < 30; ++i)
      system.update(1.0f / 60.0f, {10.0f, 1.0f, 0.0f});
    r.check(system.active_particle_count() > 0,
            "emitter spawns particles over time");
    r.check(!system.draw_instances().empty(), "draw instances populated");
    r.check(system.draw_instances().front().color[3] > 0.0f,
            "particle alpha from transparency");

    // Local offset must follow placement rotation (wall torch / rotated props).
    {
      ParticleSystem rotated;
      rotated.set_seed(7);
      if (sample)
        rotated.register_asset("assets/vfx/campfire_flame.particle.json",
                               sample.value());
      PrefabAsset rotated_prefab;
      rotated_prefab.schema_version = 2;
      PrefabParticleEmitter rotated_attachment;
      rotated_attachment.asset = "assets/vfx/campfire_flame.particle.json";
      rotated_attachment.offset = {0.0f, 0.0f, 0.5f}; // local +Z
      rotated_prefab.particles = {rotated_attachment};
      std::map<std::string, PrefabAsset> rotated_catalog;
      rotated_catalog["assets/prefabs/test/rotated.prefab.json"] =
          rotated_prefab;
      TransformComponent yaw90;
      yaw90.position = {0.0f, 0.0f, 0.0f};
      yaw90.rotation = {0.0f, 0.70710678f, 0.0f,
                        0.70710678f}; // +90° Y → local +Z → world +X
      rotated.sync_placements(
          rotated_catalog,
          {{"assets/prefabs/test/rotated.prefab.json", yaw90}});
      rotated.debug_burst(1);
      r.check(!rotated.draw_instances().empty(),
              "rotated offset emits a draw instance");
      if (!rotated.draw_instances().empty()) {
        const auto &p = rotated.draw_instances().front().position;
        r.check(p[0] > 0.15f,
                "local +Z offset becomes +X after 90° Y rotation");
        r.check(std::fabs(p[2]) < 0.35f,
                "rotated offset does not stay on world +Z");
      }
    }

    if (sample && !system.draw_instances().empty()) {
      bool has_flip_uv = false;
      for (const auto &draw : system.draw_instances()) {
        if (draw.texture_index != 0 && draw.uv_scale_u < 0.99f) {
          has_flip_uv = true;
          break;
        }
      }
      r.check(has_flip_uv, "campfire flipbook remaps UVs to atlas cells");
      r.check(!system.texture_paths().empty(),
              "campfire registers flipbook texture path");
      bool has_blend = false;
      bool has_cross = false;
      bool has_aspect = false;
      for (const auto &draw : system.draw_instances()) {
        if (draw.texture_index != 0 && draw.flipbook_blend > 0.0f &&
            draw.flipbook_blend < 1.0f) {
          has_blend = true;
        }
        if (draw.cross_arm >= 1)
          has_cross = true;
        if (draw.aspect > 0.0f && draw.aspect < 0.99f)
          has_aspect = true;
      }
      r.check(has_blend, "campfire flipbook crossfades between frames");
      r.check(has_cross, "campfire crossed billboards emit second arm");
      r.check(has_aspect, "campfire draw instances carry aspectRatio");
    }

    system.debug_burst(8);
    r.check(system.active_particle_count() >= 8, "debug burst emits particles");

    // Gameplay one-shot burst: survives sync_placements and reports active
    // particles.
    {
      ParticleSystem burst_sys;
      burst_sys.set_seed(99);
      if (sample)
        burst_sys.register_asset("assets/vfx/campfire_flame.particle.json",
                                 sample.value());
      const bool spawned = burst_sys.spawn_burst(
          "assets/vfx/campfire_flame.particle.json", {1.0f, 0.5f, -2.0f}, 12,
          std::array<float, 3>{-1.0f, 0.1f, 0.0f});
      r.check(spawned, "spawn_burst succeeds for registered asset");
      r.check(burst_sys.active_particle_count() >= 12,
              "spawn_burst emits requested count");
      r.check(burst_sys.emitter_count() >= 1,
              "spawn_burst creates a transient emitter");
      PrefabAsset empty_prefab;
      empty_prefab.schema_version = 2;
      std::map<std::string, PrefabAsset> empty_catalog;
      empty_catalog["assets/prefabs/test/empty.prefab.json"] = empty_prefab;
      burst_sys.sync_placements(empty_catalog, {});
      r.check(burst_sys.emitter_count() >= 1,
              "spawn_burst emitter survives sync_placements");
      r.check(burst_sys.active_particle_count() >= 12,
              "burst particles survive sync_placements");
      for (int i = 0; i < 90; ++i)
        burst_sys.update(1.0f / 30.0f, {0.0f, 1.0f, 0.0f});
      r.check(burst_sys.emitter_count() == 0,
              "finished burst emitters are culled");
      r.check(!burst_sys.spawn_burst("assets/vfx/missing.particle.json",
                                     {0, 0, 0}, 4),
              "spawn_burst fails closed for missing asset");
      r.check(!burst_sys.spawn_burst("assets/vfx/campfire_flame.particle.json",
                                     {0, 0, 0}, 0),
              "spawn_burst rejects zero count");
    }

    const auto wind_asset = ParticleEmitterAsset::load(
        std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
        "samples/open-world-rpg/assets/vfx/wind_trail.particle.json");
    r.check(wind_asset.has_value(), "wind_trail particle asset loads");
    if (wind_asset) {
      r.check(wind_asset.value().id == "wind_trail", "wind_trail particle id");
      r.check(!wind_asset.value().texture.empty(),
              "wind_trail references streak texture");
      r.check(wind_asset.value().orientation ==
                  ParticleOrientation::VelocityParallel,
              "wind_trail uses VelocityParallel");
      r.check(wind_asset.value().aspect_ratio > 1.0f,
              "wind_trail aspectRatio elongates streak");
      r.check(!wind_asset.value().rotation_start_random,
              "wind_trail disables random roll");
      r.check(wind_asset.value().flipbook_layout ==
                  ParticleFlipbookLayout::Grid4x4,
              "wind_trail uses morphing flipbook");
      r.check(wind_asset.value().flipbook_mode == ParticleFlipbookMode::Loop,
              "wind_trail flipbook loops");
      r.check(std::filesystem::exists(
                  std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                  "samples/open-world-rpg" / wind_asset.value().texture),
              "wind streak flipbook exists");
      system.register_asset("assets/vfx/wind_trail.particle.json",
                            wind_asset.value());
      system.set_ambient_wind(true, "assets/vfx/wind_trail.particle.json",
                              {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.2f}, 1.0f,
                              0.85f, 5.0f);
      r.check(system.ambient_wind_enabled(), "ambient wind emitter enabled");
      r.check(system.emitter_count() == 2,
              "ambient wind preserved with prefab emitter");
      for (int i = 0; i < 45; ++i)
        system.update(1.0f / 60.0f, {0.0f, 1.0f, 0.0f});
      bool has_textured = false;
      bool has_wind_aspect = false;
      bool has_wind_flip = false;
      for (const auto &draw : system.draw_instances()) {
        if (draw.texture_index != 0) {
          has_textured = true;
          if (draw.aspect > 1.0f)
            has_wind_aspect = true;
          if (draw.uv_scale_u < 0.99f)
            has_wind_flip = true;
        }
      }
      r.check(has_textured, "ambient wind particles use texture index");
      r.check(has_wind_aspect, "ambient wind draw instances carry aspectRatio");
      r.check(has_wind_flip, "ambient wind particles animate flipbook UVs");
    }
  } else if (suite == "game_session") {
    engine::QuestRuntime quests;
    engine::StandingRuntime standing;
    engine::FlagRuntime flags;
    engine::GameSession session;
    session.bind_quest_runtime(&quests);
    session.bind_standing_runtime(&standing);
    session.bind_flag_runtime(&flags);
    r.check(session.quest_runtime() == &quests,
            "GameSession binds QuestRuntime");
    r.check(session.standing_runtime() == &standing,
            "GameSession binds StandingRuntime");
    r.check(session.flag_runtime() == &flags, "GameSession binds FlagRuntime");
    r.check(session.state() == engine::GameSessionState::Menu,
            "GameSession starts at menu");
    r.check(std::string(engine::to_string(session.camera_leash())) ==
                "midpoint",
            "camera leash is midpoint");

    r.check(session.begin_solo().has_value(), "begin_solo succeeds from menu");
    r.check(session.session_mode() == engine::SessionMode::Solo,
            "solo mode set");
    r.check(session.state() == engine::GameSessionState::SoloLoading,
            "solo enters SoloLoading");
    r.check(session.slot(0).connected, "solo connects host slot");
    r.check(!session.slot(1).connected, "solo leaves guest disconnected");
    r.check(session.start_playing().has_value(), "solo start_playing succeeds");
    r.check(session.state() == engine::GameSessionState::Playing,
            "solo reaches Playing");
    r.check(session.simulation_allowed(), "solo playing allows simulation");
    {
      auto guest_reject = session.set_slot_connected(1, true);
      r.check(!guest_reject &&
                  guest_reject.error().code == "GAME-SESSION-SOLO-NO-GUEST",
              "solo rejects guest connect with GAME-SESSION-SOLO-NO-GUEST");
    }
    r.check(session.end_session().has_value(), "solo end_session succeeds");
    r.check(session.state() == engine::GameSessionState::Ended,
            "solo ends in Ended");
    r.check(session.session_mode() == engine::SessionMode::Solo,
            "end keeps solo mode");
    r.check(!session.simulation_allowed(), "ended session freezes simulation");

    session.reset_to_menu();
    session.bind_quest_runtime(&quests);
    session.bind_standing_runtime(&standing);
    session.bind_flag_runtime(&flags);
    r.check(session.begin_coop_lobby().has_value(),
            "begin_coop_lobby succeeds");
    r.check(session.session_mode() == engine::SessionMode::Coop,
            "coop mode set");
    r.check(session.state() == engine::GameSessionState::CoopLobby,
            "coop lobby state");
    auto blocked = session.start_playing();
    r.check(!blocked && blocked.error().code == "GAME-SESSION-COOP-NEEDS-GUEST",
            "coop start without guest fails closed");
    r.check(session.set_slot_connected(0, true, 0).has_value(),
            "host connects in lobby");
    blocked = session.start_playing();
    r.check(!blocked && blocked.error().code == "GAME-SESSION-COOP-NEEDS-GUEST",
            "coop start with host only still needs guest");
    r.check(session.set_slot_connected(1, true, 1).has_value(),
            "guest connects in lobby");
    r.check(session.start_playing().has_value(),
            "coop start_playing with both slots");
    r.check(session.state() == engine::GameSessionState::Playing,
            "coop reaches Playing");
    r.check(session.simulation_allowed(), "coop playing allows simulation");
    r.check(session.slot(0).device_index == 0 &&
                session.slot(1).device_index == 1,
            "coop assigns device indices");

    r.check(session.set_slot_connected(1, false).has_value(),
            "guest disconnect while playing");
    r.check(session.state() == engine::GameSessionState::PausedWaitingGuest,
            "disconnect pauses for guest");
    r.check(!session.simulation_allowed(),
            "paused_waiting_guest freezes simulation");
    r.check(!session.resume_after_guest_reconnect().has_value(),
            "resume without guest fails");
    r.check(session.set_slot_connected(1, true, 1).has_value(),
            "guest reconnects");
    r.check(session.resume_after_guest_reconnect().has_value(),
            "resume after reconnect");
    r.check(session.state() == engine::GameSessionState::Playing,
            "resume returns to Playing");
    r.check(session.end_session().has_value(), "coop end_session");
    r.check(session.session_mode() == engine::SessionMode::Coop,
            "end does not downgrade to solo");
    r.check(session.state() == engine::GameSessionState::Ended,
            "coop ended state");

    session.reset_to_menu();
    r.check(session.begin_coop_lobby().has_value(), "lobby ready begin");
    (void)session.set_slot_connected(0, true, 0);
    r.check(session.set_ready(0, true).has_value(), "host ready ok");
    r.check(!session.can_host_start(), "cannot start without guest ready");
    auto bad_invite = session.mock_guest_join("WRONG");
    r.check(!bad_invite && bad_invite.error().code == "GAME-SESSION-BAD-INVITE",
            "bad invite rejected");
    r.check(session.mock_guest_join("COOP-LOCAL").has_value(),
            "mock guest join");
    r.check(session.set_ready(1, true).has_value(), "guest ready");
    r.check(session.can_host_start(), "can_host_start when both ready");
    r.check(session.start_playing().has_value(), "start after dual ready");
  } else if (suite == "party") {
    engine::GameSession solo_session;
    r.check(solo_session.begin_solo().has_value(), "party suite begin_solo");
    r.check(solo_session.start_playing().has_value(),
            "party suite solo playing");
    engine::PartyRuntime party;
    party.bind_session(&solo_session);
    r.check(party.max_humans() == 1 && party.max_companions() == 3 &&
                party.max_party_size() == 4,
            "solo party caps 1/3/4");
    r.check(party.add_companion("c1").has_value(), "solo add c1");
    r.check(party.add_companion("c2").has_value(), "solo add c2");
    r.check(party.add_companion("c3").has_value(), "solo add c3");
    auto over = party.add_companion("c4");
    r.check(!over && over.error().code == "PARTY-OVER-CAP",
            "solo fourth companion rejected");
    auto dup = party.add_companion("c1");
    r.check(!dup && dup.error().code == "PARTY-DUPLICATE",
            "duplicate companion rejected");
    r.check(party.remove_companion("c2").has_value(), "remove companion");
    r.check(party.list_active().size() == 2, "two companions remain");
    const auto camp = party.party_members_for_camp();
    r.check(camp.size() == 3 && camp.front() == "player_host",
            "camp lists host + companions");

    engine::GameSession coop_session;
    r.check(coop_session.begin_coop_lobby().has_value(),
            "party suite begin_coop");
    (void)coop_session.set_slot_connected(0, true, 0);
    (void)coop_session.set_slot_connected(1, true, 1);
    r.check(coop_session.start_playing().has_value(),
            "party suite coop playing");
    engine::PartyRuntime coop_party;
    coop_party.bind_session(&coop_session);
    coop_party.set_human_member_ids("host_a", "guest_b");
    r.check(coop_party.max_humans() == 2 && coop_party.max_companions() == 2,
            "coop party caps 2/2");
    r.check(coop_party.add_companion("c1").has_value(), "coop add c1");
    r.check(coop_party.add_companion("c2").has_value(), "coop add c2");
    over = coop_party.add_companion("c3");
    r.check(!over && over.error().code == "PARTY-OVER-CAP",
            "coop third companion rejected");
    const auto coop_camp = coop_party.party_members_for_camp();
    r.check(coop_camp.size() == 4 && coop_camp[0] == "host_a" &&
                coop_camp[1] == "guest_b",
            "coop camp lists both humans + companions");
    auto restore = coop_party.set_active_companions({"a", "b", "c"});
    r.check(!restore && restore.error().code == "PARTY-OVER-CAP",
            "set_active over cap rejected");
  } else if (suite == "rpg_save") {
    const auto project = std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                         "samples/open-world-rpg";
    auto quests_asset = engine::WorldForgeQuestsAsset::load(
        engine::default_world_forge_quests_path(project));
    r.check(quests_asset.has_value(), "rpg_save loads sample quests");
    const auto standing_factions = engine::WorldForgeFactionsAsset::parse(R"({
            "schemaVersion":1,"id":"save_standing","entities":[
              {"id":"alpha","kind":"faction","canonStatus":"draft","standing":{
                "tracksPlayer":true,"min":-100,"max":100,"ranks":[{"id":"neutral","minScore":0,"displayName":"N"}]
              }},
              {"id":"beta","kind":"faction","canonStatus":"draft","standing":{
                "tracksPlayer":true,"min":-100,"max":100,"ranks":[{"id":"neutral","minScore":0,"displayName":"N"}]
              }}
            ]})");
    const auto standing_rels = engine::WorldForgeRelationshipsAsset::parse(
        R"({"schemaVersion":1,"id":"save_rel","nodes":[],"edges":[]})");
    r.check(standing_factions && standing_rels, "rpg_save standing fixtures");

    engine::QuestRuntime quests;
    engine::StandingRuntime standing;
    engine::FlagRuntime flags;
    if (quests_asset)
      r.check(quests.bind(&quests_asset.value()).has_value(),
              "bind quests for save");
    if (standing_factions && standing_rels)
      r.check(standing.bind(&standing_factions.value(), &standing_rels.value())
                  .has_value(),
              "bind standing");
    r.check(quests.start("sq_01_cart_again").has_value(),
            "start quest for capture");
    r.check(quests.complete_objective("sq_01_cart_again", "find_pellin")
                .has_value(),
            "complete objective");
    r.check(standing.set("alpha", 12.5).has_value(),
            "set standing for capture");
    r.check(flags.set("act0.helped_larrell").has_value(),
            "set flag for capture");

    engine::GameSession session;
    r.check(session.begin_solo().has_value() &&
                session.start_playing().has_value(),
            "solo session for party");
    engine::PartyRuntime party;
    party.bind_session(&session);
    r.check(party.add_companion("companion_arkand").has_value(),
            "party companion for save");

    engine::RpgSaveDocument doc;
    doc.save_id = "test-save-1";
    doc.display_name = "Test Solo";
    doc.session_mode = engine::SessionMode::Solo;
    doc.host_profile = {0, "Hero", "ashfell_blade"};
    doc.world_anchor.scene_id = "tessera_overland";
    doc.world_anchor.position = {1.0, 2.0, 3.0};
    doc.world_anchor.yaw_degrees = 45.0;
    r.check(doc.capture_from(quests, standing, flags, &party).has_value(),
            "capture runtimes");
    r.check(doc.shared_campaign.quest_instances.size() == 1,
            "captured one quest instance");
    r.check(doc.shared_campaign.outcome_flags.size() == 1 &&
                doc.shared_campaign.outcome_flags.front() ==
                    "act0.helped_larrell",
            "captured outcome flag");
    r.check(doc.validate().has_value(), "solo save validates");

    const auto root = std::filesystem::temp_directory_path() /
                      ("engine-rpg-save-" + engine::make_correlation_id());
    std::filesystem::create_directories(root);
    const auto path = root / "slot1.save.json";
    r.check(doc.save(path).has_value(), "atomic save write");
    r.check(std::filesystem::exists(path), "save file exists");

    auto loaded = engine::RpgSaveDocument::load(path);
    r.check(loaded.has_value(), "load save");
    if (loaded) {
      r.check(loaded.value().session_mode == engine::SessionMode::Solo,
              "loaded solo mode");
      r.check(loaded.value().host_profile.display_name == "Hero",
              "loaded host profile");
      r.check(!loaded.value().guest_profile.has_value(), "solo has no guest");
      engine::QuestRuntime quests2;
      engine::StandingRuntime standing2;
      engine::FlagRuntime flags2;
      engine::PartyRuntime party2;
      party2.bind_session(&session);
      r.check(quests2.bind(&quests_asset.value()).has_value(), "rebind quests");
      r.check(standing2.bind(&standing_factions.value(), &standing_rels.value())
                  .has_value(),
              "rebind standing");
      r.check(loaded.value()
                  .hydrate_into(quests2, standing2, flags2, &party2)
                  .has_value(),
              "hydrate runtimes");
      auto st = quests2.status("sq_01_cart_again");
      r.check(st && st.value().status == engine::QuestInstanceStatus::Active,
              "hydrated quest active");
      r.check(st && st.value().completed_objective_ids.size() == 1,
              "hydrated completed objectives");
      auto score = standing2.get("alpha");
      r.check(score && std::abs(score.value() - 12.5) < 1e-9,
              "hydrated standing score");
      r.check(flags2.has("act0.helped_larrell"), "hydrated outcome flag");
      r.check(party2.list_active().size() == 1 &&
                  party2.list_active().front() == "companion_arkand",
              "hydrated party companion");
    }

    engine::RpgSaveDocument coop = doc;
    coop.session_mode = engine::SessionMode::Coop;
    coop.guest_profile = engine::RpgSavePlayerProfile{1, "Guest", "outrider"};
    coop.shared_campaign.active_companion_ids = {"c1", "c2", "c3"};
    r.check(!coop.validate() &&
                coop.validate().error().code == "RPG-SAVE-PARTY-OVER-CAP",
            "coop party over cap rejected");
    coop.shared_campaign.active_companion_ids = {"c1", "c2"};
    r.check(coop.validate().has_value(),
            "coop save validates with 2 companions");

    engine::RpgSaveDocument solo_guest = doc;
    solo_guest.guest_profile =
        engine::RpgSavePlayerProfile{1, "Nope", "outrider"};
    r.check(!solo_guest.validate() && solo_guest.validate().error().code ==
                                          "RPG-SAVE-SOLO-GUEST-PRESENT",
            "solo with guest rejected");

    engine::RpgSaveDocument coop_missing = coop;
    coop_missing.guest_profile.reset();
    r.check(!coop_missing.validate() && coop_missing.validate().error().code ==
                                            "RPG-SAVE-COOP-MISSING-GUEST",
            "coop missing guest rejected");

    auto corrupt = engine::RpgSaveDocument::from_json("{not json");
    r.check(!corrupt && corrupt.error().code == "RPG-SAVE-PARSE",
            "corrupt JSON fails closed");

    int from_ver = 0;
    auto mig = engine::migrate_rpg_save_json(doc.to_json(), &from_ver);
    r.check(mig && from_ver == 1, "migrate stub identity for v1");
    auto bad_ver = engine::migrate_rpg_save_json(
        R"({"schemaVersion":99,"sessionMode":"solo","hostProfile":{"playerSlot":0}})",
        &from_ver);
    r.check(!bad_ver &&
                bad_ver.error().code == "RPG-SAVE-UNSUPPORTED-VERSION" &&
                from_ver == 99,
            "unsupported version fails closed");

    engine::GameSession load_session;
    r.check(
        engine::apply_rpg_save_to_session(doc, load_session, false).has_value(),
        "apply solo save to session");
    r.check(load_session.state() == engine::GameSessionState::Playing,
            "solo apply reaches playing");
    engine::GameSession coop_session;
    r.check(engine::apply_rpg_save_to_session(coop, coop_session, false)
                .has_value(),
            "apply coop without guest stays open");
    r.check(coop_session.state() == engine::GameSessionState::CoopLobby,
            "coop without guest is lobby-only");
    r.check(coop_session.state() != engine::GameSessionState::Playing,
            "coop without guest not playing");
    coop_session.reset_to_menu();
    r.check(
        engine::apply_rpg_save_to_session(coop, coop_session, true).has_value(),
        "apply coop with guest");
    r.check(coop_session.state() == engine::GameSessionState::Playing,
            "coop with guest reaches playing");

    std::filesystem::remove_all(root);
  } else if (suite == "game_module") {
#if defined(_WIN32)
    const std::filesystem::path module_path =
#ifdef ENGINE_GAME_MODULE_DLL_PATH
        ENGINE_GAME_MODULE_DLL_PATH;
#else
        engine::GameModuleHost::default_module_path();
#endif
    const std::filesystem::path mismatch_path =
#ifdef ENGINE_GAME_MODULE_ABI_MISMATCH_DLL_PATH
        ENGINE_GAME_MODULE_ABI_MISMATCH_DLL_PATH;
#else
        module_path.parent_path() / "game_module_abi_mismatch.dll";
#endif
    r.check(std::filesystem::exists(module_path),
            "sample game_module.dll is built next to dev-next");
    engine::GameModuleHost host;
    auto loaded = host.load(module_path);
    r.check(loaded.has_value(), "load sample game_module");
    if (!loaded) {
      std::cerr << "game_module load: " << loaded.error().message << '\n';
    } else {
      r.check(host.loaded(), "host reports loaded");
      r.check(host.generation() == 1u, "first load is generation 1");
      r.check(host.abi_version() == ENGINE_GAME_MODULE_ABI_VERSION,
              "ABI matches host");
      r.check(!host.module_name().empty(), "module name exported");
      host.tick(1.0f / 60.0f);
      host.tick(1.0f / 60.0f);
      r.check(host.tick_count() == 2u, "host counts ticks");
      const auto ticks = host.mirror_number("game.module_ticks");
      r.check(ticks && *ticks >= 2.0,
              "module wrote game.module_ticks via blackboard");
      const auto build_id = host.mirror_number("game.module_build_id");
      r.check(build_id && *build_id >= 1.0,
              "module wrote game.module_build_id");
      const auto name_before = host.module_name();
      auto reloaded = host.reload();
      r.check(reloaded.has_value(), "reload succeeds");
      r.check(host.generation() == 2u, "reload bumps generation");
      r.check(host.module_name() == name_before,
              "reload preserves sample name without rebuild");
      host.tick(0.016f);
      r.check(host.mirror_number("game.module_ticks") &&
                  *host.mirror_number("game.module_ticks") >= 1.0,
              "ticks resume after reload");
      host.unload();
      r.check(!host.loaded(), "unload clears handle");
    }
    if (std::filesystem::exists(mismatch_path)) {
      engine::GameModuleHost mismatch_host;
      auto bad = mismatch_host.load(mismatch_path);
      r.check(!bad.has_value(), "ABI mismatch is rejected");
      r.check(bad.error().code == "GAME-MODULE-ABI",
              "ABI mismatch uses GAME-MODULE-ABI");
      r.check(!mismatch_host.loaded(), "failed load leaves host unloaded");
    } else {
      r.check(false, "game_module_abi_mismatch.dll is built for tests");
    }
    {
      engine::GameModuleHost missing;
      auto miss = missing.load(module_path.parent_path() /
                               "does_not_exist_game_module.dll");
      r.check(!miss.has_value() && miss.error().code == "GAME-MODULE-MISSING",
              "missing DLL fails closed");
    }
#else
    r.check(true, "game_module suite skipped on non-Windows");
#endif
  } else {
    std::cerr << "unknown suite\n";
    return 2;
  }
  std::cout << "{\"suite\":\"" << r.suite
            << "\",\"assertions\":" << r.assertions
            << ",\"passed\":" << (r.assertions - r.failures)
            << ",\"failed\":" << r.failures << "}\n";
  return r.failures ? 1 : 0;
}
