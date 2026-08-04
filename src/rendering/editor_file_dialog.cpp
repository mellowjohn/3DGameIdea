#include "engine/editor/file_dialog.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shobjidl.h>

#include <string>
#endif

namespace engine {
namespace {

#if defined(_WIN32)
std::wstring widen(const std::string& text) {
    if (text.empty()) return {};
    const int needed =
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), needed);
    return wide;
}

/// RAII for the per-call COM apartment. The editor thread may already be initialized by SDL;
/// RPC_E_CHANGED_MODE means "someone else owns it", which is still usable for the shell dialog.
struct ComScope {
    bool uninitialize = false;
    ComScope() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        uninitialize = SUCCEEDED(hr);
    }
    ~ComScope() {
        if (uninitialize) CoUninitialize();
    }
    ComScope(const ComScope&) = delete;
    ComScope& operator=(const ComScope&) = delete;
};
#endif

} // namespace

bool file_dialog_available() {
#if defined(_WIN32)
    return true;
#else
    return false;
#endif
}

std::vector<FileDialogFilter> model_file_dialog_filters() {
    return {
        {"Model files (glTF, GLB, Blockbench)", "*.gltf;*.glb;*.bbmodel"},
        {"glTF (*.gltf)", "*.gltf"},
        {"Binary glTF (*.glb)", "*.glb"},
        {"Blockbench (*.bbmodel)", "*.bbmodel"},
        {"All files", "*.*"},
    };
}

std::optional<std::filesystem::path> open_file_dialog(const std::string& title,
    const std::vector<FileDialogFilter>& filters, const std::filesystem::path& initial_directory) {
#if defined(_WIN32)
    ComScope com;
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog,
            reinterpret_cast<void**>(&dialog))) ||
        dialog == nullptr) {
        return std::nullopt;
    }

    const auto wide_title = widen(title);
    if (!wide_title.empty()) dialog->SetTitle(wide_title.c_str());

    // COMDLG_FILTERSPEC borrows the strings, so the widened copies must outlive the Show() call.
    std::vector<std::wstring> labels;
    std::vector<std::wstring> patterns;
    labels.reserve(filters.size());
    patterns.reserve(filters.size());
    for (const auto& filter : filters) {
        labels.push_back(widen(filter.label));
        patterns.push_back(widen(filter.pattern));
    }
    std::vector<COMDLG_FILTERSPEC> specs;
    specs.reserve(filters.size());
    for (std::size_t i = 0; i < filters.size(); ++i)
        specs.push_back(COMDLG_FILTERSPEC{labels[i].c_str(), patterns[i].c_str()});
    if (!specs.empty()) dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());

    if (!initial_directory.empty()) {
        IShellItem* folder = nullptr;
        const auto wide_dir = initial_directory.lexically_normal().wstring();
        if (SUCCEEDED(SHCreateItemFromParsingName(wide_dir.c_str(), nullptr, IID_IShellItem,
                reinterpret_cast<void**>(&folder))) &&
            folder != nullptr) {
            dialog->SetFolder(folder);
            folder->Release();
        }
    }

    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options)))
        dialog->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);

    std::optional<std::filesystem::path> chosen;
    if (SUCCEEDED(dialog->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
            PWSTR raw = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw)) && raw != nullptr) {
                chosen = std::filesystem::path(raw);
                CoTaskMemFree(raw);
            }
            item->Release();
        }
    }
    dialog->Release();
    return chosen;
#else
    (void)title;
    (void)filters;
    (void)initial_directory;
    return std::nullopt;
#endif
}

} // namespace engine
