#include "engine/ui/app_branding.h"

#include "engine/diagnostics/logger.h"

#include <SDL3/SDL.h>

namespace engine {
namespace {

std::filesystem::path try_branding_file(const char* filename) {
    const std::filesystem::path relative = std::filesystem::path("assets/editor/branding") / filename;
    if (std::filesystem::exists(relative)) return relative;
#ifdef ENGINE_REPOSITORY_ROOT
    const std::filesystem::path rooted = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / relative;
    if (std::filesystem::exists(rooted)) return rooted;
#endif
    return {};
}

} // namespace

std::filesystem::path resolve_app_icon_png(bool prefer_256) {
    if (prefer_256) {
        if (auto path = try_branding_file("app-icon-256.png"); !path.empty()) return path;
    }
    if (auto path = try_branding_file("app-icon.png"); !path.empty()) return path;
    if (!prefer_256) {
        if (auto path = try_branding_file("app-icon-256.png"); !path.empty()) return path;
    }
    return {};
}

void apply_sdl_window_icon(SDL_Window* window) {
    if (!window) return;
    const auto path = resolve_app_icon_png(true);
    if (path.empty()) {
        Logger::instance().write(Severity::Warning, "branding",
            "App icon PNG missing under assets/editor/branding/");
        return;
    }
    const std::string utf8 = path.generic_string();
    SDL_Surface* surface = SDL_LoadPNG(utf8.c_str());
    if (!surface) {
        Logger::instance().write(Severity::Warning, "branding",
            std::string("SDL_LoadPNG failed for app icon: ") + SDL_GetError());
        return;
    }
    if (!SDL_SetWindowIcon(window, surface)) {
        Logger::instance().write(Severity::Warning, "branding",
            std::string("SDL_SetWindowIcon failed: ") + SDL_GetError());
    }
    SDL_DestroySurface(surface);
}

} // namespace engine
