#include "engine/automation/automation_trace.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace engine {
namespace {

std::mutex g_mutex;
bool g_enabled = true;
std::filesystem::path g_log_root = "out/logs";

std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += static_cast<unsigned char>(c) < 0x20 ? '?' : c; break;
        }
    }
    return out;
}

std::string channel_name(AutomationTraceChannel channel) {
    switch (channel) {
    case AutomationTraceChannel::Mcp: return "mcp";
    case AutomationTraceChannel::EditorBridge: return "editor-bridge";
    }
    return "unknown";
}

std::string file_name(AutomationTraceChannel channel) {
    switch (channel) {
    case AutomationTraceChannel::Mcp: return "mcp-trace.jsonl";
    case AutomationTraceChannel::EditorBridge: return "editor-bridge-trace.jsonl";
    }
    return "automation-trace.jsonl";
}

std::string utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm utc{};
    gmtime_s(&utc, &time);
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms << 'Z';
    return out.str();
}

} // namespace

void AutomationTrace::set_enabled(bool enabled) noexcept { g_enabled = enabled; }

bool AutomationTrace::enabled() noexcept { return g_enabled; }

void AutomationTrace::set_log_root(const std::filesystem::path& root) { g_log_root = root; }

std::filesystem::path AutomationTrace::log_path(AutomationTraceChannel channel) {
    return g_log_root / file_name(channel);
}

void AutomationTrace::log(AutomationTraceChannel channel, std::string event, std::string detail) {
    if (!g_enabled) return;
    std::map<std::string, std::string> fields;
    if (!detail.empty()) fields["detail"] = std::move(detail);
    log(channel, std::move(event), fields);
}

void AutomationTrace::log(AutomationTraceChannel channel, std::string event,
    const std::map<std::string, std::string>& fields) {
    if (!g_enabled) return;
    std::ostringstream line;
    line << "{\"ts\":\"" << utc_timestamp() << "\",\"channel\":\"" << channel_name(channel) << "\",\"event\":\""
         << json_escape(event) << '"';
    for (const auto& entry : fields) {
        line << ",\"" << json_escape(entry.first) << "\":\"" << json_escape(entry.second) << '"';
    }
    line << '}';
    const auto path = log_path(channel);
    std::lock_guard lock(g_mutex);
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::app);
    if (!stream) return;
    stream << line.str() << '\n';
}

} // namespace engine
