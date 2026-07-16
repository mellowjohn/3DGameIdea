#pragma once

#include "engine/assets/asset_registry.h"
#include "engine/assets/prefab_asset.h"
#include "engine/automation/editor_bridge.h"
#include "engine/automation/scene_commands.h"
#include "engine/automation/terrain_edit_commands.h"
#include "engine/world/foliage_density.h"
#include "engine/world/foliage_layers.h"
#include "engine/world/scene.h"
#include "engine/world/terrain_edits.h"
#include "engine/world/terrain_paint.h"

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace engine {

class LuaRuntime;
class UiCanvasStack;
class QuestRuntime;
class StandingRuntime;
class HudRuntime;

struct EditorSessionContext {
    Scene* scene = nullptr;
    LuaRuntime* lua_runtime = nullptr;
    QuestRuntime* quest_runtime = nullptr;
    StandingRuntime* standing_runtime = nullptr;
    HudRuntime* hud_runtime = nullptr;
    UiCanvasStack* ui_canvas_stack = nullptr;
    CommandHistory* history = nullptr;
    AssetRegistry* assets = nullptr;
    std::filesystem::path world_path;
    std::filesystem::path project_root;
    std::optional<EntityId>* selected = nullptr;
    std::map<std::string, PrefabAsset>* prefab_catalog = nullptr;
    bool* scene_dirty = nullptr;
    bool* prefab_meshes_dirty = nullptr;
    TerrainEditStore* terrain_edits = nullptr;
    TerrainEditHistory* terrain_history = nullptr;
    bool* terrain_edits_dirty = nullptr;
    TerrainPaintStore* terrain_paint = nullptr;
    TerrainPaintHistory* terrain_paint_history = nullptr;
    bool* terrain_paint_dirty = nullptr;
    FoliageDensityStore* foliage_density = nullptr;
    FoliageDensityHistory* foliage_density_history = nullptr;
    bool* foliage_density_dirty = nullptr;
    FoliageLayerPalette* foliage_layers = nullptr;
    /// Invoked after height/paint strokes or undo/redo. Argument is true when collision heightfields must reload.
    std::function<void(bool height_changed)> reload_terrain;
    /// Invoked after foliage density strokes or undo/redo so streamed instances rebuild.
    std::function<void()> reload_foliage;
    bool editor_running = false;
    bool live_automation_enabled = false;
    bool test_session_active = false;
    std::string test_session_state;
    std::optional<std::string> selected_entity_id;
};

struct ScenePlanResult {
    std::string target_kind;
    std::string requires_compile;
    std::string requires_reload;
    std::string recommendation;
    std::string summary;
};

[[nodiscard]] ScenePlanResult classify_scene_plan(const std::string& change_description, const std::string& target_path = {});
[[nodiscard]] EditorBridgeResponse execute_editor_operation(EditorSessionContext& context, const std::string& operation,
    const std::string& params_json);
[[nodiscard]] CommandResponse validate_project_at(const std::filesystem::path& project_root);

} // namespace engine
