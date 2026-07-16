#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace engine {

enum class ErrorCategory : std::uint8_t {
    Configuration, Validation, Io, AssetImport, Serialization, Scripting,
    Graphics, DeviceRemoval, Physics, SaveMigration, InternalInvariant
};

enum class Severity : std::uint8_t { Info, Warning, Error, Fatal };
enum class ErrorPriority : std::uint8_t { P0Critical, P1High, P2Normal, P3Low };

struct SourceContext {
    std::string file;
    std::uint32_t line = 0;
    std::string function;
};

struct EngineError {
    std::string code;
    Severity severity = Severity::Error;
    ErrorCategory category = ErrorCategory::InternalInvariant;
    std::string subsystem;
    std::string message;
    std::optional<SourceContext> source;
    std::vector<std::string> causes;
    std::string remediation;
    std::string correlation_id;
    ErrorPriority priority = ErrorPriority::P2Normal;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] std::string to_text() const;
};

[[nodiscard]] std::string make_correlation_id();
[[nodiscard]] const char* to_string(ErrorCategory value) noexcept;
[[nodiscard]] const char* to_string(Severity value) noexcept;
[[nodiscard]] const char* to_string(ErrorPriority value) noexcept;

} // namespace engine

#define ENGINE_SOURCE_CONTEXT ::engine::SourceContext{__FILE__, static_cast<std::uint32_t>(__LINE__), __func__}
