#pragma once

struct ImFont;
struct ImGuiIO;

namespace engine::GameFonts {

/// Roboto for engine/editor chrome; Cinzel for in-scene game canvases; JetBrains Mono for diagnostics.
[[nodiscard]] bool load(ImGuiIO& io);

[[nodiscard]] ImFont* ui();       // Roboto — engine / editor
[[nodiscard]] ImFont* display();  // Cinzel — scene / game assets
[[nodiscard]] ImFont* mono();
[[nodiscard]] ImFont* icons();

/// Face for game canvas / HUD overlay text (always Cinzel when loaded).
[[nodiscard]] ImFont* for_design_size(float design_px);

} // namespace engine::GameFonts
