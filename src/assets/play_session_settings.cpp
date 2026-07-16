#include "engine/assets/play_session_settings.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace engine {
namespace {

EngineError play_session_error(std::string code, ErrorCategory category, std::string message) {
    return {std::move(code), Severity::Error, category, "play-session", std::move(message), ENGINE_SOURCE_CONTEXT, {},
            "Correct play.session.json paths and schema.", make_correlation_id()};
}

} // namespace

std::filesystem::path default_play_session_settings_path(const std::filesystem::path& project_root) {
    return project_root / "play.session.json";
}

Result<void> PlaySessionSettings::validate() const {
    if (schema_version != 1)
        return Result<void>::failure(
            play_session_error("PLAY-SESSION-SCHEMA-UNSUPPORTED", ErrorCategory::Validation,
                "Only play session schema version 1 is supported"));
    if (character_asset.empty() || camera_asset.empty())
        return Result<void>::failure(play_session_error("PLAY-SESSION-PATH-MISSING", ErrorCategory::Validation,
            "characterAsset and cameraAsset paths are required"));
    return Result<void>::success();
}

std::string PlaySessionSettings::to_json() const {
    nlohmann::ordered_json root{{"schemaVersion", schema_version},
                                {"characterAsset", character_asset},
                                {"cameraAsset", camera_asset}};
    return root.dump(2) + "\n";
}

Result<PlaySessionSettings> PlaySessionSettings::from_json(const std::string& text) {
    try {
        const auto root = nlohmann::json::parse(text);
        PlaySessionSettings value;
        value.schema_version = root.at("schemaVersion").get<std::uint32_t>();
        value.character_asset = root.at("characterAsset").get<std::string>();
        value.camera_asset = root.at("cameraAsset").get<std::string>();
        if (const auto valid = value.validate(); !valid) return Result<PlaySessionSettings>::failure(valid.error());
        return Result<PlaySessionSettings>::success(std::move(value));
    } catch (const std::exception& exception) {
        auto error = play_session_error("PLAY-SESSION-PARSE-FAILED", ErrorCategory::Serialization,
            "play.session.json is malformed");
        error.causes.push_back(exception.what());
        return Result<PlaySessionSettings>::failure(std::move(error));
    }
}

Result<PlaySessionSettings> PlaySessionSettings::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return Result<PlaySessionSettings>::failure(play_session_error("PLAY-SESSION-READ-FAILED", ErrorCategory::Io,
            "Could not read play session settings: " + path.generic_string()));
    std::ostringstream text;
    text << input.rdbuf();
    return from_json(text.str());
}

Result<void> PlaySessionSettings::save(const std::filesystem::path& path) const {
    if (const auto valid = validate(); !valid) return valid;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return Result<void>::failure(play_session_error("PLAY-SESSION-WRITE-FAILED", ErrorCategory::Io,
            "Could not write play session settings: " + path.generic_string()));
    output << to_json();
    if (!output)
        return Result<void>::failure(play_session_error("PLAY-SESSION-WRITE-FAILED", ErrorCategory::Io,
            "Could not flush play session settings: " + path.generic_string()));
    return Result<void>::success();
}

} // namespace engine
