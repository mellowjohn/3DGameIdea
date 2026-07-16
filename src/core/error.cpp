#include "engine/core/error.h"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace engine {
namespace {
std::string escape_json(const std::string& value) {
    std::ostringstream out;
    for (const char c : value) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) out << "?";
            else out << c;
        }
    }
    return out.str();
}
}

const char* to_string(ErrorCategory value) noexcept {
    switch (value) {
    case ErrorCategory::Configuration: return "configuration";
    case ErrorCategory::Validation: return "validation";
    case ErrorCategory::Io: return "io";
    case ErrorCategory::AssetImport: return "asset_import";
    case ErrorCategory::Serialization: return "serialization";
    case ErrorCategory::Scripting: return "scripting";
    case ErrorCategory::Graphics: return "graphics";
    case ErrorCategory::DeviceRemoval: return "device_removal";
    case ErrorCategory::Physics: return "physics";
    case ErrorCategory::SaveMigration: return "save_migration";
    case ErrorCategory::InternalInvariant: return "internal_invariant";
    }
    return "unknown";
}

const char* to_string(Severity value) noexcept {
    switch (value) {
    case Severity::Info: return "info";
    case Severity::Warning: return "warning";
    case Severity::Error: return "error";
    case Severity::Fatal: return "fatal";
    }
    return "unknown";
}
const char* to_string(ErrorPriority value) noexcept { switch(value){case ErrorPriority::P0Critical:return "P0";case ErrorPriority::P1High:return "P1";case ErrorPriority::P2Normal:return "P2";case ErrorPriority::P3Low:return "P3";}return "P2"; }

std::string make_correlation_id() {
    static std::atomic<std::uint64_t> sequence{0};
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream out;
    out << std::hex << ticks << '-' << sequence.fetch_add(1, std::memory_order_relaxed);
    return out.str();
}

std::string EngineError::to_json() const {
    std::ostringstream out;
    out << "{\"code\":\"" << escape_json(code) << "\",\"severity\":\"" << to_string(severity)
        << "\",\"category\":\"" << to_string(category) << "\",\"subsystem\":\""
        << escape_json(subsystem) << "\",\"priority\":\"" << to_string(priority) << "\",\"message\":\"" << escape_json(message)
        << "\",\"correlationId\":\"" << escape_json(correlation_id) << "\",\"remediation\":\""
        << escape_json(remediation) << "\",\"causes\":[";
    for (std::size_t i = 0; i < causes.size(); ++i) {
        if (i) out << ',';
        out << "\"" << escape_json(causes[i]) << "\"";
    }
    out << ']';
    if (source) {
        out << ",\"source\":{\"file\":\"" << escape_json(source->file) << "\",\"line\":" << source->line
            << ",\"function\":\"" << escape_json(source->function) << "\"}";
    }
    out << '}';
    return out.str();
}

std::string EngineError::to_text() const {
    std::ostringstream out;
    out << '[' << to_string(severity) << "][" << to_string(priority) << "] " << code << " (" << subsystem << "): " << message;
    if (!remediation.empty()) out << "\n  Remedy: " << remediation;
    out << "\n  Correlation: " << correlation_id;
    return out.str();
}

} // namespace engine
