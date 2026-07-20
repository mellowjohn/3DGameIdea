#include "engine/automation/live_automation_control.h"

#include <cctype>
#include <fstream>
#include <string>

namespace engine {
namespace {

constexpr const char* k_request_rel = ".engine/live-automation.request";

} // namespace

std::filesystem::path live_automation_request_path(const std::filesystem::path& project_root) {
    return project_root / k_request_rel;
}

bool write_live_automation_request(const std::filesystem::path& project_root, bool enable, std::string* error_out) {
    try {
        const auto path = live_automation_request_path(project_root);
        std::filesystem::create_directories(path.parent_path());
        const auto tmp = path.string() + ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) {
                if (error_out) *error_out = "Could not open live-automation request temp file";
                return false;
            }
            out << (enable ? "enable" : "disable") << '\n';
        }
        std::filesystem::rename(tmp, path);
        return true;
    } catch (const std::exception& ex) {
        if (error_out) *error_out = ex.what();
        return false;
    }
}

std::optional<bool> consume_live_automation_request(const std::filesystem::path& project_root) {
    const auto path = live_automation_request_path(project_root);
    if (!std::filesystem::exists(path)) return std::nullopt;
    std::string body;
    try {
        std::ifstream in(path, std::ios::binary);
        if (!in) return std::nullopt;
        std::getline(in, body);
        in.close();
        std::filesystem::remove(path);
    } catch (...) {
        return std::nullopt;
    }
    while (!body.empty() && (body.back() == '\r' || body.back() == '\n' || body.back() == ' ')) body.pop_back();
    for (char& c : body) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (body == "enable" || body == "1" || body == "true" || body == "on") return true;
    if (body == "disable" || body == "0" || body == "false" || body == "off") return false;
    return std::nullopt;
}

} // namespace engine
