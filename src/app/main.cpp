#include "engine/automation/command.h"
#include "engine/automation/automation_trace.h"
#include "engine/automation/mcp_server.h"
#include "engine/diagnostics/crash_bundle.h"
#include "engine/diagnostics/logger.h"

#include <cstdlib>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <windows.h>

namespace {

std::string log_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &seconds);
    std::ostringstream out;
    out << std::put_time(&local, "%Y%m%d-%H%M%S");
    return out.str();
}

std::string safe_path_segment(std::string value) {
    for (char& c : value)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '_';
    return value.empty() ? "engine" : value;
}

std::filesystem::path default_session_log_path(const engine::CommandRequest* request) {
    const std::filesystem::path root = request && !request->project.empty()
                                           ? request->project / "out/logs"
                                           : std::filesystem::path("out/logs");
    const std::string mode = request ? safe_path_segment(request->name) : "invalid";
    return root / ("engine-" + mode + "-" + log_timestamp() + "-pid" +
                   std::to_string(GetCurrentProcessId()) + ".jsonl");
}

std::filesystem::path explicit_log_path(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--log-file") return argv[i + 1];
    return {};
}

std::string argument_value(const engine::CommandRequest& request, const std::string& name) {
    for (std::size_t i = 0; i + 1 < request.arguments.size(); ++i)
        if (request.arguments[i] == name) return request.arguments[i + 1];
    return {};
}

const char* build_configuration() {
#if defined(_DEBUG)
    return "Debug";
#else
    // Release and RelWithDebInfo both set NDEBUG in the MSVC configuration;
    // this is the same build classification reported by benchmark output.
    return "Release";
#endif
}

} // namespace

int main(int argc, char** argv) {
    using namespace engine;
    const std::filesystem::path requested_log_path = explicit_log_path(argc, argv);
    auto parsed = parse_command_line(argc, argv);
    if (!parsed) {
        const auto log_path = requested_log_path.empty()
                                  ? default_session_log_path(nullptr)
                                  : requested_log_path;
        Logger::instance().initialize(log_path);
        Logger::instance().write(parsed.error());
        std::cerr << parsed.error().to_text() << '\n';
        return static_cast<int>(ExitCode::InvalidArguments);
    }
    if (parsed.value().name == "mcp") {
        if (parsed.value().project.empty()) {
            std::cerr << "mcp requires --project\n";
            return static_cast<int>(ExitCode::InvalidArguments);
        }
        if (const char* trace_env = std::getenv("ENGINE_AUTOMATION_TRACE")) {
            if (trace_env[0] == '0' && (trace_env[1] == '\0' || trace_env[1] == '\n')) AutomationTrace::set_enabled(false);
        }
        AutomationTrace::set_log_root(parsed.value().project / "out/logs");
        const auto result = run_mcp_server(parsed.value().project);
        return result ? static_cast<int>(result.value()) : static_cast<int>(ExitCode::InternalError);
    }
    const auto log_path = requested_log_path.empty()
                              ? default_session_log_path(&parsed.value())
                              : requested_log_path;
    Logger::instance().initialize(log_path);
    Logger::instance().set_process_context({{}, {}, parsed.value().name,
                                            parsed.value().project.generic_string(),
                                            argument_value(parsed.value(), "--world"),
                                            build_configuration()});
    Logger::instance().write(Severity::Info,"process","Engine session started; log="+log_path.generic_string());
    CrashBundle::install("crash-bundles");
    const auto response = execute_command(parsed.value());
    for (const auto& error : response.diagnostics) Logger::instance().write(error);
    if (parsed.value().json) std::cout << response.to_json() << '\n';
    else {
        std::cout << response.summary << '\n';
        for (const auto& error : response.diagnostics) std::cout << error.to_text() << '\n';
    }
    Logger::instance().write(Severity::Info,"process","Engine session finished; exitCode="+std::to_string(static_cast<int>(response.exit_code))+"; errors="+std::to_string(Logger::instance().error_count()));
    return static_cast<int>(response.exit_code);
}
