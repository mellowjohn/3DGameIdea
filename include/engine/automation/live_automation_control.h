#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace engine {

/// Project-scoped request file agents write so a running editor can enable/disable the live bridge
/// without a Diagnostics checkbox click.
[[nodiscard]] std::filesystem::path live_automation_request_path(const std::filesystem::path& project_root);

/// Writes `enable` or `disable` for the editor to consume on the next frame.
[[nodiscard]] bool write_live_automation_request(const std::filesystem::path& project_root, bool enable,
    std::string* error_out = nullptr);

/// If a pending request exists, returns desired enabled state and removes the file.
[[nodiscard]] std::optional<bool> consume_live_automation_request(const std::filesystem::path& project_root);

} // namespace engine
