#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace engine {

enum class AutomationTraceChannel { Mcp, EditorBridge };

class AutomationTrace final {
public:
    static void set_enabled(bool enabled) noexcept;
    [[nodiscard]] static bool enabled() noexcept;
    static void set_log_root(const std::filesystem::path& root);
    static void log(AutomationTraceChannel channel, std::string event, std::string detail = {});
    static void log(AutomationTraceChannel channel, std::string event,
        const std::map<std::string, std::string>& fields);
    [[nodiscard]] static std::filesystem::path log_path(AutomationTraceChannel channel);
};

} // namespace engine
