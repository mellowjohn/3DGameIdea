#include "engine/assets/png_decode.h"

#include "engine/core/error.h"

#include <wincodec.h>
#include <wrl/client.h>

#include <cstdio>

namespace engine {
namespace {

using Microsoft::WRL::ComPtr;

EngineError png_error(std::string code, std::string message, std::string remedy = {}) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::AssetImport, "png-decode",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

EngineError png_hr_error(std::string code, std::string message, HRESULT hr) {
    auto error = png_error(std::move(code), std::move(message), "Re-export the texture as a standard 8-bit PNG.");
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(hr));
    error.causes.push_back(buf);
    return error;
}

struct ComGuard {
    bool must_uninitialize = false;
    ComGuard() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        must_uninitialize = SUCCEEDED(hr);
    }
    ~ComGuard() {
        if (must_uninitialize) CoUninitialize();
    }
    ComGuard(const ComGuard&) = delete;
    ComGuard& operator=(const ComGuard&) = delete;
};

Result<PngImage> decode_from_decoder(IWICImagingFactory* factory, IWICBitmapDecoder* decoder) {
    ComPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return Result<PngImage>::failure(png_hr_error("PNG-FRAME", "Could not read PNG frame", hr));

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return Result<PngImage>::failure(png_hr_error("PNG-CONVERTER", "Could not create WIC converter", hr));
    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return Result<PngImage>::failure(png_hr_error("PNG-FORMAT", "Could not convert PNG to RGBA", hr));

    UINT width = 0, height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0)
        return Result<PngImage>::failure(png_hr_error("PNG-SIZE", "Invalid PNG dimensions", hr));

    PngImage image;
    image.width = width;
    image.height = height;
    const UINT stride = width * 4;
    image.rgba.resize(static_cast<std::size_t>(stride) * height);
    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(image.rgba.size()), image.rgba.data());
    if (FAILED(hr)) return Result<PngImage>::failure(png_hr_error("PNG-PIXELS", "Could not copy PNG pixels", hr));
    return Result<PngImage>::success(std::move(image));
}

Result<ComPtr<IWICImagingFactory>> make_factory() {
    ComPtr<IWICImagingFactory> factory;
    const HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return Result<ComPtr<IWICImagingFactory>>::failure(png_hr_error("PNG-WIC-FACTORY", "Could not create WIC factory", hr));
    return Result<ComPtr<IWICImagingFactory>>::success(std::move(factory));
}

} // namespace

Result<PngImage> decode_png_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path))
        return Result<PngImage>::failure(png_error("PNG-MISSING", "PNG not found: " + path.generic_string(),
            "Bake or restore the texture next to its glTF."));
    ComGuard com;
    auto factory = make_factory();
    if (!factory) return Result<PngImage>::failure(factory.error());
    ComPtr<IWICBitmapDecoder> decoder;
    const HRESULT hr = factory.value()->CreateDecoderFromFilename(path.wstring().c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr))
        return Result<PngImage>::failure(png_hr_error("PNG-DECODE", "Could not decode PNG: " + path.generic_string(), hr));
    return decode_from_decoder(factory.value().Get(), decoder.Get());
}

Result<PngImage> decode_png_bytes(std::span<const std::uint8_t> bytes) {
    if (bytes.empty()) return Result<PngImage>::failure(png_error("PNG-EMPTY", "Empty PNG byte buffer"));
    ComGuard com;
    auto factory = make_factory();
    if (!factory) return Result<PngImage>::failure(factory.error());

    ComPtr<IWICStream> stream;
    HRESULT hr = factory.value()->CreateStream(&stream);
    if (FAILED(hr)) return Result<PngImage>::failure(png_hr_error("PNG-STREAM", "Could not create WIC stream", hr));
    // WIC reads the buffer synchronously during decode; the caller-owned span outlives this call.
    hr = stream->InitializeFromMemory(const_cast<WICInProcPointer>(bytes.data()), static_cast<DWORD>(bytes.size()));
    if (FAILED(hr)) return Result<PngImage>::failure(png_hr_error("PNG-STREAM-INIT", "Could not wrap PNG bytes", hr));

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory.value()->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) return Result<PngImage>::failure(png_hr_error("PNG-DECODE", "Could not decode PNG bytes", hr));
    return decode_from_decoder(factory.value().Get(), decoder.Get());
}

} // namespace engine
