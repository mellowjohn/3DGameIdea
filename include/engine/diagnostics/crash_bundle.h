#pragma once

#include "engine/core/result.h"

#include <filesystem>

namespace engine {

class CrashBundle final {
public:
    static void install(const std::filesystem::path& root);
    [[nodiscard]] static Result<std::filesystem::path> write_diagnostic_bundle(
        const std::filesystem::path& root, const EngineError& reason);
};

} // namespace engine

