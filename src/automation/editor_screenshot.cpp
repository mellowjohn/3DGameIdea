#include "engine/automation/editor_screenshot.h"

#include <wincodec.h>
#include <wrl/client.h>

#include <cctype>
#include <chrono>
#include <cstdio>
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

std::string sanitize_stem(std::string stem) {
    if (stem.empty()) stem = "editor-screenshot";
    for (char& c : stem) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.')) c = '-';
    }
    return stem;
}

std::filesystem::path make_out_path(const std::filesystem::path& project_root, const std::string& filename_stem) {
    const auto out_dir = project_root / "out";
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    return out_dir / (sanitize_stem(filename_stem) + "-" + std::to_string(stamp) + ".png");
}

Result<std::filesystem::path> encode_rgba_png(const std::filesystem::path& out_path, std::uint32_t width,
    std::uint32_t height, std::span<const std::uint8_t> rgba_bytes) {
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    if (width == 0 || height == 0 || rgba_bytes.size() < expected) {
        return Result<std::filesystem::path>::failure(
            EngineError{"SHOT-SIZE", Severity::Error, ErrorCategory::Validation, "automation",
                "RGBA buffer size does not match width/height", std::nullopt, {},
                "Pass tightly packed 32bpp RGBA matching the dimensions."});
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool need_uninit = SUCCEEDED(hr);
    ComPtr<IWICImagingFactory> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        if (need_uninit) CoUninitialize();
        return Result<std::filesystem::path>::failure(shot_hr("SHOT-WIC", "WIC factory failed", hr));
    }

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) {
        if (need_uninit) CoUninitialize();
        return Result<std::filesystem::path>::failure(shot_hr("SHOT-STREAM", "WIC stream failed", hr));
    }
    hr = stream->InitializeFromFilename(out_path.wstring().c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        if (need_uninit) CoUninitialize();
        return Result<std::filesystem::path>::failure(shot_hr("SHOT-FILE", "Could not open PNG for write", hr));
    }
    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) {
        if (need_uninit) CoUninitialize();
        return Result<std::filesystem::path>::failure(shot_hr("SHOT-ENC", "PNG encoder failed", hr));
    }
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, &props);
    if (SUCCEEDED(hr)) hr = frame->Initialize(props.Get());
    if (SUCCEEDED(hr)) hr = frame->SetSize(width, height);
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&format);
    const UINT stride = width * 4u;
    const UINT buffer_size = stride * height;
    // WIC WritePixels needs a non-const pointer; pixels are not mutated.
    auto* pixels = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(rgba_bytes.data()));
    if (SUCCEEDED(hr)) hr = frame->WritePixels(height, stride, buffer_size, pixels);
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();

    if (need_uninit) CoUninitialize();
    if (FAILED(hr)) {
        return Result<std::filesystem::path>::failure(shot_hr("SHOT-WRITE", "PNG write failed", hr));
    }
    return Result<std::filesystem::path>::success(out_path);
}

bool blit_screen_client(HWND hwnd, HDC mem_dc, int width, int height, bool client_area_only) {
    POINT origin{0, 0};
    if (client_area_only) {
        if (!ClientToScreen(hwnd, &origin)) return false;
    } else {
        RECT window_rect{};
        if (!GetWindowRect(hwnd, &window_rect)) return false;
        origin.x = window_rect.left;
        origin.y = window_rect.top;
    }
    HDC screen_dc = GetDC(nullptr);
    if (!screen_dc) return false;
    const bool ok = BitBlt(mem_dc, 0, 0, width, height, screen_dc, origin.x, origin.y, SRCCOPY) == TRUE;
    ReleaseDC(nullptr, screen_dc);
    return ok;
}

} // namespace

Result<std::filesystem::path> write_rgba_png(const std::filesystem::path& project_root,
    const std::string& filename_stem, std::uint32_t width, std::uint32_t height,
    std::span<const std::uint8_t> rgba_bytes) {
    return encode_rgba_png(make_out_path(project_root, filename_stem), width, height, rgba_bytes);
}

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
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = nullptr;
    bool ok = false;
    if (mem_dc && dib && bits) {
        old = SelectObject(mem_dc, dib);
        // Prefer a desktop BitBlt of the composed pixels (matches what you see, including D3D).
        // Fall back to PrintWindow with client+full-content flags for occluded windows.
        ok = blit_screen_client(hwnd, mem_dc, width, height, client_area_only);
        if (!ok) {
            constexpr UINT k_pw_client_only = 0x1;
            constexpr UINT k_pw_render_full_content = 0x2;
            const UINT flags =
                k_pw_render_full_content | (client_area_only ? k_pw_client_only : 0u);
            ok = PrintWindow(hwnd, mem_dc, flags) == TRUE;
        }
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

    // GDI DIB sections are BGRA; convert to tightly packed RGBA for WIC.
    auto* pixels = static_cast<std::uint8_t*>(bits);
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> rgba(count * 4u);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t* p = pixels + i * 4;
        rgba[i * 4 + 0] = p[2];
        rgba[i * 4 + 1] = p[1];
        rgba[i * 4 + 2] = p[0];
        rgba[i * 4 + 3] = 255;
    }
    DeleteObject(dib);

    return write_rgba_png(project_root, filename_stem, static_cast<std::uint32_t>(width),
        static_cast<std::uint32_t>(height), rgba);
}

} // namespace engine
