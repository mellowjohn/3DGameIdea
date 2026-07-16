#include "engine/automation/world_forge_commands.h"

#include "engine/assets/world_forge_dialogues_asset.h"
#include "engine/assets/world_forge_factions_asset.h"
#include "engine/assets/world_forge_map_asset.h"
#include "engine/assets/world_forge_quests_asset.h"
#include "engine/assets/world_forge_relationships_asset.h"
#include "engine/core/error.h"
#include "engine/dialogue/twee_import.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace engine {
namespace {

EngineError wf_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "world_forge_commands", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

EditorBridgeResponse make_response(ExitCode exit_code, std::string summary,
    std::vector<EngineError> diagnostics = {}, std::map<std::string, std::string> metadata = {}) {
    EditorBridgeResponse response;
    response.exit_code = exit_code;
    response.summary = std::move(summary);
    response.diagnostics = std::move(diagnostics);
    response.metadata = std::move(metadata);
    return response;
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

enum class WorldForgeKind : std::uint8_t { Factions, Relationships, Map, Quests, Dialogues };

std::optional<WorldForgeKind> kind_from_string(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "factions" || key == "faction") return WorldForgeKind::Factions;
    if (key == "relationships" || key == "relationship" || key == "graph") return WorldForgeKind::Relationships;
    if (key == "map" || key == "regions" || key == "pois") return WorldForgeKind::Map;
    if (key == "quests" || key == "quest") return WorldForgeKind::Quests;
    if (key == "dialogues" || key == "dialogue" || key == "dialogs" || key == "dialog")
        return WorldForgeKind::Dialogues;
    return std::nullopt;
}

std::optional<WorldForgeKind> kind_from_path(const std::string& relative) {
    const auto key = lower_copy(relative);
    if (key.find("factions.worldforge.json") != std::string::npos) return WorldForgeKind::Factions;
    if (key.find("relationships.worldforge.json") != std::string::npos) return WorldForgeKind::Relationships;
    if (key.find("map.worldforge.json") != std::string::npos) return WorldForgeKind::Map;
    if (key.find("quests.worldforge.json") != std::string::npos) return WorldForgeKind::Quests;
    if (key.find("dialogues.worldforge.json") != std::string::npos) return WorldForgeKind::Dialogues;
    return std::nullopt;
}

const char* to_string(WorldForgeKind kind) {
    switch (kind) {
    case WorldForgeKind::Factions: return "factions";
    case WorldForgeKind::Relationships: return "relationships";
    case WorldForgeKind::Map: return "map";
    case WorldForgeKind::Quests: return "quests";
    case WorldForgeKind::Dialogues: return "dialogues";
    }
    return "factions";
}

std::filesystem::path default_path(WorldForgeKind kind, const std::filesystem::path& project_root) {
    switch (kind) {
    case WorldForgeKind::Factions: return default_world_forge_factions_path(project_root);
    case WorldForgeKind::Relationships: return default_world_forge_relationships_path(project_root);
    case WorldForgeKind::Map: return default_world_forge_map_path(project_root);
    case WorldForgeKind::Quests: return default_world_forge_quests_path(project_root);
    case WorldForgeKind::Dialogues: return default_world_forge_dialogues_path(project_root);
    }
    return default_world_forge_factions_path(project_root);
}

std::unordered_set<std::string> load_faction_ids(const std::filesystem::path& project_root) {
    std::unordered_set<std::string> ids;
    const auto path = default_world_forge_factions_path(project_root);
    if (!std::filesystem::exists(path)) return ids;
    if (const auto loaded = WorldForgeFactionsAsset::load(path); loaded) {
        for (const auto& entity : loaded.value().entities) ids.insert(entity.id);
    }
    return ids;
}

std::unordered_set<std::string> load_region_ids(const std::filesystem::path& project_root) {
    std::unordered_set<std::string> ids;
    const auto path = default_world_forge_map_path(project_root);
    if (!std::filesystem::exists(path)) return ids;
    if (const auto loaded = WorldForgeMapAsset::load(path); loaded) {
        for (const auto& region : loaded.value().regions) ids.insert(region.id);
    }
    return ids;
}

std::unordered_set<std::string> load_quest_ids(const std::filesystem::path& project_root) {
    std::unordered_set<std::string> ids;
    const auto path = default_world_forge_quests_path(project_root);
    if (!std::filesystem::exists(path)) return ids;
    if (const auto loaded = WorldForgeQuestsAsset::load(path); loaded) {
        for (const auto& quest : loaded.value().quests) ids.insert(quest.id);
    }
    return ids;
}

std::string payload_text(const nlohmann::json& params) {
    if (params.contains("source") && params["source"].is_string()) return params["source"].get<std::string>();
    if (params.contains("json")) {
        if (params["json"].is_string()) return params["json"].get<std::string>();
        return params["json"].dump(2) + "\n";
    }
    return {};
}

Result<void> validate_kind_file(WorldForgeKind kind, const std::filesystem::path& absolute,
    const std::filesystem::path& project_root) {
    const auto faction_ids = load_faction_ids(project_root);
    switch (kind) {
    case WorldForgeKind::Factions:
        return WorldForgeFactionsAsset::validate_file(absolute);
    case WorldForgeKind::Relationships:
        return WorldForgeRelationshipsAsset::validate_file(absolute, faction_ids);
    case WorldForgeKind::Map:
        return WorldForgeMapAsset::validate_file(absolute, faction_ids);
    case WorldForgeKind::Quests:
        return WorldForgeQuestsAsset::validate_file(absolute, load_region_ids(project_root));
    case WorldForgeKind::Dialogues:
        return WorldForgeDialoguesAsset::validate_file(absolute, load_quest_ids(project_root));
    }
    return Result<void>::success();
}

Result<std::string> load_json_text(WorldForgeKind kind, const std::filesystem::path& absolute,
    const std::filesystem::path& project_root, std::map<std::string, std::string>& metadata) {
    const auto faction_ids = load_faction_ids(project_root);
    switch (kind) {
    case WorldForgeKind::Factions: {
        const auto loaded = WorldForgeFactionsAsset::load(absolute);
        if (!loaded) return Result<std::string>::failure(loaded.error());
        metadata["entityCount"] = std::to_string(loaded.value().entities.size());
        metadata["assetId"] = loaded.value().id;
        return Result<std::string>::success(loaded.value().to_json());
    }
    case WorldForgeKind::Relationships: {
        const auto loaded = WorldForgeRelationshipsAsset::load(absolute);
        if (!loaded) return Result<std::string>::failure(loaded.error());
        if (const auto refs = loaded.value().validate_faction_refs(faction_ids); !refs)
            return Result<std::string>::failure(refs.error());
        metadata["nodeCount"] = std::to_string(loaded.value().nodes.size());
        metadata["edgeCount"] = std::to_string(loaded.value().edges.size());
        metadata["assetId"] = loaded.value().id;
        return Result<std::string>::success(loaded.value().to_json());
    }
    case WorldForgeKind::Map: {
        const auto loaded = WorldForgeMapAsset::load(absolute);
        if (!loaded) return Result<std::string>::failure(loaded.error());
        if (const auto refs = loaded.value().validate_faction_refs(faction_ids); !refs)
            return Result<std::string>::failure(refs.error());
        metadata["regionCount"] = std::to_string(loaded.value().regions.size());
        metadata["poiCount"] = std::to_string(loaded.value().pois.size());
        metadata["linkCount"] = std::to_string(loaded.value().links.size());
        metadata["assetId"] = loaded.value().id;
        return Result<std::string>::success(loaded.value().to_json());
    }
    case WorldForgeKind::Quests: {
        const auto loaded = WorldForgeQuestsAsset::load(absolute);
        if (!loaded) return Result<std::string>::failure(loaded.error());
        if (const auto refs = loaded.value().validate_region_refs(load_region_ids(project_root)); !refs)
            return Result<std::string>::failure(refs.error());
        metadata["questCount"] = std::to_string(loaded.value().quests.size());
        metadata["assetId"] = loaded.value().id;
        return Result<std::string>::success(loaded.value().to_json());
    }
    case WorldForgeKind::Dialogues: {
        const auto loaded = WorldForgeDialoguesAsset::load(absolute);
        if (!loaded) return Result<std::string>::failure(loaded.error());
        if (const auto refs = loaded.value().validate_quest_refs(load_quest_ids(project_root)); !refs)
            return Result<std::string>::failure(refs.error());
        metadata["treeCount"] = std::to_string(loaded.value().trees.size());
        metadata["assetId"] = loaded.value().id;
        return Result<std::string>::success(loaded.value().to_json());
    }
    }
    return Result<std::string>::failure(wf_error("WORLD-FORGE-CMD-KIND", ErrorCategory::Validation,
        "Unknown World Forge kind", "Use factions, relationships, map, quests, or dialogues."));
}

Result<void> write_kind_file(WorldForgeKind kind, const std::filesystem::path& absolute, const std::string& text,
    const std::filesystem::path& project_root, std::map<std::string, std::string>& metadata) {
    const auto faction_ids = load_faction_ids(project_root);
    switch (kind) {
    case WorldForgeKind::Factions: {
        auto parsed = WorldForgeFactionsAsset::parse(text, absolute.filename().string());
        if (!parsed) return Result<void>::failure(parsed.error());
        metadata["entityCount"] = std::to_string(parsed.value().entities.size());
        metadata["assetId"] = parsed.value().id;
        return parsed.value().save_atomic(absolute);
    }
    case WorldForgeKind::Relationships: {
        auto parsed = WorldForgeRelationshipsAsset::parse(text, absolute.filename().string());
        if (!parsed) return Result<void>::failure(parsed.error());
        if (const auto refs = parsed.value().validate_faction_refs(faction_ids); !refs)
            return Result<void>::failure(refs.error());
        metadata["nodeCount"] = std::to_string(parsed.value().nodes.size());
        metadata["edgeCount"] = std::to_string(parsed.value().edges.size());
        metadata["assetId"] = parsed.value().id;
        return parsed.value().save_atomic(absolute);
    }
    case WorldForgeKind::Map: {
        auto parsed = WorldForgeMapAsset::parse(text, absolute.filename().string());
        if (!parsed) return Result<void>::failure(parsed.error());
        if (const auto refs = parsed.value().validate_faction_refs(faction_ids); !refs)
            return Result<void>::failure(refs.error());
        metadata["regionCount"] = std::to_string(parsed.value().regions.size());
        metadata["poiCount"] = std::to_string(parsed.value().pois.size());
        metadata["linkCount"] = std::to_string(parsed.value().links.size());
        metadata["assetId"] = parsed.value().id;
        return parsed.value().save_atomic(absolute);
    }
    case WorldForgeKind::Quests: {
        auto parsed = WorldForgeQuestsAsset::parse(text, absolute.filename().string());
        if (!parsed) return Result<void>::failure(parsed.error());
        if (const auto refs = parsed.value().validate_region_refs(load_region_ids(project_root)); !refs)
            return Result<void>::failure(refs.error());
        metadata["questCount"] = std::to_string(parsed.value().quests.size());
        metadata["assetId"] = parsed.value().id;
        return parsed.value().save_atomic(absolute);
    }
    case WorldForgeKind::Dialogues: {
        auto parsed = WorldForgeDialoguesAsset::parse(text, absolute.filename().string());
        if (!parsed) return Result<void>::failure(parsed.error());
        if (const auto refs = parsed.value().validate_quest_refs(load_quest_ids(project_root)); !refs)
            return Result<void>::failure(refs.error());
        metadata["treeCount"] = std::to_string(parsed.value().trees.size());
        metadata["assetId"] = parsed.value().id;
        return parsed.value().save_atomic(absolute);
    }
    }
    return Result<void>::failure(wf_error("WORLD-FORGE-CMD-KIND", ErrorCategory::Validation,
        "Unknown World Forge kind", "Use factions, relationships, map, quests, or dialogues."));
}

std::string relative_from(const std::filesystem::path& project_root, const std::filesystem::path& absolute) {
    std::error_code ec;
    const auto rel = std::filesystem::relative(absolute, project_root, ec);
    if (ec) return absolute.generic_string();
    return rel.generic_string();
}

} // namespace

EditorBridgeResponse apply_world_forge_operation(const std::filesystem::path& project_root,
    const nlohmann::json& params) {
    auto action = lower_copy(params.value("action", std::string{}));
    if (action.empty()) action = "get";
    if (action == "read") action = "get";
    if (action == "write") action = "apply";

    std::optional<WorldForgeKind> kind;
    if (params.contains("kind") && params["kind"].is_string()) kind = kind_from_string(params["kind"].get<std::string>());
    const auto relative_arg = params.value("path", std::string{});
    if (!kind && !relative_arg.empty()) kind = kind_from_path(relative_arg);
    if (!kind && action == "get") kind = WorldForgeKind::Factions;
    if ((action == "import_twee" || action == "import-twee") && !kind) kind = WorldForgeKind::Dialogues;
    if (!kind) {
        return make_response(ExitCode::InvalidArguments, "World Forge kind or path required",
            {wf_error("WORLD-FORGE-CMD-KIND", ErrorCategory::Validation,
                "Could not resolve World Forge asset kind",
                "Pass kind=factions|relationships|map|quests|dialogues or a *.worldforge.json path.")});
    }

    const auto absolute = relative_arg.empty() ? default_path(*kind, project_root) : (project_root / relative_arg);
    const auto relative = relative_from(project_root, absolute);
    std::map<std::string, std::string> metadata{
        {"kind", to_string(*kind)},
        {"assetPath", relative},
        {"action", action},
    };

    if (action == "import_twee" || action == "import-twee") {
        if (*kind != WorldForgeKind::Dialogues) {
            return make_response(ExitCode::InvalidArguments, "Twine import requires kind=dialogues",
                {wf_error("WORLD-FORGE-CMD-KIND", ErrorCategory::Validation,
                    "import_twee only applies to dialogues assets", "Pass kind=dialogues.")},
                std::move(metadata));
        }
        const auto twee_rel = params.value("tweePath", params.value("sourcePath", std::string{}));
        if (twee_rel.empty()) {
            return make_response(ExitCode::InvalidArguments, "tweePath required",
                {wf_error("WORLD-FORGE-CMD-TWEE", ErrorCategory::Validation,
                    "Twine import requires tweePath", "Pass a .twee path relative to the project or absolute.")},
                std::move(metadata));
        }
        std::filesystem::path twee_absolute = twee_rel;
        if (!twee_absolute.is_absolute()) twee_absolute = project_root / twee_rel;
        TweeImportOptions options;
        options.tree_id = params.value("treeId", std::string{});
        options.display_name = params.value("displayName", std::string{});
        options.parent_quest_id = params.value("parentQuestId", std::string{});
        options.entry_node_id = params.value("entryNodeId", std::string{});
        options.story_ref = params.value("storyRef", std::string{});
        options.source_label = relative_from(project_root, twee_absolute);
        if (options.tree_id.empty()) {
            return make_response(ExitCode::InvalidArguments, "treeId required",
                {wf_error("WORLD-FORGE-CMD-TREE", ErrorCategory::Validation,
                    "Twine import requires treeId", "Pass treeId for the dialogue tree to create or replace.")},
                std::move(metadata));
        }
        const auto imported = import_twee_dialogue_tree_file(twee_absolute, options);
        if (!imported) {
            return make_response(ExitCode::ValidationFailed, imported.error().message, {imported.error()},
                std::move(metadata));
        }
        WorldForgeDialoguesAsset asset;
        if (std::filesystem::exists(absolute)) {
            auto loaded = WorldForgeDialoguesAsset::load(absolute);
            if (!loaded) {
                return make_response(ExitCode::ValidationFailed, loaded.error().message, {loaded.error()},
                    std::move(metadata));
            }
            asset = std::move(loaded.value());
        } else {
            asset.schema_version = 1;
            asset.id = "tessera_dialogues";
        }
        if (const auto upserted = upsert_dialogue_tree(asset, imported.value()); !upserted) {
            return make_response(ExitCode::ValidationFailed, upserted.error().message, {upserted.error()},
                std::move(metadata));
        }
        if (const auto refs = asset.validate_quest_refs(load_quest_ids(project_root)); !refs) {
            return make_response(ExitCode::ValidationFailed, refs.error().message, {refs.error()},
                std::move(metadata));
        }
        if (const auto saved = asset.save_atomic(absolute); !saved) {
            return make_response(ExitCode::ValidationFailed, saved.error().message, {saved.error()},
                std::move(metadata));
        }
        metadata["treeId"] = options.tree_id;
        metadata["tweePath"] = options.source_label;
        metadata["nodeCount"] = std::to_string(imported.value().nodes.size());
        metadata["treeCount"] = std::to_string(asset.trees.size());
        metadata["requiresReload"] = "world_forge";
        return make_response(ExitCode::Success, "Twine dialogue tree imported", {}, std::move(metadata));
    }

    if (action == "get") {
        if (!std::filesystem::exists(absolute)) {
            return make_response(ExitCode::ValidationFailed, "World Forge asset not found",
                {wf_error("WORLD-FORGE-CMD-MISSING", ErrorCategory::Io,
                    "Missing World Forge asset: " + relative, "Create it with action=apply or seed the sample.")},
                std::move(metadata));
        }
        const auto loaded = load_json_text(*kind, absolute, project_root, metadata);
        if (!loaded) {
            return make_response(ExitCode::ValidationFailed, loaded.error().message, {loaded.error()},
                std::move(metadata));
        }
        metadata["content"] = loaded.value();
        return make_response(ExitCode::Success, "World Forge asset loaded", {}, std::move(metadata));
    }

    if (action == "validate") {
        if (!std::filesystem::exists(absolute)) {
            return make_response(ExitCode::ValidationFailed, "World Forge asset not found",
                {wf_error("WORLD-FORGE-CMD-MISSING", ErrorCategory::Io,
                    "Missing World Forge asset: " + relative, "Create it with action=apply first.")},
                std::move(metadata));
        }
        const auto valid = validate_kind_file(*kind, absolute, project_root);
        if (!valid) {
            return make_response(ExitCode::ValidationFailed, valid.error().message, {valid.error()},
                std::move(metadata));
        }
        return make_response(ExitCode::Success, "World Forge asset valid", {}, std::move(metadata));
    }

    if (action == "apply") {
        const auto text = payload_text(params);
        if (text.empty()) {
            return make_response(ExitCode::InvalidArguments, "json or source required for apply",
                {wf_error("WORLD-FORGE-CMD-PAYLOAD", ErrorCategory::Validation,
                    "World Forge apply requires json object or source string",
                    "Pass the full asset JSON body.")},
                std::move(metadata));
        }
        const auto written = write_kind_file(*kind, absolute, text, project_root, metadata);
        if (!written) {
            return make_response(ExitCode::ValidationFailed, written.error().message, {written.error()},
                std::move(metadata));
        }
        metadata["requiresReload"] = "none";
        return make_response(ExitCode::Success, "World Forge asset written", {}, std::move(metadata));
    }

    return make_response(ExitCode::InvalidArguments, "Unknown World Forge action",
        {wf_error("WORLD-FORGE-CMD-ACTION", ErrorCategory::Validation,
            "Unsupported action: " + action, "Use get, validate, apply, or import_twee.")},
        std::move(metadata));
}

} // namespace engine
