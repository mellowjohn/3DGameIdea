#pragma once

#include "engine/assets/hud_asset.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12DescriptorHeap;
struct ID3D12Resource;

namespace engine {

struct UiTextureEntry {
    std::uint64_t imgui_tex_id = 0;
    /// Source PNG size. UVs are normalized, so authored nine-slice insets and `contain` fitting
    /// stay correct even when the uploaded texture was pre-filtered smaller.
    unsigned width = 0;
    unsigned height = 0;
};

/// Lazy PNG → ImGui/D3D12 SRV cache for UI canvas `image` fields (TICKET-0164).
class UiTextureCache final {
public:
    UiTextureCache() = default;
    ~UiTextureCache();

    UiTextureCache(const UiTextureCache&) = delete;
    UiTextureCache& operator=(const UiTextureCache&) = delete;

    void bind_device(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12DescriptorHeap* srv_heap,
        unsigned srv_stride, unsigned srv_base, unsigned srv_count, std::function<void()> wait_for_gpu);
    void set_project_root(std::filesystem::path project_root);
    void clear();

    [[nodiscard]] bool bound() const noexcept { return device_ != nullptr && srv_heap_ != nullptr; }
    /// `desired_*` is the widget box in pixels; pass 0 to upload the PNG at full size.
    [[nodiscard]] std::optional<UiTextureEntry> get_or_load(
        const std::string& project_relative_path, unsigned desired_width = 0, unsigned desired_height = 0);

private:
    [[nodiscard]] std::filesystem::path resolve_path(const std::string& relative) const;

    ID3D12Device* device_ = nullptr;
    ID3D12CommandQueue* queue_ = nullptr;
    ID3D12DescriptorHeap* srv_heap_ = nullptr;
    unsigned srv_stride_ = 0;
    unsigned srv_base_ = 0;
    unsigned srv_count_ = 0;
    unsigned next_slot_ = 0;
    std::function<void()> wait_for_gpu_;
    std::filesystem::path project_root_;
    std::unordered_map<std::string, UiTextureEntry> by_path_;
    std::unordered_map<std::string, bool> failed_;
    std::vector<ID3D12Resource*> resources_;
};

/// Fit a texture into a widget box. `stretch` fills the box; `contain` letterboxes;
/// `nine_slice` fills like stretch (border UVs are applied by the draw path).
void hud_image_fit_rect(float box_min_x, float box_min_y, float box_max_x, float box_max_y, float tex_w, float tex_h,
    HudImageMode mode, float& out_min_x, float& out_min_y, float& out_max_x, float& out_max_y);

struct RgbaImage {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<std::uint8_t> pixels; // straight-alpha RGBA8
};

/// Area-average a straight-alpha RGBA8 image down to `target_width` x `target_height`.
/// Filtering happens in premultiplied space so transparent texels do not darken or lighten
/// opaque neighbours. ImGui's D3D12 backend clamps its sampler to mip 0, so large chrome PNGs
/// are pre-filtered on load instead of relying on GPU mips; without this, minifying art like
/// the title logo into a small widget box drops texels and looks chewed.
[[nodiscard]] RgbaImage resample_rgba_area(
    const std::uint8_t* pixels, unsigned width, unsigned height, unsigned target_width, unsigned target_height);

/// Pick the pre-filter size for a PNG drawn into a `desired_width` x `desired_height` box.
/// Sizes are rounded up to a power of two so live resizes do not thrash the cache, aspect is
/// preserved, and upscaling never happens (returns the source size when the box is larger).
void ui_texture_prefilter_size(unsigned source_width, unsigned source_height, unsigned desired_width,
    unsigned desired_height, unsigned& out_width, unsigned& out_height);

} // namespace engine
