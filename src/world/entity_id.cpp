#include "engine/world/entity_id.h"

#include <array>
#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>

namespace engine {
namespace {
EngineError invalid_id(std::string message) {
    return EngineError{"WORLD-INVALID-UUID", Severity::Error, ErrorCategory::Validation, "world",
                       std::move(message), ENGINE_SOURCE_CONTEXT, {},
                       "Use a lowercase canonical UUID such as xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx.", make_correlation_id()};
}
}

EntityId EntityId::generate() {
    thread_local std::mt19937_64 generator(std::random_device{}());
    std::array<unsigned char, 16> bytes{};
    for (auto& byte : bytes) byte = static_cast<unsigned char>(generator() & 0xffu);
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0fu) | 0x40u);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3fu) | 0x80u);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out << '-';
        out << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return EntityId(out.str());
}

Result<EntityId> EntityId::parse(std::string value) {
    if (value.size() != 36) return Result<EntityId>::failure(invalid_id("UUID must contain 36 characters"));
    for (std::size_t i = 0; i < value.size(); ++i) {
        const bool separator = i == 8 || i == 13 || i == 18 || i == 23;
        if (separator ? value[i] != '-' : !std::isxdigit(static_cast<unsigned char>(value[i])))
            return Result<EntityId>::failure(invalid_id("UUID contains invalid characters or separators"));
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    }
    return Result<EntityId>::success(EntityId(std::move(value)));
}

} // namespace engine

