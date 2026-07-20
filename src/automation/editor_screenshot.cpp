#include "engine/automation/editor_screenshot.h"

#include <wincodec.h>
#include <wrl/client.h>

#include <cctype>
#include <chrono>
#include <cstdio>
#include <optional>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace engine {
namespace {

using Microsoft::WRL::ComPtr;

EngineError shot_hr(const char* code, const char* message, HRESULT hr) {
    char buf[160]{};
    std::snprintf(buf, sizeof(buf), "%s (hr=0x%08lX)", message, static_cast<unsigned long>(hr));
    return EngineError{code, Severity::Error, ErrorCategory::Io, "automation", buf, std::nullopt, {},
        "Check editor window and disk permissions."};
}

} // namespace

Result<std::filesystem::path> capture_window_png(void* hwnd_void, const std::filesystem::path& project_root,
    const std::string& filename_stem, bool client_area_only) {
    HWND hwnd = static_cast<HWND>(hwnd_void);
    if (!hwnd || !IsWindow(hwnd)) {
        return Result<std::filesystem::path>::failure(
            EngineError{"SHOT-HWND", Severity::Error, ErrorCategory::Io, "automation",
                "Editor window handle is invalid", std::nullopt, {}, "Ensure the editor is running."});
    }

    RECT rect{};
    if (client_area_only) {
        if (!GetClientRect(hwnd, &rect)) {
            return Result<std::filesystem::path>::failure(
                EngineError{"SHOT-RECT", Severity::Error, ErrorCategory::Io, "automation", "GetClientRect failed",
                    std::nullopt, {}, "Retry with the editor focused."});
        }
    } else {
        if (!GetWindowRect(hwnd, &rect)) {
            return Result<std::filesystem::path>::failure(
                EngineError{"SHOT-RECT", Severity::Error, ErrorCategory::Io, "automation", "GetWindowRect failed",
                    std::nullopt, {}, "Retry with the editor focused."});
        }
    }
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return Result<std::filesystem::path>::failure(
            EngineError{"SHOT-SIZE", Severity::Error, ErrorCategory::Io, "automation", "Window size is empty",
                std::nullopt, {}, "Restore the editor window if minimized."});
    }

    HDC window_dc = client_area_only ? GetDC(hwnd) : GetWindowDC(hwnd);
    if (!window_dc) {
        return Result<std::filesystem::path>::failure(
            EngineError{"SHOT-DC", Severity::Error, ErrorCategory::Io, "automation", "Could not get window DC",
                std::nullopt, {}, "Retry screenshot."});
    }
    HDC mem_dc = CreateCompatibleDC(window_dc);
    HBITMAP dib = nullptr;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    dib = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = nullptr;
    bool ok = false;
    if (mem_dc && dib && bits) {
        old = SelectObject(mem_dc, dib);
        // PW_RENDERFULLCONTENT (2) captures D3D/ImGui contents; fall back to BitBlt.
        ok = PrintWindow(hwnd, mem_dc, 2) == TRUE;
        if (!ok) ok = BitBlt(mem_dc, 0, 0, width, height, window_dc, 0, 0, SRCCOPY) == TRUE;
    }
    if (old) SelectObject(mem_dc, old);
    if (mem_dc) DeleteDC(mem_dc);
    if (client_area_only) ReleaseDC(hwnd, window_dc);
    else ReleaseDC(hwnd, window_dc);

    if (!ok || !bits) {
        if (dib) DeleteObject(dib);
        return Result<std::filesystem::path>::failure(
            EngineError{"SHOT-BLIT", Severity::Error, ErrorCategory::Io, "automation", "Window capture blit failed",
                std::nullopt, {}, "Retry after the editor finishes a frame."});
    }

    // BGRA -> RGBA for WIC
    auto* pixels = static_cast<std::uint8_t*>(bits);
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (std::size_t i = 0; i < count; ++i) {
        std::uint8_t* p = pixels + i * 4;
        const std::uint8_t b = p[0];
        p[0] = p[2];
        p[2] = b;
        p[3] = 255;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool need_uninit = SUCCEEDED(hr);
    ComPtr<IWICImagingFactory> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        DeleteObject(dib);
        if (need_uninit) CoUninitialize();
        return Result<std::filesystem::path>::failure(shot_hr("SHOT-WIC", "WIC factory failed", hr));
    }

    const auto out_dir = project_root / "out";
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    std::string stem = filename_stem.empty() ? "editor-screenshot" : filename_stem;
    for (char& c : stem) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.')) c = '-';
    }
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const auto out_path = out_dir / (stem + "-" + std::to_string(stamp) + ".png");

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) {
        DeleteObject(dib);
        if (need_uninit) CoUninitialize();
        return Result<std::filesystem::path>::failure(shot_hr("SHOT-STREAM", "WIC stream failed", hr));
    }
    hr = stream->InitializeFromFilename(out_path.wstring().c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        DeleteObject(dib);
        if (need_uninit) CoUninitialize();
        return Result<std::filesystem::path>::failure(shot_hr("SHOT-FILE", "Could not open PNG for write", hr));
    }
    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) {
        DeleteObject(dib);
        if (need_uninit) CoUninitialize();
        return Result<std::filesystem::path>::failure(shot_hr("SHOT-ENC", "PNG encoder failed", hr));
    }
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, &props);
    if (SUCCEEDED(hr)) hr = frame->Initialize(props.Get());
    if (SUCCEEDED(hr)) hr = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&format);
    const UINT stride = static_cast<UINT>(width * 4);
    const UINT buffer_size = stride * static_cast<UINT>(height);
    if (SUCCEEDED(hr)) hr = frame->WritePixels(static_cast<UINT>(height), stride, buffer_size, pixels);
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();

    DeleteObject(dib);
    if (need_uninit) CoUninitialize();
    if (FAILED(hr)) {
        return Result<std::filesystem::path>::failure(shot_hr("SHOT-WRITE", "PNG write failed", hr));
    }
    return Result<std::filesystem::path>::success(out_path);
}

} // namespace engine
