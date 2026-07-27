#include "engine/dialogue/dialogue_ui.h"

#include "engine/core/error.h"
#include "engine/diagnostics/logger.h"
#include "engine/dialogue/dialogue_runtime.h"
#include "engine/standing/standing_runtime.h"
#include "engine/flag/flag_runtime.h"
#include "engine/ui/hud_runtime.h"
#include "engine/ui/ui_canvas_stack.h"

#include <cctype>
#include <cmath>
#include <sstream>

namespace engine {
namespace {

EngineError ui_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "dialogue_ui", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string choice_widget_id(std::size_t index_0) {
    return "dialogue_choice_" + std::to_string(index_0 + 1);
}

std::string choice_bind(std::size_t index_0) {
    return "dialogue.choice_" + std::to_string(index_0 + 1);
}

std::string choice_key_id(std::size_t index_0) {
    return "dialogue_choice_" + std::to_string(index_0 + 1) + "_key";
}

std::string choice_key_text_id(std::size_t index_0) {
    return "dialogue_choice_" + std::to_string(index_0 + 1) + "_key_text";
}

std::string choice_tone_id(std::size_t index_0) {
    return "dialogue_choice_" + std::to_string(index_0 + 1) + "_tone";
}

std::string choice_tone_text_id(std::size_t index_0) {
    return "dialogue_choice_" + std::to_string(index_0 + 1) + "_tone_text";
}

std::string choice_standing_id(std::size_t index_0) {
    return "dialogue_choice_" + std::to_string(index_0 + 1) + "_standing";
}

std::string choice_standing_text_id(std::size_t index_0) {
    return "dialogue_choice_" + std::to_string(index_0 + 1) + "_standing_text";
}

std::string choice_key_bind(std::size_t index_0) {
    return "dialogue.choice_" + std::to_string(index_0 + 1) + "_key";
}

std::string choice_tone_bind(std::size_t index_0) {
    return "dialogue.choice_" + std::to_string(index_0 + 1) + "_tone";
}

std::string choice_standing_bind(std::size_t index_0) {
    return "dialogue.choice_" + std::to_string(index_0 + 1) + "_standing";
}

void set_choice_chrome_visible(HudRuntime& canvas, std::size_t index_0, bool visible) {
    canvas.set_visible(choice_key_id(index_0), visible);
    canvas.set_visible(choice_key_text_id(index_0), visible);
    canvas.set_visible(choice_tone_id(index_0), visible);
    canvas.set_visible(choice_tone_text_id(index_0), visible);
    canvas.set_visible(choice_standing_id(index_0), visible);
    canvas.set_visible(choice_standing_text_id(index_0), visible);
}

void hide_choice_slots(HudRuntime& canvas, DialogueUiSession& session) {
    for (std::size_t i = 0; i < kDialogueChoiceSlotCount; ++i) {
        canvas.set_visible(choice_widget_id(i), false);
        canvas.set_text(choice_bind(i), "");
        canvas.set_text(choice_key_bind(i), "");
        canvas.set_text(choice_tone_bind(i), "");
        canvas.set_text(choice_standing_bind(i), "");
        set_choice_chrome_visible(canvas, i, false);
        session.choice_slot_ids[i].clear();
    }
    canvas.set_visible("dialogue_prompt_panel", false);
    canvas.set_visible("dialogue_prompt", false);
    canvas.set_visible("dialogue_prompt_label", false);
}

void apply_speaker_chrome(HudRuntime& canvas, const DialoguePresent& view) {
    const std::string speaker = format_dialogue_speaker_display(view.speaker_id);
    canvas.set_text("dialogue.speaker", speaker);
    canvas.set_text("dialogue.portrait", format_dialogue_portrait_initials(view.speaker_id));
    canvas.set_text("dialogue.role", view.speaker_id.empty() ? std::string{} : std::string{"Speaking"});
}

void apply_line_page(HudRuntime& canvas, DialogueUiSession& session, const DialoguePresent& view) {
    session.choices_page = false;
    session.synced_node_id = view.node_id;
    session.last_line = view.line;
    hide_choice_slots(canvas, session);
    canvas.set_visible("dialogue_body", true);
    canvas.set_visible("dialogue_continue", true);
    apply_speaker_chrome(canvas, view);
    canvas.set_text_typed("dialogue.body", view.line, 52.0f);
}

void apply_choices_page(HudRuntime& canvas, DialogueUiSession& session, const DialoguePresent& view) {
    session.choices_page = true;
    session.synced_node_id = view.node_id;
    canvas.set_visible("dialogue_body", false);
    canvas.set_visible("dialogue_continue", false);
    apply_speaker_chrome(canvas, view);
    canvas.set_text("dialogue.role", "Choose your response");
    canvas.set_text("dialogue.prompt_label", "LAST LINE");
    canvas.set_text("dialogue.prompt", session.last_line.empty() ? view.line : session.last_line);
    canvas.set_visible("dialogue_prompt_panel", true);
    canvas.set_visible("dialogue_prompt", true);
    canvas.set_visible("dialogue_prompt_label", true);

    for (std::size_t i = 0; i < kDialogueChoiceSlotCount; ++i) {
        if (i < view.choices.size()) {
            const auto& choice = view.choices[i];
            session.choice_slot_ids[i] = choice.id;
            canvas.set_text(choice_bind(i), format_dialogue_choice_label(choice));
            canvas.set_text(choice_key_bind(i), std::to_string(i + 1));
            const std::string tone = format_dialogue_tone_chip(choice);
            const std::string standing = format_dialogue_standing_chip(choice);
            canvas.set_text(choice_tone_bind(i), tone);
            canvas.set_text(choice_standing_bind(i), standing);
            canvas.set_visible(choice_widget_id(i), true);
            canvas.set_enabled(choice_widget_id(i), true);
            canvas.set_visible(choice_key_id(i), true);
            canvas.set_visible(choice_key_text_id(i), true);
            canvas.set_visible(choice_tone_id(i), !tone.empty());
            canvas.set_visible(choice_tone_text_id(i), !tone.empty());
            canvas.set_visible(choice_standing_id(i), !standing.empty());
            canvas.set_visible(choice_standing_text_id(i), !standing.empty());
            if (!tone.empty()) {
                float bg[4]{};
                float fg[4]{};
                dialogue_tone_colors(tone, bg, fg);
                canvas.set_color(choice_tone_id(i), bg[0], bg[1], bg[2], bg[3]);
                canvas.set_color(choice_tone_text_id(i), fg[0], fg[1], fg[2], fg[3]);
                // Keycap picks up the same personality hue for a quick scan.
                canvas.set_color(choice_key_id(i), bg[0], bg[1], bg[2], bg[3]);
                canvas.set_color(choice_key_text_id(i), fg[0], fg[1], fg[2], fg[3]);
            } else {
                canvas.clear_color(choice_tone_id(i));
                canvas.clear_color(choice_tone_text_id(i));
                canvas.clear_color(choice_key_id(i));
                canvas.clear_color(choice_key_text_id(i));
            }
            if (!standing.empty()) {
                // Standing chip: cool green for gain, warm red for loss, neutral otherwise.
                bool any_pos = false;
                bool any_neg = false;
                for (const auto& adjust : choice.standing_adjust) {
                    if (adjust.delta > 0.0) any_pos = true;
                    if (adjust.delta < 0.0) any_neg = true;
                }
                if (any_pos && !any_neg) {
                    canvas.set_color(choice_standing_id(i), 46.0f, 110.0f, 72.0f, 255.0f);
                    canvas.set_color(choice_standing_text_id(i), 232.0f, 245.0f, 232.0f, 255.0f);
                } else if (any_neg && !any_pos) {
                    canvas.set_color(choice_standing_id(i), 140.0f, 56.0f, 48.0f, 255.0f);
                    canvas.set_color(choice_standing_text_id(i), 255.0f, 232.0f, 226.0f, 255.0f);
                } else {
                    canvas.set_color(choice_standing_id(i), 90.0f, 84.0f, 76.0f, 255.0f);
                    canvas.set_color(choice_standing_text_id(i), 241.0f, 238.0f, 232.0f, 255.0f);
                }
            } else {
                canvas.clear_color(choice_standing_id(i));
                canvas.clear_color(choice_standing_text_id(i));
            }
        } else {
            session.choice_slot_ids[i].clear();
            canvas.set_text(choice_bind(i), "");
            canvas.set_text(choice_key_bind(i), "");
            canvas.set_text(choice_tone_bind(i), "");
            canvas.set_text(choice_standing_bind(i), "");
            canvas.set_visible(choice_widget_id(i), false);
            set_choice_chrome_visible(canvas, i, false);
        }
    }
}

const WorldForgeDialogueChoice* find_choice(const DialoguePresent& view, const std::string& choice_id) {
    for (const auto& choice : view.choices) {
        if (choice.id == choice_id) return &choice;
    }
    return nullptr;
}

} // namespace

std::string format_dialogue_choice_label(const WorldForgeDialogueChoice& choice) {
    return choice.text;
}

std::string format_dialogue_tone_chip(const WorldForgeDialogueChoice& choice) {
    return choice.tone;
}

void dialogue_tone_colors(const std::string& tone, float rgba_bg[4], float rgba_fg[4]) {
    // Light text on saturated chip fills (AA-ish on parchment UI).
    auto set = [&](float r, float g, float b) {
        rgba_bg[0] = r;
        rgba_bg[1] = g;
        rgba_bg[2] = b;
        rgba_bg[3] = 255.0f;
        rgba_fg[0] = 245.0f;
        rgba_fg[1] = 242.0f;
        rgba_fg[2] = 235.0f;
        rgba_fg[3] = 255.0f;
    };
    std::string key;
    key.reserve(tone.size());
    for (char ch : tone) key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    if (key == "curious" || key == "ask") set(58.0f, 98.0f, 148.0f);           // blue
    else if (key == "honest" || key == "truthful") set(46.0f, 110.0f, 72.0f); // green
    else if (key == "blunt" || key == "defiant") set(156.0f, 72.0f, 48.0f);   // terracotta
    else if (key == "careful" || key == "cautious") set(140.0f, 108.0f, 48.0f); // amber
    else if (key == "calm" || key == "quiet") set(48.0f, 110.0f, 112.0f);     // teal
    else if (key == "resolute" || key == "firm") set(72.0f, 88.0f, 120.0f);   // steel
    else if (key == "closing" || key == "farewell") set(96.0f, 72.0f, 112.0f); // plum
    else if (key == "neutral" || key == "default") set(90.0f, 84.0f, 76.0f);  // warm gray
    else set(60.0f, 52.0f, 38.0f); // bronze fallback
}

std::string format_dialogue_standing_chip(const WorldForgeDialogueChoice& choice) {
    std::ostringstream out;
    bool first = true;
    for (const auto& adjust : choice.standing_adjust) {
        if (adjust.faction_id.empty() || !(std::abs(adjust.delta) > 0.0)) continue;
        if (!first) out << " · ";
        first = false;
        out << adjust.faction_id;
        out << (adjust.delta > 0.0 ? " +" : " ");
        if (adjust.delta == static_cast<double>(static_cast<int>(adjust.delta))) {
            out << static_cast<int>(adjust.delta);
        } else {
            out << adjust.delta;
        }
    }
    return out.str();
}

std::string format_dialogue_speaker_display(const std::string& speaker_id) {
    if (speaker_id.empty()) return "Narrator";
    std::string out = speaker_id;
    for (char& ch : out) {
        if (ch == '_' || ch == '-') ch = ' ';
    }
    bool cap = true;
    for (char& ch : out) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            cap = true;
            continue;
        }
        if (cap) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            cap = false;
        }
    }
    return out;
}

std::string format_dialogue_portrait_initials(const std::string& speaker_id) {
    if (speaker_id.empty()) return "?";
    std::string initials;
    bool take = true;
    for (char ch : speaker_id) {
        if (ch == '_' || ch == '-' || std::isspace(static_cast<unsigned char>(ch))) {
            take = true;
            continue;
        }
        if (take && std::isalnum(static_cast<unsigned char>(ch))) {
            initials.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            take = false;
            if (initials.size() >= 2) break;
        }
    }
    if (initials.empty()) initials.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(speaker_id.front()))));
    return initials;
}

void apply_dialogue_choice_standing(StandingRuntime* standing, const WorldForgeDialogueChoice& choice) {
    if (!standing || !standing->is_bound()) return;
    for (const auto& adjust : choice.standing_adjust) {
        if (adjust.faction_id.empty() || !(std::abs(adjust.delta) > 0.0)) continue;
        if (const auto result = standing->adjust(adjust.faction_id, adjust.delta); !result) {
            Logger::instance().write(Severity::Warning, "dialogue_ui",
                "standingAdjust failed for faction '" + adjust.faction_id + "': " + result.error().message);
        }
    }
}

void apply_dialogue_choice_flags(FlagRuntime* flags, const WorldForgeDialogueChoice& choice) {
    if (!flags) return;
    for (const auto& flag_id : choice.set_flags) {
        if (flag_id.empty()) continue;
        if (const auto result = flags->set(flag_id); !result) {
            Logger::instance().write(Severity::Warning, "dialogue_ui",
                "setFlags failed for '" + flag_id + "': " + result.error().message);
        }
    }
}

void sync_dialogue_canvas(UiCanvasStack* stack, DialogueRuntime* dialogue, DialogueUiSession& session) {
    if (!stack) return;
    HudRuntime* canvas = stack->find_canvas("dialogue");
    if (!canvas) return;
    if (!dialogue || dialogue->tree_id().empty()) {
        canvas->set_text("dialogue.speaker", "");
        canvas->set_text("dialogue.role", "");
        canvas->set_text("dialogue.portrait", "");
        canvas->set_text("dialogue.body", "");
        canvas->set_text("dialogue.prompt", "");
        hide_choice_slots(*canvas, session);
        session.choices_page = false;
        session.synced_node_id.clear();
        session.last_line.clear();
        canvas->set_visible("dialogue_body", true);
        canvas->set_visible("dialogue_continue", true);
        if (stack->top_modal() && *stack->top_modal() == "dialogue") {
            (void)stack->pop();
        }
        return;
    }
    const auto present = dialogue->present();
    if (!present) {
        canvas->set_text("dialogue.speaker", "");
        canvas->set_text("dialogue.body", present.error().message);
        hide_choice_slots(*canvas, session);
        session.choices_page = false;
        return;
    }
    const auto& view = present.value();
    if (view.complete) {
        hide_choice_slots(*canvas, session);
        session.choices_page = false;
        session.synced_node_id.clear();
        if (stack->top_modal() && *stack->top_modal() == "dialogue") {
            (void)stack->pop();
        }
        return;
    }
    if (!stack->top_modal() || *stack->top_modal() != "dialogue") {
        (void)stack->push("dialogue");
    }
    apply_line_page(*canvas, session, view);
}

bool dialogue_advance_continue(UiCanvasStack* stack, DialogueRuntime* dialogue, DialogueUiSession& session) {
    if (!stack || !dialogue || dialogue->tree_id().empty()) return false;
    HudRuntime* canvas = stack->find_canvas("dialogue");
    if (!canvas) return false;
    if (session.choices_page) return true;
    const auto present = dialogue->present();
    if (!present || present.value().complete) {
        dialogue->reset();
        sync_dialogue_canvas(stack, dialogue, session);
        return true;
    }
    const auto& view = present.value();
    if (view.choices.empty()) {
        dialogue->reset();
        sync_dialogue_canvas(stack, dialogue, session);
        return true;
    }
    apply_choices_page(*canvas, session, view);
    return true;
}

Result<void> dialogue_choose_with_ui(UiCanvasStack* stack, DialogueRuntime* dialogue, DialogueUiSession& session,
    StandingRuntime* standing, FlagRuntime* flags, const std::string& choice_id) {
    if (!dialogue) {
        return Result<void>::failure(ui_error("DIALOGUE-UI-STATE", "Dialogue runtime is not available",
            "Bind DialogueRuntime before choosing."));
    }
    const auto present = dialogue->present();
    if (!present) return Result<void>::failure(present.error());
    if (const auto* choice = find_choice(present.value(), choice_id)) {
        apply_dialogue_choice_standing(standing, *choice);
        apply_dialogue_choice_flags(flags, *choice);
    }
    const auto chosen = dialogue->choose(choice_id);
    if (!chosen) return chosen;
    sync_dialogue_canvas(stack, dialogue, session);
    return Result<void>::success();
}

Result<void> dialogue_activate_choice_slot(UiCanvasStack* stack, DialogueRuntime* dialogue, DialogueUiSession& session,
    StandingRuntime* standing, FlagRuntime* flags, int slot_1_based) {
    if (slot_1_based < 1 || static_cast<std::size_t>(slot_1_based) > kDialogueChoiceSlotCount) {
        return Result<void>::failure(ui_error("DIALOGUE-UI-SLOT", "Choice slot must be 1..4",
            "Use dialogue.choice_1 through dialogue.choice_4."));
    }
    const std::string& choice_id = session.choice_slot_ids[static_cast<std::size_t>(slot_1_based - 1)];
    if (choice_id.empty()) {
        return Result<void>::failure(ui_error("DIALOGUE-UI-SLOT", "Choice slot is empty",
            "Wait until choices are shown on the dialogue canvas."));
    }
    return dialogue_choose_with_ui(stack, dialogue, session, standing, flags, choice_id);
}

} // namespace engine
