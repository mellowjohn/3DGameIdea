#pragma once

#include "engine/core/result.h"

#include <cstdint>
#include <filesystem>
#include <functional>

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Resource;
struct ID3D12DescriptorHeap;

namespace engine {

struct PngSourceSize {
    unsigned width = 0;
    unsigned height = 0;
};

/// Decode a PNG (WIC) and create a D3D12 texture + SRV at `srv_index` in `srv_heap`.
/// On success, `*out_resource` receives an AddRef'd resource (release when done) and the returned
/// value is the GPU descriptor handle cast for use as `ImTextureID`.
///
/// When `desired_width`/`desired_height` are non-zero the decoded image is area-filtered down to
/// roughly that draw size before upload, and `out_source` reports the original PNG size.
[[nodiscard]] Result<std::uint64_t> load_png_imgui_srv(ID3D12Device* device, ID3D12CommandQueue* queue,
    ID3D12DescriptorHeap* srv_heap, unsigned srv_stride, unsigned srv_index, const std::filesystem::path& path,
    ID3D12Resource** out_resource, const std::function<void()>& wait_for_gpu, unsigned desired_width = 0,
    unsigned desired_height = 0, PngSourceSize* out_source = nullptr);

} // namespace engine
