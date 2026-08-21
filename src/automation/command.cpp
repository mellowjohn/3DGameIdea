#include "engine/automation/command.h"
#include "engine/automation/asset_bake_commands.h"
#include "engine/automation/asset_import.h"
#include "engine/automation/build_coordination.h"
#include "engine/automation/project_git_commands.h"
#include "engine/animation/animation_preview.h"
#include "engine/rendering/render_app.h"
#include "engine/assets/asset_registry.h"
#include "engine/assets/material_asset.h"
#include "engine/assets/mesh_asset.h"
#include "engine/assets/particle_emitter_asset.h"
#include "engine/assets/png_decode.h"
#include "engine/assets/world_forge_archetypes_asset.h"
#include "engine/assets/world_forge_factions_asset.h"
#include "engine/assets/world_forge_pantheon_asset.h"
#include "engine/assets/world_forge_relationships_asset.h"
#include "engine/assets/world_forge_resources_asset.h"
#include "engine/assets/world_forge_map_asset.h"
#include "engine/assets/world_forge_dialogues_asset.h"
#include "engine/assets/world_forge_events_asset.h"
#include "engine/assets/world_forge_mvp_readiness_asset.h"
#include "engine/assets/world_forge_quests_asset.h"
#include "engine/testing/image_diff.h"
#include "engine/world/scene.h"
#include "engine/world/terrain_edits.h"
#include "engine/world/terrain_paint.h"
#include "engine/world/foliage_layers.h"
#include "engine/world/foliage_density.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <windows.h>

namespace engine {
namespace {
EngineError command_error(std::string code, std::string message, std::string remedy, std::string correlation) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Configuration, "automation",
                       std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), std::move(correlation)};
}
std::string quote(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += static_cast<unsigned char>(c) < 0x20 ? '?' : c; break;
        }
    }
    return out + '"';
}

std::string argument_value(const CommandRequest& request, const std::string& name, const std::string& fallback = {}) {
    for (std::size_t i = 0; i + 1 < request.arguments.size(); ++i)
        if (request.arguments[i] == name) return request.arguments[i + 1];
    return fallback;
}

std::uint32_t positive_number(const std::string& value, std::uint32_t fallback) {
    if (value.empty()) return fallback;
    try {
        const auto parsed = std::stoul(value);
        return parsed > 0 && parsed <= UINT32_MAX ? static_cast<std::uint32_t>(parsed) : fallback;
    } catch (...) { return fallback; }
}
bool has_argument(const CommandRequest& request,const std::string& name){return std::find(request.arguments.begin(),request.arguments.end(),name)!=request.arguments.end();}

const std::vector<std::string>& ctest_suite_names() {
    static const std::vector<std::string> names{
        "core", "world", "world_influence", "assets", "world_forge", "streaming", "terrain", "foliage",
        "water", "collision", "navigation", "character", "interaction", "combat", "camera", "diagnostics",
        "scripting", "automation", "hud", "animator", "audio", "particles", "game_module", "game_session", "party",
        "rpg_save", "project_validation"};
    return names;
}

bool known_ctest_suite(const std::string& name) {
    if (name == "m5-exit" || name == "visual_regression") return true;
    const auto& names = ctest_suite_names();
    return std::find(names.begin(), names.end(), name) != names.end();
}

const std::vector<std::string>& m5_exit_suite_names() {
    static const std::vector<std::string> names{"animator", "character", "interaction", "combat", "scripting"};
    return names;
}

std::wstring quote_windows_arg(const std::filesystem::path& path) {
    const std::wstring value = path.wstring();
    if (value.find_first_of(L" \t\"") == std::wstring::npos) return value;
    std::wstring quoted = L"\"";
    for (const wchar_t character : value) {
        if (character == L'"') quoted += L"\\\"";
        else quoted += character;
    }
    return quoted + L'"';
}

std::filesystem::path environment_path(const char* name) {
    char value[32768]{};
    const DWORD length = GetEnvironmentVariableA(name, value, static_cast<DWORD>(sizeof(value)));
    return length > 0 && length < sizeof(value) ? std::filesystem::path(value) : std::filesystem::path{};
}

std::filesystem::path find_ctest_executable() {
    if (const auto program_files = environment_path("ProgramFiles"); !program_files.empty()) {
        const auto installed = program_files / "CMake" / "bin" / "ctest.exe";
        if (std::filesystem::exists(installed)) return installed;
    }
    if (const auto program_files_x86 = environment_path("ProgramFiles(x86)"); !program_files_x86.empty()) {
        const auto visual_studio = program_files_x86 / "Microsoft Visual Studio" / "2019" /
                                   "Community" / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" /
                                   "CMake" / "bin" / "ctest.exe";
        if (std::filesystem::exists(visual_studio)) return visual_studio;
    }
    return "ctest.exe";
}

std::string run_ctest(const std::filesystem::path& ctest_executable, const std::filesystem::path& build_directory,
                      const std::string& suite, int& exit_code) {
    wchar_t temp_directory[MAX_PATH]{};
    wchar_t temp_file[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, temp_directory) == 0 ||
        GetTempFileNameW(temp_directory, L"eng", 0, temp_file) == 0) {
        exit_code = -1;
        return "Could not create a temporary CTest output file.";
    }
    const std::filesystem::path output_path(temp_file);
    HANDLE output = CreateFileW(temp_file, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        std::error_code cleanup;
        std::filesystem::remove(output_path, cleanup);
        exit_code = -1;
        return "Could not open a temporary CTest output file.";
    }

    std::wstring command_line = L"--test-dir " + quote_windows_arg(build_directory) + L" -C Debug -R \"^";
    command_line.append(suite.begin(), suite.end());
    command_line += L"$\" --output-on-failure";
    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = output;
    startup.hStdError = output;
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(ctest_executable.wstring().c_str(), mutable_command.data(), nullptr, nullptr,
                                        TRUE, CREATE_NO_WINDOW, nullptr, build_directory.wstring().c_str(), &startup, &process);
    CloseHandle(output);
    if (!created) {
        std::error_code cleanup;
        std::filesystem::remove(output_path, cleanup);
        exit_code = -1;
        return "Could not start CTest (" + std::to_string(GetLastError()) + ").";
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD result = 1;
    GetExitCodeProcess(process.hProcess, &result);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    std::ifstream file(output_path);
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::error_code cleanup;
    std::filesystem::remove(output_path, cleanup);
    exit_code = static_cast<int>(result);
    return text;
}
}

std::string CommandResponse::to_json() const {
    std::ostringstream out;
    out << "{\"schemaVersion\":1,\"exitCode\":" << static_cast<int>(exit_code)
        << ",\"summary\":" << quote(summary) << ",\"changedObjectIds\":[";
    for (std::size_t i = 0; i < changed_object_ids.size(); ++i) {
        if (i) out << ',';
        out << quote(changed_object_ids[i]);
    }
    out << "],\"diagnostics\":[";
    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
        if (i) out << ',';
        out << diagnostics[i].to_json();
    }
    out << "],\"metrics\":{";
    std::size_t index = 0;
    for (const auto& entry : metrics) {
        if (index++) out << ',';
        out << quote(entry.first) << ':' << entry.second;
    }
    out << "},\"metadata\":{";
    index = 0;
    for (const auto& entry : metadata) {
        if (index++) out << ',';
        out << quote(entry.first) << ':' << quote(entry.second);
    }
    out << "},\"artifacts\":[";
    for (std::size_t i = 0; i < artifacts.size(); ++i) {
        if (i) out << ',';
        out << quote(artifacts[i]);
    }
    return out.str() + "]}";
}

Result<CommandRequest> parse_command_line(int argc, char** argv) {
    const auto correlation = make_correlation_id();
    if (argc < 2) return Result<CommandRequest>::failure(command_error(
        "CLI-MISSING-COMMAND", "No command was provided.", "Run engine help.", correlation));
    CommandRequest request;
    request.name = argv[1];
    request.correlation_id = correlation;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--json") request.json = true;
        else if (arg == "--dry-run") request.dry_run = true;
        else if (arg == "--project" && i + 1 < argc) request.project = argv[++i];
        else request.arguments.push_back(arg);
    }
    return Result<CommandRequest>::success(std::move(request));
}

CommandResponse execute_command(const CommandRequest& request) {
    if (request.name == "help") return {ExitCode::Success, command_help(), {}, {}};
    const std::vector<std::string> known{"build-assets", "validate", "inspect", "run", "test", "benchmark", "capture",
        "editor", "mcp", "project-git", "build-coordination", "animation-preview", "visual-regression", "asset-bake",
        "asset-import"};
    if (std::find(known.begin(), known.end(), request.name) == known.end()) {
        auto error = command_error("CLI-UNKNOWN-COMMAND", "Unknown command: " + request.name,
                                   "Run engine help for supported commands.", request.correlation_id);
        return {ExitCode::InvalidArguments, "Command rejected", {}, {std::move(error)}};
    }
    if (request.name != "help" && request.project.empty()) {
        auto error = command_error("CLI-PROJECT-REQUIRED", "--project is required for " + request.name,
                                   "Pass --project <directory>.", request.correlation_id);
        return {ExitCode::InvalidArguments, "Command rejected", {}, {std::move(error)}};
    }
    if (request.name == "project-git") {
        nlohmann::json params = nlohmann::json::object();
        std::string action = argument_value(request, "--action", argument_value(request, "-a"));
        if (action.empty()) {
            for (std::size_t i = 0; i < request.arguments.size(); ++i) {
                const auto& arg = request.arguments[i];
                if (arg == "--action" || arg == "-a" || arg == "--message" || arg == "-m") {
                    if (i + 1 < request.arguments.size()) ++i;
                    continue;
                }
                if (arg.empty() || arg[0] == '-') continue;
                action = arg;
                break;
            }
        }
        params["action"] = action;
        const auto message = argument_value(request, "--message", argument_value(request, "-m"));
        if (!message.empty()) params["message"] = message;
        const auto bridge = apply_project_git_operation(request.project, params);
        CommandResponse response;
        response.exit_code = bridge.exit_code;
        response.summary = bridge.summary;
        response.diagnostics = bridge.diagnostics;
        response.metadata = bridge.metadata;
        response.changed_object_ids = bridge.changed_object_ids;
        return response;
    }
    if (request.name == "asset-bake") {
        nlohmann::json params = nlohmann::json::object();
        if (argument_value(request, "--list", "false") == "true" ||
            std::find(request.arguments.begin(), request.arguments.end(), "--list") != request.arguments.end()) {
            params["action"] = "list";
        } else {
            params["action"] = "bake";
            const auto target = argument_value(request, "--target");
            if (!target.empty()) params["target"] = target;
            const auto source = argument_value(request, "--source");
            if (!source.empty()) params["source"] = source;
        }
        const auto bridge = apply_asset_bake_operation(request.project, params);
        CommandResponse response;
        response.exit_code = bridge.exit_code;
        response.summary = bridge.summary;
        response.diagnostics = bridge.diagnostics;
        response.metadata = bridge.metadata;
        response.changed_object_ids = bridge.changed_object_ids;
        if (bridge.metadata.count("reportJson")) response.artifacts.push_back(bridge.metadata.at("reportJson"));
        return response;
    }
    if (request.name == "asset-import") {
        CommandResponse response;
        const auto file = argument_value(request, "--file", argument_value(request, "--source"));
        if (file.empty()) {
            response.exit_code = ExitCode::InvalidArguments;
            response.summary = "--file is required";
            response.diagnostics.push_back(asset_bake_error("ASSET-IMPORT-SOURCE-MISSING",
                "--file <path to .gltf/.glb> is required", "Pass the model file to import."));
            return response;
        }
        // --target pins the import onto a registered asset (the editor's "Replace..." on a browser row).
        const auto pinned = argument_value(request, "--target");
        const auto plan = pinned.empty() ? plan_asset_import(request.project, std::filesystem::path(file),
                                               argument_value(request, "--id"))
                                         : plan_asset_replace(request.project, pinned,
                                               std::filesystem::path(file));
        response.metadata["targetId"] = plan.target_id;
        response.metadata["kind"] = plan.kind;
        response.metadata["existingTarget"] = plan.existing_target ? "true" : "false";
        response.metadata["match"] = to_string(plan.match);
        response.metadata["meshOutput"] = plan.mesh_output;
        response.metadata["clipCount"] = std::to_string(plan.clip_names.size());
        response.metadata["prefabPath"] = plan.prefab_path;
        response.metadata["existingPrefab"] = plan.existing_prefab ? "true" : "false";
        const bool plan_only =
            std::find(request.arguments.begin(), request.arguments.end(), "--plan") != request.arguments.end();
        if (!plan.supported || plan_only) {
            response.exit_code = plan.supported ? ExitCode::Success : ExitCode::ValidationFailed;
            response.summary = plan.supported
                ? (plan.existing_target ? "would update " + plan.target_id : "would import " + plan.target_id)
                : "cannot import " + file;
            response.diagnostics = plan.diagnostics;
            return response;
        }
        const auto outcome = run_asset_import(request.project, plan);
        response.exit_code = outcome.ok ? ExitCode::Success : ExitCode::ValidationFailed;
        response.summary = outcome.summary;
        response.diagnostics = outcome.diagnostics;
        response.metadata["prefabPath"] = outcome.prefab_path;
        response.metadata["registeredTarget"] = outcome.registered_target ? "true" : "false";
        response.metadata["reportJson"] = outcome.report_json;
        for (const auto& mesh : outcome.mesh_reloads) response.changed_object_ids.push_back(mesh);
        return response;
    }
    if (request.name == "build-coordination") {
        nlohmann::json params = nlohmann::json::object();
        std::string action = argument_value(request, "--action", argument_value(request, "-a"));
        if (action.empty()) {
            for (std::size_t i = 0; i < request.arguments.size(); ++i) {
                const auto& arg = request.arguments[i];
                if (!arg.empty() && arg[0] == '-') {
                    // Skip flag values so `--agent alice` is never mistaken for the action.
                    if (arg != "--force" && i + 1 < request.arguments.size()) ++i;
                    continue;
                }
                if (arg.empty()) continue;
                action = arg;
                break;
            }
        }
        params["action"] = action;
        const auto agent = argument_value(request, "--agent");
        if (!agent.empty()) params["agentId"] = agent;
        const auto ticket = argument_value(request, "--ticket");
        if (!ticket.empty()) params["ticketId"] = ticket;
        const auto summary = argument_value(request, "--summary");
        if (!summary.empty()) params["summary"] = summary;
        const auto token = argument_value(request, "--token");
        if (!token.empty()) params["token"] = token;
        const auto command_text = argument_value(request, "--command");
        if (!command_text.empty()) params["command"] = command_text;
        const auto lease_seconds = argument_value(request, "--lease-seconds");
        if (!lease_seconds.empty()) params["leaseSeconds"] = std::atoll(lease_seconds.c_str());
        const auto timeout_seconds = argument_value(request, "--timeout-seconds");
        if (!timeout_seconds.empty()) params["timeoutSeconds"] = std::atoll(timeout_seconds.c_str());
        if (std::find(request.arguments.begin(), request.arguments.end(), "--force") != request.arguments.end())
            params["force"] = true;
        const auto bridge = apply_build_coordination_operation(request.project, params);
        CommandResponse response;
        response.exit_code = bridge.exit_code;
        response.summary = bridge.summary;
        response.diagnostics = bridge.diagnostics;
        response.metadata = bridge.metadata;
        return response;
    }
    if (!std::filesystem::exists(request.project / "project.engine.json")) {
        auto error = command_error("PROJECT-MANIFEST-MISSING", "project.engine.json was not found",
                                   "Create the project manifest or correct --project.", request.correlation_id);
        error.category = ErrorCategory::Validation;
        return {ExitCode::ValidationFailed, "Project validation failed", {}, {std::move(error)}};
    }
    std::ifstream manifest(request.project / "project.engine.json");
    const std::string manifest_text((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
    nlohmann::json manifest_json;
    try { manifest_json = nlohmann::json::parse(manifest_text); }
    catch (const std::exception& exception) {
        auto error = command_error("PROJECT-MANIFEST-INVALID", "project.engine.json is not valid JSON",
                                   "Correct the manifest syntax.", request.correlation_id);
        error.category = ErrorCategory::Serialization;
        error.causes.push_back(exception.what());
        return {ExitCode::ValidationFailed, "Project validation failed", {}, {std::move(error)}};
    }
    if (!manifest || manifest_json.value("schemaVersion", 0) != 1 ||
        !manifest_json.contains("projectId") || !manifest_json["projectId"].is_string() ||
        !manifest_json.contains("name") || !manifest_json["name"].is_string()) {
        auto error = command_error("PROJECT-MANIFEST-INVALID",
                                   "project.engine.json is unreadable or missing required fields",
                                   "Provide schemaVersion, projectId, and name fields.", request.correlation_id);
        error.category = ErrorCategory::Validation;
        return {ExitCode::ValidationFailed, "Project validation failed", {}, {std::move(error)}};
    }
    AssetRegistry assets;
    auto scanned = assets.scan(request.project);
    if (!scanned) return {ExitCode::ValidationFailed, "Asset scan failed", {}, {scanned.error()}};
    auto asset_errors = assets.validate();
    for (const auto& entry : assets.records()) {
        const auto& path = entry.second.path;
        constexpr const char* suffix = ".material.json";
        if (path.size() >= std::char_traits<char>::length(suffix) && path.compare(path.size() - std::char_traits<char>::length(suffix), std::char_traits<char>::length(suffix), suffix) == 0) {
            auto material = MaterialAsset::load(request.project / path);
            if (!material) asset_errors.push_back(material.error());
            else if (const auto maps = material.value().validate_texture_maps(request.project); !maps)
                asset_errors.push_back(maps.error());
        }
        constexpr const char* particle_suffix = ".particle.json";
        if (path.size() >= std::char_traits<char>::length(particle_suffix) &&
            path.compare(path.size() - std::char_traits<char>::length(particle_suffix),
                std::char_traits<char>::length(particle_suffix), particle_suffix) == 0) {
            auto particle = ParticleEmitterAsset::load(request.project / path);
            if (!particle) asset_errors.push_back(particle.error());
            else if (const auto texture = particle.value().validate_texture(request.project); !texture)
                asset_errors.push_back(texture.error());
        }
        const auto extension=std::filesystem::path(path).extension().string();if(extension==".gltf"||extension==".glb"){auto mesh=import_project_mesh(request.project/path);if(!mesh&&mesh.error().code!="MESH-ANIMATION-ONLY")asset_errors.push_back(mesh.error());}
    }
    const auto terrain_edit_valid = TerrainEditStore::validate_file(default_terrain_edits_path(request.project));
    if (!terrain_edit_valid) asset_errors.push_back(terrain_edit_valid.error());
    const auto terrain_paint_valid = TerrainPaintStore::validate_file(default_terrain_paint_path(request.project));
    if (!terrain_paint_valid) asset_errors.push_back(terrain_paint_valid.error());
    const auto foliage_layers_path = default_foliage_layers_path(request.project);
    const auto foliage_layers_valid = FoliageLayerPalette::validate_file(foliage_layers_path);
    if (!foliage_layers_valid) asset_errors.push_back(foliage_layers_valid.error());
    std::uint8_t max_foliage_layer_index = 0;
    if (std::filesystem::exists(foliage_layers_path)) {
        if (const auto foliage_palette = FoliageLayerPalette::load(foliage_layers_path);
            foliage_palette && !foliage_palette.value().layers.empty())
            max_foliage_layer_index =
                static_cast<std::uint8_t>(foliage_palette.value().layers.size() - 1);
    }
    const auto foliage_density_valid =
        FoliageDensityStore::validate_file(default_foliage_density_path(request.project), max_foliage_layer_index);
    if (!foliage_density_valid) asset_errors.push_back(foliage_density_valid.error());
    const auto world_forge_factions_path = default_world_forge_factions_path(request.project);
    const auto world_forge_factions_valid = WorldForgeFactionsAsset::validate_file(world_forge_factions_path);
    if (!world_forge_factions_valid) asset_errors.push_back(world_forge_factions_valid.error());
    const auto world_forge_pantheon_valid =
        WorldForgePantheonAsset::validate_file(default_world_forge_pantheon_path(request.project));
    if (!world_forge_pantheon_valid) asset_errors.push_back(world_forge_pantheon_valid.error());
    std::unordered_set<std::string> world_forge_faction_ids;
    if (std::filesystem::exists(world_forge_factions_path)) {
        if (const auto factions = WorldForgeFactionsAsset::load(world_forge_factions_path); factions) {
            for (const auto& entity : factions.value().entities) world_forge_faction_ids.insert(entity.id);
        }
    }
    const auto world_forge_archetypes_valid = WorldForgeArchetypesAsset::validate_file(
        default_world_forge_archetypes_path(request.project), world_forge_faction_ids);
    if (!world_forge_archetypes_valid) asset_errors.push_back(world_forge_archetypes_valid.error());
    const auto world_forge_relationships_valid = WorldForgeRelationshipsAsset::validate_file(
        default_world_forge_relationships_path(request.project), world_forge_faction_ids);
    if (!world_forge_relationships_valid) asset_errors.push_back(world_forge_relationships_valid.error());
    const auto world_forge_map_valid =
        WorldForgeMapAsset::validate_file(default_world_forge_map_path(request.project), world_forge_faction_ids);
    if (!world_forge_map_valid) asset_errors.push_back(world_forge_map_valid.error());
    std::unordered_set<std::string> world_forge_region_ids;
    if (std::filesystem::exists(default_world_forge_map_path(request.project))) {
        if (const auto map = WorldForgeMapAsset::load(default_world_forge_map_path(request.project)); map) {
            for (const auto& region : map.value().regions) world_forge_region_ids.insert(region.id);
        }
    }
    const auto world_forge_resources_valid = WorldForgeResourcesAsset::validate_file(
        default_world_forge_resources_path(request.project), world_forge_region_ids);
    if (!world_forge_resources_valid) asset_errors.push_back(world_forge_resources_valid.error());
    const auto world_forge_quests_valid =
        WorldForgeQuestsAsset::validate_file(default_world_forge_quests_path(request.project), world_forge_region_ids);
    if (!world_forge_quests_valid) asset_errors.push_back(world_forge_quests_valid.error());
    std::unordered_set<std::string> world_forge_quest_ids;
    if (std::filesystem::exists(default_world_forge_quests_path(request.project))) {
        if (const auto quests = WorldForgeQuestsAsset::load(default_world_forge_quests_path(request.project));
            quests) {
            for (const auto& quest : quests.value().quests) world_forge_quest_ids.insert(quest.id);
        }
    }
    const auto world_forge_dialogues_valid = WorldForgeDialoguesAsset::validate_file(
        default_world_forge_dialogues_path(request.project), world_forge_quest_ids);
    if (!world_forge_dialogues_valid) asset_errors.push_back(world_forge_dialogues_valid.error());
    std::unordered_set<std::string> world_forge_dialogue_ids;
    {
        if (const auto dialogues =
                WorldForgeDialoguesAsset::load(default_world_forge_dialogues_path(request.project));
            dialogues) {
            for (const auto& tree : dialogues.value().trees) world_forge_dialogue_ids.insert(tree.id);
        }
    }
    const auto world_forge_events_valid = WorldForgeEventsAsset::validate_file(
        default_world_forge_events_path(request.project), world_forge_dialogue_ids);
    if (!world_forge_events_valid) asset_errors.push_back(world_forge_events_valid.error());
    const auto world_forge_mvp_valid =
        WorldForgeMvpReadinessAsset::validate_file(default_world_forge_mvp_readiness_path(request.project));
    if (!world_forge_mvp_valid) asset_errors.push_back(world_forge_mvp_valid.error());
    std::size_t entity_count = 0;
    const auto world_path = request.project / manifest_json.value("defaultWorld", std::string{});
    if (!manifest_json.value("defaultWorld", std::string{}).empty()) {
        auto scene = Scene::load(world_path);
        if (!scene) return {ExitCode::ValidationFailed, "World validation failed", {}, {scene.error()}};
        entity_count = scene.value().size();
        const auto world_errors = scene.value().validate();
        asset_errors.insert(asset_errors.end(), world_errors.begin(), world_errors.end());
    }
    if (request.name == "validate") {
        if (!asset_errors.empty()) return {ExitCode::ValidationFailed, "Project validation failed", {}, std::move(asset_errors)};
        CommandResponse response{ExitCode::Success, "Project manifest, world, hierarchy, and asset dependencies are valid", {}, {}};
        response.metrics = {{"assets", static_cast<double>(assets.records().size())}, {"entities", static_cast<double>(entity_count)}};
        return response;
    }
    if (request.name == "inspect") {
        CommandResponse response{asset_errors.empty() ? ExitCode::Success : ExitCode::ValidationFailed,
                                 asset_errors.empty() ? "Project inspection complete" : "Project inspection found errors", {}, std::move(asset_errors)};
        response.metrics = {{"assets", static_cast<double>(assets.records().size())}, {"entities", static_cast<double>(entity_count)}};
        response.metadata = {{"defaultWorld", world_path.generic_string()}, {"projectName", manifest_json["name"].get<std::string>()}};
        return response;
    }
    if (request.name == "build-assets" && request.dry_run) {
        CommandResponse response{asset_errors.empty() ? ExitCode::Success : ExitCode::ValidationFailed,
                                 asset_errors.empty() ? "Asset dry run complete" : "Asset dependencies are invalid", {}, std::move(asset_errors)};
        response.metrics = {{"assets", static_cast<double>(assets.records().size())}};
        return response;
    }
    if (request.name == "build-assets") {
        if (!asset_errors.empty()) return {ExitCode::ValidationFailed, "Asset dependencies are invalid", {}, std::move(asset_errors)};
        const auto database_path = request.project / "out" / "assets" / "registry.json";
        auto written = assets.write_database_if_changed(database_path);
        if (!written) return {ExitCode::InternalError, "Asset database write failed", {}, {written.error()}};
        CommandResponse response{ExitCode::Success, written.value() ? "Asset database rebuilt" : "Asset database is current", {}, {}};
        response.metrics = {{"assets", static_cast<double>(assets.records().size())}, {"changed", written.value() ? 1.0 : 0.0}};
        response.artifacts.push_back(database_path.generic_string());
        return response;
    }
    if (request.name == "test") {
        const std::string suite = argument_value(request, "--suite");
        if (suite.empty()) {
            auto error = command_error("CLI-TEST-SUITE-REQUIRED", "test requires --suite <name>",
                                       "Run engine help for the supported suite names.", request.correlation_id);
            error.category = ErrorCategory::Validation;
            return {ExitCode::InvalidArguments, "Test command rejected", {}, {std::move(error)}};
        }
        if (!known_ctest_suite(suite)) {
            auto error = command_error("CLI-TEST-SUITE-UNKNOWN", "Unknown CTest suite: " + suite,
                                       "Use one of the suite names shown by engine help.", request.correlation_id);
            error.category = ErrorCategory::Validation;
            return {ExitCode::InvalidArguments, "Test command rejected", {}, {std::move(error)}};
        }
        const std::vector<std::string> suites_to_run =
            suite == "m5-exit" ? m5_exit_suite_names() : std::vector<std::string>{suite};
        const auto build_directory = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / "build" / "windows-msvc-debug";
        if (!std::filesystem::exists(build_directory / "CTestTestfile.cmake")) {
            auto error = command_error("CLI-TEST-BUILD-MISSING", "CTest build tree was not found: " + build_directory.generic_string(),
                                       "Configure and build the Windows debug preset before running engine test.", request.correlation_id);
            error.category = ErrorCategory::Configuration;
            return {ExitCode::Unavailable, "Test command unavailable", {}, {std::move(error)}};
        }
        CommandResponse response{ExitCode::Success,
            suite == "m5-exit" ? "M5 exit gate scheduled" : "CTest suite scheduled: " + suite, {}, {}};
        response.metrics = {{"testCount", static_cast<double>(suites_to_run.size())}};
        response.metadata = {{"suite", suite}, {"buildDirectory", build_directory.generic_string()}};
        if (request.dry_run) return response;

        const auto started = std::chrono::steady_clock::now();
        const auto ctest_executable = find_ctest_executable();
        std::ostringstream combined_output;
        int failed_count = 0;
        for (const auto& run_suite : suites_to_run) {
            int ctest_exit_code = -1;
            const std::string output = run_ctest(ctest_executable, build_directory, run_suite, ctest_exit_code);
            response.metadata["suite:" + run_suite + ":exitCode"] = std::to_string(ctest_exit_code);
            if (ctest_exit_code != 0) {
                ++failed_count;
                combined_output << "=== " << run_suite << " failed (exit " << ctest_exit_code << ") ===\n" << output << '\n';
            }
        }
        const double elapsed_milliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        response.metrics["elapsedMilliseconds"] = elapsed_milliseconds;
        response.metrics["failedSuites"] = static_cast<double>(failed_count);
        response.metadata["ctestExecutable"] = ctest_executable.generic_string();
        if (failed_count == 0) {
            response.summary = suite == "m5-exit" ? "M5 exit gate passed" : "CTest suite passed: " + suite;
            return response;
        }
        auto error = command_error("CLI-TEST-FAILED",
            suite == "m5-exit" ? "M5 exit gate failed (" + std::to_string(failed_count) + " suite(s))"
                               : "CTest suite failed: " + suite,
            "Review the captured CTest output and rerun the suite directly.", request.correlation_id);
        error.category = ErrorCategory::Validation;
        const std::string output = combined_output.str();
        if (!output.empty()) error.causes.push_back(output);
        response.exit_code = ExitCode::ValidationFailed;
        response.summary = error.message;
        response.diagnostics.push_back(std::move(error));
        return response;
    }
    if (request.name == "animation-preview") {
        AnimationPreviewRequest preview;
        preview.project_root = request.project;
        preview.controller_path = argument_value(request, "--controller");
        preview.entity_id = argument_value(request, "--entity", "animation-preview-entity");
        preview.frames = positive_number(argument_value(request, "--frames"), 60);
        preview.dt_seconds = static_cast<float>(std::atof(argument_value(request, "--dt", "0.0166667").c_str()));
        preview.speed = static_cast<float>(std::atof(argument_value(request, "--speed", "0.5").c_str()));
        preview.fire_attack_trigger = argument_value(request, "--no-trigger", "false") != "true";
        if (request.dry_run) {
            CommandResponse response{ExitCode::Success, "Animation preview scheduled", {}, {}};
            response.metadata = {{"controller", preview.controller_path.empty() ? "(auto)" : preview.controller_path},
                {"frames", std::to_string(preview.frames)}};
            return response;
        }
        auto result = run_animation_preview(preview);
        CommandResponse response;
        if (!result) {
            response.exit_code = ExitCode::ValidationFailed;
            response.summary = "Animation preview failed";
            response.diagnostics.push_back(result.error());
            return response;
        }
        const auto& report = result.value();
        response.exit_code = report.ok ? ExitCode::Success : ExitCode::ValidationFailed;
        response.summary = report.ok
            ? "Animation preview: " + report.initial_state + " -> " + report.final_state
            : "Animation preview failed";
        response.metadata = {{"controller", report.controller_path},
            {"initialState", report.initial_state}, {"finalState", report.final_state},
            {"totalEvents", std::to_string(report.total_events)},
            {"totalRootMotionZ", std::to_string(report.total_root_motion_z)},
            {"previewJson", report.to_json()}};
        if (request.json) response.artifacts.push_back(report.to_json());
        return response;
    }
    if (request.name == "visual-regression") {
        const bool update_baselines = has_argument(request, "--update-baselines");
        double threshold = 12.0;
        try {
            const auto raw = argument_value(request, "--threshold");
            if (!raw.empty()) threshold = std::stod(raw);
        } catch (...) {
        }
        const std::uint32_t width = positive_number(argument_value(request, "--width"), 1280u);
        const std::uint32_t height = positive_number(argument_value(request, "--height"), 720u);
        const std::uint32_t frames = positive_number(argument_value(request, "--frames"), 40u);
        const auto baseline_dir =
            std::filesystem::path(ENGINE_REPOSITORY_ROOT) / "context" / "testing" / "baselines" / "visual-regression";
        const auto actual_dir = request.project / "out" / "visual-regression";
        struct Case {
            const char* name;
            float look_dx;
            float look_dy;
        };
        const Case cases[] = {{"game-default", 0.0f, 0.0f}, {"game-look", 180.0f, 40.0f}};

        if (request.dry_run) {
            CommandResponse response{ExitCode::Success, "Visual regression scheduled", {}, {}};
            response.metadata = {{"baselineDir", baseline_dir.generic_string()},
                {"actualDir", actual_dir.generic_string()}, {"threshold", std::to_string(threshold)}};
            return response;
        }

        std::error_code ec;
        std::filesystem::create_directories(actual_dir, ec);
        if (update_baselines) std::filesystem::create_directories(baseline_dir, ec);

        CommandResponse response{ExitCode::Success, "Visual regression passed", {}, {}};
        response.metadata["baselineDir"] = baseline_dir.generic_string();
        response.metadata["actualDir"] = actual_dir.generic_string();
        response.metadata["threshold"] = std::to_string(threshold);
        response.metrics["threshold"] = threshold;
        double worst_mean = 0.0;
        int failed_cases = 0;

        for (const auto& shot : cases) {
            const auto actual_path = actual_dir / (std::string(shot.name) + ".png");
            const auto baseline_path = baseline_dir / (std::string(shot.name) + ".png");

            // Spawn a child editor capture so hidden frame-limit hard-exit does not kill this harness.
            wchar_t module_path[MAX_PATH]{};
            if (GetModuleFileNameW(nullptr, module_path, MAX_PATH) == 0) {
                response.exit_code = ExitCode::InternalError;
                response.summary = "Could not resolve engine.exe path for visual-regression child";
                response.diagnostics.push_back(command_error("VREG-ENGINE-PATH", response.summary,
                    "Run engine.exe directly from the build tree.", request.correlation_id));
                return response;
            }
            std::ostringstream args;
            args << "\"" << std::filesystem::path(module_path).string() << "\" editor"
                 << " --project \"" << request.project.string() << "\""
                 << " --viewport game --hidden true"
                 << " --width " << width << " --height " << height
                 << " --frames " << frames
                 << " --output \"" << actual_path.string() << "\"";
            if (shot.look_dx != 0.0f || shot.look_dy != 0.0f) {
                args << " --look-dx " << shot.look_dx << " --look-dy " << shot.look_dy
                     << " --look-at-frame " << std::min<std::uint32_t>(20u, frames > 5 ? frames - 5 : frames);
            }
            std::string cmdline = args.str();
            std::vector<char> mutable_cmd(cmdline.begin(), cmdline.end());
            mutable_cmd.push_back('\0');
            STARTUPINFOA si{};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            const BOOL created = CreateProcessA(nullptr, mutable_cmd.data(), nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
            if (!created) {
                response.exit_code = ExitCode::InternalError;
                response.summary = "Visual regression capture spawn failed: " + std::string(shot.name);
                response.diagnostics.push_back(command_error("VREG-SPAWN", response.summary,
                    "Ensure engine.exe can launch editor captures.", request.correlation_id));
                return response;
            }
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD child_exit = 1;
            GetExitCodeProcess(pi.hProcess, &child_exit);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if (child_exit != 0 || !std::filesystem::exists(actual_path)) {
                response.exit_code = ExitCode::InternalError;
                response.summary = "Visual regression capture failed: " + std::string(shot.name);
                response.diagnostics.push_back(command_error("VREG-CAPTURE",
                    response.summary + " (child exit " + std::to_string(child_exit) + ")",
                    "Inspect editor capture output and GPU availability.", request.correlation_id));
                return response;
            }
            response.artifacts.push_back(actual_path.generic_string());

            if (update_baselines) {
                std::filesystem::copy_file(actual_path, baseline_path,
                    std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) {
                    response.exit_code = ExitCode::InternalError;
                    response.summary = "Could not write baseline: " + baseline_path.generic_string();
                    response.diagnostics.push_back(command_error("VREG-BASELINE-WRITE", response.summary,
                        "Check permissions on context/testing/baselines/visual-regression/.", request.correlation_id));
                    return response;
                }
                response.artifacts.push_back(baseline_path.generic_string());
                continue;
            }

            if (!std::filesystem::exists(baseline_path)) {
                response.exit_code = ExitCode::ValidationFailed;
                response.summary = "Missing baseline: " + baseline_path.generic_string();
                response.diagnostics.push_back(command_error("VREG-BASELINE-MISSING", response.summary,
                    "Run engine visual-regression --project <path> --update-baselines once on the reference GPU.",
                    request.correlation_id));
                return response;
            }

            const auto actual = decode_png_file(actual_path);
            const auto baseline = decode_png_file(baseline_path);
            if (!actual || !baseline) {
                response.exit_code = ExitCode::InternalError;
                response.summary = "Could not decode PNG for " + std::string(shot.name);
                if (!actual) response.diagnostics.push_back(actual.error());
                if (!baseline) response.diagnostics.push_back(baseline.error());
                return response;
            }
            if (actual.value().width != baseline.value().width || actual.value().height != baseline.value().height) {
                ++failed_cases;
                response.diagnostics.push_back(command_error("VREG-SIZE-MISMATCH",
                    std::string(shot.name) + " size mismatch",
                    "Refresh baselines at the same --width/--height.", request.correlation_id));
                continue;
            }
            const auto diff =
                mean_abs_rgb_diff(actual.value().width, actual.value().height, actual.value().rgba, baseline.value().rgba);
            if (!diff) {
                response.exit_code = ExitCode::InternalError;
                response.summary = "Image diff failed: " + std::string(shot.name);
                response.diagnostics.push_back(diff.error());
                return response;
            }
            response.metrics[std::string(shot.name) + ".meanAbsRgb"] = diff.value().mean_abs_rgb;
            response.metrics[std::string(shot.name) + ".maxAbsRgb"] = diff.value().max_abs_rgb;
            worst_mean = std::max(worst_mean, diff.value().mean_abs_rgb);
            if (diff.value().mean_abs_rgb > threshold) {
                ++failed_cases;
                response.diagnostics.push_back(command_error("VREG-THRESHOLD",
                    std::string(shot.name) + " meanAbsRgb=" + std::to_string(diff.value().mean_abs_rgb) +
                        " exceeds threshold " + std::to_string(threshold),
                    "Inspect out/visual-regression vs context/testing/baselines/visual-regression; refresh with --update-baselines if intentional.",
                    request.correlation_id));
            }
        }

        response.metrics["worstMeanAbsRgb"] = worst_mean;
        response.metrics["failedCases"] = static_cast<double>(failed_cases);
        if (update_baselines) {
            response.summary = "Visual regression baselines updated";
            return response;
        }
        if (failed_cases > 0) {
            response.exit_code = ExitCode::ValidationFailed;
            response.summary = "Visual regression failed (" + std::to_string(failed_cases) + " case(s))";
            return response;
        }
        response.summary = "Visual regression passed; worstMeanAbsRgb=" + std::to_string(worst_mean);
        return response;
    }
    if (request.name == "run" || request.name == "capture" || request.name == "benchmark" || request.name == "editor") {
        RenderOptions options;
        options.project_root = request.project;
        options.width = positive_number(argument_value(request, "--width"), request.name == "benchmark" ? 2560u : 1280u);
        options.height = positive_number(argument_value(request, "--height"), request.name == "benchmark" ? 1440u : 720u);
        options.frame_limit = positive_number(argument_value(request, "--frames"),
            (request.name == "run" || request.name == "editor") ? 0u : (request.name == "capture" ? 1u : 120u));
        options.hidden = request.name == "benchmark" || argument_value(request, "--hidden", "false") == "true";
        // The D3D12 debug layer catches real API misuse, but its own worker
        // thread faults roughly 40s into an interactive editor session
        // (TICKET-0279), so the editor opts in rather than out. Short automated
        // sessions (run / capture) keep validation on by default.
        options.enable_debug_layer =
            request.name != "benchmark" &&
            (request.name == "editor" ? has_argument(request, "--debug-layer")
                                      : !has_argument(request, "--no-debug-layer"));
        // Benchmark loads the sample world (editor + debug_world) so terrain/foliage/placements are present.
        options.debug_world = has_argument(request, "--debug-world") || request.name == "editor" ||
            request.name == "benchmark";
        options.editor = request.name == "editor" || request.name == "benchmark";
        options.coop_local = has_argument(request, "--coop-local");
        options.attach_console = has_argument(request, "--console") || request.name == "editor";
        options.require_gpu_timestamps = request.name == "benchmark";
        options.cli_json = request.json;
        {
            const auto world_override = argument_value(request, "--world");
            if (!world_override.empty()) {
                const std::filesystem::path override_path(world_override);
                options.world_path = override_path.is_absolute() ? override_path : (request.project / override_path);
            } else if (request.name == "benchmark") {
                // Project defaultWorld is main-menu (preview, no play-test). The
                // 1440p gate needs a playable open-world slice (TICKET-0139).
                options.world_path = request.project / "worlds/vertical-slice.world.json";
            } else {
                options.world_path = world_path;
            }
        }
        options.initial_viewport = argument_value(request, "--viewport", request.name == "benchmark" ? "game" : "");
        if (request.name == "capture") options.capture_path = argument_value(request, "--output", "out/captures/frame.ppm");
        if (request.name == "editor" && has_argument(request, "--output")) options.capture_path = argument_value(request, "--output");
        options.capture_game_viewport =
            options.initial_viewport == "game" || has_argument(request, "--capture-game");
        try {
            const auto look_dx = argument_value(request, "--look-dx");
            const auto look_dy = argument_value(request, "--look-dy");
            if (!look_dx.empty()) options.capture_look_dx = static_cast<float>(std::stof(look_dx));
            if (!look_dy.empty()) options.capture_look_dy = static_cast<float>(std::stof(look_dy));
        } catch (...) {
        }
        if (has_argument(request, "--look-at-frame"))
            options.capture_look_at_frame = positive_number(argument_value(request, "--look-at-frame"), 20u);
        if (request.name == "benchmark") {
            const auto report = argument_value(request, "--report",
                (request.project / "out" / "benchmarks" / "open-world-1440p.json").generic_string());
            options.benchmark_report_path = report;
        }
        auto rendered = run_render_app(options);
        if (!rendered) return {ExitCode::InternalError, "Rendering failed", {}, {rendered.error()}};
        std::ostringstream summary;
        summary << (request.name == "capture" ? "Capture complete" : request.name == "benchmark" ? "Benchmark complete" : "Render session complete")
                << "; frames=" << rendered.value().frames
                << "; averageCpuMs=" << rendered.value().average_cpu_ms
                << "; averageGpuMs=" << rendered.value().average_gpu_ms
                << "; fps=" << rendered.value().frames_per_second
                << "; adapter=" << rendered.value().adapter
                << "; terrainCells=" << rendered.value().terrain_cells
                << "; drawCalls=" << rendered.value().draw_calls
                << "; instances=" << rendered.value().instances_drawn
                << "; gpuTimestampsOk=" << (rendered.value().gpu_timestamps_ok ? "true" : "false");
        if (!options.capture_path.empty()) summary << "; output=" << options.capture_path.generic_string();
        if (!options.benchmark_report_path.empty())
            summary << "; report=" << options.benchmark_report_path.generic_string();
        CommandResponse response{ExitCode::Success, summary.str(), {}, {}};
        response.metrics = {
            {"averageCpuMs", rendered.value().average_cpu_ms},
            {"averageGpuMs", rendered.value().average_gpu_ms},
            {"elapsedSeconds", rendered.value().elapsed_seconds},
            {"frames", static_cast<double>(rendered.value().frames)},
            {"framesPerSecond", rendered.value().frames_per_second},
            {"terrainCells", static_cast<double>(rendered.value().terrain_cells)},
            {"drawCalls", static_cast<double>(rendered.value().draw_calls)},
            {"instancesDrawn", static_cast<double>(rendered.value().instances_drawn)}
        };
        response.metadata["adapter"] = rendered.value().adapter;
        response.metadata["gpuTimestampsOk"] = rendered.value().gpu_timestamps_ok ? "true" : "false";
        if (!options.capture_path.empty()) response.artifacts.push_back(options.capture_path.generic_string());
        if (!options.benchmark_report_path.empty())
            response.artifacts.push_back(options.benchmark_report_path.generic_string());
        return response;
    }
    auto error = command_error("FEATURE-NOT-IMPLEMENTED", request.name + " is reserved but not implemented in milestone 1",
                               "Use validate, inspect, or build-assets --dry-run until its milestone lands.", request.correlation_id);
    return {ExitCode::Unavailable, "Feature unavailable", {}, {std::move(error)}};
}

std::string command_help() {
    return "AI RPG Engine 0.2.0\nCommands: build-assets, validate, inspect, run, test, benchmark, capture, editor, mcp, project-git, build-coordination, animation-preview, visual-regression, asset-bake, asset-import\n"
           "Options: --project <path> [--world <path>] --json --dry-run --debug-world --coop-local --log-file <path> --frames <n> --width <px> --height <px> --console\n"
           "  --world overrides project.engine.json defaultWorld (relative to project or absolute)\n"
           "Capture/editor: --output <file.ppm|.png> [--viewport scene|sculpt|game|ui|world-forge] [--look-dx N] [--look-dy N]\n"
           "Benchmark: defaults to 120 frames at 2560x1440, hidden editor+debug-world, GPU timestamps required\n"
           "  engine benchmark --project <path> [--frames 120] [--world worlds/vertical-slice.world.json] [--report out/benchmarks/open-world-1440p.json] [--json]\n"
           "Visual regression (TICKET-0145): Game RT PNGs vs context/testing/baselines/visual-regression/\n"
           "  engine visual-regression --project <path> [--update-baselines] [--threshold 12] [--frames 40] [--json]\n"
           "Test: engine test --project <path> --suite <core|world|...|animator|audio|m5-exit|visual_regression|project_validation> [--dry-run] [--json]\n"
           "  m5-exit runs animator+character+interaction+combat+scripting (M5 exit gate, TICKET-0110)\n"
           "Animation preview: engine animation-preview --project <path> [--controller <path>] [--frames 60] [--speed 0.5] [--no-trigger] [--json]\n"
           "Asset bake (TICKET-0245): engine asset-bake --project <path> --target <id> [--source <path>] [--json]\n"
           "  engine asset-bake --project <path> --list [--json]  (named targets only; fail-closed verify gates)\n"
           "Asset import: engine asset-import --project <path> --file <model.gltf|.glb> [--id <slug>] [--target <id>] [--plan] [--json]\n"
           "  Updates a registered target when the file matches one, otherwise registers it and writes a prefab\n"
           "  --target <id> pins the import onto that registered asset (editor Assets -> Replace...)\n"
           "project-git: engine project-git --project <path> --action status|fetch|pull|commit|push [--message <text>] [--json]\n"
           "build-coordination (TICKET-0228): engine build-coordination --project <path> --action status|acquire|wait|release|heartbeat|clear-stale\n"
           "  [--agent <id>] [--ticket TICKET-####] [--summary <text>] [--token <token>] [--lease-seconds N] [--timeout-seconds N] [--force] [--json]\n"
           "  Agents acquire the shared rebuild lease before MSBuild; wait queues FIFO; release with the granted token\n"
           "Sandbox: engine editor --project samples/open-world-rpg --world worlds/sandbox.world.json\n"
           "MCP: engine mcp --project <path> starts the Model Context Protocol stdio server";
}

} // namespace engine
