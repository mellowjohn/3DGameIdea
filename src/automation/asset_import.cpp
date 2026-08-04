#include "engine/automation/asset_import.h"

#include "engine/assets/prefab_asset.h"
#include "engine/core/id_slug.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <string_view>
#include <system_error>

namespace engine {
namespace {

std::string to_lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

std::string generic_path(const std::filesystem::path& path) {
    std::error_code error;
    auto canonical = std::filesystem::weakly_canonical(path, error);
    if (error) canonical = path.lexically_normal();
    return to_lower(canonical.generic_string());
}

std::filesystem::path repo_root() { return std::filesystem::path(ENGINE_REPOSITORY_ROOT); }

/// Shape facts the import UI needs before committing: does the source carry a skeleton,
/// which clips ride along, and how tall is it in authored units.
struct ModelProbe {
    bool ok = false;
    bool json_available = false;
    bool has_skin = false;
    std::vector<std::string> clips;
    float height = 0.0f;
    std::string error;
};

nlohmann::json read_glb_json_chunk(const std::filesystem::path& path, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "could not open " + path.generic_string();
        return {};
    }
    char magic[4]{};
    std::uint32_t version = 0;
    std::uint32_t total = 0;
    in.read(magic, 4);
    in.read(reinterpret_cast<char*>(&version), 4);
    in.read(reinterpret_cast<char*>(&total), 4);
    if (!in || std::memcmp(magic, "glTF", 4) != 0) {
        error = "not a GLB container";
        return {};
    }
    std::uint32_t chunk_length = 0;
    std::uint32_t chunk_type = 0;
    in.read(reinterpret_cast<char*>(&chunk_length), 4);
    in.read(reinterpret_cast<char*>(&chunk_type), 4);
    // 0x4E4F534A == 'JSON'; the spec requires it to be the first chunk.
    if (!in || chunk_type != 0x4E4F534Au || chunk_length == 0) {
        error = "GLB missing JSON chunk";
        return {};
    }
    std::string text(chunk_length, '\0');
    in.read(text.data(), static_cast<std::streamsize>(chunk_length));
    if (!in) {
        error = "GLB JSON chunk truncated";
        return {};
    }
    try {
        return nlohmann::json::parse(text);
    } catch (const std::exception& exc) {
        error = std::string("GLB JSON parse failed: ") + exc.what();
        return {};
    }
}

/// Height comes from POSITION accessor min/max, which glTF requires, so no buffer decode is needed.
float measure_gltf_height(const nlohmann::json& gltf) {
    if (!gltf.contains("accessors") || !gltf["accessors"].is_array()) return 0.0f;
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
    bool found = false;
    for (const auto& mesh : gltf.value("meshes", nlohmann::json::array())) {
        for (const auto& prim : mesh.value("primitives", nlohmann::json::array())) {
            if (!prim.contains("attributes")) continue;
            const auto& attributes = prim["attributes"];
            if (!attributes.contains("POSITION")) continue;
            const auto index = attributes["POSITION"].get<std::size_t>();
            if (index >= gltf["accessors"].size()) continue;
            const auto& accessor = gltf["accessors"][index];
            if (!accessor.contains("min") || !accessor.contains("max")) continue;
            if (!accessor["min"].is_array() || accessor["min"].size() < 2) continue;
            if (!accessor["max"].is_array() || accessor["max"].size() < 2) continue;
            min_y = (std::min)(min_y, accessor["min"][1].get<float>());
            max_y = (std::max)(max_y, accessor["max"][1].get<float>());
            found = true;
        }
    }
    if (!found || max_y <= min_y) return 0.0f;
    return max_y - min_y;
}

ModelProbe probe_bbmodel(const std::filesystem::path& source) {
    ModelProbe probe;
    std::ifstream in(source);
    if (!in) {
        probe.error = "could not open " + source.generic_string();
        return probe;
    }
    nlohmann::json document;
    try {
        in >> document;
    } catch (const std::exception& exc) {
        probe.error = std::string("bbmodel parse failed: ") + exc.what();
        return probe;
    }
    // Skinned player kits always have an armature; props usually don't list animations here.
    probe.has_skin = document.contains("animations") && !document["animations"].empty();
    for (const auto& animation : document.value("animations", nlohmann::json::array())) {
        auto name = animation.value("name", std::string{});
        if (name.empty()) name = "clip_" + std::to_string(probe.clips.size());
        probe.clips.push_back(std::move(name));
    }
    if (!probe.clips.empty()) probe.has_skin = true;
    probe.json_available = true;
    probe.ok = true;
    return probe;
}

/// Strip a trailing `_rigged` so GoodPlayerModel_rigged.bbmodel matches GoodPlayerModel.gltf.
std::string source_stem_slug(const std::filesystem::path& source) {
    auto stem = slugify_id(source.stem().string());
    constexpr std::string_view k_rigged = "_rigged";
    if (stem.size() > k_rigged.size() && stem.compare(stem.size() - k_rigged.size(), k_rigged.size(), k_rigged) == 0)
        stem.resize(stem.size() - k_rigged.size());
    return stem;
}

bool looks_like_player_authoring(const std::filesystem::path& source) {
    const auto stem = to_lower(source.stem().string());
    return stem.find("goodplayermodel") != std::string::npos || stem.find("player_v2") != std::string::npos;
}

/// List clips from the Blockbench project and point Re-import at it when present.
/// The player baker always regenerates the art glTF from this project before baking.
void enrich_player_plan_from_bbmodel(AssetImportPlan& plan) {
    if (plan.target_id != "player") return;
    if (to_lower(plan.source.extension().string()) == ".bbmodel") return;
    const std::filesystem::path candidates[] = {
        repo_root() / "tools/art/player/GoodPlayerModel_rigged.bbmodel",
        std::filesystem::path(R"(c:\Users\johnr\Documents\GoodPlayerModel_rigged.bbmodel)"),
        std::filesystem::path(R"(c:\Users\johnr\Documents\GoodPlayerModel.bbmodel)"),
    };
    std::error_code error;
    for (const auto& bb : candidates) {
        if (!std::filesystem::exists(bb, error)) continue;
        const auto probe = probe_bbmodel(bb);
        if (!probe.ok || probe.clips.empty()) continue;
        plan.clip_names = probe.clips;
        plan.has_skin = true;
        if (plan.kind.empty() || plan.kind == "static") plan.kind = "skinned";
        plan.source = bb;
        plan.source_display = bb.generic_string();
        // Promote off MeshOutput so run_asset_import passes --source to the baker.
        if (plan.match == AssetImportMatch::MeshOutput) {
            plan.match = AssetImportMatch::SourceStem;
            plan.match_detail = "GoodPlayerModel_rigged.bbmodel (Blockbench authoring source)";
        }
        return;
    }
}

ModelProbe probe_model(const std::filesystem::path& source) {
    ModelProbe probe;
    const auto extension = to_lower(source.extension().string());
    if (extension == ".bbmodel") return probe_bbmodel(source);
    if (extension != ".gltf" && extension != ".glb") {
        probe.error = "unsupported model extension " + extension;
        return probe;
    }
    nlohmann::json gltf;
    if (extension == ".glb") {
        gltf = read_glb_json_chunk(source, probe.error);
        if (gltf.is_null() || gltf.empty()) return probe;
    } else {
        std::ifstream in(source);
        if (!in) {
            probe.error = "could not open " + source.generic_string();
            return probe;
        }
        try {
            in >> gltf;
        } catch (const std::exception& exc) {
            probe.error = std::string("glTF parse failed: ") + exc.what();
            return probe;
        }
    }
    probe.json_available = true;
    probe.has_skin = gltf.contains("skins") && gltf["skins"].is_array() && !gltf["skins"].empty();
    for (const auto& animation : gltf.value("animations", nlohmann::json::array())) {
        auto name = animation.value("name", std::string{});
        if (name.empty()) name = "clip_" + std::to_string(probe.clips.size());
        probe.clips.push_back(std::move(name));
    }
    probe.height = measure_gltf_height(gltf);
    probe.ok = true;
    return probe;
}

std::string prefab_mesh_reference(const PrefabAsset& prefab) {
    if (!prefab.mesh.empty()) return prefab.mesh;
    for (const auto& part : prefab.parts) {
        if (part.mesh.asset && !part.mesh.asset->empty()) return *part.mesh.asset;
    }
    return {};
}

/// Blockbench and most DCC exports are not authored in metres, so the source extent is only a hint.
/// Default to a readable world size per kind and let the author override before importing.
float default_target_height(bool skinned) { return skinned ? 1.8f : 1.0f; }

std::string display_name_from_stem(const std::string& stem) {
    std::string out;
    bool capitalize = true;
    for (char ch : stem) {
        if (ch == '_' || ch == '-' || ch == ' ') {
            out.push_back(' ');
            capitalize = true;
            continue;
        }
        out.push_back(capitalize ? static_cast<char>(std::toupper(static_cast<unsigned char>(ch))) : ch);
        capitalize = false;
    }
    return out.empty() ? stem : out;
}

} // namespace

std::string to_string(AssetImportMatch match) {
    switch (match) {
        case AssetImportMatch::DefaultSource: return "registered source";
        case AssetImportMatch::MeshOutput: return "baked output";
        case AssetImportMatch::TargetId: return "target id";
        case AssetImportMatch::SourceStem: return "source name";
        case AssetImportMatch::None: break;
    }
    return "new asset";
}

std::string find_prefab_for_mesh(const std::filesystem::path& project_root, const std::string& mesh_output) {
    if (mesh_output.empty()) return {};
    const auto prefabs_root = project_root / "assets" / "prefabs";
    std::error_code error;
    if (!std::filesystem::exists(prefabs_root, error)) return {};
    const auto wanted = to_lower(mesh_output);
    // Several prefabs can share one mesh (player.gltf is also used by npc_test), so prefer the one
    // whose file name matches the mesh name before falling back to any referencing prefab.
    const auto wanted_stem = to_lower(std::filesystem::path(mesh_output).stem().string());
    std::string fallback;
    for (std::filesystem::recursive_directory_iterator it(prefabs_root, error), end; it != end;
         it.increment(error)) {
        if (error) break;
        if (!it->is_regular_file(error)) continue;
        const auto name = to_lower(it->path().filename().string());
        if (name.size() < 13 || name.rfind(".prefab.json") != name.size() - 12) continue;
        const auto loaded = PrefabAsset::load(it->path());
        if (!loaded) continue;
        if (to_lower(prefab_mesh_reference(loaded.value())) != wanted) continue;
        const auto relative = std::filesystem::relative(it->path(), project_root, error);
        if (error) continue;
        const auto candidate = relative.generic_string();
        if (name.substr(0, name.size() - 12) == wanted_stem) return candidate;
        if (fallback.empty()) fallback = candidate;
    }
    return fallback;
}

AssetImportPlan plan_asset_import(const std::filesystem::path& project_root,
    const std::filesystem::path& source, const std::string& requested_id) {
    AssetImportPlan plan;
    plan.source = source;
    plan.source_display = source.generic_string();

    std::error_code error;
    if (source.empty() || !std::filesystem::exists(source, error)) {
        plan.diagnostics.push_back(asset_bake_error("ASSET-IMPORT-SOURCE-MISSING",
            "Model file not found: " + plan.source_display, "Pick an existing .gltf / .glb file."));
        return plan;
    }

    const auto extension = to_lower(source.extension().string());
    if (extension != ".gltf" && extension != ".glb" && extension != ".bbmodel") {
        plan.diagnostics.push_back(asset_bake_error("ASSET-IMPORT-UNSUPPORTED",
            "Unsupported model type: " + extension, "Import .gltf or .glb (export glTF from Blockbench)."));
        return plan;
    }

    const auto probe = probe_model(source);
    if (!probe.ok) {
        plan.diagnostics.push_back(asset_bake_error("ASSET-IMPORT-PARSE", probe.error,
            "Re-export the model; the file could not be read as glTF."));
        return plan;
    }
    plan.has_skin = probe.has_skin;
    plan.clip_names = probe.clips;
    plan.source_height = probe.height;
    plan.kind = probe.has_skin ? "skinned" : "static";

    // Match against registered targets so re-importing the player kit updates it in place.
    const auto targets = list_asset_bake_targets();
    const auto source_key = generic_path(source);
    const auto stem_slug = slugify_id(source.stem().string());
    const auto authoring_stem = source_stem_slug(source);
    const AssetBakeTargetInfo* matched = nullptr;
    auto adopt = [&](const AssetBakeTargetInfo& target, AssetImportMatch match, std::string detail) {
        matched = &target;
        plan.match = match;
        plan.match_detail = std::move(detail);
    };
    for (const auto& target : targets) {
        if (target.default_source.empty()) continue;
        if (generic_path(repo_root() / target.default_source) == source_key) {
            adopt(target, AssetImportMatch::DefaultSource, target.default_source);
            break;
        }
    }
    if (matched == nullptr) {
        for (const auto& target : targets) {
            if (target.mesh_output.empty()) continue;
            if (generic_path(project_root / target.mesh_output) == source_key) {
                adopt(target, AssetImportMatch::MeshOutput, target.mesh_output);
                break;
            }
        }
    }
    if (matched == nullptr && !stem_slug.empty()) {
        for (const auto& target : targets) {
            if (slugify_id(target.id) == stem_slug) {
                adopt(target, AssetImportMatch::TargetId, target.id);
                break;
            }
        }
    }
    if (matched == nullptr && !authoring_stem.empty()) {
        for (const auto& target : targets) {
            if (target.default_source.empty()) continue;
            const auto registered_stem = std::filesystem::path(target.default_source).stem().string();
            if (slugify_id(registered_stem) == authoring_stem || slugify_id(registered_stem) == stem_slug) {
                adopt(target, AssetImportMatch::SourceStem, registered_stem);
                break;
            }
        }
    }
    // GoodPlayerModel_rigged.bbmodel / Documents drops → player, even when the stem slug
    // does not equal the catalog id (player) or the glTF stem alone.
    if (matched == nullptr && extension == ".bbmodel" && looks_like_player_authoring(source)) {
        for (const auto& target : targets) {
            if (target.id == "player") {
                adopt(target, AssetImportMatch::SourceStem, "GoodPlayerModel");
                break;
            }
        }
    }

    if (matched != nullptr) {
        plan.supported = true;
        plan.existing_target = true;
        plan.target_id = matched->id;
        plan.kind = matched->kind.empty() ? plan.kind : matched->kind;
        if (extension == ".bbmodel" && !plan.clip_names.empty()) plan.kind = "skinned";
        plan.mesh_output = matched->mesh_output;
        plan.atlas_output = matched->atlas_output;
        // Re-importing a registered target keeps its authored scale; the catalog owns that number.
        plan.target_height = 0.0f;
        plan.prefab_path = find_prefab_for_mesh(project_root, plan.mesh_output);
        plan.existing_prefab = !plan.prefab_path.empty();
        enrich_player_plan_from_bbmodel(plan);
        return plan;
    }

    if (extension == ".bbmodel") {
        plan.diagnostics.push_back(asset_bake_error("ASSET-IMPORT-UNSUPPORTED",
            "Blockbench projects can only update a registered target (e.g. player / tree).",
            "Pick GoodPlayerModel_rigged.bbmodel to update the player, or export glTF for a new prop."));
        return plan;
    }

    // New asset: derive a stable id from the file name and keep it unique against the catalog.
    const auto seed = requested_id.empty() ? source.stem().string() : requested_id;
    auto taken = [&](const std::string& candidate) {
        for (const auto& target : targets) {
            if (target.id == candidate) return true;
        }
        std::error_code exists_error;
        return std::filesystem::exists(project_root / "assets" / "models" / (candidate + ".gltf"), exists_error);
    };
    plan.target_id = unique_slugify_id(seed, taken, "imported_model");
    plan.supported = true;
    plan.existing_target = false;
    plan.mesh_output = "assets/models/" + plan.target_id + ".gltf";
    plan.atlas_output = "assets/models/" + plan.target_id + ".png";
    plan.target_height = default_target_height(probe.has_skin);
    plan.prefab_path = "assets/prefabs/Imported/" + plan.target_id + ".prefab.json";
    plan.existing_prefab = false;
    return plan;
}

AssetImportPlan plan_asset_replace(const std::filesystem::path& project_root, const std::string& target_id,
    const std::filesystem::path& source) {
    AssetImportPlan plan;
    plan.source = source;
    plan.source_display = source.generic_string();

    std::error_code error;
    if (source.empty() || !std::filesystem::exists(source, error)) {
        plan.diagnostics.push_back(asset_bake_error("ASSET-IMPORT-SOURCE-MISSING",
            "Model file not found: " + plan.source_display, "Pick an existing .gltf / .glb file."));
        return plan;
    }
    const auto extension = to_lower(source.extension().string());
    if (extension != ".gltf" && extension != ".glb" && extension != ".bbmodel") {
        plan.diagnostics.push_back(asset_bake_error("ASSET-IMPORT-UNSUPPORTED",
            "Unsupported replacement type: " + extension,
            "Replace a target with a .gltf / .glb export or the player .bbmodel."));
        return plan;
    }
    const auto targets = list_asset_bake_targets();
    const AssetBakeTargetInfo* matched = nullptr;
    for (const auto& target : targets) {
        if (target.id == target_id) {
            matched = &target;
            break;
        }
    }
    if (matched == nullptr) {
        plan.diagnostics.push_back(asset_bake_error("ASSET-BAKE-SOURCE-MISSING",
            "Unknown bake target: " + target_id, "Refresh the catalog and pick a registered target."));
        return plan;
    }
    const auto probe = probe_model(source);
    if (!probe.ok) {
        plan.diagnostics.push_back(asset_bake_error("ASSET-IMPORT-PARSE", probe.error,
            "Re-export the model; the file could not be read as glTF."));
        return plan;
    }
    plan.supported = true;
    plan.existing_target = true;
    plan.match = AssetImportMatch::TargetId;
    plan.match_detail = target_id;
    plan.target_id = target_id;
    plan.kind = matched->kind.empty() ? (probe.has_skin ? "skinned" : "static") : matched->kind;
    plan.has_skin = probe.has_skin;
    plan.clip_names = probe.clips;
    plan.source_height = probe.height;
    plan.mesh_output = matched->mesh_output;
    plan.atlas_output = matched->atlas_output;
    plan.target_height = 0.0f;
    plan.prefab_path = find_prefab_for_mesh(project_root, plan.mesh_output);
    plan.existing_prefab = !plan.prefab_path.empty();
    return plan;
}

namespace {

AssetImportOutcome register_new_target(const std::filesystem::path& project_root, const AssetImportPlan& plan) {
    AssetImportOutcome outcome;
    std::vector<std::string> args{"--register", "--id", plan.target_id, "--kind", plan.kind, "--source",
        plan.source.string(), "--target-height", std::to_string(plan.target_height)};
    for (const auto& clip : plan.clip_names) {
        args.push_back("--clip");
        args.push_back(clip);
    }
    const auto tool = run_asset_bake_tool(project_root, args);
    const bool ok = tool.exit_code == 0 && tool.payload.value("ok", false);
    if (!ok) {
        outcome.summary = tool.payload.value("summary", std::string("catalog registration failed"));
        outcome.report_json = tool.raw_stdout;
        outcome.diagnostics.push_back(asset_bake_error("ASSET-IMPORT-REGISTER", outcome.summary,
            tool.raw_stderr.empty() ? "See tools/asset_bake_catalog.json write permissions."
                                    : tool.raw_stderr));
        return outcome;
    }
    outcome.ok = true;
    outcome.registered_target = true;
    return outcome;
}

/// Minimal single-entity prefab so a freshly imported model is placeable straight from the browser.
AssetImportOutcome author_import_prefab(const std::filesystem::path& project_root, const AssetImportPlan& plan) {
    AssetImportOutcome outcome;
    const auto absolute = project_root / plan.prefab_path;
    std::error_code error;
    if (std::filesystem::exists(absolute, error)) {
        outcome.ok = true;
        outcome.prefab_path = plan.prefab_path;
        return outcome;
    }
    std::filesystem::create_directories(absolute.parent_path(), error);
    PrefabAsset prefab;
    prefab.schema_version = 2;
    PrefabPart part;
    part.name = display_name_from_stem(plan.target_id);
    part.mesh.asset = plan.mesh_output;
    prefab.parts.push_back(std::move(part));
    if (const auto saved = prefab.save(absolute); !saved) {
        outcome.diagnostics.push_back(saved.error());
        outcome.summary = saved.error().message;
        return outcome;
    }
    outcome.ok = true;
    outcome.prefab_path = plan.prefab_path;
    outcome.created_prefab = true;
    return outcome;
}

} // namespace

AssetImportPlan plan_target_rebake(const std::filesystem::path& project_root, const std::string& target_id) {
    AssetImportPlan plan;
    for (const auto& target : list_asset_bake_targets()) {
        if (target.id != target_id) continue;
        plan.supported = true;
        plan.existing_target = true;
        // MeshOutput keeps the bake on the catalog's recorded source instead of an override —
        // unless enrich_player_plan_from_bbmodel swaps in a fresher Blockbench project.
        plan.match = AssetImportMatch::MeshOutput;
        plan.match_detail = target.default_source;
        plan.target_id = target.id;
        plan.kind = target.kind.empty() ? "static" : target.kind;
        plan.mesh_output = target.mesh_output;
        plan.atlas_output = target.atlas_output;
        plan.source = repo_root() / target.default_source;
        plan.source_display = target.default_source;
        plan.prefab_path = find_prefab_for_mesh(project_root, plan.mesh_output);
        plan.existing_prefab = !plan.prefab_path.empty();
        const auto probe = probe_model(plan.source);
        if (probe.ok) {
            plan.has_skin = probe.has_skin;
            plan.clip_names = probe.clips;
            plan.source_height = probe.height;
        }
        enrich_player_plan_from_bbmodel(plan);
        return plan;
    }
    plan.diagnostics.push_back(asset_bake_error("ASSET-BAKE-SOURCE-MISSING",
        "Unknown bake target: " + target_id, "Refresh the catalog and pick a registered target."));
    return plan;
}

AssetImportOutcome run_asset_import(const std::filesystem::path& project_root, const AssetImportPlan& plan,
    const AssetImportProgressFn& progress) {
    const auto report = [&progress](std::string_view stage) {
        if (progress) progress(stage);
    };
    AssetImportOutcome outcome;
    outcome.target_id = plan.target_id;
    if (!plan.supported) {
        outcome.summary = "import plan is not supported";
        outcome.diagnostics = plan.diagnostics;
        if (outcome.diagnostics.empty()) {
            outcome.diagnostics.push_back(
                asset_bake_error("ASSET-IMPORT-UNSUPPORTED", outcome.summary, "Pick a .gltf or .glb file."));
        }
        return outcome;
    }

    if (!plan.existing_target) {
        report("Registering bake target " + plan.target_id);
        const auto registered = register_new_target(project_root, plan);
        if (!registered.ok) {
            outcome.summary = registered.summary;
            outcome.diagnostics = registered.diagnostics;
            outcome.report_json = registered.report_json;
            return outcome;
        }
        outcome.registered_target = true;
    }

    // A newly registered target already caches the picked file as its default source; passing the
    // override again for an existing target is what makes "update the player from this glTF /
    // bbmodel" work. Player Re-import may also promote a fresher .bbmodel via SourceStem.
    const std::string source_override =
        plan.existing_target && plan.match != AssetImportMatch::MeshOutput ? plan.source.string() : std::string{};
    if (to_lower(plan.source.extension().string()) == ".bbmodel")
        report("Refreshing glTF from Blockbench project, then baking " + plan.target_id);
    else
        report("Baking mesh, texture" + std::string(plan.clip_names.empty() ? "" : ", animations") +
            " and verifying " + plan.target_id);
    const auto bake = run_asset_bake(project_root, plan.target_id, source_override);
    outcome.report_json = bake.raw_json;
    outcome.mesh_reloads = bake.mesh_reloads;
    outcome.diagnostics = bake.diagnostics;
    if (!bake.ok) {
        outcome.summary = bake.summary;
        return outcome;
    }

    if (!plan.prefab_path.empty()) {
        report("Authoring prefab " + plan.prefab_path);
        const auto prefab = author_import_prefab(project_root, plan);
        outcome.prefab_path = prefab.prefab_path;
        outcome.created_prefab = prefab.created_prefab;
        if (!prefab.ok) {
            outcome.summary = "baked " + plan.target_id + " but prefab write failed: " + prefab.summary;
            for (const auto& diagnostic : prefab.diagnostics) outcome.diagnostics.push_back(diagnostic);
            return outcome;
        }
    }

    outcome.ok = true;
    outcome.summary = (plan.existing_target ? "Updated " : "Imported ") + plan.target_id + " (" + plan.kind + ")";
    if (!plan.clip_names.empty())
        outcome.summary += ", " + std::to_string(plan.clip_names.size()) + " clip(s)";
    if (outcome.created_prefab) outcome.summary += ", prefab " + outcome.prefab_path;
    return outcome;
}

} // namespace engine
