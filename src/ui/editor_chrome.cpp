#include "engine/ui/editor_chrome.h"

#include "imgui.h"

#include <algorithm>

namespace engine {
namespace EditorChrome {
namespace {

constexpr ImVec4 kChrome{0.082f, 0.090f, 0.098f, 1.0f};       // #151719
constexpr ImVec4 kPanel{0.125f, 0.137f, 0.149f, 1.0f};        // #202326
constexpr ImVec4 kPanel2{0.161f, 0.176f, 0.188f, 1.0f};       // #292D30
constexpr ImVec4 kBorder{0.227f, 0.251f, 0.267f, 1.0f};       // #3A4044
constexpr ImVec4 kHeader{0.149f, 0.141f, 0.122f, 1.0f};       // #26241F
constexpr ImVec4 kBronze{0.400f, 0.337f, 0.235f, 1.0f};       // #66563C
constexpr ImVec4 kGold{0.835f, 0.725f, 0.471f, 1.0f};         // #D5B978
constexpr ImVec4 kText{0.945f, 0.933f, 0.910f, 1.0f};         // #F1EEE8
constexpr ImVec4 kMuted{0.608f, 0.639f, 0.655f, 1.0f};        // #9BA3A7
constexpr ImVec4 kInk{0.165f, 0.141f, 0.125f, 1.0f};          // #2A2420
constexpr ImVec4 kDanger{0.710f, 0.322f, 0.271f, 1.0f};       // #B55245
constexpr ImVec4 kSaved{0.561f, 0.714f, 0.557f, 1.0f};        // #8FB68E
constexpr ImVec4 kActiveTab{0.231f, 0.212f, 0.188f, 1.0f};    // #3B3630

std::string project_display_name(const std::filesystem::path& project_root) {
    if (project_root.empty()) return "project";
    const auto stem = project_root.filename().string();
    return stem.empty() ? project_root.string() : stem;
}

} // namespace

void apply_style(ImGuiStyle& style) {
    style.WindowRounding = 3.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = kChrome;
    c[ImGuiCol_ChildBg] = kPanel;
    c[ImGuiCol_PopupBg] = kPanel2;
    c[ImGuiCol_Border] = kBorder;
    c[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_FrameBg] = kPanel2;
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.231f, 0.212f, 0.188f, 1.0f);
    c[ImGuiCol_FrameBgActive] = kActiveTab;
    c[ImGuiCol_TitleBg] = kHeader;
    c[ImGuiCol_TitleBgActive] = kHeader;
    c[ImGuiCol_TitleBgCollapsed] = kChrome;
    c[ImGuiCol_MenuBarBg] = kHeader;
    c[ImGuiCol_ScrollbarBg] = kChrome;
    c[ImGuiCol_ScrollbarGrab] = kPanel2;
    c[ImGuiCol_ScrollbarGrabHovered] = kBorder;
    c[ImGuiCol_ScrollbarGrabActive] = kBronze;
    c[ImGuiCol_CheckMark] = kGold;
    c[ImGuiCol_SliderGrab] = kGold;
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.80f, 0.55f, 1.0f);
    c[ImGuiCol_Button] = kPanel2;
    c[ImGuiCol_ButtonHovered] = ImVec4(0.231f, 0.212f, 0.188f, 1.0f);
    c[ImGuiCol_ButtonActive] = kActiveTab;
    c[ImGuiCol_Header] = ImVec4(0.231f, 0.212f, 0.188f, 1.0f);
    c[ImGuiCol_HeaderHovered] = kActiveTab;
    c[ImGuiCol_HeaderActive] = kActiveTab;
    c[ImGuiCol_Separator] = kBorder;
    c[ImGuiCol_SeparatorHovered] = kBronze;
    c[ImGuiCol_SeparatorActive] = kGold;
    c[ImGuiCol_ResizeGrip] = kPanel2;
    c[ImGuiCol_ResizeGripHovered] = kBronze;
    c[ImGuiCol_ResizeGripActive] = kGold;
    c[ImGuiCol_Tab] = kChrome;
    c[ImGuiCol_TabHovered] = ImVec4(0.231f, 0.212f, 0.188f, 1.0f);
    c[ImGuiCol_TabActive] = kActiveTab;
    c[ImGuiCol_TabUnfocused] = kChrome;
    c[ImGuiCol_TabUnfocusedActive] = kPanel;
    c[ImGuiCol_DockingPreview] = ImVec4(kGold.x, kGold.y, kGold.z, 0.35f);
    c[ImGuiCol_Text] = kText;
    c[ImGuiCol_TextDisabled] = kMuted;
    c[ImGuiCol_TextSelectedBg] = ImVec4(kGold.x, kGold.y, kGold.z, 0.35f);
    c[ImGuiCol_PlotHistogram] = kDanger;
    c[ImGuiCol_PlotHistogramHovered] = kGold;
    c[ImGuiCol_TableHeaderBg] = kHeader;
    c[ImGuiCol_TableBorderStrong] = kBorder;
    c[ImGuiCol_TableBorderLight] = ImVec4(kBorder.x, kBorder.y, kBorder.z, 0.55f);
    c[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);
}

void push_panel_colors() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanel);
    ImGui::PushStyleColor(ImGuiCol_Border, kBorder);
    ImGui::PushStyleColor(ImGuiCol_Button, kPanel2);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.231f, 0.212f, 0.188f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kActiveTab);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.231f, 0.212f, 0.188f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kActiveTab);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, kActiveTab);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, kPanel2);
    ImGui::PushStyleColor(ImGuiCol_Text, kText);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, kMuted);
}

void pop_panel_colors() { ImGui::PopStyleColor(11); }

void draw_app_header(const std::filesystem::path& project_root, const char* active_area, bool scene_dirty,
    bool world_forge_dirty, const std::string& status_line, bool* request_save, EditorUiHotspotRegistry* hotspots) {
    const auto* main = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(main->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(main->WorkSize.x, kHeaderHeight), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kHeader);
    ImGui::PushStyleColor(ImGuiCol_Border, kBronze);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 0.0f));
    ImGui::Begin("##EditorAppHeader", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav);
    // Bronze underline
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 p0 = ImGui::GetWindowPos();
        const ImVec2 p1{p0.x + ImGui::GetWindowWidth(), p0.y + kHeaderHeight};
        draw->AddLine(ImVec2{p0.x, p1.y - 1.0f}, ImVec2{p1.x, p1.y - 1.0f},
            ImGui::ColorConvertFloat4ToU32(kBronze), 2.0f);
    }

    ImGui::SetCursorPosY((kHeaderHeight - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::TextColored(kGold, "RPG ENGINE");
    ImGui::SameLine(0.0f, 16.0f);
    ImGui::TextColored(kMuted, "%s /", project_display_name(project_root).c_str());
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextUnformatted(active_area && active_area[0] ? active_area : "Scene");

    const bool dirty = scene_dirty || world_forge_dirty;
    const float right_reserve = 220.0f;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 24.0f, ImGui::GetWindowWidth() - right_reserve));
    if (dirty) {
        ImGui::TextColored(kGold, "Unsaved");
    } else {
        ImGui::TextColored(kSaved, "Saved");
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, kGold);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.80f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.65f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, kInk);
    if (ImGui::Button("Save##EditorChromeSave", ImVec2(72.0f, 0.0f)) && request_save) *request_save = true;
    register_ui_hotspot_last_item(hotspots, "Editor.Header.Save", "Save");
    ImGui::PopStyleColor(4);
    if (!status_line.empty()) {
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::TextColored(kMuted, "%s", status_line.c_str());
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

} // namespace EditorChrome
} // namespace engine
