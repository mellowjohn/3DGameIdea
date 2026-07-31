#include "engine/ui/ui_canvas_stack.h"

#include "engine/inventory/inventory_runtime.h"
#include "engine/scripting/lua_runtime.h"
#include "engine/ui/game_fonts.h"

#include <imgui.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cctype>

namespace engine {
namespace {

EngineError stack_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "ui_canvas_stack",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

const HudWidget* find_widget(const HudRuntime& runtime, const std::string& widget_id) {
    for (const auto& widget : runtime.asset().widgets) {
        if (widget.id == widget_id) return &widget;
    }
    return nullptr;
}

void nudge_slider(HudRuntime& runtime, const HudWidget& widget, bool left) {
    if (widget.bind.empty()) return;
    double current = runtime.get_number(widget.bind).value_or(0.0);
    double max_value = 1.0;
    if (!widget.max_bind.empty()) {
        if (const auto max_bound = runtime.get_number(widget.max_bind)) max_value = *max_bound;
    }
    if (!(max_value > 0.0)) max_value = 1.0;
    const double step = (std::abs(max_value - 1.0) < 0.0001) ? 0.05 : max_value * 0.05;
    if (left) current -= step;
    else current += step;
    current = std::clamp(current, 0.0, max_value);
    runtime.set_number(widget.bind, current);
}

} // namespace

void UiCanvasStack::set_texture_cache(UiTextureCache* cache) {
    textures_ = cache;
    hud_.set_texture_cache(cache);
    for (auto& [id, runtime] : canvases_) {
        (void)id;
        runtime.set_texture_cache(cache);
    }
}

Result<void> UiCanvasStack::set_hud(const std::filesystem::path& path) {
    const auto loaded = hud_.load(path);
    if (!loaded) return loaded;
    hud_.set_texture_cache(textures_);
    paths_["hud"] = path;
    return Result<void>::success();
}

Result<void> UiCanvasStack::register_canvas(std::string id, const std::filesystem::path& path) {
    if (id.empty()) {
        return Result<void>::failure(
            stack_error("UICANVAS-STACK-ID", "Canvas id is required", "Provide a non-empty id."));
    }
    if (id == "hud") {
        return Result<void>::failure(
            stack_error("UICANVAS-STACK-ID", "id 'hud' is reserved for the HUD layer", "Use set_hud / another id."));
    }
    HudRuntime runtime;
    const auto loaded = runtime.load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    runtime.set_texture_cache(textures_);
    paths_[id] = path;
    canvases_[id] = std::move(runtime);
    return Result<void>::success();
}

Result<void> UiCanvasStack::ensure_loaded(const std::string& id) {
    if (id.empty()) {
        return Result<void>::failure(
            stack_error("UICANVAS-STACK-ID", "Canvas id is required", "Provide a non-empty id."));
    }
    if (canvases_.find(id) != canvases_.end()) return Result<void>::success();
    const auto path_it = paths_.find(id);
    if (path_it == paths_.end()) {
        return Result<void>::failure(stack_error("UICANVAS-STACK-UNKNOWN", "Canvas is not registered: " + id,
            "Call register/show with a path, or register_canvas first."));
    }
    HudRuntime runtime;
    const auto loaded = runtime.load(path_it->second);
    if (!loaded) return Result<void>::failure(loaded.error());
    runtime.set_texture_cache(textures_);
    canvases_[id] = std::move(runtime);
    return Result<void>::success();
}

void UiCanvasStack::ensure_modal_focus(const HudRuntime& runtime) {
    const auto focusable = runtime.focusable_widget_ids();
    if (focusable.empty()) {
        modal_focus_widget_id_.reset();
        return;
    }
    if (!modal_focus_widget_id_ ||
        std::find(focusable.begin(), focusable.end(), *modal_focus_widget_id_) == focusable.end()) {
        modal_focus_widget_id_ = focusable.front();
    }
}

void UiCanvasStack::reset_modal_focus() {
    if (modal_stack_.empty()) {
        modal_focus_widget_id_.reset();
        return;
    }
    const auto it = canvases_.find(modal_stack_.back());
    if (it == canvases_.end()) {
        modal_focus_widget_id_.reset();
        return;
    }
    ensure_modal_focus(it->second);
}

Result<void> UiCanvasStack::push(const std::string& id) {
    const auto ensured = ensure_loaded(id);
    if (!ensured) return ensured;
    modal_stack_.erase(std::remove(modal_stack_.begin(), modal_stack_.end(), id), modal_stack_.end());
    modal_stack_.push_back(id);
    reset_modal_focus();
    return Result<void>::success();
}

Result<void> UiCanvasStack::pop() {
    if (modal_stack_.empty()) {
        return Result<void>::failure(
            stack_error("UICANVAS-STACK-EMPTY", "No modal canvas to pop", "push/show a canvas first."));
    }
    modal_stack_.pop_back();
    clear_drag_state();
    reset_modal_focus();
    return Result<void>::success();
}

Result<void> UiCanvasStack::show(const std::string& id) {
    return push(id);
}

Result<void> UiCanvasStack::hide(const std::string& id) {
    if (id.empty()) {
        return Result<void>::failure(
            stack_error("UICANVAS-STACK-ID", "Canvas id is required", "Provide a non-empty id."));
    }
    const auto before = modal_stack_.size();
    modal_stack_.erase(std::remove(modal_stack_.begin(), modal_stack_.end(), id), modal_stack_.end());
    if (modal_stack_.size() == before) {
        return Result<void>::failure(
            stack_error("UICANVAS-STACK-MISSING", "Canvas is not on the modal stack: " + id, "push/show it first."));
    }
    reset_modal_focus();
    return Result<void>::success();
}

void UiCanvasStack::clear_modals() {
    modal_stack_.clear();
    modal_focus_widget_id_.reset();
    clear_drag_state();
}

void UiCanvasStack::clear_drag_state() noexcept {
    modal_drag_ = {};
}

std::optional<std::string> UiCanvasStack::top_modal() const {
    if (modal_stack_.empty()) return std::nullopt;
    return modal_stack_.back();
}

bool UiCanvasStack::is_registered(const std::string& id) const {
    return canvases_.find(id) != canvases_.end() || paths_.find(id) != paths_.end();
}

HudRuntime* UiCanvasStack::find_canvas(const std::string& id) {
    const auto it = canvases_.find(id);
    if (it == canvases_.end()) return nullptr;
    return &it->second;
}

const HudRuntime* UiCanvasStack::find_canvas(const std::string& id) const {
    const auto it = canvases_.find(id);
    if (it == canvases_.end()) return nullptr;
    return &it->second;
}

namespace {

constexpr float kModalDragThresholdPx = 8.0f;

bool is_inventory_slot_bind(const std::string& bind) {
    return bind.rfind("inventory.select.", 0) == 0;
}

std::string inventory_image_bind_for_select(const std::string& select_bind) {
    // inventory.select.bag.3 -> inventory.bag.3.icon
    // inventory.select.hotbar.1 -> inventory.hotbar.1.icon
    // inventory.select.equip.head -> inventory.equip.head.icon
    static constexpr const char* kPrefix = "inventory.select.";
    if (select_bind.rfind(kPrefix, 0) != 0) return {};
    const std::string rest = select_bind.substr(std::string(kPrefix).size());
    return "inventory." + rest + ".icon";
}

bool parse_inventory_select_bind(const std::string& bind, std::string& region, int& index, std::string& equip_slot) {
    region.clear();
    index = -1;
    equip_slot.clear();
    static constexpr const char* kPrefix = "inventory.select.";
    if (bind.rfind(kPrefix, 0) != 0) return false;
    const std::string rest = bind.substr(std::string(kPrefix).size());
    if (rest.rfind("bag.", 0) == 0) {
        region = "bag";
        try {
            index = std::stoi(rest.substr(4));
        } catch (...) {
            return false;
        }
        return index >= 0;
    }
    if (rest.rfind("hotbar.", 0) == 0) {
        region = "hotbar";
        try {
            index = std::stoi(rest.substr(7));
        } catch (...) {
            return false;
        }
        return index >= 0;
    }
    if (rest.rfind("equip.", 0) == 0) {
        region = "equip";
        equip_slot = rest.substr(6);
        return !equip_slot.empty();
    }
    return false;
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

struct TooltipLines {
    std::string title;
    std::string subtitle;
};

std::optional<TooltipLines> inventory_tooltip_for_bind(const InventoryRuntime* inventory, const std::string& bind) {
    if (!inventory || !inventory->is_bound()) return std::nullopt;
    std::string region;
    int index = -1;
    std::string equip_slot;
    if (!parse_inventory_select_bind(bind, region, index, equip_slot)) return std::nullopt;

    const auto snap = inventory->status();
    const InventoryStack* stack = nullptr;
    std::string empty_hint;
    if (region == "bag") {
        empty_hint = "Bag slot";
        if (index >= 0 && index < static_cast<int>(snap.bag.size())) stack = &snap.bag[static_cast<std::size_t>(index)];
    } else if (region == "hotbar") {
        empty_hint = "Hotbar " + std::to_string(index + 1);
        if (index >= 0 && index < static_cast<int>(snap.hotbar.size()))
            stack = &snap.hotbar[static_cast<std::size_t>(index)];
    } else if (region == "equip") {
        empty_hint = "Equip · " + equip_slot;
        if (equip_slot == "head") stack = &snap.equipped.head;
        else if (equip_slot == "chest") stack = &snap.equipped.chest;
        else if (equip_slot == "legs") stack = &snap.equipped.legs;
        else if (equip_slot.rfind("trinket", 0) == 0 && equip_slot.size() > 7) {
            try {
                const int t = std::stoi(equip_slot.substr(7));
                if (t >= 0 && t < kInventoryTrinketSlots) stack = &snap.equipped.trinkets[static_cast<std::size_t>(t)];
            } catch (...) {
            }
        }
    }
    if (!stack || stack->empty()) {
        return TooltipLines{empty_hint.empty() ? "Empty" : "Empty", empty_hint};
    }
    TooltipLines lines;
    if (const ItemDef* def = inventory->find_def(stack->item_id)) {
        lines.title = def->display_name.empty() ? def->id : def->display_name;
        lines.subtitle = upper_ascii(def->kind);
    } else {
        lines.title = stack->item_id;
    }
    if (stack->count > 1) {
        if (!lines.subtitle.empty()) lines.subtitle += "  ·  ";
        lines.subtitle += "x" + std::to_string(stack->count);
    }
    return lines;
}

void draw_themed_tooltip(ImDrawList* draw_list, const ImVec2& mouse, const ImVec2& clip_min, const ImVec2& clip_max,
    const TooltipLines& lines) {
    if (!draw_list || lines.title.empty()) return;
    ImFont* font = GameFonts::ui() ? GameFonts::ui() : ImGui::GetFont();
    const float title_size = 16.0f;
    const float sub_size = 13.0f;
    const float pad_x = 12.0f;
    const float pad_y = 10.0f;
    const float gap = lines.subtitle.empty() ? 0.0f : 4.0f;
    const float wrap = 280.0f;
    const ImVec2 title_sz = font->CalcTextSizeA(title_size, wrap, wrap, lines.title.c_str());
    ImVec2 sub_sz{0.0f, 0.0f};
    if (!lines.subtitle.empty()) {
        sub_sz = font->CalcTextSizeA(sub_size, wrap, wrap, lines.subtitle.c_str());
    }
    const float box_w = std::max(title_sz.x, sub_sz.x) + pad_x * 2.0f;
    const float box_h = title_sz.y + gap + sub_sz.y + pad_y * 2.0f;
    float x = mouse.x + 18.0f;
    float y = mouse.y + 18.0f;
    if (x + box_w > clip_max.x - 4.0f) x = mouse.x - box_w - 12.0f;
    if (y + box_h > clip_max.y - 4.0f) y = mouse.y - box_h - 12.0f;
    x = std::clamp(x, clip_min.x + 4.0f, std::max(clip_min.x + 4.0f, clip_max.x - box_w - 4.0f));
    y = std::clamp(y, clip_min.y + 4.0f, std::max(clip_min.y + 4.0f, clip_max.y - box_h - 4.0f));
    const ImVec2 min{x, y};
    const ImVec2 max{x + box_w, y + box_h};
    draw_list->AddRectFilled(min, max, IM_COL32(30, 28, 24, 245), 4.0f);
    draw_list->AddRect(min, max, IM_COL32(213, 185, 120, 220), 4.0f, 0, 1.5f);
    const ImVec2 title_pos{min.x + pad_x, min.y + pad_y};
    draw_list->AddText(font, title_size, title_pos, IM_COL32(241, 238, 232, 255), lines.title.c_str(), nullptr, wrap);
    if (!lines.subtitle.empty()) {
        const ImVec2 sub_pos{min.x + pad_x, title_pos.y + title_sz.y + gap};
        draw_list->AddText(font, sub_size, sub_pos, IM_COL32(213, 185, 120, 230), lines.subtitle.c_str(), nullptr, wrap);
    }
}

void activate_widget(HudRuntime& runtime, LuaRuntime* lua_runtime, const std::string& canvas_id,
    const HudWidget& widget, UiCanvasInputResult& result) {
    result.handled = true;
    result.canvas_id = canvas_id;
    result.widget_id = widget.id;
    result.activated_bind = widget.bind;
    if (widget.type == HudWidgetType::Toggle) {
        const bool next = !runtime.get_bool(widget.bind).value_or(false);
        runtime.set_bool(widget.bind, next);
    } else if (widget.type == HudWidgetType::Button) {
        if (lua_runtime) lua_runtime->dispatch_ui_button(widget.bind, canvas_id, widget.id);
    }
}

} // namespace

UiCanvasInputResult UiCanvasStack::handle_modal_input(const UiCanvasInputEvent& event, LuaRuntime* lua_runtime) {
    UiCanvasInputResult result;
    if (modal_stack_.empty()) return result;
    const std::string canvas_id = modal_stack_.back();
    auto* runtime = find_canvas(canvas_id);
    if (!runtime) return result;

    const ImVec2 viewport_min{event.viewport_min.x, event.viewport_min.y};
    const ImVec2 viewport_max{event.viewport_max.x, event.viewport_max.y};
    const ImVec2 mouse_pos{event.mouse_pos.x, event.mouse_pos.y};
    const bool in_viewport = mouse_pos.x >= viewport_min.x && mouse_pos.x <= viewport_max.x &&
        mouse_pos.y >= viewport_min.y && mouse_pos.y <= viewport_max.y;

    if (event.cancel_pressed) {
        clear_drag_state();
        result.canvas_id = canvas_id;
        (void)pop();
        result.handled = true;
        result.modal_popped = true;
        return result;
    }

    if (event.digit_slot && *event.digit_slot >= 1 && *event.digit_slot <= 4 && lua_runtime) {
        const int slot = *event.digit_slot;
        const std::string widget_id = "dialogue_choice_" + std::to_string(slot);
        if (runtime->is_visible(widget_id) && runtime->is_enabled(widget_id)) {
            const std::string bind = "dialogue.choice_" + std::to_string(slot);
            modal_focus_widget_id_ = widget_id;
            lua_runtime->dispatch_ui_button(bind, canvas_id, widget_id);
            result.handled = true;
            result.activated_bind = bind;
            result.canvas_id = canvas_id;
            result.widget_id = widget_id;
            return result;
        }
    }

    auto focusable = runtime->focusable_widget_ids();
    ensure_modal_focus(*runtime);
    focusable = runtime->focusable_widget_ids();

    // Inventory slot drag: press on select.* → drag past threshold → release on another select.*.
    if (event.mouse_down && in_viewport) {
        if (const auto hit = runtime->hit_test_widget(viewport_min, viewport_max, mouse_pos)) {
            if (const HudWidget* widget = find_widget(*runtime, *hit)) {
                if (widget->type == HudWidgetType::Button && is_inventory_slot_bind(widget->bind)) {
                    modal_drag_ = {};
                    modal_drag_.active = true;
                    modal_drag_.widget_id = widget->id;
                    modal_drag_.bind = widget->bind;
                    modal_drag_.start_x = mouse_pos.x;
                    modal_drag_.start_y = mouse_pos.y;
                    modal_drag_.mouse_x = mouse_pos.x;
                    modal_drag_.mouse_y = mouse_pos.y;
                    const std::string image_bind = inventory_image_bind_for_select(widget->bind);
                    if (const auto path = runtime->get_image(image_bind)) modal_drag_.image_path = *path;
                    modal_focus_widget_id_ = *hit;
                    result.handled = true;
                    result.canvas_id = canvas_id;
                    result.widget_id = widget->id;
                    return result;
                }
            }
        }
    }

    if (modal_drag_.active && (event.mouse_held || event.mouse_released)) {
        modal_drag_.mouse_x = mouse_pos.x;
        modal_drag_.mouse_y = mouse_pos.y;
        const float dx = mouse_pos.x - modal_drag_.start_x;
        const float dy = mouse_pos.y - modal_drag_.start_y;
        if ((dx * dx + dy * dy) >= (kModalDragThresholdPx * kModalDragThresholdPx)) {
            modal_drag_.past_threshold = true;
        }
        result.handled = true;
        result.canvas_id = canvas_id;
        result.widget_id = modal_drag_.widget_id;

        if (event.mouse_released) {
            const std::string from_bind = modal_drag_.bind;
            const bool was_drag = modal_drag_.past_threshold;
            std::string to_bind;
            std::string to_widget;
            if (in_viewport) {
                if (const auto hit = runtime->hit_test_widget(viewport_min, viewport_max, mouse_pos)) {
                    if (const HudWidget* widget = find_widget(*runtime, *hit)) {
                        to_widget = widget->id;
                        to_bind = widget->bind;
                    }
                }
            }
            clear_drag_state();

            if (was_drag && is_inventory_slot_bind(from_bind) && is_inventory_slot_bind(to_bind) &&
                from_bind != to_bind) {
                result.drag_from_bind = from_bind;
                result.drag_to_bind = to_bind;
                result.widget_id = to_widget;
                nlohmann::json payload;
                payload["bind"] = "inventory.drag_drop";
                payload["fromBind"] = from_bind;
                payload["toBind"] = to_bind;
                payload["canvas"] = canvas_id;
                if (lua_runtime) {
                    (void)lua_runtime->call_handler("on_ui_button", payload.dump());
                }
                return result;
            }

            // Click (no drag): activate source slot as before.
            if (!was_drag) {
                if (const HudWidget* widget = find_widget(*runtime, result.widget_id)) {
                    if (widget->type == HudWidgetType::Slider) {
                        (void)runtime->apply_slider_click(viewport_min, viewport_max, widget->id, mouse_pos);
                        result.activated_bind = widget->bind;
                    } else {
                        activate_widget(*runtime, lua_runtime, canvas_id, *widget, result);
                    }
                }
            }
            return result;
        }
        return result;
    }

    // Non-slot buttons / sliders / toggles: click activates immediately.
    if (event.mouse_clicked && in_viewport && !modal_drag_.active) {
        if (const auto hit = runtime->hit_test_widget(viewport_min, viewport_max, mouse_pos)) {
            if (const HudWidget* widget = find_widget(*runtime, *hit)) {
                modal_focus_widget_id_ = *hit;
                result.canvas_id = canvas_id;
                result.widget_id = widget->id;
                if (widget->type == HudWidgetType::Slider) {
                    result.handled = true;
                    result.activated_bind = widget->bind;
                    (void)runtime->apply_slider_click(viewport_min, viewport_max, *hit, mouse_pos);
                } else if (widget->type == HudWidgetType::Toggle || widget->type == HudWidgetType::Button) {
                    if (!is_inventory_slot_bind(widget->bind)) {
                        activate_widget(*runtime, lua_runtime, canvas_id, *widget, result);
                    }
                }
            }
            return result;
        }
    }

    if ((event.nav_next || event.nav_prev) && !focusable.empty()) {
        std::size_t index = 0;
        if (modal_focus_widget_id_) {
            const auto it = std::find(focusable.begin(), focusable.end(), *modal_focus_widget_id_);
            if (it != focusable.end()) index = static_cast<std::size_t>(std::distance(focusable.begin(), it));
        }
        if (event.nav_next) index = (index + 1) % focusable.size();
        else index = (index + focusable.size() - 1) % focusable.size();
        modal_focus_widget_id_ = focusable[index];
        result.handled = true;
        return result;
    }

    if ((event.adjust_left || event.adjust_right) && modal_focus_widget_id_) {
        if (const HudWidget* widget = find_widget(*runtime, *modal_focus_widget_id_)) {
            if (widget->type == HudWidgetType::Slider) {
                nudge_slider(*runtime, *widget, event.adjust_left);
                result.handled = true;
                result.canvas_id = canvas_id;
                result.widget_id = widget->id;
                result.activated_bind = widget->bind;
                return result;
            }
        }
    }

    if (event.activate_pressed && modal_focus_widget_id_) {
        if (const HudWidget* widget = find_widget(*runtime, *modal_focus_widget_id_)) {
            activate_widget(*runtime, lua_runtime, canvas_id, *widget, result);
        }
        return result;
    }

    return result;
}

void UiCanvasStack::tick_typewriters(float delta_seconds) {
    hud_.tick_typewriter(delta_seconds);
    for (auto& [id, canvas] : canvases_) {
        (void)id;
        canvas.tick_typewriter(delta_seconds);
    }
}

void UiCanvasStack::draw_overlay(ImDrawList* draw_list, const ImVec2& image_min, const ImVec2& image_max) const {
    if (hud_.has_widgets()) hud_.draw_overlay(draw_list, image_min, image_max, std::nullopt);
    for (const auto& id : modal_stack_) {
        const auto it = canvases_.find(id);
        if (it == canvases_.end()) continue;
        const bool top = !modal_stack_.empty() && modal_stack_.back() == id;
        const std::optional<std::string>& focus = top ? modal_focus_widget_id_ : std::nullopt;
        it->second.draw_overlay(draw_list, image_min, image_max, focus);
    }

    // Drag ghost (simple square + optional texture).
    if (modal_drag_.active && modal_drag_.past_threshold && draw_list) {
        const float half = 28.0f;
        const ImVec2 min{modal_drag_.mouse_x - half, modal_drag_.mouse_y - half};
        const ImVec2 max{modal_drag_.mouse_x + half, modal_drag_.mouse_y + half};
        draw_list->AddRectFilled(min, max, IM_COL32(213, 185, 120, 90), 4.0f);
        draw_list->AddRect(min, max, IM_COL32(213, 185, 120, 220), 4.0f, 0, 2.0f);
        if (!modal_drag_.image_path.empty() && textures_) {
            // Texture draw goes through HudRuntime paths; ghost uses outline only if unloadable.
            (void)image_min;
            (void)image_max;
        }
    }

    // Themed hover tooltip (inventory slots + authored widget.tooltip). Skip while dragging.
    if (draw_list && !modal_stack_.empty() && !(modal_drag_.active && modal_drag_.past_threshold)) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        if (mouse.x >= image_min.x && mouse.x <= image_max.x && mouse.y >= image_min.y && mouse.y <= image_max.y) {
            const std::string& top_id = modal_stack_.back();
            if (const HudRuntime* runtime = find_canvas(top_id)) {
                if (const auto hit = runtime->hit_test_widget(image_min, image_max, mouse)) {
                    if (const HudWidget* widget = find_widget(*runtime, *hit)) {
                        std::optional<TooltipLines> tip = inventory_tooltip_for_bind(inventory_, widget->bind);
                        if (!tip && !widget->tooltip.empty()) {
                            tip = TooltipLines{widget->tooltip, {}};
                        }
                        if (tip) draw_themed_tooltip(draw_list, mouse, image_min, image_max, *tip);
                    }
                }
            }
        }
    }
}

} // namespace engine
