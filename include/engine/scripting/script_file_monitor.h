#pragma once

#include <filesystem>
#include <map>
#include <vector>

namespace engine {

class ScriptFileMonitor final {
public:
    [[nodiscard]] std::vector<std::filesystem::path> poll_changes(const std::filesystem::path& scripts_root);
private:
    std::map<std::filesystem::path, std::filesystem::file_time_type> last_write_times_;
    bool seeded_ = false;
};

} // namespace engine
