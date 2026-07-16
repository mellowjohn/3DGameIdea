#pragma once

#include "engine/core/result.h"

#include <filesystem>

namespace engine {

[[nodiscard]] Result<int> run_mcp_server(const std::filesystem::path& project_root);

} // namespace engine
