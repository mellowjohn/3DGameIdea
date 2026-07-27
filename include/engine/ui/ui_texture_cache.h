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
    [[nodiscard]] std::optional<UiTextureEntry> get_or_load(const std::string& project_relative_path);

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

/// Fit a texture into a widget box. `stretch` fills the box; `contain` letterboxes.
void hud_image_fit_rect(float box_min_x, float box_min_y, float box_max_x, float box_max_y, float tex_w, float tex_h,
    HudImageMode mode, float& out_min_x, float& out_min_y, float& out_max_x, float& out_max_y);

} // namespace engine
