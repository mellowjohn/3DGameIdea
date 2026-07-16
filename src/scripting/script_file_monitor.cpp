#include "engine/scripting/script_file_monitor.h"

namespace engine {

std::vector<std::filesystem::path> ScriptFileMonitor::poll_changes(const std::filesystem::path& scripts_root) {
    std::vector<std::filesystem::path> changed;
    if (!std::filesystem::exists(scripts_root)) return changed;

    std::map<std::filesystem::path, std::filesystem::file_time_type> observed;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(scripts_root)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".lua") continue;
        const auto canonical = entry.path().lexically_normal();
        const auto write_time = entry.last_write_time();
        observed[canonical] = write_time;
        if (!seeded_) continue;
        const auto previous = last_write_times_.find(canonical);
        if (previous == last_write_times_.end() || previous->second != write_time) changed.push_back(canonical);
    }

    for (const auto& entry : last_write_times_) {
        if (observed.find(entry.first) == observed.end()) changed.push_back(entry.first);
    }

    last_write_times_ = std::move(observed);
    seeded_ = true;
    return changed;
}

} // namespace engine
