#include "engine/ui/game_fonts.h"

#include "engine/diagnostics/logger.h"

#include <imgui.h>

#include <filesystem>
#include <vector>

namespace engine::GameFonts {
namespace {

ImFont* g_ui = nullptr;
ImFont* g_display = nullptr;
ImFont* g_mono = nullptr;
ImFont* g_icons = nullptr;

std::filesystem::path first_existing(const std::vector<std::filesystem::path>& candidates) {
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) return candidate;
    }
    return {};
}

std::filesystem::path resolve_under_assets(const char* relative) {
    return first_existing({
        relative,
#ifdef ENGINE_REPOSITORY_ROOT
        std::filesystem::path(ENGINE_REPOSITORY_ROOT) / relative,
#endif
    });
}

ImFont* add_font(ImGuiIO& io, const std::filesystem::path& path, float size_px, const char* role) {
    if (path.empty()) {
        Logger::instance().write(Severity::Warning, "game-fonts",
            std::string(role) + " font missing");
        return nullptr;
    }
    ImFont* font = io.Fonts->AddFontFromFileTTF(path.string().c_str(), size_px);
    if (!font) {
        Logger::instance().write(Severity::Warning, "game-fonts",
            std::string("Failed to load ") + role + ": " + path.generic_string());
    }
    return font;
}

} // namespace

bool load(ImGuiIO& io) {
    g_ui = g_display = g_mono = g_icons = nullptr;

    // Roboto = engine/editor chrome. Cinzel = in-scene game canvases (HUD, menus, dialogue).
    const auto ui_path = resolve_under_assets("assets/ui/fonts/roboto/Roboto-Regular.ttf");
    const auto display_path = resolve_under_assets("assets/ui/fonts/cinzel/Cinzel-Regular.ttf");
    const auto mono_path = resolve_under_assets("assets/ui/fonts/jetbrains-mono/JetBrainsMono-Regular.ttf");
    const auto icons_path = resolve_under_assets("assets/editor/fonts/fa-solid-900.ttf");

    g_ui = add_font(io, ui_path, 17.0f, "engine UI (Roboto)");
    if (!g_ui) {
        io.Fonts->AddFontDefault();
        g_ui = io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
        Logger::instance().write(Severity::Warning, "game-fonts",
            "Falling back to ImGui default font for engine UI");
    }

    if (g_ui && !icons_path.empty()) {
        ImFontConfig config;
        config.MergeMode = true;
        config.PixelSnapH = true;
        config.GlyphMinAdvanceX = 16.0f;
        static const ImWchar icon_ranges[] = {
            0xf01e, 0xf01e, 0xf03e, 0xf03e, 0xf04b, 0xf04d, 0xf065, 0xf065, 0xf07b, 0xf07b, 0xf0ac, 0xf0ac,
            0xf0b2, 0xf0b2, 0xf0c7, 0xf0c7, 0xf0e2, 0xf0e2, 0xf11b, 0xf11b, 0xf1b2, 0xf1b2, 0xf2f1, 0xf2f1, 0};
        g_icons = io.Fonts->AddFontFromFileTTF(icons_path.string().c_str(), 15.0f, &config, icon_ranges);
        if (!g_icons) {
            Logger::instance().write(Severity::Warning, "game-fonts",
                "Could not merge Font Awesome icon font: " + icons_path.generic_string());
        }
    } else if (icons_path.empty()) {
        Logger::instance().write(Severity::Warning, "game-fonts",
            "Font Awesome icon font not found; toolbar icons disabled");
    }

    g_display = add_font(io, display_path, 22.0f, "scene/game (Cinzel)");
    g_mono = add_font(io, mono_path, 16.0f, "mono (JetBrains Mono)");

    if (g_ui) io.FontDefault = g_ui;
    return g_ui != nullptr;
}

ImFont* ui() { return g_ui; }
ImFont* display() { return g_display; }
ImFont* mono() { return g_mono; }
ImFont* icons() { return g_icons; }

ImFont* for_design_size(float /*design_px*/) {
    // All in-scene canvas text uses Cinzel; call sites for engine chrome use ui() / FontDefault.
    if (g_display) return g_display;
    if (g_ui) return g_ui;
    return ImGui::GetFont();
}

} // namespace engine::GameFonts
