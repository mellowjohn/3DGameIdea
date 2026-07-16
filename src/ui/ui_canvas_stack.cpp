#include "engine/ui/ui_canvas_stack.h"

#include "engine/scripting/lua_runtime.h"

#include <imgui.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

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

Result<void> UiCanvasStack::set_hud(const std::filesystem::path& path) {
    const auto loaded = hud_.load(path);
    if (!loaded) return loaded;
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
        result.canvas_id = canvas_id;
        (void)pop();
        result.handled = true;
        result.modal_popped = true;
        return result;
    }

    auto focusable = runtime->focusable_widget_ids();
    ensure_modal_focus(*runtime);
    focusable = runtime->focusable_widget_ids();

    if (event.mouse_clicked && in_viewport) {
        if (const auto hit = runtime->hit_test_widget(viewport_min, viewport_max, mouse_pos)) {
            if (const HudWidget* widget = find_widget(*runtime, *hit)) {
                if (widget->type == HudWidgetType::Slider) {
                    modal_focus_widget_id_ = *hit;
                    (void)runtime->apply_slider_click(viewport_min, viewport_max, *hit, mouse_pos);
                    result.handled = true;
                    result.canvas_id = canvas_id;
                    result.widget_id = widget->id;
                    result.activated_bind = widget->bind;
                    return result;
                }
                if (modal_focus_widget_id_ && *modal_focus_widget_id_ == *hit) {
                    result.handled = true;
                    result.activated_bind = widget->bind;
                    result.canvas_id = canvas_id;
                    result.widget_id = widget->id;
                    if (widget->type == HudWidgetType::Toggle) {
                        const bool next = !runtime->get_bool(widget->bind).value_or(false);
                        runtime->set_bool(widget->bind, next);
                    } else if (widget->type == HudWidgetType::Button) {
                        if (lua_runtime) lua_runtime->dispatch_ui_button(widget->bind, canvas_id, widget->id);
                    }
                } else {
                    modal_focus_widget_id_ = *hit;
                    result.handled = true;
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
            result.handled = true;
            result.activated_bind = widget->bind;
            result.canvas_id = canvas_id;
            result.widget_id = widget->id;
            if (widget->type == HudWidgetType::Toggle) {
                const bool next = !runtime->get_bool(widget->bind).value_or(false);
                runtime->set_bool(widget->bind, next);
            } else if (widget->type == HudWidgetType::Button) {
                if (lua_runtime) lua_runtime->dispatch_ui_button(widget->bind, canvas_id, widget->id);
            }
        }
        return result;
    }

    return result;
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
}

} // namespace engine
