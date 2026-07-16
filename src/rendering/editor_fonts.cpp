#include "engine/editor/editor_fonts.h"

#include "engine/ui/game_fonts.h"

namespace engine::EditorFonts {

bool load(ImGuiIO& io) { return GameFonts::load(io); }

ImFont* icons() { return GameFonts::icons(); }

} // namespace engine::EditorFonts
