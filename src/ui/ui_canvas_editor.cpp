#include "engine/ui/ui_canvas_editor.h"

#include "engine/assets/ui_canvas_mutate.h"
#include "engine/ui/hud_runtime.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

namespace engine {
namespace {

constexpr float kHandleHitPx = 9.0f;
constexpr float kHandleDrawPx = 7.0f;
constexpr float kMinWidgetSize = 8.0f;

const char* type_name(HudWidgetType type) {
    switch (type) {
    case HudWidgetType::Bar: return "bar";
    case HudWidgetType::Text: return "text";
    case HudWidgetType::Panel: return "panel";
    case HudWidgetType::Button: return "button";
    case HudWidgetType::Toggle: return "toggle";
    case HudWidgetType::Slider: return "slider";
    case HudWidgetType::Image: return "image";
    }
    return "panel";
}

HudWidget* find_widget(UiCanvasAsset& canvas, const std::string& id) {
    for (auto& widget : canvas.widgets) {
        if (widget.id == id) return &widget;
    }
    return nullptr;
}

ImVec2 anchored_origin(HudAnchor anchor, const ImVec2& content_min, const ImVec2& content_max, float width,
    float height, float offset_x, float offset_y) {
    const float left = content_min.x;
    const float top = content_min.y;
    const float right = content_max.x;
    const float bottom = content_max.y;
    switch (anchor) {
    case HudAnchor::TopLeft: return ImVec2{left + offset_x, top + offset_y};
    case HudAnchor::TopRight: return ImVec2{right - width - offset_x, top + offset_y};
    case HudAnchor::BottomLeft: return ImVec2{left + offset_x, bottom - height - offset_y};
    case HudAnchor::BottomRight: return ImVec2{right - width - offset_x, bottom - height - offset_y};
    case HudAnchor::Center:
        return ImVec2{left + (right - left - width) * 0.5f + offset_x, top + (bottom - top - height) * 0.5f + offset_y};
    }
    return ImVec2{left + offset_x, top + offset_y};
}

struct DesignRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

DesignRect design_rect(const HudWidget& widget, float design_w, float design_h) {
    DesignRect rect{0.0f, 0.0f, widget.size[0], widget.size[1]};
    switch (widget.anchor) {
    case HudAnchor::TopLeft:
        rect.x = widget.offset[0];
        rect.y = widget.offset[1];
        break;
    case HudAnchor::TopRight:
        rect.x = design_w - widget.size[0] - widget.offset[0];
        rect.y = widget.offset[1];
        break;
    case HudAnchor::BottomLeft:
        rect.x = widget.offset[0];
        rect.y = design_h - widget.size[1] - widget.offset[1];
        break;
    case HudAnchor::BottomRight:
        rect.x = design_w - widget.size[0] - widget.offset[0];
        rect.y = design_h - widget.size[1] - widget.offset[1];
        break;
    case HudAnchor::Center:
        rect.x = (design_w - widget.size[0]) * 0.5f + widget.offset[0];
        rect.y = (design_h - widget.size[1]) * 0.5f + widget.offset[1];
        break;
    }
    return rect;
}

void apply_design_rect(HudWidget& widget, DesignRect rect, float design_w, float design_h) {
    rect.w = (std::max)(kMinWidgetSize, rect.w);
    rect.h = (std::max)(kMinWidgetSize, rect.h);
    widget.size[0] = rect.w;
    widget.size[1] = rect.h;
    switch (widget.anchor) {
    case HudAnchor::TopLeft:
        widget.offset[0] = rect.x;
        widget.offset[1] = rect.y;
        break;
    case HudAnchor::TopRight:
        widget.offset[0] = design_w - rect.x - rect.w;
        widget.offset[1] = rect.y;
        break;
    case HudAnchor::BottomLeft:
        widget.offset[0] = rect.x;
        widget.offset[1] = design_h - rect.y - rect.h;
        break;
    case HudAnchor::BottomRight:
        widget.offset[0] = design_w - rect.x - rect.w;
        widget.offset[1] = design_h - rect.y - rect.h;
        break;
    case HudAnchor::Center:
        widget.offset[0] = rect.x - (design_w - rect.w) * 0.5f;
        widget.offset[1] = rect.y - (design_h - rect.h) * 0.5f;
        break;
    }
}

bool contains_point(const DesignRect& rect, float px, float py) {
    return px >= rect.x && px <= rect.x + rect.w && py >= rect.y && py <= rect.y + rect.h;
}

std::vector<int> compute_group_parents(const UiCanvasAsset& canvas) {
    const float design_w = canvas.design_resolution[0];
    const float design_h = canvas.design_resolution[1];
    std::vector<int> parent(canvas.widgets.size(), -1);
    for (std::size_t i = 0; i < canvas.widgets.size(); ++i) {
        const DesignRect child = design_rect(canvas.widgets[i], design_w, design_h);
        const float cx = child.x + child.w * 0.5f;
        const float cy = child.y + child.h * 0.5f;
        for (int j = static_cast<int>(i) - 1; j >= 0; --j) {
            if (canvas.widgets[static_cast<std::size_t>(j)].type != HudWidgetType::Panel) continue;
            const DesignRect panel = design_rect(canvas.widgets[static_cast<std::size_t>(j)], design_w, design_h);
            if (contains_point(panel, cx, cy)) {
                parent[i] = j;
                break;
            }
        }
    }
    return parent;
}

UiCanvasResizeHandle hit_resize_handle(const ImVec2& mouse, const ImVec2& min, const ImVec2& max, float hit_px) {
    const bool in_x = mouse.x >= min.x - hit_px && mouse.x <= max.x + hit_px;
    const bool in_y = mouse.y >= min.y - hit_px && mouse.y <= max.y + hit_px;
    if (!in_x || !in_y) return UiCanvasResizeHandle::None;
    const bool near_l = std::abs(mouse.x - min.x) <= hit_px;
    const bool near_r = std::abs(mouse.x - max.x) <= hit_px;
    const bool near_t = std::abs(mouse.y - min.y) <= hit_px;
    const bool near_b = std::abs(mouse.y - max.y) <= hit_px;
    if (near_t && near_l) return UiCanvasResizeHandle::NW;
    if (near_t && near_r) return UiCanvasResizeHandle::NE;
    if (near_b && near_l) return UiCanvasResizeHandle::SW;
    if (near_b && near_r) return UiCanvasResizeHandle::SE;
    if (near_t) return UiCanvasResizeHandle::N;
    if (near_b) return UiCanvasResizeHandle::S;
    if (near_l) return UiCanvasResizeHandle::W;
    if (near_r) return UiCanvasResizeHandle::E;
    return UiCanvasResizeHandle::None;
}

void draw_resize_handles(ImDrawList* draw, const ImVec2& min, const ImVec2& max) {
    const float half = kHandleDrawPx * 0.5f;
    const ImVec2 points[8] = {
        {min.x, min.y},
        {(min.x + max.x) * 0.5f, min.y},
        {max.x, min.y},
        {max.x, (min.y + max.y) * 0.5f},
        {max.x, max.y},
        {(min.x + max.x) * 0.5f, max.y},
        {min.x, max.y},
        {min.x, (min.y + max.y) * 0.5f},
    };
    for (const ImVec2& p : points) {
        draw->AddRectFilled(ImVec2{p.x - half, p.y - half}, ImVec2{p.x + half, p.y + half},
            IM_COL32(255, 230, 120, 255));
        draw->AddRect(ImVec2{p.x - half, p.y - half}, ImVec2{p.x + half, p.y + half}, IM_COL32(40, 36, 20, 255));
    }
}

DesignRect resize_rect(DesignRect start, UiCanvasResizeHandle handle, float dx, float dy) {
    DesignRect rect = start;
    switch (handle) {
    case UiCanvasResizeHandle::E:
        rect.w += dx;
        break;
    case UiCanvasResizeHandle::W:
        rect.x += dx;
        rect.w -= dx;
        break;
    case UiCanvasResizeHandle::S:
        rect.h += dy;
        break;
    case UiCanvasResizeHandle::N:
        rect.y += dy;
        rect.h -= dy;
        break;
    case UiCanvasResizeHandle::SE:
        rect.w += dx;
        rect.h += dy;
        break;
    case UiCanvasResizeHandle::SW:
        rect.x += dx;
        rect.w -= dx;
        rect.h += dy;
        break;
    case UiCanvasResizeHandle::NE:
        rect.w += dx;
        rect.y += dy;
        rect.h -= dy;
        break;
    case UiCanvasResizeHandle::NW:
        rect.x += dx;
        rect.y += dy;
        rect.w -= dx;
        rect.h -= dy;
        break;
    case UiCanvasResizeHandle::None:
        break;
    }
    if (rect.w < kMinWidgetSize) {
        if (handle == UiCanvasResizeHandle::W || handle == UiCanvasResizeHandle::NW ||
            handle == UiCanvasResizeHandle::SW)
            rect.x = start.x + start.w - kMinWidgetSize;
        rect.w = kMinWidgetSize;
    }
    if (rect.h < kMinWidgetSize) {
        if (handle == UiCanvasResizeHandle::N || handle == UiCanvasResizeHandle::NW ||
            handle == UiCanvasResizeHandle::NE)
            rect.y = start.y + start.h - kMinWidgetSize;
        rect.h = kMinWidgetSize;
    }
    return rect;
}

bool looks_like_player_hud(const std::filesystem::path& path, const UiCanvasAsset& canvas) {
    const auto name = path.filename().generic_string();
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower == "player.uicanvas.json" || canvas.id == "player_hud";
}

std::string sanitize_canvas_id(std::string raw) {
    for (char& c : raw) {
        const auto u = static_cast<unsigned char>(c);
        if (std::isalnum(u) || c == '_' || c == '-') continue;
        c = '_';
    }
    while (!raw.empty() && (raw.front() == '_' || raw.front() == '-')) raw.erase(raw.begin());
    if (raw.empty()) raw = "ui_screen";
    return raw;
}

std::vector<std::filesystem::path> list_ui_canvases(const std::filesystem::path& project_root) {
    std::vector<std::filesystem::path> paths;
    const auto dir = project_root / "assets" / "ui";
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) return paths;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file()) continue;
        const auto name = entry.path().filename().generic_string();
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower.size() >= 14 && lower.substr(lower.size() - 14) == ".uicanvas.json")
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

void draw_widget_outliner_node(UiCanvasEditorSession& session, const std::vector<int>& parents, int index,
    int depth) {
    const auto& widget = session.canvas.widgets[static_cast<std::size_t>(index)];
    ImGui::PushID(index);
    const bool selected = session.selected_id && *session.selected_id == widget.id;
    if (depth > 0) ImGui::Indent(static_cast<float>(depth) * 12.0f);
    char label[192]{};
    std::snprintf(label, sizeof(label), "%s  (%s)", widget.id.c_str(), type_name(widget.type));
    if (ImGui::Selectable(label, selected)) {
        session.selected_id = widget.id;
        session.edit_mode = UiCanvasEditMode::Idle;
    }
    if (depth > 0) ImGui::Unindent(static_cast<float>(depth) * 12.0f);
    ImGui::PopID();

    for (std::size_t child = 0; child < parents.size(); ++child) {
        if (parents[child] == index)
            draw_widget_outliner_node(session, parents, static_cast<int>(child), depth + 1);
    }
}

void draw_widget_outliner(UiCanvasEditorSession& session) {
    ImGui::Separator();
    ImGui::TextUnformatted("Widgets");
    ImGui::TextDisabled("Alt+click cycles stacked picks");
    if (session.canvas.widgets.empty()) {
        ImGui::TextDisabled("(empty canvas)");
        return;
    }
    const auto parents = compute_group_parents(session.canvas);
    ImGui::BeginChild("UICanvasOutliner", ImVec2(0.0f, 160.0f), true);
    for (std::size_t i = 0; i < parents.size(); ++i) {
        if (parents[i] < 0) draw_widget_outliner_node(session, parents, static_cast<int>(i), 0);
    }
    ImGui::EndChild();
}

void draw_inspector(UiCanvasEditorSession& session) {
    draw_widget_outliner(session);
    ImGui::Separator();
    ImGui::TextUnformatted("Inspector");
    if (!session.selected_id) {
        ImGui::TextDisabled("Select a widget in the canvas or list");
        return;
    }
    auto* widget = find_widget(session.canvas, *session.selected_id);
    if (!widget) {
        ImGui::TextDisabled("Selection missing");
        return;
    }
    ImGui::Text("id: %s", widget->id.c_str());
    ImGui::Text("type: %s", type_name(widget->type));
    float offset[2] = {widget->offset[0], widget->offset[1]};
    if (ImGui::DragFloat2("offset", offset, 1.0f)) {
        widget->offset[0] = offset[0];
        widget->offset[1] = offset[1];
        session.dirty = true;
    }
    float size[2] = {widget->size[0], widget->size[1]};
    if (ImGui::DragFloat2("size", size, 1.0f, 1.0f, 4096.0f)) {
        widget->size[0] = std::max(1.0f, size[0]);
        widget->size[1] = std::max(1.0f, size[1]);
        session.dirty = true;
    }
    float color[4] = {
        widget->has_color() ? widget->color[0] / 255.0f : 0.85f,
        widget->has_color() ? widget->color[1] / 255.0f : 0.88f,
        widget->has_color() ? widget->color[2] / 255.0f : 0.95f,
        widget->has_color() ? widget->color[3] / 255.0f : 1.0f,
    };
    if (ImGui::ColorEdit4("color", color)) {
        widget->color[0] = color[0] * 255.0f;
        widget->color[1] = color[1] * 255.0f;
        widget->color[2] = color[2] * 255.0f;
        widget->color[3] = color[3] * 255.0f;
        session.dirty = true;
    }
    float opacity = widget->opacity;
    if (ImGui::SliderFloat("opacity", &opacity, 0.0f, 1.0f, "%.2f")) {
        widget->opacity = std::clamp(opacity, 0.0f, 1.0f);
        session.dirty = true;
    }
    if (ImGui::Checkbox("visible", &widget->visible)) session.dirty = true;
    if (ImGui::Checkbox("enabled (active)", &widget->enabled)) session.dirty = true;
    float font_size = widget->font_size > 0.0f ? widget->font_size : widget->size[1];
    if (ImGui::DragFloat("fontSize", &font_size, 0.5f, 0.0f, 256.0f)) {
        widget->font_size = font_size;
        session.dirty = true;
    }
    if (widget->type == HudWidgetType::Text || widget->type == HudWidgetType::Button ||
        widget->type == HudWidgetType::Toggle || widget->type == HudWidgetType::Slider) {
        char text[256]{};
        std::snprintf(text, sizeof(text), "%s", widget->text.c_str());
        if (ImGui::InputText("text", text, sizeof(text))) {
            widget->text = text;
            session.dirty = true;
        }
        char bind[128]{};
        std::snprintf(bind, sizeof(bind), "%s", widget->bind.c_str());
        if (ImGui::InputText("bind", bind, sizeof(bind))) {
            widget->bind = bind;
            session.dirty = true;
        }
        if (widget->type == HudWidgetType::Slider) {
            char max_bind[128]{};
            std::snprintf(max_bind, sizeof(max_bind), "%s", widget->max_bind.c_str());
            if (ImGui::InputText("maxBind", max_bind, sizeof(max_bind))) {
                widget->max_bind = max_bind;
                session.dirty = true;
            }
        }
        if (widget->type == HudWidgetType::Text || widget->type == HudWidgetType::Button) {
            const char* h_align[] = {"left", "center", "right"};
            int h_index =
                widget->text_align == HudTextAlign::Center ? 1 : widget->text_align == HudTextAlign::Right ? 2 : 0;
            if (ImGui::Combo("textAlign", &h_index, h_align, IM_ARRAYSIZE(h_align))) {
                widget->text_align =
                    h_index == 1 ? HudTextAlign::Center : h_index == 2 ? HudTextAlign::Right : HudTextAlign::Left;
                session.dirty = true;
            }
            const char* v_align[] = {"top", "middle", "bottom"};
            int v_index = widget->text_v_align == HudTextVAlign::Middle
                ? 1
                : widget->text_v_align == HudTextVAlign::Bottom ? 2 : 0;
            if (ImGui::Combo("textVAlign", &v_index, v_align, IM_ARRAYSIZE(v_align))) {
                widget->text_v_align =
                    v_index == 1 ? HudTextVAlign::Middle : v_index == 2 ? HudTextVAlign::Bottom : HudTextVAlign::Top;
                session.dirty = true;
            }
        }
    }
    if (widget->type == HudWidgetType::Image || widget->type == HudWidgetType::Button ||
        widget->type == HudWidgetType::Panel) {
        char image[256]{};
        std::snprintf(image, sizeof(image), "%s", widget->image.c_str());
        if (ImGui::InputText("image", image, sizeof(image))) {
            widget->image = image;
            session.dirty = true;
        }
        const char* modes[] = {"stretch", "contain"};
        int mode_index = widget->image_mode == HudImageMode::Contain ? 1 : 0;
        if (ImGui::Combo("imageMode", &mode_index, modes, IM_ARRAYSIZE(modes))) {
            widget->image_mode = mode_index == 1 ? HudImageMode::Contain : HudImageMode::Stretch;
            session.dirty = true;
        }
    }
    char label[128]{};
    std::snprintf(label, sizeof(label), "%s", widget->label.c_str());
    if (ImGui::InputText("label", label, sizeof(label))) {
        widget->label = label;
        session.dirty = true;
    }
    if (ImGui::Button("Remove widget")) {
        nlohmann::json params{{"id", widget->id}};
        auto mutated = mutate_ui_canvas_asset(session.canvas, "remove", params.dump());
        if (mutated) {
            session.canvas = std::move(mutated.value());
            session.selected_id.reset();
            session.dirty = true;
        }
    }
}

void draw_canvas_toolbar(UiCanvasEditorSession& session, const std::filesystem::path& project_root,
    const std::function<void(const std::filesystem::path&)>& on_saved) {
    const auto canvases = list_ui_canvases(project_root);
    const std::string current = session.path.empty() ? std::string("(none)") : session.path.filename().generic_string();
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("##canvas_file", current.c_str())) {
        for (const auto& path : canvases) {
            const auto name = path.filename().generic_string();
            const bool selected = !session.path.empty() && session.path == path;
            if (ImGui::Selectable(name.c_str(), selected)) {
                if (session.dirty) {
                    // Soft switch: discard unsaved unless they Save first — still allow switch.
                }
                (void)session.load(path);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("New...")) {
        session.show_new_popup = true;
        if (session.new_canvas_id[0] == '\0')
            std::snprintf(session.new_canvas_id.data(), session.new_canvas_id.size(), "main_menu");
        ImGui::OpenPopup("New UI Canvas");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        if (!session.path.empty()) (void)session.load(session.path);
        else if (!project_root.empty()) (void)session.load(default_player_ui_canvas_path(project_root));
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (session.dirty && session.save() && on_saved) on_saved(session.path);
    }
    ImGui::SameLine();
    if (ImGui::Button("Add panel")) {
        nlohmann::json params;
        params["id"] = "panel_" + std::to_string(session.canvas.widgets.size() + 1);
        params["type"] = "panel";
        params["offset"] = {64.0f, 64.0f};
        params["size"] = {240.0f, 120.0f};
        params["color"] = {30.0f, 36.0f, 48.0f, 220.0f};
        auto mutated = mutate_ui_canvas_asset(session.canvas, "add", params.dump());
        if (mutated) {
            session.selected_id = params["id"].get<std::string>();
            session.canvas = std::move(mutated.value());
            session.dirty = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Add button")) {
        const std::string id = "button_" + std::to_string(session.canvas.widgets.size() + 1);
        nlohmann::json params;
        params["id"] = id;
        params["type"] = "button";
        params["anchor"] = "center";
        params["offset"] = {0.0f, 0.0f};
        params["size"] = {200.0f, 48.0f};
        params["bind"] = id;
        params["text"] = "Button";
        params["textAlign"] = "center";
        params["textVAlign"] = "middle";
        params["fontSize"] = 28.0f;
        params["color"] = {48.0f, 58.0f, 78.0f, 255.0f};
        auto mutated = mutate_ui_canvas_asset(session.canvas, "add", params.dump());
        if (mutated) {
            session.selected_id = id;
            session.canvas = std::move(mutated.value());
            session.dirty = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Add toggle")) {
        const std::string id = "toggle_" + std::to_string(session.canvas.widgets.size() + 1);
        nlohmann::json params;
        params["id"] = id;
        params["type"] = "toggle";
        params["anchor"] = "center";
        params["offset"] = {0.0f, 0.0f};
        params["size"] = {280.0f, 40.0f};
        params["bind"] = id;
        params["text"] = "Toggle";
        params["fontSize"] = 24.0f;
        auto mutated = mutate_ui_canvas_asset(session.canvas, "add", params.dump());
        if (mutated) {
            session.selected_id = id;
            session.canvas = std::move(mutated.value());
            session.dirty = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Add slider")) {
        const std::string id = "slider_" + std::to_string(session.canvas.widgets.size() + 1);
        nlohmann::json params;
        params["id"] = id;
        params["type"] = "slider";
        params["anchor"] = "center";
        params["offset"] = {0.0f, 0.0f};
        params["size"] = {320.0f, 28.0f};
        params["bind"] = id;
        params["label"] = "Slider";
        auto mutated = mutate_ui_canvas_asset(session.canvas, "add", params.dump());
        if (mutated) {
            session.selected_id = id;
            session.canvas = std::move(mutated.value());
            session.dirty = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Add image")) {
        const std::string id = "image_" + std::to_string(session.canvas.widgets.size() + 1);
        nlohmann::json params;
        params["id"] = id;
        params["type"] = "image";
        params["offset"] = {64.0f, 64.0f};
        params["size"] = {128.0f, 128.0f};
        params["image"] = "assets/ui/textures/placeholder.png";
        auto mutated = mutate_ui_canvas_asset(session.canvas, "add", params.dump());
        if (mutated) {
            session.selected_id = id;
            session.canvas = std::move(mutated.value());
            session.dirty = true;
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Use as HUD", &session.apply_as_hud);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("HUD = always-on player UI. Uncheck to register this file as a modal/screen by canvas id.");
    }
    if (session.dirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "unsaved");
    }
    if (!session.canvas.id.empty()) {
        ImGui::TextDisabled("canvas id: %s  ·  drag body to move, corners/edges to resize", session.canvas.id.c_str());
        ImGui::SameLine();
        const char* scale_modes[] = {"letterbox", "fill_edges"};
        int scale_index = session.canvas.scale_mode == UiCanvasScaleMode::FillEdges ? 1 : 0;
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::Combo("scaleMode", &scale_index, scale_modes, IM_ARRAYSIZE(scale_modes))) {
            session.canvas.scale_mode =
                scale_index == 1 ? UiCanvasScaleMode::FillEdges : UiCanvasScaleMode::Letterbox;
            session.dirty = true;
        }
    }

    if (session.show_new_popup) ImGui::OpenPopup("New UI Canvas");
    if (ImGui::BeginPopupModal("New UI Canvas", &session.show_new_popup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Creates assets/ui/<id>.uicanvas.json");
        ImGui::InputText("id", session.new_canvas_id.data(), session.new_canvas_id.size());
        if (ImGui::Button("Create")) {
            const std::string id = sanitize_canvas_id(session.new_canvas_id.data());
            if (session.create_new(project_root, id)) {
                session.show_new_popup = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            session.show_new_popup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace

Result<void> UiCanvasEditorSession::load(const std::filesystem::path& absolute_path) {
    auto loaded = UiCanvasAsset::load(absolute_path);
    if (!loaded) return Result<void>::failure(loaded.error());
    path = absolute_path;
    canvas = std::move(loaded.value());
    dirty = false;
    edit_mode = UiCanvasEditMode::Idle;
    resize_handle = UiCanvasResizeHandle::None;
    apply_as_hud = looks_like_player_hud(path, canvas);
    if (selected_id && !find_widget(canvas, *selected_id)) selected_id.reset();
    return Result<void>::success();
}

Result<void> UiCanvasEditorSession::save() {
    if (path.empty()) {
        return Result<void>::failure(EngineError{"UICANVAS-EDITOR-PATH", Severity::Error, ErrorCategory::Validation,
            "ui_canvas_editor", "No canvas path loaded", ENGINE_SOURCE_CONTEXT, {}, "Load a *.uicanvas.json first.",
            make_correlation_id()});
    }
    const auto written = write_ui_canvas_json_atomic(path, canvas.to_json());
    if (!written) return Result<void>::failure(written.error());
    dirty = false;
    return Result<void>::success();
}

Result<void> UiCanvasEditorSession::create_new(const std::filesystem::path& project_root, const std::string& canvas_id) {
    const std::string id = sanitize_canvas_id(canvas_id);
    if (id == "hud") {
        return Result<void>::failure(EngineError{"UICANVAS-EDITOR-ID", Severity::Error, ErrorCategory::Validation,
            "ui_canvas_editor", "id 'hud' is reserved", ENGINE_SOURCE_CONTEXT, {}, "Use set_hud via 'Use as HUD', or pick another id.",
            make_correlation_id()});
    }
    const auto absolute = project_root / "assets" / "ui" / (id + ".uicanvas.json");
    if (std::filesystem::exists(absolute)) {
        return Result<void>::failure(EngineError{"UICANVAS-EDITOR-EXISTS", Severity::Error, ErrorCategory::Validation,
            "ui_canvas_editor", "Canvas file already exists", ENGINE_SOURCE_CONTEXT, {{"path", absolute.generic_string()}},
            "Pick a new id or open the existing canvas.", make_correlation_id()});
    }
    UiCanvasAsset created;
    created.schema_version = 1;
    created.id = id;
    created.design_resolution = {{1920.0f, 1080.0f}};
    HudWidget panel;
    panel.id = id + "_panel";
    panel.type = HudWidgetType::Panel;
    panel.anchor = HudAnchor::Center;
    panel.offset = {{0.0f, 0.0f}};
    panel.size = {{640.0f, 360.0f}};
    panel.color = {{24.0f, 28.0f, 40.0f, 230.0f}};
    created.widgets.push_back(panel);
    HudWidget title;
    title.id = id + "_title";
    title.type = HudWidgetType::Text;
    title.anchor = HudAnchor::Center;
    title.offset = {{-160.0f, -24.0f}};
    title.size = {{320.0f, 48.0f}};
    title.bind = id + ".title";
    title.text = id;
    title.label = id;
    title.font_size = 36.0f;
    title.color = {{230.0f, 235.0f, 245.0f, 255.0f}};
    created.widgets.push_back(title);

    const auto written = write_ui_canvas_json_atomic(absolute, created.to_json());
    if (!written) return Result<void>::failure(written.error());
    auto loaded = load(absolute);
    if (!loaded) return loaded;
    apply_as_hud = false;
    selected_id = panel.id;
    dirty = false;
    return Result<void>::success();
}

void draw_ui_canvas_viewport(UiCanvasEditorSession& session, const std::filesystem::path& project_root,
    const std::function<void(const std::filesystem::path&)>& on_saved) {
    if (session.path.empty() && !project_root.empty()) {
        (void)session.load(default_player_ui_canvas_path(project_root));
    }

    draw_canvas_toolbar(session, project_root, on_saved);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.x = (std::max)(1.0f, avail.x);
    avail.y = (std::max)(1.0f, avail.y);
    const float inspector_w = (std::clamp)(avail.x * 0.28f, 220.0f, 360.0f);
    const float preview_w = (std::max)(1.0f, avail.x - inspector_w - 8.0f);

    ImGui::BeginChild("UICanvasPreview", ImVec2(preview_w, avail.y), true);
    {
        const ImVec2 canvas_min = ImGui::GetCursorScreenPos();
        ImVec2 preview_avail = ImGui::GetContentRegionAvail();
        preview_avail.x = (std::max)(1.0f, preview_avail.x);
        preview_avail.y = (std::max)(1.0f, preview_avail.y);
        const ImVec2 canvas_max{canvas_min.x + preview_avail.x, canvas_min.y + preview_avail.y};
        ImGui::InvisibleButton("ui_canvas_hit", preview_avail);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(canvas_min, canvas_max, IM_COL32(24, 26, 32, 255));

        const float design_w = session.canvas.design_resolution[0];
        const float design_h = session.canvas.design_resolution[1];
        const auto letterbox = compute_ui_canvas_layout(canvas_min.x, canvas_min.y, canvas_max.x, canvas_max.y, design_w,
            design_h, session.canvas.scale_mode);
        const ImVec2 content_min{letterbox.content_min_x, letterbox.content_min_y};
        const ImVec2 content_max{letterbox.content_max_x, letterbox.content_max_y};
        draw->PushClipRect(canvas_min, canvas_max, true);
        draw->AddRectFilled(content_min, content_max, IM_COL32(40, 44, 54, 255));
        draw->AddRect(content_min, content_max, IM_COL32(90, 100, 120, 255));
        draw->PopClipRect();

        HudRuntime preview;
        (void)preview.load_from_json(session.canvas.to_json(), "ui-canvas-preview");
        preview.reset_player_health(72.0, 100.0);
        // Re-apply authored text each frame so inspector edits preview immediately.
        for (const auto& widget : session.canvas.widgets) {
            if ((widget.type == HudWidgetType::Text || widget.type == HudWidgetType::Button ||
                    widget.type == HudWidgetType::Toggle) &&
                !widget.bind.empty() && !widget.text.empty())
                preview.set_text(widget.bind, widget.text);
        }
        preview.draw_overlay(draw, canvas_min, canvas_max);

        const float scale = letterbox.scale;
        std::vector<std::string> hits;
        std::optional<std::string> hover_id;
        UiCanvasResizeHandle hover_handle = UiCanvasResizeHandle::None;
        ImVec2 selected_min{};
        ImVec2 selected_max{};
        bool has_selected_bounds = false;

        if (scale > 0.0f) {
            for (const auto& widget : session.canvas.widgets) {
                const float width = widget.size[0] * scale;
                const float height = widget.size[1] * scale;
                if (width <= 0.0f || height <= 0.0f) continue;
                const ImVec2 origin = anchored_origin(widget.anchor, content_min, content_max, width, height,
                    widget.offset[0] * scale, widget.offset[1] * scale);
                const ImVec2 max{origin.x + width, origin.y + height};
                if (session.selected_id && *session.selected_id == widget.id) {
                    selected_min = origin;
                    selected_max = max;
                    has_selected_bounds = true;
                }
            }

            if (ImGui::IsItemHovered()) {
                const ImVec2 mouse = ImGui::GetIO().MousePos;
                if (has_selected_bounds && session.edit_mode == UiCanvasEditMode::Idle) {
                    hover_handle = hit_resize_handle(mouse, selected_min, selected_max, kHandleHitPx);
                }
                for (auto it = session.canvas.widgets.rbegin(); it != session.canvas.widgets.rend(); ++it) {
                    const float width = it->size[0] * scale;
                    const float height = it->size[1] * scale;
                    const ImVec2 origin = anchored_origin(it->anchor, content_min, content_max, width, height,
                        it->offset[0] * scale, it->offset[1] * scale);
                    const ImVec2 max{origin.x + width, origin.y + height};
                    if (mouse.x >= origin.x && mouse.x <= max.x && mouse.y >= origin.y && mouse.y <= max.y)
                        hits.push_back(it->id);
                }
                if (!hits.empty()) hover_id = hits.front();
            }

            for (const auto& widget : session.canvas.widgets) {
                const float width = widget.size[0] * scale;
                const float height = widget.size[1] * scale;
                if (width <= 0.0f || height <= 0.0f) continue;
                const ImVec2 origin = anchored_origin(widget.anchor, content_min, content_max, width, height,
                    widget.offset[0] * scale, widget.offset[1] * scale);
                const ImVec2 max{origin.x + width, origin.y + height};
                const bool selected = session.selected_id && *session.selected_id == widget.id;
                const bool hovered = hover_id && *hover_id == widget.id;
                if (selected) {
                    draw->AddRect(origin, max, IM_COL32(255, 210, 80, 255), 0.0f, 0, 2.0f);
                    draw_resize_handles(draw, origin, max);
                } else if (hovered) {
                    draw->AddRect(origin, max, IM_COL32(140, 200, 255, 200), 0.0f, 0, 1.5f);
                }
            }
        }

        if (ImGui::IsItemClicked() && session.edit_mode == UiCanvasEditMode::Idle) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            if (has_selected_bounds) {
                const auto handle = hit_resize_handle(mouse, selected_min, selected_max, kHandleHitPx);
                if (handle != UiCanvasResizeHandle::None) {
                    if (auto* widget = find_widget(session.canvas, *session.selected_id)) {
                        session.edit_mode = UiCanvasEditMode::Resize;
                        session.resize_handle = handle;
                        session.drag_start_mouse_x = mouse.x;
                        session.drag_start_mouse_y = mouse.y;
                        const DesignRect rect = design_rect(*widget, design_w, design_h);
                        session.drag_start_rect = {{rect.x, rect.y, rect.w, rect.h}};
                    }
                }
            }
            if (session.edit_mode == UiCanvasEditMode::Idle) {
                if (hits.empty()) {
                    session.selected_id.reset();
                } else {
                    std::string pick = hits.front();
                    if (ImGui::GetIO().KeyAlt && session.selected_id) {
                        const auto it = std::find(hits.begin(), hits.end(), *session.selected_id);
                        if (it != hits.end()) {
                            const auto next = std::next(it);
                            pick = next != hits.end() ? *next : hits.front();
                        }
                    }
                    session.selected_id = pick;
                    if (auto* widget = find_widget(session.canvas, *session.selected_id)) {
                        session.edit_mode = UiCanvasEditMode::Move;
                        session.drag_start_mouse_x = mouse.x;
                        session.drag_start_mouse_y = mouse.y;
                        session.drag_start_offset = widget->offset;
                    }
                }
            }
        }

        if (session.edit_mode != UiCanvasEditMode::Idle && session.selected_id &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left) && scale > 0.0f) {
            if (auto* widget = find_widget(session.canvas, *session.selected_id)) {
                const float dx = (ImGui::GetIO().MousePos.x - session.drag_start_mouse_x) / scale;
                const float dy = (ImGui::GetIO().MousePos.y - session.drag_start_mouse_y) / scale;
                if (session.edit_mode == UiCanvasEditMode::Move) {
                    widget->offset[0] = session.drag_start_offset[0] + dx;
                    widget->offset[1] = session.drag_start_offset[1] + dy;
                    session.dirty = true;
                } else if (session.edit_mode == UiCanvasEditMode::Resize) {
                    DesignRect start{session.drag_start_rect[0], session.drag_start_rect[1], session.drag_start_rect[2],
                        session.drag_start_rect[3]};
                    apply_design_rect(*widget, resize_rect(start, session.resize_handle, dx, dy), design_w, design_h);
                    session.dirty = true;
                }
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            session.edit_mode = UiCanvasEditMode::Idle;
            session.resize_handle = UiCanvasResizeHandle::None;
        }

        (void)hover_handle;
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("UICanvasInspector", ImVec2(inspector_w, avail.y), true);
    draw_inspector(session);
    ImGui::EndChild();
}

} // namespace engine
