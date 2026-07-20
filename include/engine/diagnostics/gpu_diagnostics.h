#pragma once

#include <cstdint>
#include <string>

struct IDXGIAdapter1;
struct ID3D12Device;

namespace engine {

struct GpuDiagnostics {
    bool available = false;
    std::string adapter_name;
    std::string driver_version;
    std::uint32_t vendor_id = 0;
    std::uint32_t device_id = 0;
    std::uint64_t dedicated_video_memory = 0;
    std::string feature_level;
    std::string device_removal_hresult;

    [[nodiscard]] static GpuDiagnostics capture();
    [[nodiscard]] static GpuDiagnostics from_device(IDXGIAdapter1* adapter, ID3D12Device* device);
    [[nodiscard]] std::string to_json() const;
};

void set_process_gpu_diagnostics(GpuDiagnostics diagnostics);
void set_process_gpu_device_removal_hresult(long hresult);
[[nodiscard]] GpuDiagnostics process_gpu_diagnostics();

} // namespace engine
