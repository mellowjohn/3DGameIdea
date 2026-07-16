#include "engine/ui/imgui_png_texture.h"

#include "engine/core/error.h"

#include <d3d12.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace engine {
namespace {

using Microsoft::WRL::ComPtr;

EngineError png_error(std::string code, std::string message, std::string remedy = {}) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "imgui_png",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

EngineError png_hr_error(std::string code, std::string message, HRESULT hr) {
    auto error = png_error(std::move(code), std::move(message), "Check PNG path and Direct3D device state.");
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(hr));
    error.causes.push_back(buf);
    return error;
}

} // namespace

Result<std::uint64_t> load_png_imgui_srv(ID3D12Device* device, ID3D12CommandQueue* queue,
    ID3D12DescriptorHeap* srv_heap, unsigned srv_stride, unsigned srv_index, const std::filesystem::path& path,
    ID3D12Resource** out_resource, const std::function<void()>& wait_for_gpu) {
    if (!device || !queue || !srv_heap || !out_resource || !wait_for_gpu)
        return Result<std::uint64_t>::failure(png_error("PNG-ARGS", "Missing D3D12 arguments for PNG upload"));
    if (!std::filesystem::exists(path))
        return Result<std::uint64_t>::failure(
            png_error("PNG-MISSING", "PNG not found: " + path.generic_string(), "Restore assets/world-forge/placeholders."));

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_inited = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    if (!com_inited && FAILED(hr))
        return Result<std::uint64_t>::failure(png_hr_error("PNG-COM", "Could not initialize COM for WIC", hr));

    ComPtr<IWICImagingFactory> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return Result<std::uint64_t>::failure(png_hr_error("PNG-WIC-FACTORY", "Could not create WIC factory", hr));

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(path.wstring().c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr))
        return Result<std::uint64_t>::failure(png_hr_error("PNG-DECODE", "Could not decode PNG", hr));

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return Result<std::uint64_t>::failure(png_hr_error("PNG-FRAME", "Could not read PNG frame", hr));

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr))
        return Result<std::uint64_t>::failure(png_hr_error("PNG-CONVERTER", "Could not create WIC converter", hr));

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
        return Result<std::uint64_t>::failure(png_hr_error("PNG-FORMAT", "Could not convert PNG to RGBA", hr));

    UINT width = 0, height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0)
        return Result<std::uint64_t>::failure(png_hr_error("PNG-SIZE", "Invalid PNG dimensions", hr));

    const UINT stride = width * 4;
    const UINT image_size = stride * height;
    std::vector<std::uint8_t> pixels(image_size);
    hr = converter->CopyPixels(nullptr, stride, image_size, pixels.data());
    if (FAILED(hr))
        return Result<std::uint64_t>::failure(png_hr_error("PNG-PIXELS", "Could not copy PNG pixels", hr));

    D3D12_RESOURCE_DESC tex_desc{};
    tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex_desc.Width = width;
    tex_desc.Height = height;
    tex_desc.DepthOrArraySize = 1;
    tex_desc.MipLevels = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES default_heap{};
    default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    ComPtr<ID3D12Resource> texture;
    hr = device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &tex_desc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture));
    if (FAILED(hr))
        return Result<std::uint64_t>::failure(png_hr_error("PNG-TEXTURE", "Could not create D3D12 texture", hr));

    UINT64 upload_bytes = 0;
    device->GetCopyableFootprints(&tex_desc, 0, 1, 0, nullptr, nullptr, nullptr, &upload_bytes);

    D3D12_HEAP_PROPERTIES upload_heap{};
    upload_heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC upload_desc{};
    upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    upload_desc.Width = upload_bytes;
    upload_desc.Height = 1;
    upload_desc.DepthOrArraySize = 1;
    upload_desc.MipLevels = 1;
    upload_desc.SampleDesc.Count = 1;
    upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> upload;
    hr = device->CreateCommittedResource(&upload_heap, D3D12_HEAP_FLAG_NONE, &upload_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
    if (FAILED(hr))
        return Result<std::uint64_t>::failure(png_hr_error("PNG-UPLOAD", "Could not create upload buffer", hr));

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT num_rows = 0;
    UINT64 row_size = 0;
    UINT64 total = 0;
    device->GetCopyableFootprints(&tex_desc, 0, 1, 0, &footprint, &num_rows, &row_size, &total);

    void* mapped = nullptr;
    hr = upload->Map(0, nullptr, &mapped);
    if (FAILED(hr))
        return Result<std::uint64_t>::failure(png_hr_error("PNG-MAP", "Could not map upload buffer", hr));
    auto* dst = static_cast<std::uint8_t*>(mapped);
    for (UINT y = 0; y < height; ++y) {
        std::memcpy(dst + footprint.Offset + static_cast<std::size_t>(y) * footprint.Footprint.RowPitch,
            pixels.data() + static_cast<std::size_t>(y) * stride, stride);
    }
    upload->Unmap(0, nullptr);

    ComPtr<ID3D12CommandAllocator> allocator;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
    if (FAILED(hr))
        return Result<std::uint64_t>::failure(png_hr_error("PNG-ALLOCATOR", "Could not create upload allocator", hr));
    ComPtr<ID3D12GraphicsCommandList> list;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list));
    if (FAILED(hr))
        return Result<std::uint64_t>::failure(png_hr_error("PNG-CMDLIST", "Could not create upload command list", hr));

    D3D12_TEXTURE_COPY_LOCATION dst_loc{};
    dst_loc.pResource = texture.Get();
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_loc.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION src_loc{};
    src_loc.pResource = upload.Get();
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src_loc.PlacedFootprint = footprint;
    list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->ResourceBarrier(1, &barrier);
    list->Close();

    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    wait_for_gpu();

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = srv_heap->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += static_cast<SIZE_T>(srv_index) * srv_stride;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = srv_heap->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += static_cast<SIZE_T>(srv_index) * srv_stride;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(texture.Get(), &srv, cpu);

    *out_resource = texture.Detach();
    return Result<std::uint64_t>::success(static_cast<std::uint64_t>(gpu.ptr));
}

} // namespace engine
