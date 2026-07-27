#pragma once

#include <filesystem>

struct SDL_Window;

namespace engine {

/// Resolve `assets/editor/branding/app-icon*.png` (cwd, then ENGINE_REPOSITORY_ROOT).
[[nodiscard]] std::filesystem::path resolve_app_icon_png(bool prefer_256 = false);

/// Set the OS window / taskbar icon from the branding PNG. Safe no-op if missing.
void apply_sdl_window_icon(SDL_Window* window);

} // namespace engine
