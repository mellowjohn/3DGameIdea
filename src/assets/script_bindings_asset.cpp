#include "engine/assets/script_bindings_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>

namespace engine {

std::filesystem::path default_script_bindings_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "scripts" / "bindings.script.json";
}

Result<ScriptBindingsAsset> ScriptBindingsAsset::load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        ScriptBindingsAsset empty;
        return Result<ScriptBindingsAsset>::success(std::move(empty));
    }
    std::ifstream input(path);
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<ScriptBindingsAsset>::failure(EngineError{
                "SCRIPT-BINDINGS-INVALID", Severity::Error, ErrorCategory::Serialization, "scripting",
                "bindings.script.json must be an object", ENGINE_SOURCE_CONTEXT, {}, "Fix JSON object root.",
                make_correlation_id()});
        }
        ScriptBindingsAsset asset;
        asset.schema_version = json.value("schemaVersion", 1);
        const auto read_entries = [](const nlohmann::json& node, std::vector<ScriptBindingEntry>& out) {
            if (!node.is_array()) return;
            for (const auto& entry : node) {
                if (!entry.is_object()) continue;
                ScriptBindingEntry binding;
                binding.id = entry.value("id", std::string{});
                binding.handler = entry.value("handler", std::string{});
                binding.script_path = entry.value("script", std::string{});
                if (!binding.id.empty() && !binding.handler.empty()) out.push_back(std::move(binding));
            }
        };
        read_entries(json.value("interactions", nlohmann::json::array()), asset.interactions);
        read_entries(json.value("combatHits", nlohmann::json::array()), asset.combat_hits);
        read_entries(json.value("combatHurts", nlohmann::json::array()), asset.combat_hurts);
        read_entries(json.value("uiButtons", nlohmann::json::array()), asset.ui_buttons);
        return Result<ScriptBindingsAsset>::success(std::move(asset));
    } catch (const std::exception& exception) {
        return Result<ScriptBindingsAsset>::failure(EngineError{
            "SCRIPT-BINDINGS-PARSE", Severity::Error, ErrorCategory::Serialization, "scripting",
            "Failed to parse bindings.script.json", ENGINE_SOURCE_CONTEXT, {exception.what()},
            "Fix JSON syntax.", make_correlation_id()});
    }
}

std::string ScriptBindingsAsset::to_json() const {
    nlohmann::json json;
    json["schemaVersion"] = schema_version;
    const auto write_entries = [](const std::vector<ScriptBindingEntry>& entries) {
        nlohmann::json out = nlohmann::json::array();
        for (const auto& entry : entries) {
            nlohmann::json node;
            node["id"] = entry.id;
            node["handler"] = entry.handler;
            if (!entry.script_path.empty()) node["script"] = entry.script_path;
            out.push_back(std::move(node));
        }
        return out;
    };
    json["interactions"] = write_entries(interactions);
    json["combatHits"] = write_entries(combat_hits);
    json["combatHurts"] = write_entries(combat_hurts);
    json["uiButtons"] = write_entries(ui_buttons);
    return json.dump(2);
}

Result<void> ScriptBindingsAsset::save_atomic(const std::filesystem::path& path) const {
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    const auto temp = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            return Result<void>::failure(EngineError{"SCRIPT-BINDINGS-IO", Severity::Error, ErrorCategory::Io, "scripting",
                "Failed to open temp bindings file", ENGINE_SOURCE_CONTEXT, {}, "Check permissions.", make_correlation_id()});
        }
        out << to_json();
    }
    if (std::filesystem::exists(path)) std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::rename(temp, path);
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup);
    return Result<void>::success();
}

namespace {

std::string normalize_binding_kind_key(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::optional<std::string> find_binding_script(const std::vector<ScriptBindingEntry>& entries, const std::string& binding_id) {
    for (const auto& entry : entries) {
        if (entry.id != binding_id) continue;
        if (entry.script_path.empty()) return std::nullopt;
        return entry.script_path;
    }
    return std::nullopt;
}

} // namespace

Result<std::string> ScriptBindingsAsset::resolve_script_path(const std::string& kind, const std::string& binding_id) const {
    if (binding_id.empty()) {
        return Result<std::string>::failure(EngineError{"SCRIPT-BINDING-EMPTY", Severity::Error, ErrorCategory::Validation,
            "scripting", "Script binding id is empty", ENGINE_SOURCE_CONTEXT, {},
            "Set bindingId to an id from bindings.script.json.", make_correlation_id()});
    }
    const auto key = normalize_binding_kind_key(kind);
    std::optional<std::string> path;
    if (key == "interaction" || key.empty()) path = find_binding_script(interactions, binding_id);
    else if (key == "combathit") path = find_binding_script(combat_hits, binding_id);
    else if (key == "combathurt") path = find_binding_script(combat_hurts, binding_id);
    else if (key == "uibutton" || key == "ui_button") path = find_binding_script(ui_buttons, binding_id);
    else if (key == "handler") {
        path = find_binding_script(interactions, binding_id);
        if (!path) path = find_binding_script(combat_hits, binding_id);
        if (!path) path = find_binding_script(combat_hurts, binding_id);
        if (!path) path = find_binding_script(ui_buttons, binding_id);
    } else {
        return Result<std::string>::failure(EngineError{"SCRIPT-BINDING-KIND", Severity::Error, ErrorCategory::Validation,
            "scripting", "Unknown script binding kind: " + kind, ENGINE_SOURCE_CONTEXT, {},
            "Use interaction, combatHit, combatHurt, uiButton, or handler.", make_correlation_id()});
    }
    if (!path) {
        return Result<std::string>::failure(EngineError{"SCRIPT-BINDING-MISSING", Severity::Error, ErrorCategory::Validation,
            "scripting", "No script path for binding '" + binding_id + "' (kind " + kind + ")", ENGINE_SOURCE_CONTEXT, {},
            "Add the id to assets/scripts/bindings.script.json with a script path.", make_correlation_id()});
    }
    return Result<std::string>::success(*path);
}

Result<std::string> resolve_script_binding_path(const std::filesystem::path& project_root, const std::string& kind,
    const std::string& binding_id) {
    const auto loaded = ScriptBindingsAsset::load(default_script_bindings_path(project_root));
    if (!loaded) return Result<std::string>::failure(loaded.error());
    return loaded.value().resolve_script_path(kind, binding_id);
}

} // namespace engine
