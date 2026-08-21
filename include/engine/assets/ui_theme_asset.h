#pragma once

#include "engine/assets/hud_asset.h"
#include "engine/core/result.h"

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace engine {

/// Named chrome role: token names for fill (panels/buttons) and text (labels).
struct UiThemeRole {
    std::string fill;
    std::string text;
    std::string border;
};

/// Project UI theme (`assets/ui/ui-theme.json`). Tokens are RGBA 0–255.
/// Widgets reference `themeRole` / `colorToken` / `textColorToken`; MCP and Lua
/// can rewrite this object and the next frame picks up the new colors.
struct UiThemeAsset {
    int schema_version = 1;
    std::string id;
    std::map<std::string, std::array<float, 4>> tokens;
    std::map<std::string, UiThemeRole> roles;

    [[nodiscard]] static UiThemeAsset built_in();
    [[nodiscard]] static Result<UiThemeAsset> parse(const std::string& text,
        const std::string& source_name = "ui-theme.json");
    [[nodiscard]] static Result<UiThemeAsset> load(const std::filesystem::path& path);

    void set_token(std::string name, std::array<float, 4> rgba);
    void set_role(std::string name, UiThemeRole role);

    [[nodiscard]] std::optional<std::array<float, 4>> token(const std::string& name) const;
    [[nodiscard]] const UiThemeRole* role(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> token_names() const;
    [[nodiscard]] std::vector<std::string> role_names() const;

    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
};

[[nodiscard]] std::filesystem::path default_ui_theme_path(const std::filesystem::path& project_root);

/// Fill color for panels, bars, and button plates. Zero alpha means "use draw default".
[[nodiscard]] std::array<float, 4> ui_theme_resolve_fill(const HudWidget& widget, const UiThemeAsset* theme,
    const std::array<float, 4>* color_override = nullptr);

/// Label / body text color. Buttons never reuse fill as the glyph color.
[[nodiscard]] std::array<float, 4> ui_theme_resolve_text(const HudWidget& widget, const UiThemeAsset* theme,
    const std::array<float, 4>& fill, const std::array<float, 4>* color_override = nullptr);

} // namespace engine
