#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace engine {

struct FileDialogFilter {
    std::string label;   // "glTF model"
    std::string pattern; // "*.gltf;*.glb"
};

/// True when the running platform can present a native open dialog.
[[nodiscard]] bool file_dialog_available();

/// Present a native "open file" dialog. Returns nullopt when cancelled or unsupported.
[[nodiscard]] std::optional<std::filesystem::path> open_file_dialog(const std::string& title,
    const std::vector<FileDialogFilter>& filters, const std::filesystem::path& initial_directory = {});

/// Filters for model import (glTF / GLB, plus Blockbench sources handled by registered bakers).
[[nodiscard]] std::vector<FileDialogFilter> model_file_dialog_filters();

} // namespace engine
