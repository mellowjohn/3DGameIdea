#include "engine/ui/game_fonts.h"

#include "engine/diagnostics/logger.h"

#include <imgui.h>

#include <filesystem>
#include <string>
#include <vector>

namespace engine::GameFonts {
namespace {

ImFont* g_ui = nullptr;
ImFont* g_display = nullptr;
ImFont* g_mono = nullptr;
ImFont* g_icons = nullptr;
ImFont* g_map_forum = nullptr;
ImFont* g_map_eb_garamond = nullptr;
ImFont* g_map_uncial = nullptr;
ImFont* g_map_metamorphous = nullptr;
ImFont* g_map_medievalsharp = nullptr;
constexpr float k_map_culture_font_px = 18.0f;

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
    g_map_forum = g_map_eb_garamond = g_map_uncial = g_map_metamorphous = g_map_medievalsharp = nullptr;

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
            0xf01e, 0xf01e, 0xf02d, 0xf02d, 0xf03e, 0xf03e, 0xf04b, 0xf04d, 0xf065, 0xf065, 0xf07b, 0xf07b,
            0xf0ac, 0xf0ac, 0xf0b2, 0xf0b2, 0xf0c7, 0xf0c7, 0xf0e2, 0xf0e2, 0xf11b, 0xf11b, 0xf1b2, 0xf1b2,
            0xf2f1, 0xf2f1, 0};
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

    const auto forum_path = resolve_under_assets("assets/ui/fonts/forum/Forum-Regular.ttf");
    const auto eb_path = resolve_under_assets("assets/ui/fonts/eb-garamond/EBGaramond-Variable.ttf");
    const auto uncial_path = resolve_under_assets("assets/ui/fonts/uncial-antiqua/UncialAntiqua-Regular.ttf");
    const auto morph_path = resolve_under_assets("assets/ui/fonts/metamorphous/Metamorphous-Regular.ttf");
    const auto medievalsharp_path = resolve_under_assets("assets/ui/fonts/medievalsharp/MedievalSharp.ttf");
    g_map_forum = add_font(io, forum_path, k_map_culture_font_px, "map culture (Forum)");
    g_map_eb_garamond = add_font(io, eb_path, k_map_culture_font_px, "map culture (EB Garamond)");
    if (!g_map_eb_garamond) {
        const auto cormorant_path =
            resolve_under_assets("assets/ui/fonts/cormorant-garamond/CormorantGaramond-Variable.ttf");
        g_map_eb_garamond = add_font(io, cormorant_path, k_map_culture_font_px, "map culture (Cormorant Garamond)");
    }
    g_map_uncial = add_font(io, uncial_path, k_map_culture_font_px, "map culture (Uncial Antiqua)");
    g_map_metamorphous = add_font(io, morph_path, k_map_culture_font_px, "map culture (Metamorphous)");
    g_map_medievalsharp = add_font(io, medievalsharp_path, k_map_culture_font_px, "map culture (MedievalSharp)");

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

ImFont* map_typeface(const std::string& id) {
    std::string key = id;
    for (char& ch : key) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    for (char& ch : key) {
        if (ch == '-') ch = '_';
    }
    if (key == "forum" && g_map_forum) return g_map_forum;
    if ((key == "eb_garamond" || key == "ebgaramond") && g_map_eb_garamond) return g_map_eb_garamond;
    if (key == "uncial_antiqua" && g_map_uncial) return g_map_uncial;
    if (key == "metamorphous" && g_map_metamorphous) return g_map_metamorphous;
    if (key == "medievalsharp" && g_map_medievalsharp) return g_map_medievalsharp;
    if (g_display) return g_display;
    if (g_ui) return g_ui;
    return ImGui::GetFont();
}

} // namespace engine::GameFonts
