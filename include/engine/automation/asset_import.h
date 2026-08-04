#pragma once

#include "engine/automation/asset_bake_commands.h"
#include "engine/core/error.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

/// How a picked model file was matched to a registered bake target.
enum class AssetImportMatch : std::uint8_t {
    None,          ///< No catalog target; importing registers a new one.
    DefaultSource, ///< Picked the target's registered source file.
    MeshOutput,    ///< Picked the baked output under assets/models (re-import).
    TargetId,      ///< File stem slugs to an existing target id.
    SourceStem,    ///< File stem slugs to the registered source's stem.
};

[[nodiscard]] std::string to_string(AssetImportMatch match);

/// Everything the editor needs to describe an import before running it.
struct AssetImportPlan {
    bool supported = false;
    std::filesystem::path source;
    std::string source_display;
    std::string target_id;
    /// True when the bake overwrites an already-registered target (update in place).
    bool existing_target = false;
    AssetImportMatch match = AssetImportMatch::None;
    std::string match_detail;
    std::string kind = "static"; // static | skinned
    std::string mesh_output;     // assets/models/<id>.gltf
    std::string atlas_output;    // assets/models/<id>.png
    /// Prefab that already references `mesh_output`, or the path a new prefab would take.
    std::string prefab_path;
    bool existing_prefab = false;
    bool has_skin = false;
    std::vector<std::string> clip_names;
    /// Authored height in source units (0 when unknown, e.g. .bbmodel sources).
    float source_height = 0.0f;
    /// World height the bake normalizes to. Editable before import for new targets.
    float target_height = 0.0f;
    std::vector<EngineError> diagnostics;
};

struct AssetImportOutcome {
    bool ok = false;
    std::string summary;
    std::string target_id;
    bool registered_target = false;
    std::string prefab_path;
    bool created_prefab = false;
    std::vector<std::string> mesh_reloads;
    std::string report_json;
    std::vector<EngineError> diagnostics;
};

/// Inspect `source` and decide whether it updates a registered target or introduces a new one.
/// `requested_id` overrides the derived id for new targets (still slugified).
[[nodiscard]] AssetImportPlan plan_asset_import(const std::filesystem::path& project_root,
    const std::filesystem::path& source, const std::string& requested_id = {});

/// Build a plan that rebakes an already-registered catalog target from its recorded source.
[[nodiscard]] AssetImportPlan plan_target_rebake(const std::filesystem::path& project_root,
    const std::string& target_id);

/// Build a plan that points an existing catalog target at a different source file ("Replace...").
/// Unlike `plan_asset_import`, the target is pinned, so a renamed export still updates the same asset.
[[nodiscard]] AssetImportPlan plan_asset_replace(const std::filesystem::path& project_root,
    const std::string& target_id, const std::filesystem::path& source);

/// Called from the worker thread as the import moves between stages so the UI can show progress.
using AssetImportProgressFn = std::function<void(std::string_view stage)>;

/// Register (when new) then bake, then author a prefab for new targets. Blocking; call off the UI thread.
[[nodiscard]] AssetImportOutcome run_asset_import(const std::filesystem::path& project_root,
    const AssetImportPlan& plan, const AssetImportProgressFn& progress = {});

/// Locate a prefab under `<project>/assets/prefabs` whose mesh reference is `mesh_output`.
[[nodiscard]] std::string find_prefab_for_mesh(const std::filesystem::path& project_root,
    const std::string& mesh_output);

} // namespace engine
