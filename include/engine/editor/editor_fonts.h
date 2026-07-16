#pragma once

struct ImFont;
struct ImGuiIO;

namespace engine::EditorFonts {

[[nodiscard]] bool load(ImGuiIO& io);
[[nodiscard]] ImFont* icons();

} // namespace engine::EditorFonts
