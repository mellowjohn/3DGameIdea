#pragma once

#include <string>

struct ImFont;
struct ImGuiIO;

namespace engine::GameFonts {

/// Roboto for engine/editor chrome; Cinzel for in-scene game canvases; JetBrains Mono for diagnostics.
[[nodiscard]] bool load(ImGuiIO& io);

[[nodiscard]] ImFont* ui();       // Roboto — engine / editor
[[nodiscard]] ImFont* display();  // Cinzel — scene / game display titles
[[nodiscard]] ImFont* body();     // Roboto @ larger size — readable dialogue / long prose
[[nodiscard]] ImFont* mono();
[[nodiscard]] ImFont* icons();

/// Face for game canvas overlay text. Prefer `body()` for long dialogue lines.
[[nodiscard]] ImFont* for_design_size(float design_px);

/// World Forge cartography labels (`cinzel`, `forum`, `eb_garamond`, `uncial_antiqua`, `metamorphous`,
/// `medievalsharp`).
[[nodiscard]] ImFont* map_typeface(const std::string& id);

} // namespace engine::GameFonts
