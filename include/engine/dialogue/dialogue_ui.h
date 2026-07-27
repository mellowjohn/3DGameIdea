#pragma once

#include "engine/assets/world_forge_dialogues_asset.h"
#include "engine/core/result.h"

#include <array>
#include <cstddef>
#include <string>

namespace engine {

class DialogueRuntime;
class StandingRuntime;
class FlagRuntime;
class UiCanvasStack;

inline constexpr std::size_t kDialogueChoiceSlotCount = 4;

struct DialogueUiSession {
    bool choices_page = false;
    std::string synced_node_id;
    std::string last_line;
    std::array<std::string, kDialogueChoiceSlotCount> choice_slot_ids{};
};

/// Primary choice button label (act/line text only — tone/standing use chips).
[[nodiscard]] std::string format_dialogue_choice_label(const WorldForgeDialogueChoice& choice);
[[nodiscard]] std::string format_dialogue_tone_chip(const WorldForgeDialogueChoice& choice);
[[nodiscard]] std::string format_dialogue_standing_chip(const WorldForgeDialogueChoice& choice);
/// Fills rgba[4] 0–255 for a tone label (Curious/Honest/Blunt/…). Unknown → neutral bronze.
void dialogue_tone_colors(const std::string& tone, float rgba_bg[4], float rgba_fg[4]);
[[nodiscard]] std::string format_dialogue_speaker_display(const std::string& speaker_id);
[[nodiscard]] std::string format_dialogue_portrait_initials(const std::string& speaker_id);

void apply_dialogue_choice_standing(StandingRuntime* standing, const WorldForgeDialogueChoice& choice);
void apply_dialogue_choice_flags(FlagRuntime* flags, const WorldForgeDialogueChoice& choice);

/// Push/update the dialogue canvas for the active node on the line page (typewriter body).
void sync_dialogue_canvas(UiCanvasStack* stack, DialogueRuntime* dialogue, DialogueUiSession& session);

/// After typewriter is complete: show choices page, or close if none remain.
/// Returns true when the continue press was fully handled in C++.
[[nodiscard]] bool dialogue_advance_continue(UiCanvasStack* stack, DialogueRuntime* dialogue,
    DialogueUiSession& session);

/// Activate choice slot 1..4 (bind dialogue.choice_N). Applies standing + flags, chooses, resyncs line page.
[[nodiscard]] Result<void> dialogue_activate_choice_slot(UiCanvasStack* stack, DialogueRuntime* dialogue,
    DialogueUiSession& session, StandingRuntime* standing, FlagRuntime* flags, int slot_1_based);

/// Choose by id with standing/flag apply + UI resync (Lua / MCP).
[[nodiscard]] Result<void> dialogue_choose_with_ui(UiCanvasStack* stack, DialogueRuntime* dialogue,
    DialogueUiSession& session, StandingRuntime* standing, FlagRuntime* flags, const std::string& choice_id);

} // namespace engine
