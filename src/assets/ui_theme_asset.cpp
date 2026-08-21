#include "engine/assets/ui_theme_asset.h"

#include "engine/core/error.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace engine {
namespace {

EngineError theme_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "ui_theme", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

Result<std::array<float, 4>> parse_rgba(const nlohmann::json& node, const std::string& label) {
    if (!node.is_array() || node.size() < 3) {
        return Result<std::array<float, 4>>::failure(theme_error("UITHEME-COLOR",
            label + " must be [r,g,b] or [r,g,b,a] (0–255)", "Use chrome-direction RGBA arrays."));
    }
    std::array<float, 4> rgba{{0.0f, 0.0f, 0.0f, 255.0f}};
    for (std::size_t i = 0; i < 3; ++i) {
        if (!node[i].is_number()) {
            return Result<std::array<float, 4>>::failure(
                theme_error("UITHEME-COLOR", label + " channels must be numbers", "Use 0–255 floats."));
        }
        rgba[i] = node[i].get<float>();
        if (!std::isfinite(rgba[i])) {
            return Result<std::array<float, 4>>::failure(
                theme_error("UITHEME-COLOR", label + " channels must be finite", "Use 0–255 floats."));
        }
    }
    if (node.size() >= 4) {
        if (!node[3].is_number()) {
            return Result<std::array<float, 4>>::failure(
                theme_error("UITHEME-COLOR", label + " alpha must be a number", "Use 0–255."));
        }
        rgba[3] = node[3].get<float>();
        if (!std::isfinite(rgba[3])) {
            return Result<std::array<float, 4>>::failure(
                theme_error("UITHEME-COLOR", label + " alpha must be finite", "Use 0–255."));
        }
    }
    return Result<std::array<float, 4>>::success(rgba);
}

std::array<float, 4> contrast_text_for_fill(const std::array<float, 4>& fill) {
    const float lum = fill[0] + fill[1] + fill[2];
    if (lum > 420.0f) return {{72.0f, 62.0f, 48.0f, 255.0f}}; // ink on gold / parchment
    return {{241.0f, 238.0f, 232.0f, 255.0f}}; // chrome on iron
}

} // namespace

std::filesystem::path default_ui_theme_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "ui" / "ui-theme.json";
}

UiThemeAsset UiThemeAsset::built_in() {
    UiThemeAsset asset;
    asset.schema_version = 1;
    asset.id = "open_world_rpg_ui";
    auto put = [&](const char* name, float r, float g, float b, float a = 255.0f) {
        asset.tokens[name] = {{r, g, b, a}};
    };
    put("ironDeep", 30.0f, 28.0f, 24.0f);
    put("ironPanel", 45.0f, 41.0f, 35.0f);
    put("ironCharcoal", 42.0f, 40.0f, 38.0f);
    put("bronzeMid", 58.0f, 52.0f, 44.0f);
    put("bronzeFace", 92.0f, 78.0f, 58.0f);
    put("parchment", 201.0f, 184.0f, 150.0f);
    put("parchmentChoice", 232.0f, 220.0f, 198.0f);
    put("goldAccent", 213.0f, 185.0f, 120.0f);
    put("goldStamina", 196.0f, 162.0f, 74.0f);
    put("chromeText", 241.0f, 238.0f, 232.0f);
    put("mutedSteel", 155.0f, 163.0f, 167.0f);
    put("ink", 72.0f, 62.0f, 48.0f);
    put("inkMuted", 96.0f, 84.0f, 68.0f);
    put("dimScrim", 21.0f, 23.0f, 25.0f, 180.0f);
    put("healthCrimson", 139.0f, 46.0f, 46.0f);
    put("magicBlue", 70.0f, 90.0f, 160.0f);
    asset.roles["primaryButton"] = UiThemeRole{"goldAccent", "ink", "goldAccent"};
    asset.roles["secondaryButton"] = UiThemeRole{"bronzeMid", "chromeText", {}};
    asset.roles["panel"] = UiThemeRole{"ironPanel", "chromeText", {}};
    asset.roles["panelDeep"] = UiThemeRole{"ironDeep", "chromeText", {}};
    asset.roles["title"] = UiThemeRole{{}, "chromeText", {}};
    asset.roles["label"] = UiThemeRole{{}, "chromeText", {}};
    asset.roles["muted"] = UiThemeRole{{}, "mutedSteel", {}};
    asset.roles["goldLabel"] = UiThemeRole{{}, "goldAccent", {}};
    asset.roles["dim"] = UiThemeRole{"dimScrim", {}, {}};
    return asset;
}

Result<UiThemeAsset> UiThemeAsset::parse(const std::string& text, const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<UiThemeAsset>::failure(
                theme_error("UITHEME-ROOT", source_name + " must be a JSON object", "Wrap tokens and roles in an object."));
        }
        UiThemeAsset asset;
        asset.schema_version = json.value("schemaVersion", 1);
        if (asset.schema_version != 1) {
            return Result<UiThemeAsset>::failure(
                theme_error("UITHEME-SCHEMA", "Unsupported UI theme schemaVersion", "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        const auto tokens = json.value("tokens", nlohmann::json::object());
        if (!tokens.is_object()) {
            return Result<UiThemeAsset>::failure(
                theme_error("UITHEME-TOKENS", "tokens must be an object", "Map token names to [r,g,b,a]."));
        }
        for (auto it = tokens.begin(); it != tokens.end(); ++it) {
            if (it.key().empty()) {
                return Result<UiThemeAsset>::failure(
                    theme_error("UITHEME-TOKEN-NAME", "Token name must be non-empty", "Use camelCase ids like goldAccent."));
            }
            auto rgba = parse_rgba(it.value(), "token '" + it.key() + "'");
            if (!rgba) return Result<UiThemeAsset>::failure(rgba.error());
            asset.tokens[it.key()] = rgba.value();
        }
        const auto roles = json.value("roles", nlohmann::json::object());
        if (!roles.is_object()) {
            return Result<UiThemeAsset>::failure(
                theme_error("UITHEME-ROLES", "roles must be an object", "Map role names to {fill,text,border} token ids."));
        }
        for (auto it = roles.begin(); it != roles.end(); ++it) {
            if (it.key().empty()) {
                return Result<UiThemeAsset>::failure(
                    theme_error("UITHEME-ROLE-NAME", "Role name must be non-empty", "Use names like primaryButton."));
            }
            if (!it.value().is_object()) {
                return Result<UiThemeAsset>::failure(theme_error(
                    "UITHEME-ROLE", "Role '" + it.key() + "' must be an object", "Use {\"fill\":\"goldAccent\",\"text\":\"ink\"}."));
            }
            UiThemeRole role;
            role.fill = it.value().value("fill", std::string{});
            role.text = it.value().value("text", std::string{});
            role.border = it.value().value("border", std::string{});
            auto check_token = [&](const std::string& token_name, const char* field) -> Result<void> {
                if (token_name.empty()) return Result<void>::success();
                if (asset.tokens.find(token_name) == asset.tokens.end()) {
                    return Result<void>::failure(theme_error("UITHEME-ROLE-TOKEN",
                        "Role '" + it.key() + "' " + field + " references unknown token '" + token_name + "'",
                        "Add the token under tokens or fix the role."));
                }
                return Result<void>::success();
            };
            if (const auto ok = check_token(role.fill, "fill"); !ok) return Result<UiThemeAsset>::failure(ok.error());
            if (const auto ok = check_token(role.text, "text"); !ok) return Result<UiThemeAsset>::failure(ok.error());
            if (const auto ok = check_token(role.border, "border"); !ok)
                return Result<UiThemeAsset>::failure(ok.error());
            asset.roles[it.key()] = std::move(role);
        }
        return Result<UiThemeAsset>::success(std::move(asset));
    } catch (const std::exception& ex) {
        return Result<UiThemeAsset>::failure(
            theme_error("UITHEME-PARSE", "Failed to parse " + source_name, std::string(ex.what())));
    }
}

Result<UiThemeAsset> UiThemeAsset::load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<UiThemeAsset>::failure(theme_error(
            "UITHEME-IO", "UI theme file not found: " + path.generic_string(), "Create assets/ui/ui-theme.json."));
    }
    std::ifstream input(path);
    if (!input) {
        return Result<UiThemeAsset>::failure(
            theme_error("UITHEME-IO", "Failed to read UI theme: " + path.generic_string(), "Check permissions."));
    }
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return parse(text, path.generic_string());
}

void UiThemeAsset::set_token(std::string name, std::array<float, 4> rgba) {
    if (name.empty()) return;
    tokens[std::move(name)] = rgba;
}

void UiThemeAsset::set_role(std::string name, UiThemeRole role) {
    if (name.empty()) return;
    roles[std::move(name)] = std::move(role);
}

std::optional<std::array<float, 4>> UiThemeAsset::token(const std::string& name) const {
    const auto it = tokens.find(name);
    if (it == tokens.end()) return std::nullopt;
    return it->second;
}

const UiThemeRole* UiThemeAsset::role(const std::string& name) const {
    const auto it = roles.find(name);
    if (it == roles.end()) return nullptr;
    return &it->second;
}

std::vector<std::string> UiThemeAsset::token_names() const {
    std::vector<std::string> names;
    names.reserve(tokens.size());
    for (const auto& [name, _] : tokens) names.push_back(name);
    return names;
}

std::vector<std::string> UiThemeAsset::role_names() const {
    std::vector<std::string> names;
    names.reserve(roles.size());
    for (const auto& [name, _] : roles) names.push_back(name);
    return names;
}

std::string UiThemeAsset::to_json() const {
    nlohmann::json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    nlohmann::json tokens_json = nlohmann::json::object();
    for (const auto& [name, rgba] : tokens) {
        tokens_json[name] = {rgba[0], rgba[1], rgba[2], rgba[3]};
    }
    json["tokens"] = std::move(tokens_json);
    nlohmann::json roles_json = nlohmann::json::object();
    for (const auto& [name, role] : roles) {
        nlohmann::json node = nlohmann::json::object();
        if (!role.fill.empty()) node["fill"] = role.fill;
        if (!role.text.empty()) node["text"] = role.text;
        if (!role.border.empty()) node["border"] = role.border;
        roles_json[name] = std::move(node);
    }
    json["roles"] = std::move(roles_json);
    return json.dump(2);
}

Result<void> UiThemeAsset::save_atomic(const std::filesystem::path& path) const {
    const auto source = to_json();
    const auto validated = parse(source, path.generic_string());
    if (!validated) return Result<void>::failure(validated.error());
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    const auto temp = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            return Result<void>::failure(theme_error("UITHEME-IO", "Failed to open temp UI theme file", "Check permissions."));
        }
        out << source;
        if (!source.empty() && source.back() != '\n') out << '\n';
    }
    if (std::filesystem::exists(path))
        std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::rename(temp, path);
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup);
    return Result<void>::success();
}

std::array<float, 4> ui_theme_resolve_fill(const HudWidget& widget, const UiThemeAsset* theme,
    const std::array<float, 4>* color_override) {
    if (color_override && (*color_override)[3] > 0.0f) return *color_override;
    if (widget.has_color()) return widget.color;
    if (theme) {
        if (const auto token = theme->token(widget.color_token)) return *token;
        if (const auto* role = theme->role(widget.theme_role)) {
            if (const auto token = theme->token(role->fill)) return *token;
        }
    }
    return {{0.0f, 0.0f, 0.0f, 0.0f}};
}

std::array<float, 4> ui_theme_resolve_text(const HudWidget& widget, const UiThemeAsset* theme,
    const std::array<float, 4>& fill, const std::array<float, 4>* color_override) {
    if (widget.has_text_color()) return widget.text_color;
    if (theme) {
        if (const auto token = theme->token(widget.text_color_token)) return *token;
        if (const auto* role = theme->role(widget.theme_role)) {
            if (const auto token = theme->token(role->text)) return *token;
        }
    }
    // Non-button widgets historically stored label color in `color`. Runtime
    // set_color on those widgets is a text tint, not a plate fill.
    if (widget.type != HudWidgetType::Button) {
        if (color_override && (*color_override)[3] > 0.0f) return *color_override;
        if (widget.has_color()) return widget.color;
        if (theme) {
            if (const auto token = theme->token(widget.color_token)) return *token;
            if (const auto* role = theme->role(widget.theme_role)) {
                if (const auto token = theme->token(role->text)) return *token;
            }
        }
    }
    const std::array<float, 4> plate = (fill[3] > 0.0f) ? fill : ui_theme_resolve_fill(widget, theme, color_override);
    if (theme) {
        if (plate[0] + plate[1] + plate[2] > 420.0f) {
            if (const auto ink = theme->token("ink")) return *ink;
        } else if (const auto chrome = theme->token("chromeText")) {
            return *chrome;
        }
    }
    return contrast_text_for_fill(plate);
}

} // namespace engine
