#include "engine/ui/ui_texture_cache.h"

#include "engine/diagnostics/logger.h"
#include "engine/ui/imgui_png_texture.h"

#include <d3d12.h>

#include <algorithm>
#include <cmath>

namespace engine {

void hud_image_fit_rect(float box_min_x, float box_min_y, float box_max_x, float box_max_y, float tex_w, float tex_h,
    HudImageMode mode, float& out_min_x, float& out_min_y, float& out_max_x, float& out_max_y) {
    out_min_x = box_min_x;
    out_min_y = box_min_y;
    out_max_x = box_max_x;
    out_max_y = box_max_y;
    if (mode != HudImageMode::Contain || !(tex_w > 0.0f) || !(tex_h > 0.0f)) return;
    const float box_w = box_max_x - box_min_x;
    const float box_h = box_max_y - box_min_y;
    if (!(box_w > 0.0f) || !(box_h > 0.0f)) return;
    const float scale = std::min(box_w / tex_w, box_h / tex_h);
    const float draw_w = tex_w * scale;
    const float draw_h = tex_h * scale;
    out_min_x = box_min_x + (box_w - draw_w) * 0.5f;
    out_min_y = box_min_y + (box_h - draw_h) * 0.5f;
    out_max_x = out_min_x + draw_w;
    out_max_y = out_min_y + draw_h;
}

UiTextureCache::~UiTextureCache() {
    clear();
}

void UiTextureCache::bind_device(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12DescriptorHeap* srv_heap,
    unsigned srv_stride, unsigned srv_base, unsigned srv_count, std::function<void()> wait_for_gpu) {
    clear();
    device_ = device;
    queue_ = queue;
    srv_heap_ = srv_heap;
    srv_stride_ = srv_stride;
    srv_base_ = srv_base;
    srv_count_ = srv_count;
    next_slot_ = 0;
    wait_for_gpu_ = std::move(wait_for_gpu);
}

void UiTextureCache::set_project_root(std::filesystem::path project_root) {
    project_root_ = std::move(project_root);
}

void UiTextureCache::clear() {
    for (ID3D12Resource* resource : resources_) {
        if (resource) resource->Release();
    }
    resources_.clear();
    by_path_.clear();
    failed_.clear();
    next_slot_ = 0;
}

std::filesystem::path UiTextureCache::resolve_path(const std::string& relative) const {
    std::filesystem::path path = relative;
    if (path.is_absolute() && std::filesystem::exists(path)) return path;
    if (!project_root_.empty()) {
        const auto under_project = project_root_ / relative;
        if (std::filesystem::exists(under_project)) return under_project;
    }
#ifdef ENGINE_REPOSITORY_ROOT
    const auto under_repo = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / relative;
    if (std::filesystem::exists(under_repo)) return under_repo;
    if (!project_root_.empty()) {
        const auto repo_project = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / project_root_ / relative;
        if (std::filesystem::exists(repo_project)) return repo_project;
    }
#endif
    return project_root_.empty() ? path : project_root_ / relative;
}

std::optional<UiTextureEntry> UiTextureCache::get_or_load(const std::string& project_relative_path) {
    if (project_relative_path.empty() || !bound()) return std::nullopt;
    if (const auto it = by_path_.find(project_relative_path); it != by_path_.end()) return it->second;
    if (failed_.find(project_relative_path) != failed_.end()) return std::nullopt;
    if (next_slot_ >= srv_count_) {
        Logger::instance().write(Severity::Warning, "ui_texture",
            "UI texture SRV pool exhausted (need more imgui heap slots)");
        failed_[project_relative_path] = true;
        return std::nullopt;
    }

    const auto path = resolve_path(project_relative_path);
    ID3D12Resource* raw = nullptr;
    const unsigned srv_index = srv_base_ + next_slot_;
    auto loaded = load_png_imgui_srv(device_, queue_, srv_heap_, srv_stride_, srv_index, path, &raw, wait_for_gpu_);
    if (!loaded || !raw) {
        Logger::instance().write(Severity::Warning, "ui_texture",
            "UI PNG failed: " + project_relative_path + " (" + path.generic_string() + ")");
        failed_[project_relative_path] = true;
        return std::nullopt;
    }

    UiTextureEntry entry{};
    entry.imgui_tex_id = loaded.value();
    const D3D12_RESOURCE_DESC desc = raw->GetDesc();
    entry.width = static_cast<unsigned>(desc.Width);
    entry.height = static_cast<unsigned>(desc.Height);
    resources_.push_back(raw);
    by_path_[project_relative_path] = entry;
    ++next_slot_;
    return entry;
}

} // namespace engine
