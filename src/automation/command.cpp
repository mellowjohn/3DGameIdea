#include "engine/automation/command.h"
#include "engine/automation/project_git_commands.h"
#include "engine/animation/animation_preview.h"
#include "engine/rendering/render_app.h"
#include "engine/assets/asset_registry.h"
#include "engine/assets/material_asset.h"
#include "engine/assets/mesh_asset.h"
#include "engine/assets/world_forge_archetypes_asset.h"
#include "engine/assets/world_forge_factions_asset.h"
#include "engine/assets/world_forge_pantheon_asset.h"
#include "engine/assets/world_forge_relationships_asset.h"
#include "engine/assets/world_forge_resources_asset.h"
#include "engine/assets/world_forge_map_asset.h"
#include "engine/assets/world_forge_dialogues_asset.h"
#include "engine/assets/world_forge_quests_asset.h"
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
        "scripting", "automation", "hud", "animator", "audio", "project_validation"};
    return names;
}

bool known_ctest_suite(const std::string& name) {
    if (name == "m5-exit") return true;
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
        "editor", "mcp", "project-git", "animation-preview"};
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
        }
        const auto extension=std::filesystem::path(path).extension().string();if(extension==".gltf"||extension==".glb"){auto mesh=import_project_mesh(request.project/path);if(!mesh)asset_errors.push_back(mesh.error());}
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
    if (request.name == "run" || request.name == "capture" || request.name == "benchmark" || request.name == "editor") {
        RenderOptions options;
        options.project_root = request.project;
        options.width = positive_number(argument_value(request, "--width"), request.name == "benchmark" ? 2560u : 1280u);
        options.height = positive_number(argument_value(request, "--height"), request.name == "benchmark" ? 1440u : 720u);
        options.frame_limit = positive_number(argument_value(request, "--frames"), (request.name == "run" || request.name == "editor") ? 0u : (request.name == "capture" ? 1u : 300u));
        options.hidden = request.name == "benchmark" || argument_value(request, "--hidden", "false") == "true";
        options.enable_debug_layer = request.name != "benchmark";
        options.debug_world = has_argument(request, "--debug-world") || request.name == "editor";
        options.editor = request.name == "editor";
        options.attach_console = has_argument(request, "--console") || request.name == "editor";
        options.world_path = world_path;
        options.initial_viewport = argument_value(request, "--viewport", "");
        if (request.name == "capture") options.capture_path = argument_value(request, "--output", "out/captures/frame.ppm");
        if (request.name == "editor" && has_argument(request, "--output")) options.capture_path = argument_value(request, "--output");
        auto rendered = run_render_app(options);
        if (!rendered) return {ExitCode::InternalError, "Rendering failed", {}, {rendered.error()}};
        std::ostringstream summary;
        summary << (request.name == "capture" ? "Capture complete" : request.name == "benchmark" ? "Benchmark complete" : "Render session complete")
                << "; frames=" << rendered.value().frames
                << "; averageCpuMs=" << rendered.value().average_cpu_ms
                << "; averageGpuMs=" << rendered.value().average_gpu_ms
                << "; fps=" << rendered.value().frames_per_second
                << "; adapter=" << rendered.value().adapter;
        if (!options.capture_path.empty()) summary << "; output=" << options.capture_path.generic_string();
        CommandResponse response{ExitCode::Success, summary.str(), {}, {}};
        response.metrics = {
            {"averageCpuMs", rendered.value().average_cpu_ms},
            {"averageGpuMs", rendered.value().average_gpu_ms},
            {"elapsedSeconds", rendered.value().elapsed_seconds},
            {"frames", static_cast<double>(rendered.value().frames)},
            {"framesPerSecond", rendered.value().frames_per_second}
        };
        response.metadata["adapter"] = rendered.value().adapter;
        if (!options.capture_path.empty()) response.artifacts.push_back(options.capture_path.generic_string());
        return response;
    }
    auto error = command_error("FEATURE-NOT-IMPLEMENTED", request.name + " is reserved but not implemented in milestone 1",
                               "Use validate, inspect, or build-assets --dry-run until its milestone lands.", request.correlation_id);
    return {ExitCode::Unavailable, "Feature unavailable", {}, {std::move(error)}};
}

std::string command_help() {
    return "AI RPG Engine 0.2.0\nCommands: build-assets, validate, inspect, run, test, benchmark, capture, editor, mcp, project-git, animation-preview\n"
           "Options: --project <path> --json --dry-run --debug-world --log-file <path> --frames <n> --width <px> --height <px> --console\n"
           "Capture/editor: --output <file.ppm> [--viewport scene|sculpt|game|ui|world-forge]\n"
           "Benchmark: defaults to 300 frames at 2560x1440\n"
           "Test: engine test --project <path> --suite <core|world|...|animator|audio|m5-exit|project_validation> [--dry-run] [--json]\n"
           "  m5-exit runs animator+character+interaction+combat+scripting (M5 exit gate, TICKET-0110)\n"
           "Animation preview: engine animation-preview --project <path> [--controller <path>] [--frames 60] [--speed 0.5] [--no-trigger] [--json]\n"
           "project-git: engine project-git --project <path> --action status|fetch|pull|commit|push [--message <text>] [--json]\n"
           "MCP: engine mcp --project <path> starts the Model Context Protocol stdio server";
}

} // namespace engine
