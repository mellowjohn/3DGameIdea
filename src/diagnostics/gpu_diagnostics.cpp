#include "engine/diagnostics/gpu_diagnostics.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl/client.h>

#include <iomanip>
#include <mutex>
#include <sstream>

namespace engine {
namespace {

std::mutex g_gpu_mutex;
GpuDiagnostics g_gpu_diagnostics;

std::string escape_json(const std::string& value) {
    std::ostringstream out;
    for (const char character : value) {
        switch (character) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) out << '?';
            else out << character;
        }
    }
    return out.str();
}

std::string utf8(const wchar_t* value) {
    if (!value || !*value) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string output(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, output.data(), size, nullptr, nullptr);
    output.pop_back();
    return output;
}

std::string driver_version(const LARGE_INTEGER version) {
    std::ostringstream out;
    out << HIWORD(version.HighPart) << '.' << LOWORD(version.HighPart) << '.'
        << HIWORD(version.LowPart) << '.' << LOWORD(version.LowPart);
    return out.str();
}

std::string hresult_string(const long hresult) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << static_cast<unsigned long>(hresult);
    return out.str();
}

const char* feature_level_name(const D3D_FEATURE_LEVEL level) {
    switch (level) {
    case D3D_FEATURE_LEVEL_12_1: return "12_1";
    case D3D_FEATURE_LEVEL_12_0: return "12_0";
    case D3D_FEATURE_LEVEL_11_1: return "11_1";
    case D3D_FEATURE_LEVEL_11_0: return "11_0";
    default: return "unknown";
    }
}

GpuDiagnostics from_adapter(IDXGIAdapter1* adapter, ID3D12Device* device) {
    GpuDiagnostics diagnostics;
    if (!adapter) return diagnostics;

    DXGI_ADAPTER_DESC1 description{};
    if (FAILED(adapter->GetDesc1(&description))) return diagnostics;

    diagnostics.available = true;
    diagnostics.adapter_name = utf8(description.Description);
    diagnostics.vendor_id = description.VendorId;
    diagnostics.device_id = description.DeviceId;
    diagnostics.dedicated_video_memory = description.DedicatedVideoMemory;

    LARGE_INTEGER version{};
    if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(ID3D12Device), &version))) {
        diagnostics.driver_version = driver_version(version);
    } else {
        diagnostics.driver_version = "unknown";
    }

    if (device) {
        D3D12_FEATURE_DATA_FEATURE_LEVELS levels{};
        const D3D_FEATURE_LEVEL requested[] = {
            D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        levels.NumFeatureLevels = static_cast<UINT>(std::size(requested));
        levels.pFeatureLevelsRequested = requested;
        if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof(levels)))) {
            diagnostics.feature_level = feature_level_name(levels.MaxSupportedFeatureLevel);
        }
    }
    if (diagnostics.feature_level.empty()) diagnostics.feature_level = "unknown";
    return diagnostics;
}

} // namespace

GpuDiagnostics GpuDiagnostics::capture() {
    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return {};

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT index = 0; factory->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND; ++index) {
        DXGI_ADAPTER_DESC1 description{};
        if (SUCCEEDED(adapter->GetDesc1(&description)) && (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
            Microsoft::WRL::ComPtr<ID3D12Device> device;
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
                return from_adapter(adapter.Get(), device.Get());
            }
        }
        adapter.Reset();
    }
    return {};
}

GpuDiagnostics GpuDiagnostics::from_device(IDXGIAdapter1* adapter, ID3D12Device* device) {
    return from_adapter(adapter, device);
}

std::string GpuDiagnostics::to_json() const {
    std::ostringstream out;
    out << "{\"available\":" << (available ? "true" : "false")
        << ",\"adapterName\":\"" << escape_json(adapter_name)
        << "\",\"driverVersion\":\"" << escape_json(driver_version)
        << "\",\"vendorId\":" << vendor_id
        << ",\"deviceId\":" << device_id
        << ",\"dedicatedVideoMemory\":" << dedicated_video_memory
        << ",\"featureLevel\":\"" << escape_json(feature_level)
        << "\",\"deviceRemovalHresult\":";
    if (device_removal_hresult.empty()) out << "null";
    else out << "\"" << escape_json(device_removal_hresult) << "\"";
    out << '}';
    return out.str();
}

void set_process_gpu_diagnostics(GpuDiagnostics diagnostics) {
    std::lock_guard<std::mutex> lock(g_gpu_mutex);
    g_gpu_diagnostics = std::move(diagnostics);
}

void set_process_gpu_device_removal_hresult(const long hresult) {
    std::lock_guard<std::mutex> lock(g_gpu_mutex);
    g_gpu_diagnostics.device_removal_hresult = hresult_string(hresult);
}

GpuDiagnostics process_gpu_diagnostics() {
    std::lock_guard<std::mutex> lock(g_gpu_mutex);
    return g_gpu_diagnostics;
}

} // namespace engine
