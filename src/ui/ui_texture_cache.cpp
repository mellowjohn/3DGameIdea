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
    if (mode == HudImageMode::NineSlice || mode != HudImageMode::Contain || !(tex_w > 0.0f) || !(tex_h > 0.0f))
        return;
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

RgbaImage resample_rgba_area(
    const std::uint8_t* pixels, unsigned width, unsigned height, unsigned target_width, unsigned target_height) {
    RgbaImage out{};
    if (!pixels || width == 0 || height == 0 || target_width == 0 || target_height == 0) return out;
    out.width = target_width;
    out.height = target_height;
    out.pixels.assign(static_cast<std::size_t>(target_width) * target_height * 4, 0);

    const double x_ratio = static_cast<double>(width) / static_cast<double>(target_width);
    const double y_ratio = static_cast<double>(height) / static_cast<double>(target_height);
    for (unsigned y = 0; y < target_height; ++y) {
        const unsigned y0 = static_cast<unsigned>(y * y_ratio);
        const unsigned y1 = std::max(y0 + 1, std::min(height, static_cast<unsigned>((y + 1) * y_ratio)));
        for (unsigned x = 0; x < target_width; ++x) {
            const unsigned x0 = static_cast<unsigned>(x * x_ratio);
            const unsigned x1 = std::max(x0 + 1, std::min(width, static_cast<unsigned>((x + 1) * x_ratio)));
            double colour[3] = {0.0, 0.0, 0.0};
            double alpha_sum = 0.0;
            unsigned samples = 0;
            for (unsigned sy = y0; sy < y1; ++sy) {
                for (unsigned sx = x0; sx < x1; ++sx) {
                    const std::size_t index = (static_cast<std::size_t>(sy) * width + sx) * 4;
                    const double alpha = static_cast<double>(pixels[index + 3]) / 255.0;
                    colour[0] += static_cast<double>(pixels[index + 0]) * alpha;
                    colour[1] += static_cast<double>(pixels[index + 1]) * alpha;
                    colour[2] += static_cast<double>(pixels[index + 2]) * alpha;
                    alpha_sum += alpha;
                    ++samples;
                }
            }
            const std::size_t out_index = (static_cast<std::size_t>(y) * target_width + x) * 4;
            if (samples == 0) continue;
            if (alpha_sum > 0.0) {
                for (int c = 0; c < 3; ++c) {
                    out.pixels[out_index + c] = static_cast<std::uint8_t>(
                        std::clamp(std::lround(colour[c] / alpha_sum), 0L, 255L));
                }
            }
            out.pixels[out_index + 3] = static_cast<std::uint8_t>(
                std::clamp(std::lround(alpha_sum / static_cast<double>(samples) * 255.0), 0L, 255L));
        }
    }
    return out;
}

void ui_texture_prefilter_size(unsigned source_width, unsigned source_height, unsigned desired_width,
    unsigned desired_height, unsigned& out_width, unsigned& out_height) {
    out_width = source_width;
    out_height = source_height;
    if (source_width == 0 || source_height == 0 || desired_width == 0 || desired_height == 0) return;

    const auto round_up_pow2 = [](unsigned value) {
        unsigned result = 32;
        while (result < value && result < (1u << 14)) result <<= 1;
        return result;
    };
    const unsigned bucket_w = round_up_pow2(desired_width);
    const unsigned bucket_h = round_up_pow2(desired_height);
    const double scale = std::min(1.0,
        std::max(static_cast<double>(bucket_w) / static_cast<double>(source_width),
            static_cast<double>(bucket_h) / static_cast<double>(source_height)));
    if (scale >= 1.0) return;
    out_width = std::max(1u, static_cast<unsigned>(std::lround(source_width * scale)));
    out_height = std::max(1u, static_cast<unsigned>(std::lround(source_height * scale)));
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

std::optional<UiTextureEntry> UiTextureCache::get_or_load(
    const std::string& project_relative_path, unsigned desired_width, unsigned desired_height) {
    if (project_relative_path.empty() || !bound()) return std::nullopt;

    // One cached upload per draw-size bucket so a pre-filtered copy is reused across frames.
    std::string key = project_relative_path;
    if (desired_width > 0 && desired_height > 0) {
        unsigned bucket_w = 0;
        unsigned bucket_h = 0;
        ui_texture_prefilter_size(1u << 14, 1u << 14, desired_width, desired_height, bucket_w, bucket_h);
        key += "@" + std::to_string(bucket_w) + "x" + std::to_string(bucket_h);
    }
    if (const auto it = by_path_.find(key); it != by_path_.end()) return it->second;
    if (failed_.find(key) != failed_.end()) return std::nullopt;
    if (next_slot_ >= srv_count_) {
        Logger::instance().write(Severity::Warning, "ui_texture",
            "UI texture SRV pool exhausted (need more imgui heap slots)");
        failed_[key] = true;
        return std::nullopt;
    }

    const auto path = resolve_path(project_relative_path);
    ID3D12Resource* raw = nullptr;
    const unsigned srv_index = srv_base_ + next_slot_;
    PngSourceSize source{};
    auto loaded = load_png_imgui_srv(device_, queue_, srv_heap_, srv_stride_, srv_index, path, &raw, wait_for_gpu_,
        desired_width, desired_height, &source);
    if (!loaded || !raw) {
        Logger::instance().write(Severity::Warning, "ui_texture",
            "UI PNG failed: " + project_relative_path + " (" + path.generic_string() + ")");
        failed_[key] = true;
        return std::nullopt;
    }

    UiTextureEntry entry{};
    entry.imgui_tex_id = loaded.value();
    if (source.width > 0 && source.height > 0) {
        entry.width = source.width;
        entry.height = source.height;
    } else {
        const D3D12_RESOURCE_DESC desc = raw->GetDesc();
        entry.width = static_cast<unsigned>(desc.Width);
        entry.height = static_cast<unsigned>(desc.Height);
    }
    resources_.push_back(raw);
    by_path_[key] = entry;
    ++next_slot_;
    return entry;
}

} // namespace engine
